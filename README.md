
```cpp

function the_toggle_led_function(Agent &agent, JsonObject args, replyChannel: Channel<bool>) -> bool {
    bool state = args["state"] | false; // Default to false if not provided
    const int ledPin = LED_BUILTIN; // Use built-in LED pin

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, state ? HIGH : LOW);

    returns["pin"] = ledPin;
    returns["state"] = state;



    replyChannel.yield(state)

}

function a_service_caller(Agent &agent, JsonObject args, replyChannel: Channel<bool>) -> bool {
    
    agent.getService("mikro").graphQlRekuest("mutation { toggleLed(state: " + (args["state"] | false ? "true" : "false") + ") { state } }")


    replyChannel.yield(state)

}


function backgroundTask(Agent: &agent) {
    // This function can perform background tasks if needed
    // For example, it could monitor sensor data or handle other asynchronous events
    while True:
        // Perform background operations here
        delay(1000); // Sleep for a while to avoid busy-waiting
        agent.getState("some_state_key").update("some_port_key" value); // Example of updating state
}

setup() {


    app = App( 
        "default"
        "Agent Example Device"
        requirements=[
            Requirement("key", "string", "A required string argument for the app")
        ]
    );



    definition = FunctionDefinition(
        "toggle_led",
    ).withDescription("Toggle the built-in LED on or off").withArgs([
        Arg("state", "boolean", "Desired state of the LED (true for ON, false for OFF)")
        Arg("pin", "integer", "The pin number of the LED (optional, defaults to built-in LED)")
    ])  

    app.registerFunction(
        definition,
        the_toggle_led_function
    )
    // Register more functions as needed

    state = StateDefinition(
        "my_state"
        ).withPorts([
            Port("some_port_key", "string", "A port for some state")
        ]).withDescription("An example state definition")

    app.registerState(
        "some_state_key",
        state
    )




    app.registerBackgroundTask(
        backgroundTask
    )


    
    app.run(
        ble=True,
        enable_wpa2_enterprise=True,
        boot_reconfiure_timeout=5000,

    ); // Should start the app, 

}

loop() {
    app.loop();
}

```