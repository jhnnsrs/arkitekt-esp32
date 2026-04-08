#ifndef AGENT_H
#define AGENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <map>
#include "app.h"

// Forward declaration
class Agent;

// Function callback type: takes (Agent&, JsonObject args) and returns JsonObject with results
typedef std::function<bool(Agent &, JsonObject, JsonObject &)> AgentFunction;

// Definition structure for a function
struct FunctionDefinition
{
    String key;
    String name;
    String description;
    String version;
    String kind; // "FUNCTION" or "GENERATOR"
    bool stateful;
    bool isDev;
    JsonArray collections;
    JsonArray interfaces;
    JsonArray isTestFor;
    JsonArray args;       // Array of PortInput definitions
    JsonArray returns;    // Array of PortInput definitions
    JsonArray portGroups; // Array of PortGroupInput definitions

    FunctionDefinition() : stateful(false), isDev(false), kind("FUNCTION"), version("0.0.1") {}

    FunctionDefinition(const String &name, const String &desc)
        : key(name), name(name), description(desc), stateful(false), isDev(false), kind("FUNCTION"), version("0.0.1") {}

    void toJson(JsonObject obj) const
    {
        obj["key"] = key;
        obj["name"] = name;
        obj["version"] = version;
        obj["description"] = description;
        obj["kind"] = kind;
        obj["stateful"] = stateful;
        obj["isDev"] = isDev;

        if (collections.size() > 0)
        {
            obj["collections"] = collections;
        }
        else
        {
            JsonArray emptyCollections = obj["collections"].to<JsonArray>();
        }

        if (interfaces.size() > 0)
        {
            obj["interfaces"] = interfaces;
        }
        else
        {
            JsonArray emptyInterfaces = obj["interfaces"].to<JsonArray>();
        }

        if (isTestFor.size() > 0)
        {
            obj["isTestFor"] = isTestFor;
        }
        else
        {
            JsonArray emptyIsTestFor = obj["isTestFor"].to<JsonArray>();
        }

        if (args.size() > 0)
        {
            obj["args"] = args;
        }
        else
        {
            JsonArray emptyArgs = obj["args"].to<JsonArray>();
        }

        if (returns.size() > 0)
        {
            obj["returns"] = returns;
        }
        else
        {
            JsonArray emptyReturns = obj["returns"].to<JsonArray>();
        }

        if (portGroups.size() > 0)
        {
            obj["portGroups"] = portGroups;
        }
        else
        {
            JsonArray emptyPortGroups = obj["portGroups"].to<JsonArray>();
        }
    }
};

// Function Registry
class FunctionRegistry
{
private:
    std::map<String, AgentFunction> functions;
    std::map<String, FunctionDefinition> definitions;

public:
    FunctionRegistry() {}

    void registerFunction(const String &functionName, const FunctionDefinition &definition, AgentFunction callback)
    {
        functions[functionName] = callback;
        definitions[functionName] = definition;
        Serial.println("Registered function: " + functionName);
    }

    AgentFunction getFunction(const String &functionName)
    {
        auto it = functions.find(functionName);
        if (it != functions.end())
        {
            return it->second;
        }
        return nullptr;
    }

    const std::map<String, FunctionDefinition> &getDefinitions() const
    {
        return definitions;
    }

    int getFunctionCount() const
    {
        return functions.size();
    }
};

// Agent class
class Agent
{
private:
    App *app;
    String serviceIdentifier;
    String instanceId;
    String agentName;
    FunctionRegistry *registry;
    String currentAssignation;

public:
    Agent(App *appInstance, const String &service, const String &instance, const String &name)
        : app(appInstance), serviceIdentifier(service), instanceId(instance), agentName(name)
    {
        registry = new FunctionRegistry();
    }

    ~Agent()
    {
        delete registry;
    }

    FunctionRegistry *getRegistry()
    {
        return registry;
    }

    void registerFunction(const String &functionName, const FunctionDefinition &definition, AgentFunction callback)
    {
        registry->registerFunction(functionName, definition, callback);
    }

    bool ensureAgent(const String &name, const JsonArray &extensions, String &errorMessage)
    {
        Serial.println("\n=== Ensuring Agent ===");

        // Step 1: Call ensureAgent to get current hash
        String ensureMutation = R"(
mutation EnsureAgent($input: AgentInput!) {
  ensureAgent(input: $input) {
    id
    instanceId
    name
    hash
  }
}
)";

        DynamicJsonDocument ensureVarsDoc(512);
        JsonObject ensureVars = ensureVarsDoc.to<JsonObject>();
        JsonObject ensureInput = ensureVars["input"].to<JsonObject>();
        ensureInput["instanceId"] = instanceId;
        ensureInput["name"] = name;

        String ensureResponse;
        if (!app->graphqlRequest(serviceIdentifier, ensureMutation, ensureVars, ensureResponse, errorMessage))
        {
            Serial.println("Failed to ensure agent: " + errorMessage);
            return false;
        }

        Serial.println("✓ Agent ensured successfully");

        // Parse the server hash from response
        DynamicJsonDocument ensureRespDoc(1024);
        deserializeJson(ensureRespDoc, ensureResponse);
        String serverHash = ensureRespDoc["data"]["ensureAgent"]["hash"] | "";
        Serial.println("Server agent hash: " + serverHash);

        // Compute local hash from registered definitions
        String localHash = computeDefinitionsHash();
        Serial.println("Local definitions hash: " + localHash);

        // Step 2: Only call implementAgent if hashes differ
        if (serverHash == localHash)
        {
            Serial.println("✓ Agent is up to date, skipping implementAgent");
            return true;
        }

        Serial.println("Hashes differ, re-implementing agent...");
        return implementAgent(name, extensions, errorMessage);
    }

private:
    String computeDefinitionsHash()
    {
        // Build a deterministic string from all definitions and hash it
        const auto &definitions = registry->getDefinitions();
        String hashInput = "";
        for (const auto &pair : definitions)
        {
            hashInput += pair.first + "|";
            hashInput += pair.second.name + "|";
            hashInput += pair.second.key + "|";
            hashInput += pair.second.version + "|";
            hashInput += pair.second.kind + "|";
            hashInput += String(pair.second.stateful) + "|";
            hashInput += String(pair.second.args.size()) + "|";
            hashInput += String(pair.second.returns.size()) + ";";
        }

        // Simple hash: djb2
        unsigned long hash = 5381;
        for (unsigned int i = 0; i < hashInput.length(); i++)
        {
            hash = ((hash << 5) + hash) + hashInput[i];
        }

        return String(hash, HEX);
    }

    bool implementAgent(const String &name, const JsonArray &extensions, String &errorMessage)
    {
        Serial.println("\n=== Implementing Agent ===");

        const auto &definitions = registry->getDefinitions();

        // Build GraphQL mutation
        String mutation = R"(
mutation ImplementAgent($input: ImplementAgentInput!) {
  implementAgent(input: $input) {
    id
    instanceId
    name
    extensions
  }
}
)";

        // Build variables according to ImplementAgentInput schema
        DynamicJsonDocument varsDoc(8192);
        JsonObject vars = varsDoc.to<JsonObject>();
        JsonObject input = vars["input"].to<JsonObject>();
        input["instanceId"] = instanceId;
        input["name"] = name;

        // Extensions
        if (extensions.size() > 0)
        {
            input["extensions"] = extensions;
        }

        // Implementations
        JsonArray implementations = input["implementations"].to<JsonArray>();
        for (const auto &pair : definitions)
        {
            JsonObject implInput = implementations.add<JsonObject>();

            // interface - the function key used for dispatching
            implInput["interface"] = pair.first;

            // extension
            implInput["extension"] = "default";

            // definition (DefinitionInput)
            JsonObject definition = implInput["definition"].to<JsonObject>();
            definition["key"] = pair.second.key.length() > 0 ? pair.second.key : pair.first;
            definition["name"] = pair.second.name;
            definition["version"] = pair.second.version;
            definition["description"] = pair.second.description;
            definition["kind"] = pair.second.kind;
            definition["stateful"] = pair.second.stateful;
            definition["isDev"] = pair.second.isDev;

            // collections, interfaces, isTestFor
            if (pair.second.collections.size() > 0)
                definition["collections"] = pair.second.collections;
            else
                definition["collections"].to<JsonArray>();

            if (pair.second.interfaces.size() > 0)
                definition["interfaces"] = pair.second.interfaces;
            else
                definition["interfaces"].to<JsonArray>();

            if (pair.second.isTestFor.size() > 0)
                definition["isTestFor"] = pair.second.isTestFor;
            else
                definition["isTestFor"].to<JsonArray>();

            // args
            if (pair.second.args.size() > 0)
                definition["args"] = pair.second.args;
            else
                definition["args"].to<JsonArray>();

            // returns
            if (pair.second.returns.size() > 0)
                definition["returns"] = pair.second.returns;
            else
                definition["returns"].to<JsonArray>();

            // portGroups
            if (pair.second.portGroups.size() > 0)
                definition["portGroups"] = pair.second.portGroups;
            else
                definition["portGroups"].to<JsonArray>();

            // dependencies (AgentDependencyInput[]) - empty for now
            implInput["dependencies"].to<JsonArray>();
        }

        // States - empty for now
        // input["states"].to<JsonArray>();

        // Locks - empty for now
        // input["locks"].to<JsonArray>();

        Serial.println("Implementing agent with " + String(definitions.size()) + " function(s)");

        String response;
        if (!app->graphqlRequest(serviceIdentifier, mutation, vars, response, errorMessage))
        {
            Serial.println("Failed to implement agent: " + errorMessage);
            return false;
        }

        Serial.println("✓ Agent implemented successfully");
        Serial.println("Response: " + response);
        return true;
    }

public:
    // Kept for backward compatibility but now just calls ensureAgent
    bool registerFunctions(String &errorMessage)
    {
        Serial.println("registerFunctions() is deprecated - functions are registered via ensureAgent/implementAgent");
        return true;
    }

    bool handleAssignment(const String &functionName, const String &assignation, JsonObject args)
    {
        Serial.println("→ Handling assignment: " + functionName);
        Serial.println("  Assignation ID: " + assignation);

        currentAssignation = assignation;

        AgentFunction func = registry->getFunction(functionName);
        if (func == nullptr)
        {
            Serial.println("✗ Function not found: " + functionName);
            return false;
        }

        // Prepare returns object
        DynamicJsonDocument returnsDoc(1024);
        JsonObject returns = returnsDoc.to<JsonObject>();

        // Execute the function
        bool success = func(*this, args, returns);

        if (success)
        {
            Serial.println("✓ Function executed successfully");
            // Returns will be sent by the caller
            return true;
        }
        else
        {
            Serial.println("✗ Function execution failed");
            return false;
        }
    }

    const String &getInstanceId() const
    {
        return instanceId;
    }

    const String &getAgentName() const
    {
        return agentName;
    }

    App *getApp()
    {
        return app;
    }

    void printRegistry()
    {
        Serial.println("\n=== Agent Function Registry ===");
        Serial.println("Agent: " + agentName);
        Serial.println("Instance ID: " + instanceId);
        Serial.println("Functions: " + String(registry->getFunctionCount()));

        const auto &definitions = registry->getDefinitions();
        for (const auto &pair : definitions)
        {
            Serial.println("  - " + pair.first + ": " + pair.second.description);
        }
        Serial.println("===============================\n");
    }
};

#endif // AGENT_H
