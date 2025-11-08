
```cpp

function the_toggle_led_function(Agent &agent, JsonObject args, replyChannel:) -> bool {
    bool state = args["state"] | false; // Default to false if not provided
    const int ledPin = LED_BUILTIN; // Use built-in LED pin

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, state ? HIGH : LOW);

    returns["pin"] = ledPin;
    returns["state"] = state;

    replyChannel.yield(state)

}


setup() {


    app = App(
        redeemToken, 
        "default"
        "Agent Example Device"
    );
    app.begin();


    definition = FunctionDefinition(
        "toggle_led", 
        "Toggles the built-in LED on or off"
        args={
            {"state", "boolean", "Desired LED state: true for ON, false for OFF"}
        },
        returns={
            {"pin", "integer", "The pin number of the LED"},
        }
    );

    app.registerFunction(

    )

}

loop() {
    app.loop();
}

```