#ifndef MANIFEST_H
#define MANIFEST_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Structures for Manifest Configuration
struct Requirement
{
    String key;
    String service;
    bool optional;

    Requirement() : optional(false) {}

    Requirement(const String &k, const String &s, bool opt = false)
        : key(k), service(s), optional(opt) {}

    void toJson(JsonObject obj) const
    {
        obj["key"] = key;
        obj["service"] = service;
        obj["optional"] = optional;
    }
};

struct Manifest
{
    String identifier;
    String version;
    String device_id;
    String scopes[10]; // Fixed size array for scopes
    int scopeCount;
    Requirement requirements[5]; // Fixed size array for requirements
    int requirementCount;

    Manifest() : scopeCount(0), requirementCount(0) {}

    Manifest(const String &id, const String &ver)
        : identifier(id), version(ver), scopeCount(0), requirementCount(0) {}

    void addScope(const String &scope)
    {
        if (scopeCount < 10)
        {
            scopes[scopeCount++] = scope;
        }
    }

    void addDeviceId(const String &deviceId)
    {
        device_id = deviceId;
    }

    void addRequirement(const Requirement &req)
    {
        if (requirementCount < 5)
        {
            requirements[requirementCount++] = req;
        }
    }

    void addRequirement(const String &key, const String &service, bool optional = false)
    {
        addRequirement(Requirement(key, service, optional));
    }

    void toJson(JsonObject obj) const
    {
        obj["identifier"] = identifier;
        obj["version"] = version;
        obj["device_id"] = device_id;

        JsonArray scopesArray = obj["scopes"].to<JsonArray>();
        for (int i = 0; i < scopeCount; i++)
        {
            scopesArray.add(scopes[i]);
        }

        JsonArray requirementsArray = obj["requirements"].to<JsonArray>();
        for (int i = 0; i < requirementCount; i++)
        {
            JsonObject reqObj = requirementsArray.add<JsonObject>();
            requirements[i].toJson(reqObj);
        }
    }

    void print() const
    {
        Serial.println("Manifest:");
        Serial.println("  Identifier: " + identifier);
        Serial.println("  Version: " + version);
        Serial.print("  Scopes: ");
        Serial.print("Device ID: " + device_id);
        for (int i = 0; i < scopeCount; i++)
        {
            Serial.print(scopes[i]);
            if (i < scopeCount - 1)
                Serial.print(", ");
        }
        Serial.println();
        Serial.println("  Requirements:");
        for (int i = 0; i < requirementCount; i++)
        {
            Serial.println("    - Key: " + requirements[i].key + ", Service: " + requirements[i].service + ", Optional: " + String(requirements[i].optional));
        }
    }
};

#endif // MANIFEST_H
