#ifndef ARKITEKT_H
#define ARKITEKT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "manifest.h"
#include "fakts.h"

// Function to get OAuth2 access token using client credentials
inline bool getOAuth2Token(const AuthConfig &auth, String &accessToken, String &errorMessage)
{
    if (!auth.isValid)
    {
        errorMessage = "Invalid auth configuration";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        errorMessage = "WiFi not connected";
        return false;
    }

    HTTPClient http;
    http.begin(auth.token_url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // Build form-encoded body for client credentials grant
    String requestBody = "grant_type=client_credentials";
    requestBody += "&client_id=" + auth.client_id;
    requestBody += "&client_secret=" + auth.client_secret;

    Serial.println("Requesting OAuth2 token from: " + auth.token_url);
    Serial.println("Grant type: client_credentials");
    Serial.println("Client ID: " + auth.client_id);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String payload = http.getString();
        Serial.println("Response: " + payload);

        if (httpResponseCode == 200)
        {
            DynamicJsonDocument responseDoc(1536);
            DeserializationError error = deserializeJson(responseDoc, payload);

            if (error)
            {
                errorMessage = "Failed to parse OAuth2 response JSON";
                http.end();
                return false;
            }

            if (!responseDoc.containsKey("access_token"))
            {
                errorMessage = "No access_token in response";
                http.end();
                return false;
            }

            accessToken = responseDoc["access_token"].as<String>();

            // Optional: also get token type and expiry
            if (responseDoc.containsKey("token_type"))
            {
                String tokenType = responseDoc["token_type"].as<String>();
                Serial.println("Token type: " + tokenType);
            }

            if (responseDoc.containsKey("expires_in"))
            {
                int expiresIn = responseDoc["expires_in"];
                Serial.println("Expires in: " + String(expiresIn) + " seconds");
            }

            http.end();
            return true;
        }
        else
        {
            errorMessage = "OAuth2 request failed with code: " + String(httpResponseCode);
            http.end();
            return false;
        }
    }
    else
    {
        errorMessage = "HTTP request failed with code: " + String(httpResponseCode);
        http.end();
        return false;
    }
}

// Function to perform the redeem request
inline bool redeemToken_request(const Manifest &manifest, const String &redeemToken, const String &retrieveUrl, String &outToken, String &errorMessage)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        errorMessage = "WiFi not connected";
        return false;
    }

    HTTPClient http;
    http.begin(retrieveUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload with minimal stack footprint
    StaticJsonDocument<512> doc;

    // Add manifest to JSON
    JsonObject manifestObj = doc["manifest"].to<JsonObject>();
    manifest.toJson(manifestObj);

    // Add token
    doc["token"] = redeemToken;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("Requesting token from: " + String(retrieveUrl));
    Serial.println("Request body: " + requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String payload = http.getString();
        Serial.println("Response: " + payload);

        if (httpResponseCode == 200)
        {
            DynamicJsonDocument responseDoc(1536);
            DeserializationError error = deserializeJson(responseDoc, payload);

            if (error)
            {
                errorMessage = "Malformed Answer - JSON parse error";
                http.end();
                return false;
            }

            if (!responseDoc.containsKey("status"))
            {
                errorMessage = "Malformed Answer - no status field";
                http.end();
                return false;
            }

            const char *status = responseDoc["status"];

            if (strcmp(status, "error") == 0)
            {
                errorMessage = responseDoc["message"] | "Unknown error";
                http.end();
                return false;
            }

            if (strcmp(status, "granted") == 0)
            {
                outToken = responseDoc["token"] | "";
                if (outToken.length() == 0)
                {
                    errorMessage = "Token not found in response";
                    http.end();
                    return false;
                }
                http.end();
                return true;
            }

            errorMessage = "Unexpected status: " + String(status);
            http.end();
            return false;
        }
        else
        {
            errorMessage = "Error! Could not claim this app on this endpoint";
            http.end();
            return false;
        }
    }
    else
    {
        errorMessage = "HTTP request failed with code: " + String(httpResponseCode);
        http.end();
        return false;
    }
}

// Function to claim fakts configuration
inline bool claimFakts(const String &token, const String &claimUrl, FaktsConfig &faktsConfig, String &errorMessage)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        errorMessage = "WiFi not connected";
        return false;
    }

    HTTPClient http;
    http.begin(claimUrl);
    http.addHeader("Content-Type", "application/json");

    // Create JSON payload
    StaticJsonDocument<128> doc;
    doc["token"] = token;
    doc["secure"] = true; // baseUrl starts with https

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("Claiming fakts from: " + String(claimUrl));
    Serial.println("Request body: " + requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        String payload = http.getString();
        Serial.println("Response: " + payload);

        if (httpResponseCode == 200)
        {
            DynamicJsonDocument responseDoc(4096);
            DeserializationError error = deserializeJson(responseDoc, payload);

            if (error)
            {
                errorMessage = "Malformed Answer - JSON parse error";
                http.end();
                return false;
            }

            if (!responseDoc.containsKey("status"))
            {
                errorMessage = "Malformed Answer - no status field";
                http.end();
                return false;
            }

            const char *status = responseDoc["status"];

            if (strcmp(status, "error") == 0)
            {
                errorMessage = responseDoc["message"] | "Unknown error";
                http.end();
                return false;
            }

            if (strcmp(status, "granted") == 0)
            {
                // Extract and serialize the config object
                if (responseDoc.containsKey("config"))
                {
                    JsonObject config = responseDoc["config"];

                    // Serialize the config to a string for parsing
                    String faktsJson;
                    serializeJson(config, faktsJson);

                    // Parse into FaktsConfig structure
                    if (!faktsConfig.parseFromJson(faktsJson))
                    {
                        errorMessage = "Failed to parse fakts configuration";
                        http.end();
                        return false;
                    }

                    http.end();
                    return true;
                }
                else
                {
                    errorMessage = "Config not found in response";
                    http.end();
                    return false;
                }
            }

            if (strcmp(status, "denied") == 0)
            {
                errorMessage = "Access denied";
                http.end();
                return false;
            }

            errorMessage = "Unexpected status: " + String(status);
            http.end();
            return false;
        }
        else
        {
            errorMessage = "Error! Could not claim this app on this endpoint";
            http.end();
            return false;
        }
    }
    else
    {
        errorMessage = "HTTP request failed with code: " + String(httpResponseCode);
        http.end();
        return false;
    }
}

#endif // ARKITEKT_H
