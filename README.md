# Arkitekt ESP32

An Arduino/PlatformIO library that turns an ESP32 into a first-class [Arkitekt](https://arkitekt.live) agent. Define **functions**, **states**, and **background tasks** in a fluent C++ API, and let the library handle Wi-Fi provisioning, OAuth2 authentication, and the WebSocket connection to the Arkitekt platform.

## How It Works

```
┌──────────┐   BLE / Token    ┌──────────────┐   WebSocket   ┌──────────────┐
│  Pokket  │ ───────────────► │   ESP32      │ ◄───────────► │  Arkitekt    │
│  (Phone) │   provisioning   │  (this lib)  │   agent proto │  (Server)    │
└──────────┘                  └──────────────┘               └──────────────┘
```

1. **Provisioning** — The ESP32 advertises a BLE service. A companion app (or a pre-set token) delivers Wi-Fi credentials and the Arkitekt connection token.
2. **Authentication** — The device redeems the token with `lok` (Arkitekt's auth service) to obtain an OAuth2 access token.
3. **Agent Connection** — A WebSocket connection is established with `rekuest` (Arkitekt's task service). The device registers its functions, states, and begins listening for assignments.

## Provisioning Workflows

### BLE Provisioning with Pokket (recommended)

[**Pokket**](https://github.com/jhnnsrs/pokket) is the companion mobile app for the Arkitekt ecosystem. It uses BLE to send Wi-Fi credentials and the Arkitekt connection token to the ESP32 — no serial connection or hardcoded secrets needed.

1. Flash this firmware to your ESP32.
2. Open Pokket on your Android phone.
3. Scan for nearby devices and select the ESP32.
4. Enter your Wi-Fi profile and Arkitekt instance URL.
5. The ESP32 connects and registers as an agent automatically.

### Pre-configured Token

For headless deployments or CI, you can skip BLE and supply credentials directly in the `RunConfig`:

```cpp
RunConfig cfg;
cfg.ble = false;
cfg.baseUrl = "http://your-arkitekt-instance:8000/";
cfg.redeemToken = "your-token";
app.run(cfg);
```

### Serial / Re-provisioning

On boot the device waits briefly (`bootReconfigureTimeout`) for a BLE connection. If no provisioning happens and valid credentials are already stored in flash (NVS), the device reconnects automatically. To force re-provisioning, simply erase the stored preferences or hold the boot button during startup.

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- An ESP32 dev board (e.g. AZ-Delivery DevKit v4)
- An [Arkitekt](https://arkitekt.live) server instance

### 1. Clone & build

```bash
git clone https://github.com/jhnnsrs/arkitekt-esp32.git
cd arkitekt-esp32
pio run
```

### 2. Flash

```bash
pio run --target upload
```

### 3. Monitor

```bash
pio device monitor
```

## Usage

### Minimal Example

```cpp
#include <Arduino.h>
#include "lib/arkitekt_app.h"

ArkitektApp app("my-device", "1.0.0", "default", "My Device");

void setup() {
    Serial.begin(115200);

    app.addRequirement("rekuest", "live.arkitekt.rekuest");

    // Register a simple function
    auto def = FunctionBuilder("hello", "Says hello")
        .argString("name", "Name", "Who to greet")
        .returnString("greeting", "Greeting")
        .build();

    app.registerFunction("hello", def,
        [](ArkitektApp &app, Agent &agent, JsonObject args, ReplyChannel &reply) -> bool {
            String name = args["name"] | "World";
            StaticJsonDocument<256> doc;
            JsonObject ret = doc.to<JsonObject>();
            ret["greeting"] = "Hello, " + name + "!";
            reply.done(ret);
            return true;
        });

    RunConfig cfg;
    cfg.ble = true;
    app.run(cfg);
}

void loop() { app.loop(); }
```

### Defining Functions

Use `FunctionBuilder` to declare typed argument and return ports:

```cpp
auto def = FunctionBuilder("calculator", "Arithmetic on two numbers", 1024, 512)
    .argFloat("a", "A", "First operand")
    .argFloat("b", "B", "Second operand")
    .argStringChoice("operation", "Operation", "Which operation", {
        {"Add", "add"}, {"Subtract", "subtract"},
        {"Multiply", "multiply"}, {"Divide", "divide"}
    })
    .returnFloat("result", "Result", "Calculation result")
    .returnString("operation_performed", "Operation", "Operation that was executed")
    .build();
```

Available port types: `argInt`, `argFloat`, `argString`, `argBool`, `argStringChoice`, `argStructure` (and their `return*` counterparts).

### Defining States

States are live values that the ESP32 pushes to Arkitekt. Clients can observe them in real time.

```cpp
auto def = StateBuilder("sensor_status", "Sensor Status")
    .portFloat("temperature", "Temperature", "Celsius")
    .portInt("readings_count", "Readings", "Total readings")
    .build();

app.registerState("sensor_status", def, [](AgentState *state) {
    state->setPort("temperature", 0.0f);
    state->setPort("readings_count", 0);
});
```

Update state from anywhere (functions, background tasks):

```cpp
AgentState *state = agent.getState("sensor_status");
if (state) {
    state->setPort("temperature", 23.5f);
}
```

### Background Tasks

Periodic tasks run alongside the agent loop:

```cpp
app.registerBackgroundTask(
    [](ArkitektApp &app, Agent &agent) {
        AgentState *state = agent.getState("sensor_status");
        if (state) {
            float temp = readSensor();
            state->setPort("temperature", temp);
        }
    },
    5000); // every 5 seconds
```

### Run Configuration

```cpp
RunConfig cfg;
cfg.ble = true;                     // Enable BLE provisioning
cfg.enableWpa2Enterprise = true;    // Support Eduroam / WPA2-Enterprise
cfg.bootReconfigureTimeout = 5000;  // ms to wait for BLE before using stored creds
cfg.bleDeviceName = "MY_DEVICE";    // Custom BLE advertised name
app.run(cfg);
```

## Project Structure

```
src/
  main.cpp              — Application entry point (setup/loop)
  lib/
    arkitekt_app.h      — High-level app orchestrator (provisioning → auth → agent)
    agent.h             — WebSocket agent, function registry, state management
    function_builder.h  — Fluent builder for FunctionDefinition
    state_builder.h     — Fluent builder for StateDefinition
    port_builder.h      — Low-level port creation helpers
    reply_channel.h     — Typed reply channel for function results
    fakts.h             — Service discovery (fakts protocol)
    auth.h              — OAuth2 token exchange via lok
    manifest.h          — Device manifest & requirements
    run_config.h        — Runtime configuration struct
    config_defaults.h   — Default URLs and tokens
```

## Dependencies

| Library | Purpose |
|---------|---------|
| [ArduinoJson](https://arduinojson.org/) | JSON serialization for the agent protocol |
| [WebSockets](https://github.com/Links2004/arduinoWebSockets) | WebSocket client for agent ↔ server communication |
| ESP32 BLE (built-in) | BLE provisioning service |

## License

MIT