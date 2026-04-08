#ifndef ARKITEKT_EASY_H
#define ARKITEKT_EASY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include "agent.h"
#include "port_builder.h"
#include "app.h"
#include "arkitekt.h"

/*
 * Simplified Arkitekt API for easier function registration
 *
 * This provides a cleaner interface similar to:
 *
 * ArkitektApp.begin(redeemToken, instanceId, agentName);
 * ArkitektApp.registerFunction("name", definition, callback);
 * ArkitektApp.loop();
 */

// Reply channel for yielding results and logging
class ReplyChannel
{
private:
    JsonObject *returns;
    String assignation;
    WebSocketsClient *ws;

    // Generate a simple UUID4-like string
    String generateUUID4()
    {
        String uuid = "";
        const char *hexChars = "0123456789abcdef";

        for (int i = 0; i < 36; i++)
        {
            if (i == 8 || i == 13 || i == 18 || i == 23)
            {
                uuid += "-";
            }
            else if (i == 14)
            {
                uuid += "4"; // Version 4
            }
            else if (i == 19)
            {
                uuid += hexChars[(random(0, 4) + 8)];
            }
            else
            {
                uuid += hexChars[random(0, 16)];
            }
        }
        return uuid;
    }

public:
    ReplyChannel(JsonObject *ret, const String &assignId, WebSocketsClient *socket)
        : returns(ret), assignation(assignId), ws(socket) {}

    // Yield a result back to the server
    void yield(const JsonObject &result)
    {
        if (returns)
        {
            *returns = result;
        }

        // Send YIELD message over WebSocket
        if (ws)
        {
            StaticJsonDocument<1024> yieldDoc;
            yieldDoc["type"] = "YIELD";
            yieldDoc["assignation"] = assignation;
            yieldDoc["id"] = generateUUID4();

            // Copy returns to the message
            JsonObject returnsObj = yieldDoc["returns"].to<JsonObject>();
            for (JsonPair kv : result)
            {
                returnsObj[kv.key()] = kv.value();
            }

            String yieldMsg;
            serializeJson(yieldDoc, yieldMsg);
            Serial.println("WS >> " + yieldMsg);
            ws->sendTXT(yieldMsg);
        }
    }

    // Yield a simple value
    void yield(const String &key, JsonVariant value)
    {
        if (returns)
        {
            (*returns)[key] = value;
        }

        // Send YIELD message over WebSocket
        if (ws)
        {
            StaticJsonDocument<512> yieldDoc;
            yieldDoc["type"] = "YIELD";
            yieldDoc["assignation"] = assignation;
            yieldDoc["id"] = generateUUID4();
            yieldDoc["returns"][key] = value;

            String yieldMsg;
            serializeJson(yieldDoc, yieldMsg);
            Serial.println("WS >> " + yieldMsg);
            ws->sendTXT(yieldMsg);
        }
    }

    // Yield a simple value with const char* key
    void yield(const char *key, JsonVariant value)
    {
        if (returns)
        {
            (*returns)[key] = value;
        }
    }

    // Log a message
    void log(const String &message, const String &level = "INFO")
    {
        Serial.println("[" + level + "] " + message);

        // Send LOG message over WebSocket
        if (ws)
        {
            StaticJsonDocument<512> logDoc;
            logDoc["type"] = "LOG";
            logDoc["assignation"] = assignation;
            logDoc["id"] = generateUUID4();
            logDoc["message"] = message;
            logDoc["level"] = level;

            String logMsg;
            serializeJson(logDoc, logMsg);
            Serial.println("WS >> " + logMsg);
            ws->sendTXT(logMsg);
        }
    }

    // Report progress (0-100)
    void progress(int percent, const String &message = "")
    {
        Serial.println("Progress: " + String(percent) + "% " + message);

        // Send PROGRESS message over WebSocket
        if (ws)
        {
            StaticJsonDocument<512> progressDoc;
            progressDoc["type"] = "PROGRESS";
            progressDoc["assignation"] = assignation;
            progressDoc["id"] = generateUUID4();
            progressDoc["progress"] = percent;
            if (message.length() > 0)
            {
                progressDoc["message"] = message;
            }

            String progressMsg;
            serializeJson(progressDoc, progressMsg);
            Serial.println("WS >> " + progressMsg);
            ws->sendTXT(progressMsg);
        }
    }
};

// Simplified function callback type
typedef std::function<bool(ReplyChannel &)> SimpleFunctionCallback;
typedef std::function<bool(JsonObject, ReplyChannel &)> FunctionCallback;

// Simplified Arkitekt App wrapper
class ArkitektApp
{
private:
    App *app;
    Agent *agent;
    FaktsConfig faktsConfig;
    String accessToken;
    String instanceId;
    String agentName;
    bool initialized;

    // Store documents for function definitions
    std::map<String, DynamicJsonDocument *> functionDocs;

public:
    ArkitektApp() : app(nullptr), agent(nullptr), initialized(false) {}

    ~ArkitektApp()
    {
        if (agent)
            delete agent;
        if (app)
            delete app;

        // Clean up function documents
        for (auto &pair : functionDocs)
        {
            delete pair.second;
        }
    }

    // Initialize the app with authentication
    bool begin(const char *redeemToken, const String &instance = "default", const String &name = "ESP32-Agent")
    {
        instanceId = instance;
        agentName = name;

        Serial.println("\n=== Arkitekt App Initialization ===");

        // Redeem token
        String token, errorMessage;
        const char *retrieveUrl = "https://go.arkitekt.live/lok/f/redeem/";
        const char *claimUrl = "https://go.arkitekt.live/lok/f/claim/";

        Manifest manifest("esp32-app", "1.0.0");
        manifest.addRequirement("rekuest", "live.arkitekt.rekuest", false);

        if (!redeemToken_request(manifest, redeemToken, retrieveUrl, token, errorMessage))
        {
            Serial.println("✗ Redeem failed: " + errorMessage);
            return false;
        }

        // Claim fakts
        faktsConfig.reset();
        if (!claimFakts(token, claimUrl, faktsConfig, errorMessage))
        {
            Serial.println("✗ Claim failed: " + errorMessage);
            return false;
        }

        // Get OAuth2 token
        if (!getOAuth2Token(faktsConfig.auth, accessToken, errorMessage))
        {
            Serial.println("✗ OAuth2 failed: " + errorMessage);
            return false;
        }

        // Initialize App
        app = new App();
        app->reset();
        if (!app->initialize(faktsConfig, accessToken))
        {
            Serial.println("✗ App initialization failed");
            return false;
        }

        // Create Agent
        agent = new Agent(app, "rekuest", instanceId, agentName);

        initialized = true;
        Serial.println("✓ Arkitekt App initialized");
        return true;
    }

    // Register a function with simple pattern
    bool registerFunction(
        const String &name,
        const String &description,
        FunctionCallback callback,
        JsonArray *args = nullptr,
        JsonArray *returns = nullptr)
    {
        if (!initialized || !agent)
        {
            Serial.println("✗ App not initialized");
            return false;
        }

        // Create a document to hold the definition
        DynamicJsonDocument *doc = new DynamicJsonDocument(2048);
        functionDocs[name] = doc;

        FunctionDefinition def = DefinitionBuilder::create(name, description, "FUNCTION", false);

        if (args)
        {
            def.args = *args;
        }
        if (returns)
        {
            def.returns = *returns;
        }

        // Wrap the callback to provide ReplyChannel
        agent->registerFunction(
            name,
            def,
            [callback](Agent &agent, JsonObject args, JsonObject &returns) -> bool
            {
                ReplyChannel channel(&returns, "", nullptr);
                return callback(args, channel);
            });

        Serial.println("✓ Registered function: " + name);
        return true;
    }

    // Register a function with no arguments
    bool registerFunction(
        const String &name,
        const String &description,
        SimpleFunctionCallback callback,
        JsonArray *returns = nullptr)
    {
        return registerFunction(
            name,
            description,
            [callback](JsonObject args, ReplyChannel &channel) -> bool
            {
                return callback(channel);
            },
            nullptr,
            returns);
    }

    // Finalize registration (ensure agent and register functions on server)
    bool finalize()
    {
        if (!initialized || !agent)
        {
            Serial.println("✗ App not initialized");
            return false;
        }

        Serial.println("\n=== Finalizing Agent ===");

        // Ensure agent on server
        String error;
        DynamicJsonDocument extensionsDoc(256);
        JsonArray extensions = extensionsDoc.to<JsonArray>();
        extensions.add("default");

        if (!agent->ensureAgent(agentName, extensions, error))
        {
            Serial.println("✗ Failed to ensure agent: " + error);
            return false;
        }

        // Register functions on server
        if (!agent->registerFunctions(error))
        {
            Serial.println("✗ Failed to register functions: " + error);
            return false;
        }

        Serial.println("✓ Agent finalized and ready");
        return true;
    }

    // Get the underlying App instance
    App *getApp() { return app; }

    // Get the underlying Agent instance
    Agent *getAgent() { return agent; }

    // Check if initialized
    bool isReady() { return initialized; }
};

// Global instance helper (optional)
extern ArkitektApp Arkitekt;

#endif // ARKITEKT_EASY_H
