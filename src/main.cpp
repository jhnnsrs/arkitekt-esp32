#include <Arduino.h>
#include "lib/arkitekt_app.h"

// ==================== App Setup ====================

ArkitektApp app("test-esp32", "1.0.0", "default", "ESP32 Agent");

// ==================== Persistent JSON documents ====================
// These must outlive the function registrations (ArduinoJson requirement)

DynamicJsonDocument toggleReturnsDoc(256);

DynamicJsonDocument calcArgsDoc(1024);
DynamicJsonDocument calcReturnsDoc(512);

DynamicJsonDocument deviceInfoReturnsDoc(512);

DynamicJsonDocument ledStatePortsDoc(256);
DynamicJsonDocument sensorStatePortsDoc(256);

// ==================== Functions ====================

void registerToggleLed()
{
    FunctionDefinition def("toggle_led", "Toggles the built-in LED on GPIO2");

    JsonArray returns = toggleReturnsDoc.to<JsonArray>();
    PortBuilder::createIntPort(returns, "pin", "Pin", "GPIO pin toggled");
    PortBuilder::createBoolPort(returns, "state", "State", "New LED state");
    def.returns = returns;

    app.registerFunction("toggle_led", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
                             static bool ledState = true;
                             ledState = !ledState;
                             digitalWrite(2, ledState ? HIGH : LOW);

                             DynamicJsonDocument retDoc(256);
                             JsonObject ret = retDoc.to<JsonObject>();
                             ret["pin"] = 2;
                             ret["state"] = ledState;
                             reply.done(ret);

                             // Update state
                             AgentState *st = agent.getState("led_status");
                             if (st)
                                 st->setPort("on", ledState);

                             return true;
                         });
}

void registerCalculator()
{
    FunctionDefinition def("calculator", "Performs arithmetic on two numbers");

    JsonArray args = calcArgsDoc.to<JsonArray>();
    PortBuilder::createFloatPort(args, "a", "A", "First operand");
    PortBuilder::createFloatPort(args, "b", "B", "Second operand");
    JsonObject opArg = PortBuilder::createStringPort(args, "operation", "Operation", "add/subtract/multiply/divide");
    PortBuilder::addChoice(opArg, "Add", "add");
    PortBuilder::addChoice(opArg, "Subtract", "subtract");
    PortBuilder::addChoice(opArg, "Multiply", "multiply");
    PortBuilder::addChoice(opArg, "Divide", "divide");
    PortBuilder::addChoiceWidget(opArg);
    def.args = args;

    JsonArray returns = calcReturnsDoc.to<JsonArray>();
    PortBuilder::createFloatPort(returns, "result", "Result", "Calculation result");
    PortBuilder::createStringPort(returns, "operation_performed", "Operation", "Operation executed");
    def.returns = returns;

    app.registerFunction("calculator", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
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

                             DynamicJsonDocument retDoc(256);
                             JsonObject ret = retDoc.to<JsonObject>();
                             ret["result"] = result;
                             ret["operation_performed"] = op;
                             reply.done(ret);
                             return true;
                         });
}

void registerDeviceInfo()
{
    FunctionDefinition def("get_device_info", "Returns ESP32 hardware information");

    JsonArray returns = deviceInfoReturnsDoc.to<JsonArray>();
    PortBuilder::createStringPort(returns, "chip_model", "Chip Model");
    PortBuilder::createIntPort(returns, "free_heap", "Free Heap", "Bytes");
    PortBuilder::createIntPort(returns, "cpu_freq_mhz", "CPU MHz");
    def.returns = returns;

    app.registerFunction("get_device_info", def,
                         [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool
                         {
                             DynamicJsonDocument retDoc(256);
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
    // LED status state
    {
        StateDefinition def("led_status", "LED Status");
        JsonArray ports = ledStatePortsDoc.to<JsonArray>();
        PortBuilder::createBoolPort(ports, "on", "On", "LED on/off");
        PortBuilder::createIntPort(ports, "pin", "Pin", "GPIO pin");
        def.ports = ports;

        app.registerState("led_status", def, [](AgentState *state)
                          {
            state->setPort("on", true);
            state->setPort("pin", 2); });
    }

    // Sensor status state
    {
        StateDefinition def("sensor_status", "Sensor Status");
        JsonArray ports = sensorStatePortsDoc.to<JsonArray>();
        PortBuilder::createFloatPort(ports, "temperature", "Temperature", "Celsius");
        PortBuilder::createIntPort(ports, "readings_count", "Readings", "Total readings");
        def.ports = ports;

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
    while (!Serial)
        ;

    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);

    // Add service requirements
    app.addRequirement("rekuest", "live.arkitekt.rekuest");

    // Register functions
    registerToggleLed();
    registerCalculator();
    registerDeviceInfo();

    // Register states
    registerStates();

    // Register background task: update temperature every 5s
    app.registerBackgroundTask(
        [](ArkitektApp &app, Agent &agent)
        {
            AgentState *state = agent.getState("sensor_status");
            if (state)
            {
                float temp = 4.0f + (random(0, 3000) / 100.0f);
                state->setPort("temperature", temp);
                Serial.println("[BG] Temperature: " + String(temp) + "°C");
            }
        },
        5000);

    // Run: BLE provisioning -> WiFi -> OAuth2 -> Agent -> WebSocket
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
