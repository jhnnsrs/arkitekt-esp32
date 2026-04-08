#ifndef FAKTS_H
#define FAKTS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Auth Configuration
struct AuthConfig
{
    String client_id;
    String client_secret;
    String token_url;
    bool isValid;

    AuthConfig() { reset(); }

    void reset()
    {
        client_id = "";
        client_secret = "";
        token_url = "";
        isValid = false;
    }

    bool parseFromJson(JsonObject authObj)
    {
        if (!authObj.containsKey("client_id") || !authObj.containsKey("client_secret") || !authObj.containsKey("token_url"))
        {
            return false;
        }

        client_id = authObj["client_id"].as<String>();
        client_secret = authObj["client_secret"].as<String>();
        token_url = authObj["token_url"].as<String>();
        isValid = true;
        return true;
    }

    void print()
    {
        Serial.println("Auth Configuration:");
        Serial.println("  Client ID: " + client_id);
        Serial.println("  Client Secret: " + client_secret);
        Serial.println("  Token URL: " + token_url);
    }
};

// Alias Configuration
struct Alias
{
    bool ssl;
    String host;
    int port; // -1 means null/not specified
    String path;
    String challenge;
    bool isValid;

    Alias() { reset(); }

    void reset()
    {
        ssl = false;
        host = "";
        port = -1;
        path = "";
        challenge = "";
        isValid = false;
    }

    bool parseFromJson(JsonObject aliasObj)
    {
        if (!aliasObj.containsKey("ssl") || !aliasObj.containsKey("host"))
        {
            return false;
        }

        ssl = aliasObj["ssl"].as<bool>();
        host = aliasObj["host"].as<String>();

        if (aliasObj.containsKey("port") && !aliasObj["port"].isNull())
        {
            port = aliasObj["port"].as<int>();
        }
        else
        {
            port = -1;
        }

        path = aliasObj["path"] | "";
        challenge = aliasObj["challenge"] | "";
        isValid = true;
        return true;
    }

    String getBaseUrl() const
    {
        String protocol = ssl ? "https://" : "http://";
        String url = protocol + host;

        if (port != -1)
        {
            url += ":" + String(port);
        }

        if (path.length() > 0)
        {
            if (!path.startsWith("/"))
            {
                url += "/";
            }
            url += path;
        }

        return url;
    }

    String getChallengeUrl() const
    {
        String baseUrl = getBaseUrl();
        if (challenge.length() > 0)
        {
            if (!baseUrl.endsWith("/"))
            {
                baseUrl += "/";
            }
            baseUrl += challenge;
        }
        return baseUrl;
    }

    void print() const
    {
        Serial.println("    Alias:");
        Serial.println("      Host: " + host);
        Serial.println("      SSL: " + String(ssl ? "true" : "false"));
        Serial.println("      Port: " + String(port == -1 ? "null" : String(port)));
        Serial.println("      Path: " + (path.length() > 0 ? path : "null"));
        Serial.println("      Challenge: " + (challenge.length() > 0 ? challenge : "null"));
        Serial.println("      Base URL: " + getBaseUrl());
        Serial.println("      Challenge URL: " + getChallengeUrl());
    }
};

// Service Instance
struct ServiceInstance
{
    String key;        // The record key (e.g. "rekuest")
    String service;
    String identifier;
    Alias aliases[5]; // Support up to 5 aliases
    int aliasCount;
    Alias *activeAlias; // Pointer to the active/reachable alias
    bool isValid;

    ServiceInstance() { reset(); }

    void reset()
    {
        key = "";
        service = "";
        identifier = "";
        aliasCount = 0;
        activeAlias = nullptr;
        isValid = false;
        for (int i = 0; i < 5; i++)
        {
            aliases[i].reset();
        }
    }

    bool parseFromJson(JsonObject instanceObj)
    {
        reset();
        if (!instanceObj.containsKey("service") || !instanceObj.containsKey("identifier"))
        {
            return false;
        }

        service = instanceObj["service"].as<String>();
        identifier = instanceObj["identifier"].as<String>();

        if (instanceObj.containsKey("aliases"))
        {
            JsonArray aliasesArray = instanceObj["aliases"];
            for (JsonObject aliasObj : aliasesArray)
            {
                if (aliasCount < 5)
                {
                    if (aliases[aliasCount].parseFromJson(aliasObj))
                    {
                        aliasCount++;
                    }
                }
            }
        }

        isValid = true;
        return true;
    }

    bool testAlias(const Alias &alias)
    {
        String challengeUrl = alias.getChallengeUrl();
        Serial.println("  Testing alias: " + challengeUrl);

        HTTPClient http;
        http.begin(challengeUrl);
        http.setTimeout(5000); // 5 second timeout

        int httpCode = http.GET();
        http.end();

        if (httpCode > 0 && httpCode < 400)
        {
            Serial.println("  ✓ Alias reachable (HTTP " + String(httpCode) + ")");
            return true;
        }
        else
        {
            Serial.println("  ✗ Alias unreachable (HTTP " + String(httpCode) + ")");
            return false;
        }
    }

    bool findReachableAlias()
    {
        Serial.println("Finding reachable alias for service: " + identifier);

        for (int i = 0; i < aliasCount; i++)
        {
            if (testAlias(aliases[i]))
            {
                activeAlias = &aliases[i];
                Serial.println("✓ Active alias set for " + identifier);
                return true;
            }
        }

        Serial.println("✗ No reachable alias found for " + identifier);
        return false;
    }

    String getBaseUrl() const
    {
        if (activeAlias != nullptr)
        {
            return activeAlias->getBaseUrl();
        }
        return "";
    }

    void print() const
    {
        Serial.println("  Service Instance:");
        Serial.println("    Key: " + key);
        Serial.println("    Service: " + service);
        Serial.println("    Identifier: " + identifier);
        Serial.println("    Aliases: " + String(aliasCount));
        for (int i = 0; i < aliasCount; i++)
        {
            aliases[i].print();
        }
        if (activeAlias != nullptr)
        {
            Serial.println("    Active Alias: " + activeAlias->getBaseUrl());
        }
    }
};

// Self Configuration
struct SelfConfig
{
    String deployment_name;
    bool isValid;

    SelfConfig() { reset(); }

    void reset()
    {
        deployment_name = "";
        isValid = false;
    }

    bool parseFromJson(JsonObject selfObj)
    {
        if (!selfObj.containsKey("deployment_name"))
        {
            return false;
        }

        deployment_name = selfObj["deployment_name"].as<String>();
        isValid = true;
        return true;
    }

    void print()
    {
        Serial.println("Self Configuration:");
        Serial.println("  Deployment Name: " + deployment_name);
    }
};

// Fakts Configuration
struct FaktsConfig
{
    SelfConfig self;
    AuthConfig auth;
    ServiceInstance instances[10]; // Support up to 10 service instances
    int instanceCount;
    bool isValid;

    FaktsConfig() { reset(); }

    void reset()
    {
        self.reset();
        auth.reset();
        for (int i = 0; i < 10; i++)
        {
            instances[i].reset();
        }
        instanceCount = 0;
        isValid = false;
    }

    bool parseFromJson(const String &jsonStr)
    {
        reset();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, jsonStr);

        if (error)
        {
            Serial.println("Failed to parse fakts JSON");
            return false;
        }

        if (!doc.containsKey("self") || !doc.containsKey("auth"))
        {
            Serial.println("Missing required fakts fields");
            return false;
        }

        if (!self.parseFromJson(doc["self"]))
        {
            Serial.println("Failed to parse self config");
            return false;
        }

        if (!auth.parseFromJson(doc["auth"]))
        {
            Serial.println("Failed to parse auth config");
            return false;
        }

        // Parse instances
        if (doc.containsKey("instances"))
        {
            JsonObject instancesObj = doc["instances"];
            for (JsonPair kv : instancesObj)
            {
                if (instanceCount < 10)
                {
                    instances[instanceCount].reset();
                    if (instances[instanceCount].parseFromJson(kv.value()))
                    {
                        instances[instanceCount].key = String(kv.key().c_str());
                        instanceCount++;
                    }
                }
            }
        }

        isValid = true;
        return true;
    }

    ServiceInstance *findInstance(const String &key)
    {
        for (int i = 0; i < instanceCount; i++)
        {
            if (instances[i].key == key)
            {
                return &instances[i];
            }
        }
        return nullptr;
    }

    void print()
    {
        Serial.println("\n=== Fakts Configuration ===");
        self.print();
        auth.print();
        Serial.println("Instances: " + String(instanceCount));
        for (int i = 0; i < instanceCount; i++)
        {
            instances[i].print();
        }
        Serial.println("===========================\n");
    }
};

#endif // FAKTS_H
