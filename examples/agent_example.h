#ifndef AGENT_EXAMPLE_H
#define AGENT_EXAMPLE_H

/*
 * Example: How to register functions with the Agent
 *
 * This file demonstrates various patterns for registering functions
 * with proper definitions including argument and return specifications.
 */

#include "agent.h"

// Helper function to create an argument definition
void addArgument(JsonArray &args, const String &key, const String &type, const String &description, bool required = true)
{
    DynamicJsonDocument argDoc(256);
    JsonObject arg = argDoc.to<JsonObject>();
    arg["key"] = key;
    arg["type"] = type;
    arg["description"] = description;
    arg["required"] = required;
    args.add(arg);
}

// Helper function to create a return definition
void addReturn(JsonArray &returns, const String &key, const String &type, const String &description)
{
    DynamicJsonDocument returnDoc(256);
    JsonObject ret = returnDoc.to<JsonObject>();
    ret["key"] = key;
    ret["type"] = type;
    ret["description"] = description;
    returns.add(ret);
}

// Example 1: Function with defined arguments and returns
void registerCalculatorFunction(Agent *agent)
{
    FunctionDefinition def("calculator", "Performs arithmetic operations");

    // Define arguments
    DynamicJsonDocument argsDoc(512);
    JsonArray args = argsDoc.to<JsonArray>();
    addArgument(args, "a", "float", "First operand", true);
    addArgument(args, "b", "float", "Second operand", true);
    addArgument(args, "operation", "string", "Operation: add, subtract, multiply, divide", true);
    def.args = args;

    // Define returns
    DynamicJsonDocument returnsDoc(256);
    JsonArray returns = returnsDoc.to<JsonArray>();
    addReturn(returns, "result", "float", "Result of the operation");
    addReturn(returns, "operation_performed", "string", "The operation that was performed");
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

// Example 2: Function that reads sensor data (simulated)
void registerSensorFunction(Agent *agent)
{
    FunctionDefinition def("read_sensor", "Reads data from a simulated sensor");

    // Define arguments
    DynamicJsonDocument argsDoc(256);
    JsonArray args = argsDoc.to<JsonArray>();
    addArgument(args, "sensor_id", "string", "Sensor identifier", true);
    addArgument(args, "samples", "int", "Number of samples to read", false);
    def.args = args;

    // Define returns
    DynamicJsonDocument returnsDoc(512);
    JsonArray returns = returnsDoc.to<JsonArray>();
    addReturn(returns, "sensor_id", "string", "Sensor identifier");
    addReturn(returns, "value", "float", "Sensor reading");
    addReturn(returns, "timestamp", "int", "Unix timestamp");
    addReturn(returns, "unit", "string", "Unit of measurement");
    def.returns = returns;

    agent->registerFunction(
        "read_sensor",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            String sensorId = args["sensor_id"] | "temp_1";
            int samples = args["samples"] | 1;

            // Simulate sensor reading
            float value = 20.0 + (random(0, 100) / 10.0); // Random temp between 20-30°C
            unsigned long timestamp = millis() / 1000;

            returns["sensor_id"] = sensorId;
            returns["value"] = value;
            returns["timestamp"] = timestamp;
            returns["unit"] = "celsius";
            returns["samples"] = samples;

            Serial.println("Sensor " + sensorId + ": " + String(value) + "°C");

            return true;
        });
}

// Example 3: Function that can access the App instance
void registerGraphQLTestFunction(Agent *agent)
{
    FunctionDefinition def("graphql_test", "Tests a GraphQL query to a service");

    // Define arguments
    DynamicJsonDocument argsDoc(256);
    JsonArray args = argsDoc.to<JsonArray>();
    addArgument(args, "service", "string", "Service identifier", true);
    addArgument(args, "query", "string", "GraphQL query to execute", true);
    def.args = args;

    // Define returns
    DynamicJsonDocument returnsDoc(256);
    JsonArray returns = returnsDoc.to<JsonArray>();
    addReturn(returns, "success", "bool", "Whether the query succeeded");
    addReturn(returns, "response", "string", "Query response");
    def.returns = returns;

    agent->registerFunction(
        "graphql_test",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            String service = args["service"] | "rekuest";
            String query = args["query"] | "{ __typename }";

            // Access the app instance through the agent
            App *app = agent.getApp();

            String response;
            String error;

            bool success = app->graphqlRequest(service, query, response, error);

            returns["success"] = success;
            if (success)
            {
                returns["response"] = response;
            }
            else
            {
                returns["error"] = error;
            }

            return success;
        });
}

// Example 4: Function with array/object returns
void registerSystemStatsFunction(Agent *agent)
{
    FunctionDefinition def("get_system_stats", "Returns comprehensive system statistics");

    // Define returns
    DynamicJsonDocument returnsDoc(512);
    JsonArray returns = returnsDoc.to<JsonArray>();
    addReturn(returns, "memory", "object", "Memory statistics");
    addReturn(returns, "cpu", "object", "CPU information");
    addReturn(returns, "network", "object", "Network status");
    def.returns = returns;

    agent->registerFunction(
        "get_system_stats",
        def,
        [](Agent &agent, JsonObject args, JsonObject &returns) -> bool
        {
            // Memory stats
            JsonObject memory = returns["memory"].to<JsonObject>();
            memory["free_heap"] = ESP.getFreeHeap();
            memory["total_heap"] = ESP.getHeapSize();
            memory["min_free_heap"] = ESP.getMinFreeHeap();

            // CPU stats
            JsonObject cpu = returns["cpu"].to<JsonObject>();
            cpu["model"] = ESP.getChipModel();
            cpu["revision"] = ESP.getChipRevision();
            cpu["cores"] = ESP.getChipCores();
            cpu["frequency_mhz"] = ESP.getCpuFreqMHz();

            // Network stats
            JsonObject network = returns["network"].to<JsonObject>();
            network["connected"] = WiFi.status() == WL_CONNECTED;
            if (WiFi.status() == WL_CONNECTED)
            {
                network["ip"] = WiFi.localIP().toString();
                network["rssi"] = WiFi.RSSI();
                network["ssid"] = WiFi.SSID();
            }

            return true;
        });
}

// Register all example functions
void registerAllExampleFunctions(Agent *agent)
{
    Serial.println("\n=== Registering Example Functions ===");

    registerCalculatorFunction(agent);
    registerSensorFunction(agent);
    registerGraphQLTestFunction(agent);
    registerSystemStatsFunction(agent);

    Serial.println("✓ All example functions registered\n");
}

#endif // AGENT_EXAMPLE_H
