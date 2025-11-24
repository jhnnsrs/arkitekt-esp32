# Simplified Arkitekt API

## Overview

This simplified API provides a cleaner, more intuitive way to build Arkitekt agents on ESP32, similar to the pattern you suggested.

## Quick Start

### Minimal Example

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include "arkitekt_easy.h"
#include "function_builder.h"

const char *ssid = "YourWiFi";
const char *password = "YourPassword";
const char *redeemToken = "your-token";

ArkitektApp app;

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialize Arkitekt
    app.begin(redeemToken, "default", "My-Agent");
    
    // Register your functions
    registerMyFunctions();
    
    // Finalize
    app.finalize();
}

void loop() {
    // Handle assignments
    delay(100);
}
```

## API Reference

### ArkitektApp Class

Main application class that handles authentication and agent management.

#### Methods

##### `begin(redeemToken, instanceId, agentName)`

Initialize the app with authentication.

```cpp
bool begin(
    const char *redeemToken,    // Token from Arkitekt server
    const String &instanceId,   // Instance ID (default: "default")
    const String &agentName     // Human-readable agent name
);
```

Returns `true` on success, `false` on failure.

##### `registerFunction(name, description, callback, args, returns)`

Register a function with the agent.

**With Arguments:**
```cpp
bool registerFunction(
    const String &name,
    const String &description,
    FunctionCallback callback,  // (JsonObject args, ReplyChannel &reply) -> bool
    JsonArray *args,           // Optional: argument definitions
    JsonArray *returns         // Optional: return value definitions
);
```

**Without Arguments:**
```cpp
bool registerFunction(
    const String &name,
    const String &description,
    SimpleFunctionCallback callback,  // (ReplyChannel &reply) -> bool
    JsonArray *returns                // Optional: return value definitions
);
```

##### `finalize()`

Ensure agent exists on server and register all functions.

```cpp
bool finalize();
```

Must be called after registering all functions.

### ReplyChannel Class

Provides methods to communicate results and status back to the server.

#### Methods

##### `yield(key, value)`

Yield a single return value.

```cpp
void yield(const String &key, JsonVariant value);
```

**Example:**
```cpp
reply.yield("result", 42);
reply.yield("success", true);
reply.yield("message", "Done!");
```

##### `log(message, level)`

Log a message (defaults to INFO level).

```cpp
void log(const String &message, const String &level = "INFO");
```

**Example:**
```cpp
reply.log("Processing data...");
reply.log("Warning: Low memory", "WARN");
reply.log("Critical error!", "ERROR");
```

##### `progress(percent, message)`

Report progress (0-100%).

```cpp
void progress(int percent, const String &message = "");
```

**Example:**
```cpp
reply.progress(0, "Starting...");
reply.progress(50, "Half done");
reply.progress(100, "Complete!");
```

### FunctionBuilder Class

Fluent API for building function definitions.

#### Methods

##### Constructor

```cpp
FunctionBuilder(
    const String &name,
    const String &description,
    size_t argsSize = 1024,     // Memory for arguments
    size_t returnsSize = 512    // Memory for returns
);
```

##### Add Arguments

```cpp
FunctionBuilder &addIntArg(key, label, description, nullable);
FunctionBuilder &addFloatArg(key, label, description, nullable);
FunctionBuilder &addStringArg(key, label, description, nullable);
FunctionBuilder &addBoolArg(key, label, description, nullable);
```

##### Add Returns

```cpp
FunctionBuilder &addIntReturn(key, label, description, nullable);
FunctionBuilder &addFloatReturn(key, label, description, nullable);
FunctionBuilder &addStringReturn(key, label, description, nullable);
FunctionBuilder &addBoolReturn(key, label, description, nullable);
```

##### Set Defaults

```cpp
FunctionBuilder &withDefaultInt(key, value);
FunctionBuilder &withDefaultFloat(key, value);
FunctionBuilder &withDefaultString(key, value);
FunctionBuilder &withDefaultBool(key, value);
```

##### Add Widgets

```cpp
FunctionBuilder &withSlider(key, min, max, step);
FunctionBuilder &withChoice(key, label, value, description);
FunctionBuilder &withChoiceWidget(key);
```

## Complete Examples

### Example 1: Toggle LED (No Arguments)

```cpp
FunctionBuilder *toggleBuilder = new FunctionBuilder("toggle_led", "Toggles LED")
    .addIntReturn("pin", "Pin Number", "GPIO pin")
    .addBoolReturn("state", "State", "New LED state");

app.registerFunction(
    toggleBuilder->getName(),
    toggleBuilder->getDescription(),
    [](ReplyChannel &reply) -> bool {
        static bool state = false;
        state = !state;
        digitalWrite(2, state ? HIGH : LOW);
        
        reply.yield("pin", 2);
        reply.yield("state", state);
        reply.log("LED toggled to " + String(state ? "ON" : "OFF"));
        
        return true;
    },
    &toggleBuilder->getReturns()
);
```

### Example 2: Add Numbers (With Arguments)

```cpp
FunctionBuilder *addBuilder = new FunctionBuilder("add", "Adds two numbers")
    .addFloatArg("a", "First", "First number")
    .withDefaultFloat("a", 0.0)
    .addFloatArg("b", "Second", "Second number")
    .withDefaultFloat("b", 0.0)
    .addFloatReturn("sum", "Sum", "Result");

app.registerFunction(
    addBuilder->getName(),
    addBuilder->getDescription(),
    [](JsonObject args, ReplyChannel &reply) -> bool {
        float a = args["a"] | 0.0;
        float b = args["b"] | 0.0;
        float sum = a + b;
        
        reply.log("Computing " + String(a) + " + " + String(b));
        reply.yield("sum", sum);
        
        return true;
    },
    &addBuilder->getArgs(),
    &addBuilder->getReturns()
);
```

### Example 3: Sensor Reading (With Slider)

```cpp
FunctionBuilder *sensorBuilder = new FunctionBuilder("read_sensor", "Reads sensor")
    .addStringArg("sensor_id", "Sensor ID", "Sensor identifier")
    .addIntArg("samples", "Samples", "Number of samples")
    .withDefaultInt("samples", 1)
    .withSlider("samples", 1, 10, 1)
    .addFloatReturn("value", "Value", "Sensor reading")
    .addStringReturn("unit", "Unit", "Measurement unit");

app.registerFunction(
    sensorBuilder->getName(),
    sensorBuilder->getDescription(),
    [](JsonObject args, ReplyChannel &reply) -> bool {
        String sensorId = args["sensor_id"] | "temp_1";
        int samples = args["samples"] | 1;
        
        reply.log("Reading sensor: " + sensorId);
        reply.progress(0, "Starting...");
        
        float total = 0;
        for (int i = 0; i < samples; i++) {
            total += 20.0 + random(0, 100) / 10.0;
            reply.progress((i + 1) * 100 / samples, "Reading...");
            delay(100);
        }
        
        float avg = total / samples;
        reply.yield("value", avg);
        reply.yield("unit", "celsius");
        reply.log("Read complete: " + String(avg) + "°C");
        
        return true;
    },
    &sensorBuilder->getArgs(),
    &sensorBuilder->getReturns()
);
```

### Example 4: Dropdown Choices

```cpp
FunctionBuilder *modeBuilder = new FunctionBuilder("set_mode", "Sets operation mode")
    .addStringArg("mode", "Mode", "Operation mode")
    .withChoice("mode", "Fast", "fast", "Fast processing")
    .withChoice("mode", "Accurate", "accurate", "Accurate mode")
    .withChoice("mode", "Balanced", "balanced", "Balanced mode")
    .withChoiceWidget("mode")
    .withDefaultString("mode", "balanced")
    .addStringReturn("selected_mode", "Selected", "Mode that was set");

app.registerFunction(
    modeBuilder->getName(),
    modeBuilder->getDescription(),
    [](JsonObject args, ReplyChannel &reply) -> bool {
        String mode = args["mode"] | "balanced";
        
        reply.log("Setting mode to: " + mode);
        // Apply mode logic here...
        
        reply.yield("selected_mode", mode);
        return true;
    },
    &modeBuilder->getArgs(),
    &modeBuilder->getReturns()
);
```

## Pattern Comparison

### Old Pattern (Complex)

```cpp
DynamicJsonDocument argsDoc(512);
JsonArray args = argsDoc.to<JsonArray>();
JsonObject arg = PortBuilder::createFloatPort(args, "value", "Value", "Input", false);
PortBuilder::setDefault(arg, 0.0f);

FunctionDefinition def = DefinitionBuilder::create("func", "Description");
def.args = args;

agent->registerFunction("func", def, 
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        float value = args["value"] | 0.0;
        returns["result"] = value * 2;
        return true;
    }
);
```

### New Pattern (Simplified)

```cpp
auto *builder = new FunctionBuilder("func", "Description")
    .addFloatArg("value", "Value", "Input")
    .withDefaultFloat("value", 0.0)
    .addFloatReturn("result", "Result", "Output");

app.registerFunction(
    builder->getName(),
    builder->getDescription(),
    [](JsonObject args, ReplyChannel &reply) -> bool {
        float value = args["value"] | 0.0;
        reply.yield("result", value * 2);
        return true;
    },
    &builder->getArgs(),
    &builder->getReturns()
);
```

## Best Practices

### 1. Keep FunctionBuilders Alive

Store builders globally or as class members - they must persist!

```cpp
// ✓ CORRECT - Global storage
FunctionBuilder *myBuilder = nullptr;

void setup() {
    myBuilder = new FunctionBuilder(...);
    app.registerFunction(..., &myBuilder->getArgs(), ...);
}

// ✗ WRONG - Will be destroyed
void setup() {
    FunctionBuilder tempBuilder(...);  // Stack allocation
    app.registerFunction(...);  // Dangling reference!
}
```

### 2. Use ReplyChannel for Feedback

Always provide user feedback through the reply channel:

```cpp
[](JsonObject args, ReplyChannel &reply) -> bool {
    reply.log("Starting operation");
    reply.progress(0);
    
    // Do work...
    
    reply.progress(100);
    reply.log("Complete!");
    return true;
}
```

### 3. Handle Errors Gracefully

Return `false` on errors and log what went wrong:

```cpp
[](JsonObject args, ReplyChannel &reply) -> bool {
    if (someError) {
        reply.log("Error: Something went wrong", "ERROR");
        return false;
    }
    return true;
}
```

### 4. Use Appropriate Widget Types

- **Slider**: For numeric ranges (brightness, volume, etc.)
- **Choices**: For predefined options (modes, selections)
- **String**: For text input

## Memory Considerations

The simplified API handles most memory management automatically, but:

- FunctionBuilders must persist (store globally)
- Default sizes: 1024 bytes for args, 512 for returns
- Increase if needed: `new FunctionBuilder(name, desc, 2048, 1024)`

## Migration Guide

### From Old API to New API

**Before:**
```cpp
Agent *agent = new Agent(&app, "rekuest", "default", "MyAgent");
registerAllUpdatedExamples(agent);
agent->ensureAgent(...);
agent->registerFunctions(...);
```

**After:**
```cpp
ArkitektApp app;
app.begin(redeemToken, "default", "MyAgent");
registerSimpleFunctions(app);
app.finalize();
```

## Examples Included

See `arkitekt_simple_example.h` for complete working examples:
- `toggle_led`: No arguments, simple toggle
- `add_numbers`: Two float arguments, one return
- `get_device_info`: No arguments, multiple returns

## Next Steps

- [ ] Integrate WebSocket handling into ArkitektApp.loop()
- [ ] Implement YIELD/DONE message sending
- [ ] Add error handling and retry logic
- [ ] Support for stateful functions
- [ ] Support for generator functions (streaming)
