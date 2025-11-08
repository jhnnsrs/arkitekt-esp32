# Rekuest GraphQL Schema Implementation for ESP32

## Overview

This implementation provides full GraphQL schema compliance for registering ESP32 agents and their function implementations with the Arkitekt/Rekuest server.

## Files

### Core Files

- **`agent.h`**: Agent class with schema-compliant GraphQL mutations
- **`port_builder.h`**: Helper functions to build PortInput definitions
- **`agent_example_updated.h`**: Complete examples using proper schema types

### Legacy Files

- **`agent_example.h`**: Original examples (use `agent_example_updated.h` instead)

## GraphQL Schema Compliance

### SetExtensionImplementationsInput

The `registerFunctions()` method now constructs requests according to:

```graphql
input SetExtensionImplementationsInput {
  implementations: [ImplementationInput!]!
  instanceId: InstanceId!
  extension: String!
  runCleanup: Boolean! = false
}
```

### ImplementationInput

Each implementation includes:

```graphql
input ImplementationInput {
  definition: DefinitionInput!
  dependencies: [ActionDependencyInput!]!
  interface: String
  params: AnyDefault = null
  dynamic: Boolean! = false
  logo: String = null
}
```

### DefinitionInput

Function definitions include all required fields:

```graphql
input DefinitionInput {
  name: String!
  description: String = null
  kind: ActionKind!              # FUNCTION or GENERATOR
  stateful: Boolean! = false
  args: [PortInput!]! = []
  returns: [PortInput!]! = []
  collections: [String!]! = []
  interfaces: [String!]! = []
  isTestFor: [String!]! = []
  portGroups: [PortGroupInput!]! = []
  isDev: Boolean! = false
  logo: String = null
}
```

### PortInput

Ports are typed according to PortKind enum:

```graphql
input PortInput {
  key: String!
  label: String = null
  kind: PortKind!                # INT, STRING, FLOAT, BOOL, etc.
  description: String = null
  identifier: String = null      # For STRUCTURE kind
  nullable: Boolean! = false
  default: AnyDefault = null
  children: [PortInput!] = null  # For nested structures
  choices: [ChoiceInput!] = null
  assignWidget: AssignWidgetInput = null
  returnWidget: ReturnWidgetInput = null
  validators: [ValidatorInput!] = null
  effects: [EffectInput!] = null
  descriptors: [DescriptorInput!] = null
}
```

### PortKind Enum

Supported types:
- `INT`: Integer values
- `FLOAT`: Floating-point numbers
- `STRING`: Text strings
- `BOOL`: Boolean values
- `DICT`: Dictionary/object
- `LIST`: Array/list
- `STRUCTURE`: Structured data with identifier
- `DATE`: Date/time values
- `UNION`: Union types
- `ENUM`: Enumeration
- `MODEL`: Model references
- `MEMORY_STRUCTURE`: Memory-based structures
- `INTERFACE`: Interface types

## Usage

### 1. Basic Function Registration

```cpp
#include "agent.h"
#include "port_builder.h"

// Create documents to hold port arrays (must persist!)
DynamicJsonDocument argsDoc(512);
DynamicJsonDocument returnsDoc(256);

// Create function definition
FunctionDefinition def = DefinitionBuilder::create(
    "my_function",
    "Does something useful",
    "FUNCTION",
    false
);

// Define arguments
JsonArray args = argsDoc.to<JsonArray>();
PortBuilder::createIntPort(args, "count", "Count", "Number of iterations", false);
PortBuilder::createStringPort(args, "message", "Message", "Text to process", false);
def.args = args;

// Define returns
JsonArray returns = returnsDoc.to<JsonArray>();
PortBuilder::createBoolPort(returns, "success", "Success", "Operation result", false);
def.returns = returns;

// Register with agent
agent->registerFunction("my_function", def, callback);
```

### 2. Advanced Port Configuration

#### With Default Values

```cpp
JsonObject arg = PortBuilder::createFloatPort(args, "threshold", "Threshold", "Detection threshold");
PortBuilder::setDefault(arg, 0.5f);
```

#### With Choices (Dropdown)

```cpp
JsonObject arg = PortBuilder::createStringPort(args, "mode", "Mode", "Operation mode");
PortBuilder::addChoice(arg, "Fast", "fast", "Fast processing");
PortBuilder::addChoice(arg, "Accurate", "accurate", "Accurate processing");
PortBuilder::addChoice(arg, "Balanced", "balanced", "Balanced mode");
PortBuilder::addChoiceWidget(arg);
```

#### With Slider Widget

```cpp
JsonObject arg = PortBuilder::createIntPort(args, "brightness", "Brightness", "LED brightness");
PortBuilder::addSliderWidget(arg, 0, 255, 1);  // min, max, step
```

#### With String Widget (Paragraph)

```cpp
JsonObject arg = PortBuilder::createStringPort(args, "description", "Description", "Long text");
PortBuilder::addStringWidget(arg, "Enter description...", true);  // placeholder, asParagraph
```

#### Structure Ports

```cpp
JsonObject arg = PortBuilder::createStructurePort(
    args, 
    "image", 
    "@mikro/image",  // identifier
    "Image",
    "Input image data"
);
```

### 3. Complete Example

See `agent_example_updated.h` for full working examples:

```cpp
#include "agent.h"
#include "port_builder.h"
#include "agent_example_updated.h"

// In setup():
Agent *agent = new Agent(&app, "rekuest", "default", "ESP32-Agent");

// Register functions with proper schema types
registerAllUpdatedExamples(agent);

// Ensure agent on server
String error;
DynamicJsonDocument extensionsDoc(256);
JsonArray extensions = extensionsDoc.to<JsonArray>();
extensions.add("default");
agent->ensureAgent("ESP32-Agent", extensions, error);

// Register functions on server
agent->registerFunctions(error);
```

## Port Builder API

### Creating Ports

```cpp
// Basic types
PortBuilder::createIntPort(ports, key, label, description, nullable);
PortBuilder::createFloatPort(ports, key, label, description, nullable);
PortBuilder::createStringPort(ports, key, label, description, nullable);
PortBuilder::createBoolPort(ports, key, label, description, nullable);
PortBuilder::createDictPort(ports, key, label, description, nullable);
PortBuilder::createListPort(ports, key, label, description, nullable);

// Structure type
PortBuilder::createStructurePort(ports, key, identifier, label, description, nullable);
```

### Adding Widgets

```cpp
// Slider for numeric input
PortBuilder::addSliderWidget(port, min, max, step);

// String input with placeholder
PortBuilder::addStringWidget(port, placeholder, asParagraph);

// Dropdown with choices (choices must be added first)
PortBuilder::addChoiceWidget(port);
```

### Adding Choices

```cpp
PortBuilder::addChoice(port, label, value, description);
```

### Setting Defaults

```cpp
PortBuilder::setDefault(port, intValue);
PortBuilder::setDefault(port, floatValue);
PortBuilder::setDefault(port, stringValue);
PortBuilder::setDefault(port, boolValue);
```

### Adding Descriptors

```cpp
PortBuilder::addDescriptor(port, key, value);
```

## Memory Management

**IMPORTANT**: JsonArrays for args and returns must persist for the lifetime of the agent!

```cpp
// ✓ CORRECT - Document persists
DynamicJsonDocument argsDoc(512);
JsonArray args = argsDoc.to<JsonArray>();
def.args = args;

// ✗ WRONG - Document will be destroyed
{
    DynamicJsonDocument tempDoc(512);
    JsonArray args = tempDoc.to<JsonArray>();
    def.args = args;  // Dangling reference!
}
```

**Recommended approach**: Create global documents for each function's ports:

```cpp
// Global scope
DynamicJsonDocument myFunctionArgsDoc(512);
DynamicJsonDocument myFunctionReturnsDoc(256);

void registerMyFunction(Agent *agent) {
    JsonArray args = myFunctionArgsDoc.to<JsonArray>();
    JsonArray returns = myFunctionReturnsDoc.to<JsonArray>();
    // ... build ports ...
}
```

## Examples Included

### 1. Calculator (`calculator`)
- **Args**: `a` (FLOAT), `b` (FLOAT), `operation` (STRING with choices)
- **Returns**: `result` (FLOAT), `operation_performed` (STRING)
- **Features**: Choice widget with dropdown

### 2. Sensor Reader (`read_sensor`)
- **Args**: `sensor_id` (STRING), `samples` (INT with slider)
- **Returns**: `sensor_id`, `value` (FLOAT), `timestamp` (INT), `unit`, `samples`
- **Features**: Slider widget for sample count

### 3. LED Control (`set_led`)
- **Args**: `pin` (INT with slider), `state` (BOOL)
- **Returns**: `pin`, `state`, `success`
- **Features**: Pin selection slider

### 4. Device Info (`get_device_info`)
- **Args**: None
- **Returns**: `chip_model`, `chip_revision`, `cpu_freq_mhz`, `free_heap`, `cpu_cores`, `sdk_version`
- **Features**: Read-only system information

## Testing

To verify your implementation:

1. **Build and upload**:
   ```bash
   pio run --target upload
   ```

2. **Monitor serial output**:
   ```bash
   pio device monitor
   ```

3. **Look for**:
   ```
   === Registering Functions ===
   Registering 4 function(s)
   ✓ Functions registered successfully
   ```

4. **Check server** (via Arkitekt UI or GraphQL):
   ```graphql
   query {
     agent(id: "your-agent-id") {
       implementations {
         interface
         action {
           name
           args {
             key
             kind
             label
           }
           returns {
             key
             kind
             label
           }
         }
       }
     }
   }
   ```

## Troubleshooting

### "Failed to register functions"

- Check JSON document sizes - may need to increase
- Verify all required fields are set (name, kind, args, returns)
- Ensure empty arrays are properly initialized

### "Function not found" on assignment

- Verify interface name matches between registration and server
- Check agent is connected and implementations are active

### Memory issues

- Use larger JsonDocument sizes for complex definitions
- Ensure port arrays persist in global or long-lived documents
- Monitor free heap: `ESP.getFreeHeap()`

## Schema Validation

The implementation validates against Rekuest schema version as of 2024. Key validations:

✓ All required DefinitionInput fields present  
✓ PortKind enum values correct  
✓ ActionKind set to FUNCTION or GENERATOR  
✓ Empty arrays for collections, interfaces, isTestFor  
✓ Dependencies array present (can be empty)  
✓ Proper nesting of ImplementationInput → DefinitionInput → PortInput  

## References

- [Arkitekt Documentation](https://arkitekt.live)
- [Rekuest GraphQL Schema](https://github.com/arkitektio/rekuest-server-next)
- [ArduinoJson Library](https://arduinojson.org/)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/)
