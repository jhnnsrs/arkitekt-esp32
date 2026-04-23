#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

/*
 * Stepper Motor Control via FastAccelStepper
 *
 * Registers two agent functions and one live state:
 *   - stepper_move : move absolute or relative; sign of steps sets direction;
 *                    speed and acceleration are optional per-call overrides
 *   - stepper_stop : smooth deceleration or immediate force-stop
 *
 * Motor position / state is pushed automatically as an AgentState
 * ("stepper_state") via a background task – same pattern as sensor_status.
 *
 * Adjust the pin constants below to match your wiring.
 */

#include "lib/arkitekt_app.h"
#include <FastAccelStepper.h>

// ── Pin configuration ─────────────────────────────────────────────────────────
#if defined(USE_TCA_PINS)
// TCA9535/TCA9555 variant: STEP on a native GPIO, ENABLE + DIR through I2C
// shift-register.  Build with -DUSE_TCA_PINS (env:az-delivery-devkit-v4-tca).
#include <Wire.h>
#include <TCA9555.h>

static constexpr uint8_t TCA_I2C_SDA   = 21;    // SDA GPIO
static constexpr uint8_t TCA_I2C_SCL   = 22;    // SCL GPIO
static constexpr uint8_t TCA_I2C_ADDR  = 0x27;  // A2=A1=A0=1 → 0x27
static constexpr uint8_t TCA_ENABLE    = 0;     // TCA P0.0
static constexpr uint8_t TCA_DIR       = 4;     // TCA P0.4
static constexpr int8_t  STEPPER_STEP_PIN = GPIO_NUM_15; // native ESP32 GPIO

// FastAccelStepper marks external pins with bit 7 set
static constexpr uint8_t PIN_EXT_FLAG  = 0x80;

// Enable is active-LOW on most stepper drivers
static constexpr bool ENABLE_INVERTED  = true;

static TCA9555 tca(TCA_I2C_ADDR);

// Called by FastAccelStepper whenever it drives an external (TCA) pin
static bool externalPinCallback(uint8_t pin, uint8_t value)
{
    uint8_t tcaPin = pin & ~PIN_EXT_FLAG; // strip external-pin flag
    tca.write1(tcaPin, value);
    return value;
}

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define STEPPER_STEP_PIN   GPIO_NUM_8   // GPIO connected to driver STEP input
#define STEPPER_DIR_PIN    GPIO_NUM_7   // GPIO connected to driver DIR  input
#define STEPPER_ENABLE_PIN GPIO_NUM_9   // GPIO connected to driver EN   input; -1 to skip
#else
#define STEPPER_STEP_PIN   GPIO_NUM_15  // GPIO connected to driver STEP input
#define STEPPER_DIR_PIN    GPIO_NUM_25  // GPIO connected to driver DIR  input
#define STEPPER_ENABLE_PIN GPIO_NUM_33  // GPIO connected to driver EN   input; -1 to skip
#endif
// ─────────────────────────────────────────────────────────────────────────────

// Global stepper engine and motor handle (file-scope, persist for program lifetime)
static FastAccelStepperEngine stepperEngine;
static FastAccelStepper *stepper = nullptr;
static bool stepperInitialized = false;

// Initialize the FastAccelStepper engine and bind to the configured pins.
// Call once from setup() before registerAllStepperFunctions().
void initStepper()
{
#if defined(USE_TCA_PINS)
    log_i("Initializing stepper motor (TCA9535 mode)  step=%d  enable=TCA[%d]  dir=TCA[%d]",
          STEPPER_STEP_PIN, TCA_ENABLE, TCA_DIR);

    // Init I2C and TCA9535/TCA9555
    Wire.begin(TCA_I2C_SDA, TCA_I2C_SCL, 100000UL);
    tca.begin();
    tca.pinMode1(TCA_ENABLE, OUTPUT);
    tca.pinMode1(TCA_DIR,    OUTPUT);
    // Disable motor (active-LOW driver: write HIGH = disabled)
    tca.write1(TCA_ENABLE, HIGH);

    stepperEngine.init();
    // Register the TCA callback so FastAccelStepper can drive external pins
    stepperEngine.setExternalCallForPin(externalPinCallback);

    stepper = stepperEngine.stepperConnectToPin(STEPPER_STEP_PIN);
    if (stepper == nullptr)
    {
        Serial.println("✗ Stepper (TCA): failed to connect to step pin " + String(STEPPER_STEP_PIN));
        return;
    }

    // Route ENABLE and DIR through TCA via the external-pin flag
    stepper->setEnablePin(TCA_ENABLE | PIN_EXT_FLAG, ENABLE_INVERTED);
    stepper->setDirectionPin(TCA_DIR | PIN_EXT_FLAG);
    stepper->setAutoEnable(true);
    stepper->setDelayToDisable(500);

    stepper->setSpeedInHz(1000);
    stepper->setAcceleration(500);

    stepperInitialized = true;
    Serial.println("✓ Stepper (TCA) initialized  step=" + String(STEPPER_STEP_PIN) +
                   "  enable=TCA[" + String(TCA_ENABLE) + "]" +
                   "  dir=TCA[" + String(TCA_DIR) + "]");
#else
    log_i("Initializing stepper motor...  step=%d  dir=%d  en=%d",
          STEPPER_STEP_PIN, STEPPER_DIR_PIN, STEPPER_ENABLE_PIN);
    stepperEngine.init();
    stepper = stepperEngine.stepperConnectToPin(STEPPER_STEP_PIN);

    if (stepper == nullptr)
    {
        Serial.println("✗ Stepper: failed to connect to step pin " + String(STEPPER_STEP_PIN));
        return;
    }

    stepper->setDirectionPin(STEPPER_DIR_PIN);

    if (STEPPER_ENABLE_PIN >= 0)
    {
        stepper->setEnablePin(STEPPER_ENABLE_PIN);
        stepper->setAutoEnable(true);
    }

    // Sensible power-on defaults
    stepper->setSpeedInHz(1000);
    stepper->setAcceleration(500);

    stepperInitialized = true;
    Serial.println("✓ Stepper initialized  step=" + String(STEPPER_STEP_PIN) +
                   "  dir=" + String(STEPPER_DIR_PIN) +
                   "  en=" + String(STEPPER_ENABLE_PIN));
#endif
}

// ── Helper: push current motor values into the AgentState ────────────────────
static void updateStepperState(AgentState *state)
{
    if (!state)
        return;

    if (!stepperInitialized || stepper == nullptr)
    {
        state->setPort("current_position", 0);
        state->setPort("target_position",  0);
        state->setPort("is_running",        false);
        state->setPort("speed_hz",          0);
        return;
    }

    state->setPort("current_position", (int)stepper->getCurrentPosition());
    state->setPort("target_position",  (int)stepper->getPositionAfterCommandsCompleted());
    state->setPort("is_running",        stepper->isRunning());
    state->setPort("speed_hz",         (int)(stepper->getCurrentSpeedInMilliHz() / 1000));
}

// ── Register everything with the ArkitektApp ─────────────────────────────────
void registerAllStepperFunctions(ArkitektApp &app)
{
    Serial.println("\n=== Registering Stepper Motor Functions ===");

    // ── State: stepper_state (pushed periodically from background task) ───────
    {
        auto def = StateBuilder("stepper_state", "Stepper Motor State", 256)
            .portInt("current_position", "Current Position", "Steps from origin")
            .portInt("target_position",  "Target Position",  "Steps target (equals current when idle)")
            .portBool("is_running",       "Running",          "True while motor is moving")
            .portInt("speed_hz",          "Speed (steps/s)",  "Instantaneous speed in steps/s")
            .build();

        app.registerState("stepper_state", def, [](AgentState *state)
                          {
                              state->setPort("current_position", 0);
                              state->setPort("target_position",  0);
                              state->setPort("is_running",        false);
                              state->setPort("speed_hz",          0);
                          });
    }

    // ── Background task: update stepper_state every 500 ms ───────────────────
    app.registerBackgroundTask(
        [](ArkitektApp &app, Agent &agent)
        {
            updateStepperState(agent.getState("stepper_state"));
        },
        5000);

    // ── Function: stepper_move ────────────────────────────────────────────────
    {
        auto def = FunctionBuilder("stepper_move",
                                   "Move the stepper motor. Positive steps → forward, negative → backward. "
                                   "Use isRel=true to target an exact position, 'relative' for an offset.",
                                   512, 256)
            .argInt("steps", "Steps / Target",
                    "Steps to move (relative) or target position (absolute). Sign sets direction.")
            .withDefault("steps", 0)
            .argBool("isRel", "isRel",
                             "absolute: go to exact step position; relative: offset from current", true)
                                        .withDefault("isRel", true)
            .argInt("speed_hz", "Speed (steps/s)",
                    "Motor speed in steps per second (keeps last value if 0)")
            .withDefault("speed_hz", 1000)
            .withSlider("speed_hz", 1, 10000, 100)
            .argInt("acceleration", "Acceleration (steps/s²)",
                    "Ramp acceleration, steps/s² (keeps last value if 0)")
            .withDefault("acceleration", 500)
            .withSlider("acceleration", 1, 5000, 100)
            .returnInt("current_position", "Current Position", "Step position when command was issued")
            .returnInt("target_position",  "Target Position",  "Destination step position")
            .returnBool("success",          "Success",          "Whether the move command was accepted")
            .build();

        app.registerFunction(
            "stepper_move", def,
            [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
            {
                if (!stepperInitialized || stepper == nullptr)
                {
                    reply.critical("Stepper not initialized");
                    return false;
                }

                int32_t steps = (int32_t)(args["steps"] | 0);
                bool isRel   = args["isRel"] | 1;
                int speedHz   = args["speed_hz"]    | 0;
                int accel     = args["acceleration"] | 0;

                if (speedHz > 0)
                    stepper->setSpeedInHz((uint32_t)speedHz);
                if (accel > 0)
                    stepper->setAcceleration((uint32_t)accel);

                if (isRel == 1) // absolute
                    stepper->moveTo(steps);
                else
                    stepper->move(steps);

                // Immediately push updated state
                updateStepperState(agent.getState("stepper_state"));

                StaticJsonDocument<128> retDoc;
                JsonObject ret = retDoc.to<JsonObject>();
                ret["current_position"] = (int32_t)stepper->getCurrentPosition();
                ret["target_position"]  = (int32_t)stepper->getPositionAfterCommandsCompleted();
                ret["success"]          = true;
                reply.done(ret);

                Serial.println("[STEPPER] move  isRel=" + String(isRel) +
                               "  steps=" + String(steps) +
                               "  pos="   + String(stepper->getCurrentPosition()));
                return true;
            });
    }

    // ── Function: stepper_stop ────────────────────────────────────────────────
    {
        auto def = FunctionBuilder("stepper_stop",
                                   "Stop the stepper motor. emergency=false ramps down smoothly; "
                                   "emergency=true halts immediately.",
                                   256, 128)
            .argBool("emergency", "Emergency Stop",
                     "true = immediate stop; false = decelerate to stop")
            .withDefault("emergency", false)
            .returnInt("position", "Position", "Step position when stop was issued")
            .returnBool("success",  "Success",  "Whether the stop command was issued")
            .build();

        app.registerFunction(
            "stepper_stop", def,
            [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
            {
                if (!stepperInitialized || stepper == nullptr)
                {
                    reply.critical("Stepper not initialized");
                    return false;
                }

                bool emergency = args["emergency"] | false;

                if (emergency)
                    stepper->forceStop();
                else
                    stepper->stopMove();

                // Immediately push updated state
                updateStepperState(agent.getState("stepper_state"));

                StaticJsonDocument<64> retDoc;
                JsonObject ret = retDoc.to<JsonObject>();
                ret["position"] = (int32_t)stepper->getCurrentPosition();
                ret["success"]  = true;
                reply.done(ret);

                Serial.println(String("[STEPPER] stop") +
                               (emergency ? " (emergency)" : " (decel)") +
                               "  pos=" + String(stepper->getCurrentPosition()));
                return true;
            });
    }

    Serial.println("✓ All stepper motor functions registered\n");
}

#endif // STEPPER_MOTOR_H
