#include <Arduino.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "manifest.h"
#include "fakts.h"
#include "arkitekt.h"
#include "app.h"
#include "agent.h"
#include "agent_example_updated.h"
#include "config_defaults.h"

// BLE UUIDs for our custom provisioning service
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define WIFI_SSID_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define WIFI_PASSWORD_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define BASE_URL_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define FAKTS_TOKEN_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define WIFI_IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ae"
#define WIFI_ANONYMOUS_IDENTITY_UUID "beb5483e-36e1-4688-b7f5-ea07361b26af"
#define WIFI_PEM_CERTIFICATE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b0"
#define MANIFEST_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"
#define STATUS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ac"

/*
 * Arkitekt Authentication Flow for ESP32
 *
 * This program demonstrates the complete authentication flow:
 * 1. Claim Fakts: Use the fakts token to get the configuration (fakts)
 * 2. OAuth2 Authentication: Use client credentials from fakts to get an access token
 *
 * The fakts configuration includes:
 * - self: deployment information
 * - auth: OAuth2 credentials (client_id, client_secret, token_url)
 * - instances: service endpoints with aliases
 */

// Configuration
char baseUrl[128] = DEFAULT_BASE_URL;
char faktsToken[128] = DEFAULT_REDEEM_TOKEN; // Will be updated via BLE
String claimUrl;

Preferences preferences;
bool isProvisioned = false;

// BLE variables
BLEServer *pServer = NULL;
BLECharacteristic *statusCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

String wifiSSID = "";
String wifiPassword = "";
String wifiIdentity = "";
String wifiAnonIdentity = "";
String wifiPemCertificate = "";
String configBaseUrl = "";
String configToken = "";
bool hasNewConfig = false;

// Factory reset button tracking
unsigned long bootButtonPressStart = 0;
bool bootButtonHeld = false;

// BLE Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("\n[BLE] ========== CLIENT CONNECTED ==========");
    Serial.println("[BLE] BLE client has connected to ARKITEKT_CONFIG");
    Serial.println("[BLE] =========================================\n");
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("\n[BLE] ========== CLIENT DISCONNECTED ==========");
    Serial.println("[BLE] BLE client has disconnected");
    Serial.println("[BLE] ===========================================\n");
  }
};

// Characteristic Callbacks for receiving data
class WiFiSSIDCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] WiFi SSID characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      wifiSSID = String(value.c_str());
      Serial.println("[BLE] Received WiFi SSID: " + wifiSSID);
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty WiFi SSID received");
    }
  }
};

class WiFiPasswordCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] WiFi Password characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      wifiPassword = String(value.c_str());
      Serial.println("[BLE] Received WiFi Password: " + String(value.length()) + " chars");
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty WiFi Password received");
    }
  }
};

class WiFiIdentityCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] WiFi Identity characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      wifiIdentity = String(value.c_str());
      Serial.println("[BLE] Received WiFi Identity: " + wifiIdentity);
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty WiFi Identity received");
    }
  }
};

class WiFiAnonIdentityCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] WiFi Anonymous Identity characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      wifiAnonIdentity = String(value.c_str());
      Serial.println("[BLE] Received WiFi Anonymous Identity: " + wifiAnonIdentity);
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty WiFi Anonymous Identity received");
    }
  }
};

class WiFiPemCertificateCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] WiFi PEM Certificate characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      // Accumulate chunks - client must send data in <=500 byte chunks
      // Send "CLEAR" as first write to reset the buffer before sending chunks
      if (value == "CLEAR")
      {
        wifiPemCertificate = "";
        Serial.println("[BLE] PEM Certificate buffer cleared");
      }
      else
      {
        wifiPemCertificate += String(value.c_str());
        Serial.println("[BLE] PEM Certificate chunk: " + String(value.length()) + " bytes, total: " + String(wifiPemCertificate.length()) + " bytes");
      }
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty WiFi PEM Certificate received");
    }
  }
};

class BaseURLCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] Base URL characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      configBaseUrl = String(value.c_str());
      Serial.println("[BLE] Received Base URL: " + configBaseUrl);
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty Base URL received");
    }
  }
};

class FaktsTokenCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    Serial.println("[BLE] Fakts Token characteristic WRITE received");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0)
    {
      configToken = String(value.c_str());
      Serial.println("[BLE] Received Fakts Token: " + String(value.length()) + " chars");
      Serial.println("[BLE] Token preview: " + String(value.c_str()).substring(0, min(20, (int)value.length())) + "...");

      // When token is received, trigger configuration save
      hasNewConfig = true;
      Serial.println("[BLE] Configuration complete flag set to TRUE");
    }
    else
    {
      Serial.println("[BLE] WARNING: Empty Fakts Token received");
    }
  }
};

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

void startBLEProvisioning()
{
  Serial.println("\n=== Starting Custom BLE Provisioning ===");

  // Initialize BLE
  BLEDevice::init("ARKITEKT_CONFIG");
  BLEDevice::setMTU(517); // Request max BLE MTU for larger writes

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service with enough handles for all characteristics
  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID), 30);

  // Create Characteristics
  BLECharacteristic *wifiSSIDChar = pService->createCharacteristic(
      WIFI_SSID_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  wifiSSIDChar->setCallbacks(new WiFiSSIDCallbacks());

  BLECharacteristic *wifiPasswordChar = pService->createCharacteristic(
      WIFI_PASSWORD_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  wifiPasswordChar->setCallbacks(new WiFiPasswordCallbacks());

  BLECharacteristic *wifiIdentityChar = pService->createCharacteristic(
      WIFI_IDENTITY_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  wifiIdentityChar->setCallbacks(new WiFiIdentityCallbacks());

  BLECharacteristic *wifiAnonIdentityChar = pService->createCharacteristic(
      WIFI_ANONYMOUS_IDENTITY_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  wifiAnonIdentityChar->setCallbacks(new WiFiAnonIdentityCallbacks());

  BLECharacteristic *wifiPemCertChar = pService->createCharacteristic(
      WIFI_PEM_CERTIFICATE_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  wifiPemCertChar->setCallbacks(new WiFiPemCertificateCallbacks());

  BLECharacteristic *baseUrlChar = pService->createCharacteristic(
      BASE_URL_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  baseUrlChar->setCallbacks(new BaseURLCallbacks());

  BLECharacteristic *faktsTokenChar = pService->createCharacteristic(
      FAKTS_TOKEN_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  faktsTokenChar->setCallbacks(new FaktsTokenCallbacks());

  // Manifest characteristic (read-only) - returns JSON manifest
  BLECharacteristic *manifestChar = pService->createCharacteristic(
      MANIFEST_UUID,
      BLECharacteristic::PROPERTY_READ);

  // Serialize manifest to JSON string
  JsonDocument manifestDoc;
  JsonObject manifestObj = manifestDoc.to<JsonObject>();
  appManifest.toJson(manifestObj);
  String manifestJson;
  serializeJson(manifestDoc, manifestJson);
  manifestChar->setValue(manifestJson.c_str());

  // Add read callback for logging
  class ManifestCallbacks : public BLECharacteristicCallbacks
  {
    void onRead(BLECharacteristic *pCharacteristic)
    {
      Serial.println("[BLE] Manifest characteristic READ by client");
      Serial.println("[BLE] Manifest content: " + String(pCharacteristic->getValue().c_str()));
    }
  };
  manifestChar->setCallbacks(new ManifestCallbacks());

  statusCharacteristic = pService->createCharacteristic(
      STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusCharacteristic->addDescriptor(new BLE2902());
  statusCharacteristic->setValue("Ready");

  // Add read callback for logging
  class StatusCallbacks : public BLECharacteristicCallbacks
  {
    void onRead(BLECharacteristic *pCharacteristic)
    {
      Serial.println("[BLE] Status characteristic READ by client");
      Serial.println("[BLE] Status: " + String(pCharacteristic->getValue().c_str()));
    }
  };
  statusCharacteristic->setCallbacks(new StatusCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE Service Started!");
  Serial.println("Service UUID: " + String(SERVICE_UUID));
  Serial.println("Device Name: ARKITEKT_CONFIG");
  Serial.println("\nCharacteristics:");
  Serial.println("  WiFi SSID:      " + String(WIFI_SSID_UUID));
  Serial.println("  WiFi Password:  " + String(WIFI_PASSWORD_UUID));
  Serial.println("  WiFi Identity:  " + String(WIFI_IDENTITY_UUID));
  Serial.println("  WiFi AnonId:    " + String(WIFI_ANONYMOUS_IDENTITY_UUID));
  Serial.println("  WiFi PEM Cert:  " + String(WIFI_PEM_CERTIFICATE_UUID));
  Serial.println("  Base URL:       " + String(BASE_URL_UUID));
  Serial.println("  Fakts Token:    " + String(FAKTS_TOKEN_UUID));
  Serial.println("  Manifest (R):   " + String(MANIFEST_UUID));
  Serial.println("  Status:         " + String(STATUS_UUID));
  Serial.println("\nManifest JSON: " + manifestJson);
  Serial.println("\nWaiting for BLE connection...");
}

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  } // (optional, native USB boards)

  Serial.println("Hello");

  // Setup BOOT button (GPIO0) for factory reset - hold 5 seconds anytime
  pinMode(0, INPUT_PULLUP);
  Serial.println("[RESET] Hold BOOT button for 5 seconds at any time to factory reset.");

#ifdef DEBUG
  Serial.println("\n=== DEBUG MODE ===");
  Serial.println("Send any character via Serial to start...");

  // Wait for serial input
  while (!Serial.available())
  {
    delay(100);
  }

  // Clear the serial buffer
  while (Serial.available())
  {
    Serial.read();
  }

  Serial.println("Starting...\n");
#endif

  // Turn on LED on GPIO2 to indicate startup
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  Serial.println("LED on GPIO2 turned ON");

  // Initialize the manifest with requirements
  appManifest.addRequirement("rekuest", "live.arkitekt.rekuest", false);
  appManifest.addDeviceId("esp32-001");
  // You can add more requirements or scopes here
  // appManifest.addScope("read:nodes");
  // appManifest.addRequirement("fluss", "live.arkitekt.fluss", true);

  Serial.println("\n=== Application Manifest ===");
  appManifest.print();
  Serial.println("============================\n");

  // Load configuration from Preferences
  Serial.println("\n=== Loading Saved Configuration ===");
  preferences.begin("arkitekt", false);
  String savedBaseUrl = preferences.getString("baseUrl", DEFAULT_BASE_URL);
  savedBaseUrl.toCharArray(baseUrl, 128);
  Serial.println("Base URL: " + savedBaseUrl);

  String savedToken = preferences.getString("faktsToken", DEFAULT_REDEEM_TOKEN);
  savedToken.toCharArray(faktsToken, 128);
  Serial.println("Fakts Token: " + savedToken.substring(0, 10) + "...");

  bool savedEnterprise = preferences.getBool("wifiEnterprise", false);
  String savedWifiSSID = preferences.getString("wifiSSID", "");
  String savedWifiIdentity = preferences.getString("wifiIdentity", "");
  String savedWifiPassword = preferences.getString("wifiPassword", "");
  String savedWifiAnonId = preferences.getString("wifiAnonId", "");
  String savedWifiPemCert = preferences.getString("wifiPemCert", "");
  preferences.end();
  Serial.println("Enterprise WiFi: " + String(savedEnterprise ? "yes" : "no"));
  Serial.println("Saved WiFi SSID: '" + savedWifiSSID + "' (" + String(savedWifiSSID.length()) + " chars)");
  Serial.println("Saved WiFi Identity: '" + savedWifiIdentity + "' (" + String(savedWifiIdentity.length()) + " chars)");
  Serial.println("Saved WiFi Password: " + String(savedWifiPassword.length()) + " chars");
  Serial.println("Saved WiFi AnonId: '" + savedWifiAnonId + "' (" + String(savedWifiAnonId.length()) + " chars)");
  Serial.println("Saved WiFi PEM Cert: " + String(savedWifiPemCert.length()) + " chars");
  if (savedWifiPemCert.length() > 0)
  {
    Serial.println("PEM Cert preview: " + savedWifiPemCert.substring(0, min(80, (int)savedWifiPemCert.length())) + "...");
  }
  Serial.println("===================================\n");

  // Check if we are already provisioned
  Serial.println("\n=== Checking WiFi Status ===");
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status());
  Serial.print("Saved SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("SSID Length: ");
  Serial.println(WiFi.SSID().length());

  bool hasStoredWifi = (WiFi.status() == WL_CONNECTED || WiFi.SSID().length() > 0 || (savedEnterprise && savedWifiSSID.length() > 0));

  if (hasStoredWifi)
  {
    Serial.println("Already Provisioned. Starting Normal Mode...");

    if (savedEnterprise && savedWifiSSID.length() > 0)
    {
      // Reconnect using WPA2-Enterprise credentials from preferences
      Serial.println("Reconnecting to WPA2-Enterprise WiFi: " + savedWifiSSID);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_STA);

      esp_err_t err;

      if (savedWifiAnonId.length() > 0)
      {
        err = esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)savedWifiAnonId.c_str(), savedWifiAnonId.length());
        Serial.println("[WPA2] Set anonymous identity: '" + savedWifiAnonId + "' (err=" + String(err) + ")");
      }
      else
      {
        err = esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)savedWifiIdentity.c_str(), savedWifiIdentity.length());
        Serial.println("[WPA2] Set identity as anonymous: '" + savedWifiIdentity + "' (err=" + String(err) + ")");
      }

      err = esp_wifi_sta_wpa2_ent_set_username((uint8_t *)savedWifiIdentity.c_str(), savedWifiIdentity.length());
      Serial.println("[WPA2] Set username: '" + savedWifiIdentity + "' (err=" + String(err) + ")");

      if (savedWifiPassword.length() > 0)
      {
        err = esp_wifi_sta_wpa2_ent_set_password((uint8_t *)savedWifiPassword.c_str(), savedWifiPassword.length());
        Serial.println("[WPA2] Set password: " + String(savedWifiPassword.length()) + " chars (err=" + String(err) + ")");
      }
      else
      {
        Serial.println("[WPA2] WARNING: No password saved!");
      }

      if (savedWifiPemCert.length() > 0)
      {
        err = esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)savedWifiPemCert.c_str(), savedWifiPemCert.length() + 1);
        Serial.println("[WPA2] Set CA cert: " + String(savedWifiPemCert.length()) + " bytes (err=" + String(err) + ")");
      }
      else
      {
        Serial.println("[WPA2] WARNING: No PEM certificate saved! Connection may fail.");
      }

      err = esp_wifi_sta_wpa2_ent_enable();
      Serial.println("[WPA2] WPA2-Enterprise enabled (err=" + String(err) + ")");
      WiFi.begin(savedWifiSSID.c_str());
      Serial.println("[WPA2] WiFi.begin() called with SSID: " + savedWifiSSID);
    }
    else
    {
      Serial.print("Attempting to connect to: ");
      Serial.println(WiFi.SSID());
      WiFi.begin(); // Connect to saved WiFi (WPA-Personal)
    }

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000)
    {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nWiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    else
    {
      int status = WiFi.status();
      Serial.println("\nFailed to connect. WiFi status: " + String(status));
      switch (status)
      {
      case WL_NO_SSID_AVAIL:
        Serial.println("[WiFi] Reason: SSID not found/available");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("[WiFi] Reason: Connection failed (wrong password or auth rejected)");
        break;
      case WL_DISCONNECTED:
        Serial.println("[WiFi] Reason: Disconnected (auth may have failed)");
        break;
      case 255:
        Serial.println("[WiFi] Reason: WiFi not initialized properly");
        break;
      default:
        Serial.println("[WiFi] Reason: Unknown status code " + String(status));
        break;
      }
      Serial.println("Erasing WiFi config...");
      WiFi.disconnect(true, true);
      ESP.restart();
    }
  }
  else
  {
    // Start custom BLE provisioning
    startBLEProvisioning();

    // Wait for configuration via BLE
    Serial.println("Waiting for configuration via BLE...");
    while (!hasNewConfig)
    {
      delay(100);

      // Handle BLE disconnection for restart advertising
      if (!deviceConnected && oldDeviceConnected)
      {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Restarting advertising...");
        oldDeviceConnected = deviceConnected;
      }

      // Handle BLE connection
      if (deviceConnected && !oldDeviceConnected)
      {
        oldDeviceConnected = deviceConnected;
      }
    }

    Serial.println("\n=== Configuration Received ===");

    // Update status
    if (statusCharacteristic)
    {
      statusCharacteristic->setValue("Connecting...");
      statusCharacteristic->notify();
    }

    // Save configuration to preferences
    preferences.begin("arkitekt", false);
    if (configBaseUrl.length() > 0)
    {
      preferences.putString("baseUrl", configBaseUrl);
      configBaseUrl.toCharArray(baseUrl, 128);
      Serial.println("Base URL saved: " + configBaseUrl);
    }

    if (configToken.length() > 0)
    {
      preferences.putString("faktsToken", configToken);
      configToken.toCharArray(faktsToken, 128);
      Serial.println("Fakts Token saved: " + String(configToken.length()) + " chars");
    }

    // Save enterprise WiFi credentials
    bool isEnterprise = wifiIdentity.length() > 0;
    preferences.putBool("wifiEnterprise", isEnterprise);
    if (isEnterprise)
    {
      preferences.putString("wifiSSID", wifiSSID);
      preferences.putString("wifiIdentity", wifiIdentity);
      preferences.putString("wifiPassword", wifiPassword);
      preferences.putString("wifiAnonId", wifiAnonIdentity);
      preferences.putString("wifiPemCert", wifiPemCertificate);
      Serial.println("Enterprise WiFi credentials saved:");
      Serial.println("  SSID: " + wifiSSID);
      Serial.println("  Identity: " + wifiIdentity);
      Serial.println("  Password: " + String(wifiPassword.length()) + " chars");
      Serial.println("  AnonId: " + wifiAnonIdentity);
      Serial.println("  PEM Cert: " + String(wifiPemCertificate.length()) + " chars");
    }
    preferences.end();

    // Connect to WiFi
    if (wifiSSID.length() > 0)
    {
      if (isEnterprise)
      {
        // WPA2-Enterprise (eduroam) connection
        Serial.println("Connecting to WPA2-Enterprise WiFi: " + wifiSSID);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);

        // Set outer/anonymous identity
        if (wifiAnonIdentity.length() > 0)
        {
          esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifiAnonIdentity.c_str(), wifiAnonIdentity.length());
          Serial.println("Set anonymous identity: " + wifiAnonIdentity);
        }
        else
        {
          esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifiIdentity.c_str(), wifiIdentity.length());
        }

        // Set inner identity/username
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wifiIdentity.c_str(), wifiIdentity.length());
        Serial.println("Set identity/username: " + wifiIdentity);

        // Set password
        if (wifiPassword.length() > 0)
        {
          esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wifiPassword.c_str(), wifiPassword.length());
          Serial.println("Set password: " + String(wifiPassword.length()) + " chars");
        }

        // Set CA certificate if provided
        if (wifiPemCertificate.length() > 0)
        {
          esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)wifiPemCertificate.c_str(), wifiPemCertificate.length() + 1);
          Serial.println("Set CA certificate: " + String(wifiPemCertificate.length()) + " bytes");
        }

        // Enable WPA2-Enterprise
        esp_wifi_sta_wpa2_ent_enable();

        // Connect without password (enterprise handles auth)
        WiFi.begin(wifiSSID.c_str());
      }
      else if (wifiPassword.length() > 0)
      {
        // Standard WPA/WPA2-Personal connection
        Serial.println("Connecting to WiFi: " + wifiSSID);
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
      }
      else
      {
        Serial.println("No WiFi credentials provided!");
        if (statusCharacteristic)
        {
          statusCharacteristic->setValue("No Credentials");
          statusCharacteristic->notify();
        }
        delay(3000);
        ESP.restart();
      }

      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000)
      {
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        if (statusCharacteristic)
        {
          String status = "Connected: " + WiFi.localIP().toString();
          statusCharacteristic->setValue(status.c_str());
          statusCharacteristic->notify();
        }

        // Stop BLE to free memory
        delay(2000); // Give time for status to be sent
        BLEDevice::deinit();
        Serial.println("BLE stopped");
      }
      else
      {
        Serial.println("\nFailed to connect to WiFi!");

        if (statusCharacteristic)
        {
          statusCharacteristic->setValue("WiFi Failed");
          statusCharacteristic->notify();
        }

        delay(3000);
        ESP.restart();
      }
    }
  }

  // Construct URLs
  String base = String(baseUrl);
  if (!base.endsWith("/"))
    base += "/";
  claimUrl = base + "lok/f/claim/";

  Serial.println("Base URL: " + base);
  Serial.println("Claim URL: " + claimUrl);

  appReady = initializeAppFlow();

  if (!appReady)
  {
    Serial.println("⚠ App initialization failed. Check logs above.");
  }
}

// Check BOOT button for factory reset (call from loop)
void checkFactoryReset()
{
  if (digitalRead(0) == LOW)
  {
    if (!bootButtonHeld)
    {
      bootButtonHeld = true;
      bootButtonPressStart = millis();
      Serial.println("[RESET] BOOT button pressed...");
    }
    else if (millis() - bootButtonPressStart >= 5000)
    {
      Serial.println("\n[RESET] BOOT button held for 5 seconds! Factory reset triggered!");
      Serial.println("[RESET] Erasing all saved config...");
      Preferences resetPrefs;
      resetPrefs.begin("arkitekt", false);
      resetPrefs.clear();
      resetPrefs.end();
      WiFi.disconnect(true, true);
      Serial.println("[RESET] Done. Restarting...");
      delay(1000);
      ESP.restart();
    }
  }
  else
  {
    if (bootButtonHeld)
    {
      Serial.println("[RESET] BOOT button released before 5 seconds.");
    }
    bootButtonHeld = false;
  }
}

// Background temperature update
unsigned long lastTempUpdate = 0;

void updateTemperatureState()
{
  if (globalAgent == nullptr)
    return;
  if (millis() - lastTempUpdate < 5000)
    return;
  lastTempUpdate = millis();

  AgentState *state = globalAgent->getState("sensor_status");
  if (state)
  {
    float temp = 4.0f + (random(0, 3000) / 100.0f); // 4.0 - 34.0
    state->setPort("temperature", temp);
    Serial.println("[BG] Temperature updated: " + String(temp) + "°C");
  }
}

void loop()
{
  checkFactoryReset();

  if (appReady)
  {
    webSocket.loop();
    updateTemperatureState();
    return;
  }

  delay(1000);
}
bool initializeAppFlow()
{
  Serial.println("\n=== Claiming Fakts Configuration ===");
  Serial.println("Using faktsToken: " + String(faktsToken).substring(0, min(20, (int)strlen(faktsToken))) + "...");
  globalFaktsConfig.reset();
  String claimError;

  // Use faktsToken directly to claim configuration
  if (!claimFakts(faktsToken, claimUrl, globalFaktsConfig, claimError))
  {
    Serial.println("✗ Claim failed!");
    Serial.println("Error: " + claimError);

    // Clear all saved configuration
    Serial.println("\n=== Clearing Saved Configuration ===");
    preferences.begin("arkitekt", false);
    preferences.remove("baseUrl");
    preferences.remove("faktsToken");
    preferences.remove("wifiEnterprise");
    preferences.remove("wifiSSID");
    preferences.remove("wifiIdentity");
    preferences.remove("wifiPassword");
    preferences.remove("wifiAnonId");
    preferences.remove("wifiPemCert");
    preferences.end();
    Serial.println("All configuration cleared");

    // Clear WiFi credentials to force BLE provisioning on restart
    Serial.println("Erasing WiFi credentials...");
    WiFi.disconnect(true, true);

    Serial.println("Restarting in 3 seconds to enter BLE provisioning mode...");
    delay(3000);
    ESP.restart();

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

  // Set WebSocket reference on agent before connecting
  globalAgent->setWebSocket(&webSocket);

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
      if (typeStr == "INIT")
      {
        Serial.println("✓ Server acknowledged REGISTER with INIT");
        if (globalAgent != nullptr)
        {
          globalAgent->sendSessionInit();
          globalAgent->sendStateSnapshot();
        }
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

            // Send DONE event
            StaticJsonDocument<256> doneDoc;
            doneDoc["type"] = "DONE";
            doneDoc["assignation"] = assignation;
            doneDoc["id"] = generateUUID4();

            String doneMsg;
            serializeJson(doneDoc, doneMsg);
            Serial.println("WS >> " + doneMsg);
            webSocket.sendTXT(doneMsg);
          }
          else
          {
            Serial.println("✗ Assignment failed");

            // Send CRITICAL error event
            StaticJsonDocument<512> criticalDoc;
            criticalDoc["type"] = "CRITICAL";
            criticalDoc["assignation"] = assignation;
            criticalDoc["id"] = generateUUID4();
            criticalDoc["error"] = "Function execution failed";

            String criticalMsg;
            serializeJson(criticalDoc, criticalMsg);
            Serial.println("WS >> " + criticalMsg);
            webSocket.sendTXT(criticalMsg);
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
