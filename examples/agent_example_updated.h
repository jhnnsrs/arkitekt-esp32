#ifndef AGENT_EXAMPLE_UPDATED_H
#define AGENT_EXAMPLE_UPDATED_H

/*
 * Updated Examples: How to register functions with the Agent using proper schema types
 *
 * This file demonstrates various patterns for registering functions
 * with proper PortInput definitions according to the Rekuest GraphQL schema.
 */

#include "agent.h"
#include "port_builder.h"

// Global documents to keep JsonArrays alive
// Note: These must persist for the lifetime of the agent
DynamicJsonDocument calculatorArgsDoc(1024);
DynamicJsonDocument calculatorReturnsDoc(512);
DynamicJsonDocument sensorArgsDoc(512);
DynamicJsonDocument sensorReturnsDoc(1024);
DynamicJsonDocument toggleLedReturnsDoc(256);

// Example 1: Calculator with proper typed ports
void registerCalculatorFunctionUpdated(Agent *agent)
{
    FunctionDefinition def = DefinitionBuilder::create(
        "calculator",
        "Performs arithmetic operations on two numbers",
        "FUNCTION",
        false);

    // Define arguments using PortBuilder
    JsonArray args = calculatorArgsDoc.to<JsonArray>();

    JsonObject argA = PortBuilder::createFloatPort(args, "a", "First Number", "The first operand", false);
    PortBuilder::setDefault(argA, 0.0f);

    JsonObject argB = PortBuilder::createFloatPort(args, "b", "Second Number", "The second operand", false);
    PortBuilder::setDefault(argB, 0.0f);

    JsonObject argOp = PortBuilder::createStringPort(args, "operation", "Operation", "Mathematical operation to perform", false);
    PortBuilder::addChoice(argOp, "Add", "add", "Add two numbers");
    PortBuilder::addChoice(argOp, "Subtract", "subtract", "Subtract second from first");
    PortBuilder::addChoice(argOp, "Multiply", "multiply", "Multiply two numbers");
    PortBuilder::addChoice(argOp, "Divide", "divide", "Divide first by second");
    PortBuilder::addChoiceWidget(argOp);
    PortBuilder::setDefault(argOp, "add");

    def.args = args;

    // Define returns
    JsonArray returns = calculatorReturnsDoc.to<JsonArray>();
    PortBuilder::createFloatPort(returns, "result", "Result", "The result of the operation", false);
    PortBuilder::createStringPort(returns, "operation_performed", "Operation", "The operation that was executed", false);

    def.returns = returns;

    // Register the function
    agent->registerFunction(
        "calculator",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            float a = args["a"] | 0.0;
            float b = args["b"] | 0.0;
            String operation = args["operation"] | "add";

            float result = 0.0;
            bool success = true;

            if (operation == "add")
            {
                result = a + b;
            }
            else if (operation == "subtract")
            {
                result = a - b;
            }
            else if (operation == "multiply")
            {
                result = a * b;
            }
            else if (operation == "divide")
            {
                if (b != 0)
                {
                    result = a / b;
                }
                else
                {
                    Serial.println("Error: Division by zero");
                    returns["error"] = "Division by zero";
                    success = false;
                }
            }
            else
            {
                Serial.println("Error: Unknown operation");
                returns["error"] = "Unknown operation";
                success = false;
            }

            if (success)
            {
                returns["result"] = result;
                returns["operation_performed"] = operation;
            }

            return success;
        });
}

// Example 2: Sensor reading with proper schema
void registerSensorFunctionUpdated(Agent *agent)
{
    FunctionDefinition def = DefinitionBuilder::create(
        "read_sensor",
        "Reads simulated sensor data",
        "FUNCTION",
        false);

    // Define arguments
    JsonArray args = sensorArgsDoc.to<JsonArray>();
    PortBuilder::createStringPort(args, "sensor_id", "Sensor ID", "Unique identifier for the sensor", false);

    JsonObject samplesArg = PortBuilder::createIntPort(args, "samples", "Sample Count", "Number of readings to take", true);
    PortBuilder::setDefault(samplesArg, 1);
    PortBuilder::addSliderWidget(samplesArg, 1, 10, 1);

    def.args = args;

    // Define returns
    JsonArray returns = sensorReturnsDoc.to<JsonArray>();
    PortBuilder::createStringPort(returns, "sensor_id", "Sensor ID", "Sensor that was read", false);
    PortBuilder::createFloatPort(returns, "value", "Value", "Sensor reading value", false);
    PortBuilder::createIntPort(returns, "timestamp", "Timestamp", "Unix timestamp of reading", false);
    PortBuilder::createStringPort(returns, "unit", "Unit", "Unit of measurement", false);
    PortBuilder::createIntPort(returns, "samples", "Samples", "Number of samples taken", false);

    def.returns = returns;

    agent->registerFunction(
        "read_sensor",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            String sensorId = args["sensor_id"] | "temp_1";
            int samples = args["samples"] | 1;

            // Simulate sensor reading (average over samples)
            float total = 0;
            for (int i = 0; i < samples; i++)
            {
                total += 20.0 + (random(0, 100) / 10.0);
            }
            float value = total / samples;

            unsigned long timestamp = millis() / 1000;

            returns["sensor_id"] = sensorId;
            returns["value"] = value;
            returns["timestamp"] = timestamp;
            returns["unit"] = "celsius";
            returns["samples"] = samples;

            // Update agent state
            AgentState *sensorStatus = agent.getState("sensor_status");
            if (sensorStatus)
            {
                sensorStatus->setPort("temperature", value);
                static int readingsCount = 0;
                readingsCount++;
                sensorStatus->setPort("readings_count", readingsCount);
            }

            Serial.println("Sensor " + sensorId + ": " + String(value) + "°C (avg of " + String(samples) + " samples)");

            return true;
        });
}

// Example 3: LED control with slider
DynamicJsonDocument ledArgsDoc(512);
DynamicJsonDocument ledReturnsDoc(256);

void registerLEDControlUpdated(Agent *agent)
{
    FunctionDefinition def = DefinitionBuilder::create(
        "set_led",
        "Controls an LED on a GPIO pin",
        "FUNCTION",
        false);

    JsonArray args = ledArgsDoc.to<JsonArray>();

    JsonObject pinArg = PortBuilder::createIntPort(args, "pin", "Pin Number", "GPIO pin number", false);
    PortBuilder::setDefault(pinArg, 2);
    PortBuilder::addSliderWidget(pinArg, 0, 40, 1);

    JsonObject stateArg = PortBuilder::createBoolPort(args, "state", "LED State", "Turn LED on (true) or off (false)", false);
    PortBuilder::setDefault(stateArg, false);

    def.args = args;

    JsonArray returns = ledReturnsDoc.to<JsonArray>();
    PortBuilder::createIntPort(returns, "pin", "Pin", "GPIO pin that was set", false);
    PortBuilder::createBoolPort(returns, "state", "State", "New state of LED", false);
    PortBuilder::createBoolPort(returns, "success", "Success", "Whether operation succeeded", false);

    def.returns = returns;

    agent->registerFunction(
        "set_led",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            int pin = args["pin"] | 2;
            bool state = args["state"] | false;

            pinMode(pin, OUTPUT);
            digitalWrite(pin, state ? HIGH : LOW);

            returns["pin"] = pin;
            returns["state"] = state;
            returns["success"] = true;

            // Update agent state
            AgentState *ledStatus = agent.getState("led_status");
            if (ledStatus)
            {
                ledStatus->setPort("on", state);
                ledStatus->setPort("pin", pin);
            }

            Serial.println("LED on pin " + String(pin) + " set to " + String(state ? "ON" : "OFF"));

            return true;
        });
}

// Example 4: Device info (no arguments, multiple returns)
DynamicJsonDocument deviceInfoReturnsDoc(512);

void registerDeviceInfoUpdated(Agent *agent)
{
    FunctionDefinition def = DefinitionBuilder::create(
        "get_device_info",
        "Returns ESP32 hardware and system information",
        "FUNCTION",
        false);

    // No arguments needed
    JsonArray args;
    def.args = args;

    // Multiple return values
    JsonArray returns = deviceInfoReturnsDoc.to<JsonArray>();
    PortBuilder::createStringPort(returns, "chip_model", "Chip Model", "ESP32 chip model", false);
    PortBuilder::createIntPort(returns, "chip_revision", "Chip Revision", "Hardware revision", false);
    PortBuilder::createIntPort(returns, "cpu_freq_mhz", "CPU Frequency", "CPU frequency in MHz", false);
    PortBuilder::createIntPort(returns, "free_heap", "Free Heap", "Available heap memory in bytes", false);
    PortBuilder::createIntPort(returns, "cpu_cores", "CPU Cores", "Number of CPU cores", false);
    PortBuilder::createStringPort(returns, "sdk_version", "SDK Version", "ESP-IDF version", false);

    def.returns = returns;

    agent->registerFunction(
        "get_device_info",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            returns["chip_model"] = ESP.getChipModel();
            returns["chip_revision"] = ESP.getChipRevision();
            returns["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
            returns["free_heap"] = ESP.getFreeHeap();
            returns["cpu_cores"] = ESP.getChipCores();
            returns["sdk_version"] = ESP.getSdkVersion();

            Serial.println("Device Info: " + String(ESP.getChipModel()) + " @ " + String(ESP.getCpuFreqMHz()) + " MHz");

            return true;
        });
}

// Example 5: Toggle LED (no arguments, just toggles GPIO2)
void registerToggleLedUpdated(Agent *agent)
{
    FunctionDefinition def = DefinitionBuilder::create(
        "toggle_led",
        "Toggles the LED on GPIO2",
        "FUNCTION",
        false);

    // No arguments needed
    JsonArray args;
    def.args = args;

    // Define returns
    JsonArray returns = toggleLedReturnsDoc.to<JsonArray>();
    PortBuilder::createIntPort(returns, "pin", "Pin", "GPIO pin that was toggled", false);
    PortBuilder::createBoolPort(returns, "state", "State", "New state of LED", false);
    PortBuilder::createBoolPort(returns, "success", "Success", "Whether operation succeeded", false);

    def.returns = returns;

    agent->registerFunction(
        "toggle_led",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            Serial.println("\n>>> Executing TOGGLE_LED function");

            static bool ledState = true; // Starts HIGH from setup()

            // Toggle the state
            ledState = !ledState;
            digitalWrite(2, ledState ? HIGH : LOW);

            returns["pin"] = 2;
            returns["state"] = ledState;
            returns["success"] = true;

            // Update agent state
            AgentState *ledStatus = agent.getState("led_status");
            if (ledStatus)
            {
                ledStatus->setPort("on", ledState);
            }

            Serial.println("LED on GPIO2 toggled to " + String(ledState ? "ON" : "OFF"));

            return true;
        });
}

// State definition documents (must persist)
DynamicJsonDocument ledStatePortsDoc(512);
DynamicJsonDocument sensorStatePortsDoc(512);

// Register example states
void registerExampleStates(Agent *agent)
{
    Serial.println("\n=== Registering Example States ===");

    // LED state - tracks the current LED status
    {
        StateDefinition def("led_status", "LED Status");
        JsonArray ports = ledStatePortsDoc.to<JsonArray>();
        PortBuilder::createBoolPort(ports, "on", "On", "Whether the LED is currently on", false);
        PortBuilder::createIntPort(ports, "pin", "Pin", "GPIO pin number", false);
        def.ports = ports;

        agent->registerState("led_status", def);

        // Set initial values
        AgentState *state = agent->getState("led_status");
        if (state)
        {
            state->setPort("on", true); // LED starts HIGH in setup()
            state->setPort("pin", 2);
        }
    }

    // Sensor state - tracks last sensor reading
    {
        StateDefinition def("sensor_status", "Sensor Status");
        JsonArray ports = sensorStatePortsDoc.to<JsonArray>();
        PortBuilder::createFloatPort(ports, "temperature", "Temperature", "Last temperature reading in celsius", false);
        PortBuilder::createIntPort(ports, "readings_count", "Readings Count", "Total number of readings taken", false);
        def.ports = ports;

        agent->registerState("sensor_status", def);

        // Set initial values
        AgentState *state = agent->getState("sensor_status");
        if (state)
        {
            state->setPort("temperature", 0.0f);
            state->setPort("readings_count", 0);
        }
    }

    Serial.println("✓ All example states registered\n");
}

// Register all updated example functions
void registerAllUpdatedExamples(Agent *agent)
{
    Serial.println("\n=== Registering Updated Example Functions ===");

    registerCalculatorFunctionUpdated(agent);
    registerSensorFunctionUpdated(agent);
    registerLEDControlUpdated(agent);
    registerDeviceInfoUpdated(agent);
    registerToggleLedUpdated(agent);

    Serial.println("✓ All updated example functions registered\n");

    registerExampleStates(agent);
}

#endif // AGENT_EXAMPLE_UPDATED_H
