#include <Arduino.h>
#include "lib/arkitekt_app.h"
#include "stepper_motor.h"

// ==================== Configuration & Constants ====================
constexpr uint8_t LED_PIN = 2;
constexpr uint32_t SENSOR_UPDATE_INTERVAL_MS = 5000;
constexpr uint32_t SERIAL_TIMEOUT_MS = 3000;

// ==================== App Setup ====================

ArkitektApp app("test-esp32", "1.0.0", "default", "ESP32 Agent");

// ==================== Functions ====================

void registerToggleLed()
{
    auto def = FunctionBuilder("toggle_led", "Toggles the built-in LED", 16, 256)
        .returnInt("pin", "Pin", "GPIO pin toggled")
        .returnBool("state", "State", "New LED state")
        .build();

    app.registerFunction("toggle_led", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
                             static bool ledState = true;
                             ledState = !ledState;
                             digitalWrite(LED_PIN, ledState ? HIGH : LOW);

                             StaticJsonDocument<256> retDoc;
                             JsonObject ret = retDoc.to<JsonObject>();
                             ret["pin"] = LED_PIN;
                             ret["state"] = ledState;
                             reply.done(ret);

                             AgentState *st = agent.getState("led_status");
                             if (st)
                                 st->setPort("on", ledState);

                             return true;
                         });
}

void registerCalculator()
{
    auto def = FunctionBuilder("calculator", "Performs arithmetic on two numbers", 1024, 512)
        .argFloat("a", "A", "First operand")
        .argFloat("b", "B", "Second operand")
        .argStringChoice("operation", "Operation", "add/subtract/multiply/divide", {
            {"Add", "add"},
            {"Subtract", "subtract"},
            {"Multiply", "multiply"},
            {"Divide", "divide"}
        })
        .returnFloat("result", "Result", "Calculation result")
        .returnString("operation_performed", "Operation", "Operation executed")
        .build();

    app.registerFunction("calculator", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
                             if (!args.containsKey("a") || !args.containsKey("b"))
                             {
                                 reply.critical("Missing required arguments 'a' or 'b'");
                                 return false;
                             }

                             float a = args["a"] | 0.0f;
                             float b = args["b"] | 0.0f;
                             String op = args["operation"] | "add";

                             float result = 0;
                             if (op == "add")
                                 result = a + b;
                             else if (op == "subtract")
                                 result = a - b;
                             else if (op == "multiply")
                                 result = a * b;
                             else if (op == "divide")
                             {
                                 if (b == 0)
                                 {
                                     reply.critical("Division by zero");
                                     return false;
                                 }
                                 result = a / b;
                             }

                             StaticJsonDocument<256> retDoc;
                             JsonObject ret = retDoc.to<JsonObject>();
                             ret["result"] = result;
                             ret["operation_performed"] = op;
                             reply.done(ret);
                             return true;
                         });
}

void registerDeviceInfo()
{
    auto def = FunctionBuilder("get_device_info", "Returns ESP32 hardware information", 16, 512)
        .returnString("chip_model", "Chip Model")
        .returnInt("free_heap", "Free Heap", "Bytes")
        .returnInt("cpu_freq_mhz", "CPU MHz")
        .build();

    app.registerFunction("get_device_info", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
                             StaticJsonDocument<256> retDoc;
                             JsonObject ret = retDoc.to<JsonObject>();
                             ret["chip_model"] = ESP.getChipModel();
                             ret["free_heap"] = ESP.getFreeHeap();
                             ret["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
                             reply.done(ret);
                             return true;
                         });
}

// ==================== States ====================

void registerStates()
{
    {
        auto def = StateBuilder("led_status", "LED Status", 256)
            .portBool("on", "On", "LED on/off")
            .portInt("pin", "Pin", "GPIO pin")
            .build();

        app.registerState("led_status", def, [](AgentState *state)
                          {
            state->setPort("on", true);
            state->setPort("pin", (int)LED_PIN); });
    }

    {
        auto def = StateBuilder("sensor_status", "Sensor Status", 256)
            .portFloat("temperature", "Temperature", "Celsius")
            .portInt("readings_count", "Readings", "Total readings")
            .build();

        app.registerState("sensor_status", def, [](AgentState *state)
                          {
            state->setPort("temperature", 0.0f);
            state->setPort("readings_count", 0); });
    }
}

// ==================== Arduino Entry Points ====================

void setup()
{
    Serial.begin(115200);

    uint32_t start_time = millis();
    while (!Serial && (millis() - start_time < SERIAL_TIMEOUT_MS));

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    app.addRequirement("rekuest", "live.arkitekt.rekuest");

    registerToggleLed();
    registerCalculator();
    registerDeviceInfo();

    initStepper();
    registerAllStepperFunctions(app);

    registerStates();

    app.registerBackgroundTask(
        [](ArkitektApp &app, Agent &agent)
        {
            static int readingCount = 0;
            readingCount++;

            AgentState *state = agent.getState("sensor_status");
            if (state)
            {
                float temp = 4.0f + (random(0, 3000) / 100.0f);
                state->setPort("temperature", temp);
                state->setPort("readings_count", readingCount);
                Serial.println("[BG] Reading #" + String(readingCount) + " | Temperature: " + String(temp) + "°C");
            }
        },
        SENSOR_UPDATE_INTERVAL_MS);

    // Startup self-test: move +100 steps, wait, then return -100 steps
    if (stepperInitialized && stepper != nullptr)
    {
        Serial.println("[STEPPER] Startup test: +100 steps");
        stepper->move(100);
        delay(2000);
        Serial.println("[STEPPER] Startup test: -100 steps");
        stepper->move(-100);
        delay(2000);
        Serial.println("[STEPPER] Startup test complete  pos=" + String(stepper->getCurrentPosition()));
    }

    RunConfig cfg;
    cfg.ble = true;
    cfg.enableWpa2Enterprise = true;
    cfg.bootReconfigureTimeout = 5000;
    app.run(cfg);
}

void loop()
{
    app.loop();
}
