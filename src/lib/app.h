#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "fakts.h"

// App class to manage services and make requests
class App
{
private:
    String accessToken;
    ServiceInstance *services[10]; // Pointers to service instances
    int serviceCount;
    bool initialized;

public:
    App() : serviceCount(0), initialized(false)
    {
        reset();
    }

    void reset()
    {
        accessToken = "";
        serviceCount = 0;
        initialized = false;
        for (int i = 0; i < 10; i++)
        {
            services[i] = nullptr;
        }
    }

    bool initialize(FaktsConfig &fakts, const String &token)
    {
        reset();
        accessToken = token;

        Serial.println("\n=== Initializing App ===");

        // Test and activate all service instances
        for (int i = 0; i < fakts.instanceCount; i++)
        {
            if (fakts.instances[i].findReachableAlias())
            {
                services[serviceCount++] = &fakts.instances[i];
            }
        }

        if (serviceCount > 0)
        {
            initialized = true;
            Serial.println("✓ App initialized with " + String(serviceCount) + " service(s)");
            return true;
        }
        else
        {
            Serial.println("✗ No services available");
            return false;
        }
    }

    ServiceInstance *getService(const String &key)
    {
        for (int i = 0; i < serviceCount; i++)
        {
            if (services[i]->key == key)
            {
                return services[i];
            }
        }
        return nullptr;
    }

    bool httpRequest(const String &serviceIdentifier, const String &endpoint, const String &method, const String &body, String &response, String &errorMessage)
    {
        if (!initialized)
        {
            errorMessage = "App not initialized";
            return false;
        }

        ServiceInstance *service = getService(serviceIdentifier);
        if (service == nullptr)
        {
            errorMessage = "Service not found: " + serviceIdentifier;
            return false;
        }

        String url = service->getBaseUrl();
        if (!endpoint.startsWith("/"))
        {
            url += "/";
        }
        url += endpoint;

        Serial.println("Making " + method + " request to: " + url);

        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        if (accessToken.length() > 0)
        {
            http.addHeader("Authorization", "Bearer " + accessToken);
        }

        int httpCode;
        if (method == "GET")
        {
            httpCode = http.GET();
        }
        else if (method == "POST")
        {
            httpCode = http.POST(body);
        }
        else if (method == "PUT")
        {
            httpCode = http.PUT(body);
        }
        else if (method == "DELETE")
        {
            httpCode = http.sendRequest("DELETE", body);
        }
        else
        {
            errorMessage = "Unsupported HTTP method: " + method;
            http.end();
            return false;
        }

        if (httpCode > 0)
        {
            response = http.getString();
            http.end();

            if (httpCode >= 200 && httpCode < 300)
            {
                Serial.println("✓ Request successful (HTTP " + String(httpCode) + ")");
                return true;
            }
            else
            {
                errorMessage = "HTTP error " + String(httpCode) + ": " + response;
                return false;
            }
        }
        else
        {
            errorMessage = "Request failed: " + String(httpCode);
            http.end();
            return false;
        }
    }

    bool httpGet(const String &serviceIdentifier, const String &endpoint, String &response, String &errorMessage)
    {
        return httpRequest(serviceIdentifier, endpoint, "GET", "", response, errorMessage);
    }

    bool httpPost(const String &serviceIdentifier, const String &endpoint, const String &body, String &response, String &errorMessage)
    {
        return httpRequest(serviceIdentifier, endpoint, "POST", body, response, errorMessage);
    }

    bool graphqlRequest(const String &serviceIdentifier, const String &query, JsonObject variables, String &response, String &errorMessage)
    {
        if (!initialized)
        {
            errorMessage = "App not initialized";
            return false;
        }

        ServiceInstance *service = getService(serviceIdentifier);
        if (service == nullptr)
        {
            errorMessage = "Service not found: " + serviceIdentifier;
            return false;
        }

        String url = service->getBaseUrl();
        if (!url.endsWith("/"))
        {
            url += "/";
        }
        url += "graphql";

        Serial.println("Making GraphQL request to: " + url);

        // Build GraphQL request body
        DynamicJsonDocument requestDoc(2048);
        requestDoc["query"] = query;

        if (!variables.isNull())
        {
            requestDoc["variables"] = variables;
        }

        String requestBody;
        serializeJson(requestDoc, requestBody);

        Serial.println("GraphQL Query: " + query);
        if (!variables.isNull())
        {
            String varsStr;
            serializeJson(variables, varsStr);
            Serial.println("Variables: " + varsStr);
        }

        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        if (accessToken.length() > 0)
        {
            http.addHeader("Authorization", "Bearer " + accessToken);
        }

        int httpCode = http.POST(requestBody);

        if (httpCode > 0)
        {
            response = http.getString();
            http.end();

            if (httpCode >= 200 && httpCode < 300)
            {
                Serial.println("✓ GraphQL request successful (HTTP " + String(httpCode) + ")");
                return true;
            }
            else
            {
                errorMessage = "HTTP error " + String(httpCode) + ": " + response;
                return false;
            }
        }
        else
        {
            errorMessage = "Request failed: " + String(httpCode);
            http.end();
            return false;
        }
    }

    bool graphqlRequest(const String &serviceIdentifier, const String &query, String &response, String &errorMessage)
    {
        // Overload for queries without variables
        DynamicJsonDocument emptyDoc(16);
        JsonObject emptyVars = emptyDoc.to<JsonObject>();
        return graphqlRequest(serviceIdentifier, query, emptyVars, response, errorMessage);
    }

    String getWebSocketUrl(const String &serviceIdentifier, const String &endpoint)
    {
        if (!initialized)
        {
            return "";
        }

        ServiceInstance *service = getService(serviceIdentifier);
        if (service == nullptr || service->activeAlias == nullptr)
        {
            return "";
        }

        // Convert http(s) to ws(s)
        String baseUrl = service->getBaseUrl();
        if (baseUrl.startsWith("https://"))
        {
            baseUrl.replace("https://", "wss://");
        }
        else if (baseUrl.startsWith("http://"))
        {
            baseUrl.replace("http://", "ws://");
        }

        if (!endpoint.startsWith("/"))
        {
            baseUrl += "/";
        }
        baseUrl += endpoint;

        return baseUrl;
    }

    void printServices()
    {
        Serial.println("\n=== App Services ===");
        Serial.println("Access Token: " + (accessToken.length() > 0 ? "Set (" + String(accessToken.length()) + " chars)" : "Not set"));
        Serial.println("Services: " + String(serviceCount));
        for (int i = 0; i < serviceCount; i++)
        {
            Serial.println("  - " + services[i]->identifier + " @ " + services[i]->getBaseUrl());
        }
        Serial.println("====================\n");
    }
};

#endif // APP_H
