#ifndef ARKITEKT_SIMPLE_EXAMPLE_H
#define ARKITEKT_SIMPLE_EXAMPLE_H

/*
 * Simplified Arkitekt Example
 *
 * This demonstrates a cleaner API for registering functions
 */

#include "arkitekt_easy.h"
#include "function_builder.h"

// Store function builders globally to keep their JsonArrays alive
static FunctionBuilder *toggleLedBuilder = nullptr;
static FunctionBuilder *addNumbersBuilder = nullptr;
static FunctionBuilder *getInfoBuilder = nullptr;

inline void registerSimpleFunctions(ArkitektApp &app)
{
    Serial.println("\n=== Registering Simple Functions ===");

    // Example 1: Toggle LED (no arguments)
    toggleLedBuilder = new FunctionBuilder("toggle_led", "Toggles the LED on GPIO2");
    toggleLedBuilder->addIntReturn("pin", "Pin Number", "GPIO pin that was toggled")
        .addBoolReturn("state", "LED State", "New state after toggle")
        .addBoolReturn("success", "Success", "Operation succeeded");

    app.registerFunction(
        toggleLedBuilder->getName(),
        toggleLedBuilder->getDescription(),
        [](ReplyChannel &reply) -> bool
        {
            static bool ledState = true;

            reply.log("Toggling LED on GPIO2");

            ledState = !ledState;
            digitalWrite(2, ledState ? HIGH : LOW);

            JsonDocument doc;
            doc["pin"] = 2;
            doc["state"] = ledState;
            doc["success"] = true;
            reply.yield(doc.as<JsonObject>());

            reply.log("LED toggled to " + String(ledState ? "ON" : "OFF"));

            return true;
        },
        &toggleLedBuilder->getReturns());

    // Example 2: Add two numbers
    addNumbersBuilder = new FunctionBuilder("add_numbers", "Adds two numbers together");
    addNumbersBuilder->addFloatArg("a", "First Number", "First operand")
        .withDefaultFloat("a", 0.0)
        .addFloatArg("b", "Second Number", "Second operand")
        .withDefaultFloat("b", 0.0)
        .addFloatReturn("sum", "Sum", "Result of addition")
        .addBoolReturn("success", "Success", "Operation succeeded");

    app.registerFunction(
        addNumbersBuilder->getName(),
        addNumbersBuilder->getDescription(),
        [](JsonObject args, ReplyChannel &reply) -> bool
        {
            float a = args["a"] | 0.0;
            float b = args["b"] | 0.0;

            reply.log("Adding " + String(a) + " + " + String(b));
            reply.progress(50, "Computing...");

            float sum = a + b;

            JsonDocument doc;
            doc["sum"] = sum;
            doc["success"] = true;
            reply.yield(doc.as<JsonObject>());

            reply.progress(100, "Done");
            reply.log("Result: " + String(sum));

            return true;
        },
        &addNumbersBuilder->getArgs(),
        &addNumbersBuilder->getReturns());

    // Example 3: Get device info (no arguments)
    getInfoBuilder = new FunctionBuilder("get_device_info", "Returns ESP32 device information");
    getInfoBuilder->addStringReturn("chip_model", "Chip Model", "ESP32 chip model")
        .addIntReturn("chip_revision", "Chip Revision", "Hardware revision")
        .addIntReturn("cpu_freq_mhz", "CPU Frequency", "CPU frequency in MHz")
        .addIntReturn("free_heap", "Free Heap", "Available heap memory")
        .addIntReturn("cpu_cores", "CPU Cores", "Number of CPU cores")
        .addStringReturn("sdk_version", "SDK Version", "ESP-IDF version");

    app.registerFunction(
        getInfoBuilder->getName(),
        getInfoBuilder->getDescription(),
        [](ReplyChannel &reply) -> bool
        {
            reply.log("Reading device information");

            JsonDocument doc;
            doc["chip_model"] = ESP.getChipModel();
            doc["chip_revision"] = ESP.getChipRevision();
            doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
            doc["free_heap"] = ESP.getFreeHeap();
            doc["cpu_cores"] = ESP.getChipCores();
            doc["sdk_version"] = ESP.getSdkVersion();
            reply.yield(doc.as<JsonObject>());

            reply.log("Device info retrieved");

            return true;
        },
        &getInfoBuilder->getReturns());

    Serial.println("✓ All simple functions registered\n");
}

#endif // ARKITEKT_SIMPLE_EXAMPLE_H
