#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "manifest.h"
#include "fakts.h"
#include "arkitekt.h"
#include "app.h"
#include "agent.h"
#include "agent_example_updated.h"
/*
 * Arkitekt Authentication Flow for ESP32
 *
 * This program demonstrates the complete authentication flow:
 * 1. Redeem Token: Exchange a redeem token for a claim token
 * 2. Claim Fakts: Use the claim token to get the configuration (fakts)
 * 3. OAuth2 Authentication: Use client credentials from fakts to get an access token
 *
 * The fakts configuration includes:
 * - self: deployment information
 * - auth: OAuth2 credentials (client_id, client_secret, token_url)
 * - instances: service endpoints with aliases
 */

// WiFi credentials
const char *ssid = "FRITZ!Box 7530 XJ";
const char *password = "86398244112958418480";

// Redeem configuration
const char *redeemToken = "anothersecrettoken";
const char *baseUrl = "http://go.arkitekt.live/";
const char *retrieveUrl = "https://go.arkitekt.live/lok/f/redeem/";
const char *claimUrl = "https://go.arkitekt.live/lok/f/claim/";

// Create manifest instance at startup
Manifest appManifest("test-esp32", "1.0.0");
FaktsConfig globalFaktsConfig;
App globalApp;
Agent *globalAgent = nullptr;
String globalAccessToken;
bool appReady = false;
String lastRedeemedToken;
WebSocketsClient webSocket;

// Forward declarations
bool initializeAppFlow();
void setupWebSocket(ServiceInstance *service);
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  } // (optional, native USB boards)
  Serial.println("Hello");

  // Turn on LED on GPIO2 to indicate startup
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Serial.println("LED on GPIO2 turned ON");

  // Initialize the manifest with requirements
  appManifest.addRequirement("rekuest", "live.arkitekt.rekuest", false);
  // You can add more requirements or scopes here
  // appManifest.addScope("read:nodes");
  // appManifest.addRequirement("fluss", "live.arkitekt.fluss", true);

  Serial.println("\n=== Application Manifest ===");
  appManifest.print();
  Serial.println("============================\n");

  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    appReady = initializeAppFlow();

    if (!appReady)
    {
      Serial.println("⚠ App initialization failed. Check logs above.");
    }
  }
  else
  {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    Serial.println("Please check your WiFi credentials and network availability");
  }
}

void loop()
{
  if (appReady)
  {
    webSocket.loop();
    return;
  }

  delay(1000);
}

bool initializeAppFlow()
{
  Serial.println("\n=== Starting Redeem Request ===");

  String token;
  String errorMessage;

  if (!redeemToken_request(appManifest, redeemToken, retrieveUrl, token, errorMessage))
  {
    Serial.println("✗ Redeem failed!");
    Serial.println("Error: " + errorMessage);
    return false;
  }

  Serial.println("✓ Token retrieved successfully!");
  Serial.println("Token: " + token);
  lastRedeemedToken = token;

  Serial.println("\n=== Claiming Fakts Configuration ===");
  globalFaktsConfig.reset();
  String claimError;

  if (!claimFakts(token, claimUrl, globalFaktsConfig, claimError))
  {
    Serial.println("✗ Claim failed!");
    Serial.println("Error: " + claimError);
    return false;
  }

  Serial.println("✓ Fakts configuration retrieved successfully!");
  globalFaktsConfig.print();

  Serial.println("\n=== Getting OAuth2 Access Token ===");
  String accessToken;
  String oauth2Error;

  if (!getOAuth2Token(globalFaktsConfig.auth, accessToken, oauth2Error))
  {
    Serial.println("✗ OAuth2 token request failed!");
    Serial.println("Error: " + oauth2Error);
    return false;
  }

  Serial.println("✓ OAuth2 access token retrieved successfully!");
  Serial.println("Access Token: " + accessToken);
  globalAccessToken = accessToken;

  Serial.println("\n=== Initializing App ===");
  globalApp.reset();

  if (!globalApp.initialize(globalFaktsConfig, accessToken))
  {
    Serial.println("✗ Failed to initialize app");
    return false;
  }

  globalApp.printServices();

  ServiceInstance *rekuestService = globalApp.getService("rekuest");
  if (rekuestService == nullptr)
  {
    Serial.println("⚠ Rekuest service not available");
    return false;
  }

  if (rekuestService->activeAlias == nullptr)
  {
    Serial.println("⚠ Rekuest service has no reachable alias");
    return false;
  }

  // Create and initialize agent
  Serial.println("\n=== Creating Agent ===");
  globalAgent = new Agent(&globalApp, "rekuest", "default", "ESP32-Agent");

  // Register functions
  registerAllUpdatedExamples(globalAgent);
  globalAgent->printRegistry();

  // Ensure agent on server
  String agentError;
  DynamicJsonDocument extensionsDoc(256);
  JsonArray extensions = extensionsDoc.to<JsonArray>();
  extensions.add("default");

  if (!globalAgent->ensureAgent("ESP32-Agent", extensions, agentError))
  {
    Serial.println("⚠ Failed to ensure agent: " + agentError);
    return false;
  }

  // Register functions on server
  String registerError;
  if (!globalAgent->registerFunctions(registerError))
  {
    Serial.println("⚠ Failed to register functions: " + registerError);
    return false;
  }

  setupWebSocket(rekuestService);

  Serial.println("\n=== App Ready ===");
  Serial.println("Listening for assignments over WebSocket...");
  return true;
}

void setupWebSocket(ServiceInstance *service)
{
  Alias *alias = service->activeAlias;
  if (alias == nullptr)
  {
    Serial.println("Cannot setup WebSocket: no active alias");
    return;
  }

  uint16_t port = (alias->port != -1) ? static_cast<uint16_t>(alias->port) : static_cast<uint16_t>(alias->ssl ? 443 : 80);

  String path = alias->path;
  if (path.length() == 0)
  {
    path = "/";
  }
  else if (!path.startsWith("/"))
  {
    path = "/" + path;
  }

  if (!path.endsWith("/"))
  {
    path += "/";
  }

  path += "agi";

  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);

  Serial.print("Connecting to WebSocket at ");
  Serial.print(alias->host);
  Serial.print(":");
  Serial.print(port);
  Serial.println(path);

  if (alias->ssl)
  {
    webSocket.beginSSL(alias->host.c_str(), port, path.c_str());
  }
  else
  {
    webSocket.begin(alias->host.c_str(), port, path.c_str());
  }
}

static String payloadToString(uint8_t *payload, size_t length)
{
  String message;
  message.reserve(length);
  for (size_t i = 0; i < length; ++i)
  {
    message += static_cast<char>(payload[i]);
  }
  return message;
}

String generateUUID4()
{
  // Generate a simple UUID4-like string using random bytes
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
      // Variant bits: 10xx
      uuid += hexChars[(random(0, 4) + 8)];
    }
    else
    {
      uuid += hexChars[random(0, 16)];
    }
  }

  return uuid;
}

void handleHeartbeat()
{
  Serial.println("↪ Responding to HEARTBEAT");

  // Create HEARTBEAT_ANSWER with random UUID4
  StaticJsonDocument<256> heartbeatDoc;
  heartbeatDoc["type"] = "HEARTBEAT_ANSWER";
  heartbeatDoc["id"] = generateUUID4();

  String heartbeatMsg;
  serializeJson(heartbeatDoc, heartbeatMsg);

  Serial.println("WS >> " + heartbeatMsg);
  webSocket.sendTXT(heartbeatMsg);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.println("WebSocket connected");
    if (globalAccessToken.length() > 0)
    {
      Serial.println("Sending REGISTER message with access token");

      // Create registration message: {type: "REGISTER", instance_id: "default", token: ACCESS_TOKEN}
      StaticJsonDocument<512> registerDoc;
      registerDoc["type"] = "REGISTER";
      registerDoc["instance_id"] = "default";
      registerDoc["token"] = globalAccessToken;

      String registerMsg;
      serializeJson(registerDoc, registerMsg);

      Serial.println("WS >> " + registerMsg);
      webSocket.sendTXT(registerMsg);
    }
    break;

  case WStype_DISCONNECTED:
  {
    // Extract disconnect reason if available
    uint16_t statusCode = 0;
    if (length >= 2)
    {
      statusCode = (payload[0] << 8) | payload[1];
    }

    Serial.print("WebSocket disconnected");
    if (statusCode > 0)
    {
      Serial.print(" - Status code: ");
      Serial.print(statusCode);
    }
    Serial.println();
  }
  break;

  case WStype_TEXT:
  {
    String message = payloadToString(payload, length);
    Serial.println("WS << " + message);

    String trimmed = message;
    trimmed.trim();

    if (trimmed.equalsIgnoreCase("HEARTBEAT"))
    {
      handleHeartbeat();
      return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, message);
    if (!err)
    {
      const char *typeField = doc["type"] | "";
      String typeStr(typeField);
      typeStr.toUpperCase();
      if (typeStr == "HEARTBEAT")
      {
        handleHeartbeat();
        return;
      }
      if (typeStr == "ASSIGN")
      {
        Serial.println("\n=== Received Assignment ===");

        if (globalAgent != nullptr)
        {
          String interfaceName = doc["interface"] | "";
          String assignation = doc["assignation"] | "";

          Serial.println("InterfaceName: " + interfaceName);
          Serial.println("Assignation: " + assignation);

          // Extract args
          JsonObject args = doc["args"].as<JsonObject>();

          // Handle assignment via agent
          if (globalAgent->handleAssignment(interfaceName, assignation, args))
          {
            Serial.println("✓ Assignment handled successfully");

            // TODO: Send YIELD and DONE events via WebSocket
            // For now, just log
            Serial.println("TODO: Send YIELD and DONE messages");
          }
          else
          {
            Serial.println("✗ Assignment failed");
            // TODO: Send CRITICAL error event
          }
        }
        else
        {
          Serial.println("⚠ Agent not initialized");
        }
        return;
      }
    }

    // Fallback: if message contains ASSIGN as plain text
    if (trimmed.indexOf("ASSIGN") >= 0)
    {
      Serial.println("Assignment to the serial port");
    }
    break;
  }

  case WStype_BIN:
    Serial.println("Binary message received (ignored)");
    break;

  case WStype_ERROR:
    Serial.println("WebSocket error");
    break;

  case WStype_PING:
    Serial.println("WebSocket ping");
    break;

  case WStype_PONG:
    Serial.println("WebSocket pong");
    break;

  default:
    break;
  }
}
