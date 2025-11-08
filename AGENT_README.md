# Arkitekt Agent System for ESP32

## Overview

The Agent system provides a complete implementation of the Arkitekt agent protocol, similar to the Kotlin implementation. It allows you to:

1. Register functions with typed definitions
2. Automatically register these functions on the Arkitekt server via GraphQL
3. Receive and handle task assignments over WebSocket
4. Execute registered functions with proper argument passing
5. Send results back to the server

## Architecture

### Core Components

1. **Agent** (`agent.h`): Main agent class that manages function registration and execution
2. **FunctionRegistry**: Stores registered functions and their definitions
3. **FunctionDefinition**: Describes a function's interface (args, returns, description)
4. **AgentFunction**: Callback type for function implementations

### File Structure

```
src/
├── agent.h              # Agent and FunctionRegistry classes
├── agent_example.h      # Example function registrations
├── main.cpp             # Main application with WebSocket handling
├── app.h                # App class with HTTP/GraphQL/WebSocket support
├── arkitekt.h           # Authentication flow (redeem, claim, OAuth2)
├── fakts.h              # Configuration structures
└── manifest.h           # Application manifest
```

## Usage

### 1. Create an Agent

```cpp
// In setup() after app initialization:
Agent *agent = new Agent(&app, "rekuest", "default", "ESP32-Agent");
```

Parameters:
- `app`: Pointer to the App instance
- `serviceIdentifier`: Service to connect to (e.g., "rekuest")
- `instanceId`: Unique instance identifier
- `agentName`: Human-readable agent name

### 2. Register Functions

Simple function:
```cpp
agent->registerFunction(
    "echo",
    FunctionDefinition("echo", "Echoes back the input"),
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        String message = args["message"] | "No message";
        returns["echo"] = message;
        returns["success"] = true;
        return true;
    }
);
```

Function with defined arguments and returns:
```cpp
FunctionDefinition def("calculator", "Performs arithmetic operations");

// Define arguments
DynamicJsonDocument argsDoc(512);
JsonArray args = argsDoc.to<JsonArray>();
JsonObject arg1 = args.add<JsonObject>();
arg1["key"] = "a";
arg1["type"] = "float";
arg1["description"] = "First operand";
arg1["required"] = true;

// Define returns
DynamicJsonDocument returnsDoc(256);
JsonArray returns = returnsDoc.to<JsonArray>();
JsonObject ret1 = returns.add<JsonObject>();
ret1["key"] = "result";
ret1["type"] = "float";
ret1["description"] = "Result of operation";

def.args = args;
def.returns = returns;

agent->registerFunction("calculator", def, callback);
```

### 3. Register Agent on Server

```cpp
// Create extensions array
DynamicJsonDocument extensionsDoc(256);
JsonArray extensions = extensionsDoc.to<JsonArray>();
extensions.add("default");

// Ensure agent exists
String error;
if (!agent->ensureAgent("ESP32-Agent", extensions, error)) {
    Serial.println("Failed: " + error);
}
```

### 4. Register Functions on Server

```cpp
String error;
if (!agent->registerFunctions(error)) {
    Serial.println("Failed: " + error);
}
```

This sends a GraphQL mutation (`SetExtensionImplementations`) to register all defined functions.

### 5. Handle Assignments

In the WebSocket event handler:

```cpp
if (typeStr == "ASSIGN") {
    String functionName = doc["function_name"] | "";
    String assignation = doc["assignation"] | "";
    JsonObject args = doc["args"].as<JsonObject>();
    
    if (agent->handleAssignment(functionName, assignation, args)) {
        // Success - send YIELD and DONE messages
    } else {
        // Failure - send CRITICAL error message
    }
}
```

## Function Callback Signature

```cpp
bool callback(Agent &agent, JsonObject args, JsonObject &returns)
```

Parameters:
- `agent`: Reference to the Agent instance (access to App, etc.)
- `args`: Input arguments as JsonObject
- `returns`: Output object to populate with results

Return value:
- `true`: Function succeeded
- `false`: Function failed

## Examples

### Example 1: Simple Data Processing

```cpp
agent->registerFunction(
    "add",
    FunctionDefinition("add", "Adds two numbers"),
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        float a = args["a"] | 0.0;
        float b = args["b"] | 0.0;
        returns["result"] = a + b;
        return true;
    }
);
```

### Example 2: Hardware Control

```cpp
agent->registerFunction(
    "set_led",
    FunctionDefinition("set_led", "Controls LED"),
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        bool state = args["state"] | false;
        int pin = args["pin"] | 2;
        
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state ? HIGH : LOW);
        
        returns["pin"] = pin;
        returns["state"] = state;
        return true;
    }
);
```

### Example 3: Using App for GraphQL Queries

```cpp
agent->registerFunction(
    "query_server",
    FunctionDefinition("query_server", "Queries the server"),
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        App *app = agent.getApp();
        
        String query = args["query"] | "{ __typename }";
        String response, error;
        
        bool success = app->graphqlRequest("rekuest", query, response, error);
        
        returns["success"] = success;
        returns["response"] = success ? response : error;
        
        return success;
    }
);
```

### Example 4: Complex Returns

```cpp
agent->registerFunction(
    "get_stats",
    FunctionDefinition("get_stats", "System statistics"),
    [](Agent &agent, JsonObject args, JsonObject &returns) -> bool {
        JsonObject memory = returns["memory"].to<JsonObject>();
        memory["free"] = ESP.getFreeHeap();
        memory["total"] = ESP.getHeapSize();
        
        JsonObject cpu = returns["cpu"].to<JsonObject>();
        cpu["model"] = ESP.getChipModel();
        cpu["freq"] = ESP.getCpuFreqMHz();
        
        return true;
    }
);
```

## GraphQL Mutations

### EnsureAgent

Creates or updates the agent on the server:

```graphql
mutation EnsureAgent($input: AgentInput!) {
  ensureAgent(input: $input) {
    id
    instanceId
    name
  }
}
```

Variables:
```json
{
  "input": {
    "instanceId": "default",
    "name": "ESP32-Agent",
    "extensions": ["default"]
  }
}
```

### SetExtensionImplementations

Registers function implementations:

```graphql
mutation SetExtensionImplementations($input: SetExtensionImplementationsInput!) {
  setExtensionImplementations(input: $input) {
    id
  }
}
```

Variables:
```json
{
  "input": {
    "instanceId": "default",
    "extension": "default",
    "runCleanup": true,
    "implementations": [
      {
        "interface": "function_name",
        "definition": {
          "interface": "function_name",
          "description": "Function description",
          "args": [...],
          "returns": [...]
        },
        "dependencies": []
      }
    ]
  }
}
```

## WebSocket Protocol

### Messages Sent

**REGISTER** (on connect):
```json
{
  "type": "REGISTER",
  "instance_id": "default",
  "token": "<access_token>"
}
```

**HEARTBEAT_ANSWER** (response to HEARTBEAT):
```json
{
  "type": "HEARTBEAT_ANSWER",
  "id": "<uuid4>"
}
```

**YIELD** (function result):
```json
{
  "type": "YIELD",
  "assignation": "<assignation_id>",
  "id": "<uuid4>",
  "returns": { ... }
}
```

**DONE** (function complete):
```json
{
  "type": "DONE",
  "assignation": "<assignation_id>",
  "id": "<uuid4>"
}
```

**CRITICAL** (error):
```json
{
  "type": "CRITICAL",
  "assignation": "<assignation_id>",
  "id": "<uuid4>",
  "error": "<error_message>"
}
```

### Messages Received

**HEARTBEAT**:
```
"HEARTBEAT"
```
or
```json
{
  "type": "HEARTBEAT"
}
```

**ASSIGN**:
```json
{
  "type": "ASSIGN",
  "function_name": "echo",
  "assignation": "<assignation_id>",
  "args": {
    "message": "Hello"
  }
}
```

## Memory Considerations

The ESP32 has limited stack space (~8KB for tasks). When working with the Agent:

1. **Use appropriate JSON document sizes**:
   - Simple functions: 512-1024 bytes
   - Complex with args/returns: 2048-4096 bytes

2. **Store large objects globally** if they're long-lived

3. **Clean up** after function execution if using dynamic allocation

4. **Test memory usage**:
   ```cpp
   Serial.println("Free heap: " + String(ESP.getFreeHeap()));
   ```

## Debugging

Enable verbose output:
```cpp
// In your function:
Serial.println("\n>>> Executing FUNCTION_NAME");
Serial.println("Input arg: " + String(args["key"] | "default"));
Serial.println("Output: " + String(result));
```

Print registry:
```cpp
agent->printRegistry();
```

Monitor WebSocket:
```cpp
// Already implemented in main.cpp
Serial.println("WS << " + message);  // Received
Serial.println("WS >> " + message);  // Sent
```

## Complete Flow

1. **Setup Phase** (once):
   ```
   Connect to WiFi
   → Redeem token
   → Claim fakts
   → Get OAuth2 token
   → Initialize App
   → Create Agent
   → Register functions (local)
   → Ensure agent (server)
   → Register functions (server)
   → Connect WebSocket
   ```

2. **Runtime Phase** (loop):
   ```
   WebSocket.loop()
   → Receive HEARTBEAT → Send HEARTBEAT_ANSWER
   → Receive ASSIGN → Execute function → Send YIELD → Send DONE
   ```

## Next Steps

- **Implement YIELD/DONE/CRITICAL message sending** in WebSocket handler
- **Add error recovery** for failed function executions
- **Implement function cancellation** (if needed)
- **Add function progress reporting** for long-running tasks
- **Test with real Arkitekt server** assignments

## See Also

- `agent_example.h` - Complete examples with argument/return definitions
- Kotlin implementation - Reference for protocol details
- Arkitekt documentation - Server-side API reference
