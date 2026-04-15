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
    String name;
    String description;
    String kind; // "FUNCTION" or "GENERATOR"
    bool stateful;
    bool isDev;
    JsonArray collections;
    JsonArray interfaces;
    JsonArray isTestFor;
    JsonArray args;       // Array of PortInput definitions
    JsonArray returns;    // Array of PortInput definitions
    JsonArray portGroups; // Array of PortGroupInput definitions

    FunctionDefinition() : kind("FUNCTION"), stateful(false), isDev(false) {}

    FunctionDefinition(const String &name, const String &desc)
        : name(name), description(desc), kind("FUNCTION"), stateful(false), isDev(false) {}

    void toJson(JsonObject obj) const
    {
        obj["name"] = name;
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

        // Build GraphQL mutation
        String mutation = R"(
mutation EnsureAgent($input: AgentInput!) {
  ensureAgent(input: $input) {
    id
    instanceId
    name
  }
}
)";

        // Build variables
        DynamicJsonDocument varsDoc(1024);
        JsonObject vars = varsDoc.to<JsonObject>();
        JsonObject input = vars["input"].to<JsonObject>();
        input["instanceId"] = instanceId;
        input["name"] = name;

        if (extensions.size() > 0)
        {
            input["extensions"] = extensions;
        }

        String response;
        if (!app->graphqlRequest(serviceIdentifier, mutation, vars, response, errorMessage))
        {
            Serial.println("Failed to ensure agent: " + errorMessage);
            return false;
        }

        Serial.println("✓ Agent ensured successfully");
        Serial.println("Response: " + response);
        return true;
    }

    bool registerFunctions(String &errorMessage)
    {
        Serial.println("\n=== Registering Functions ===");

        const auto &definitions = registry->getDefinitions();
        if (definitions.empty())
        {
            errorMessage = "No functions to register";
            return false;
        }

        // Build GraphQL mutation
        String mutation = R"(
mutation SetExtensionImplementations($input: SetExtensionImplementationsInput!) {
  setExtensionImplementations(input: $input) {
    id
    interface
    extension
  }
}
)";

        // Build variables according to SetExtensionImplementationsInput schema
        DynamicJsonDocument varsDoc(8192);
        JsonObject vars = varsDoc.to<JsonObject>();
        JsonObject input = vars["input"].to<JsonObject>();
        input["instanceId"] = instanceId;
        input["extension"] = "default";
        input["runCleanup"] = true;

        JsonArray implementations = input["implementations"].to<JsonArray>();

        for (const auto &pair : definitions)
        {
            // Create ImplementationInput structure
            JsonObject implInput = implementations.add<JsonObject>();

            // Set interface (optional in schema)
            implInput["interface"] = pair.first;

            // Create DefinitionInput according to schema
            JsonObject definition = implInput["definition"].to<JsonObject>();
            definition["name"] = pair.second.name;
            definition["description"] = pair.second.description;
            definition["kind"] = "FUNCTION"; // ActionKind enum: FUNCTION or GENERATOR
            definition["stateful"] = false;
            definition["isDev"] = false;

            // Empty arrays for collections, interfaces, isTestFor
            JsonArray collections = definition["collections"].to<JsonArray>();
            JsonArray interfaces = definition["interfaces"].to<JsonArray>();
            JsonArray isTestFor = definition["isTestFor"].to<JsonArray>();

            // Args (PortInput[])
            if (pair.second.args.size() > 0)
            {
                definition["args"] = pair.second.args;
            }
            else
            {
                JsonArray emptyArgs = definition["args"].to<JsonArray>();
            }

            // Returns (PortInput[])
            if (pair.second.returns.size() > 0)
            {
                definition["returns"] = pair.second.returns;
            }
            else
            {
                JsonArray emptyReturns = definition["returns"].to<JsonArray>();
            }

            // Port groups (optional)
            JsonArray portGroups = definition["portGroups"].to<JsonArray>();

            // Dependencies (ActionDependencyInput[]) - required but can be empty
            JsonArray dependencies = implInput["dependencies"].to<JsonArray>();

            // Dynamic flag (defaults to false)
            implInput["dynamic"] = false;
        }

        Serial.println("Registering " + String(definitions.size()) + " function(s)");

        String response;
        if (!app->graphqlRequest(serviceIdentifier, mutation, vars, response, errorMessage))
        {
            Serial.println("Failed to register functions: " + errorMessage);
            return false;
        }

        Serial.println("✓ Functions registered successfully");
        Serial.println("Response: " + response);
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
