#ifndef REPLY_CHANNEL_H
#define REPLY_CHANNEL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

// Forward declaration
inline String replyGenerateUUID4()
{
    String uuid = "";
    const char *hexChars = "0123456789abcdef";
    for (int i = 0; i < 36; i++)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            uuid += "-";
        else if (i == 14)
            uuid += "4";
        else if (i == 19)
            uuid += hexChars[(random(0, 4) + 8)];
        else
            uuid += hexChars[random(0, 16)];
    }
    return uuid;
}

// ReplyChannel - provides yield, done, log, progress for function callbacks
class ReplyChannel
{
private:
    WebSocketsClient *ws;
    String assignation;

public:
    ReplyChannel(WebSocketsClient *websocket, const String &assignationId)
        : ws(websocket), assignation(assignationId) {}

    // Yield intermediate results (for generators)
    void yield(JsonObject returns)
    {
        if (!ws)
            return;

        DynamicJsonDocument doc(1024);
        doc["type"] = "YIELD";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();
        doc["returns"] = returns;

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] YIELD >> " + msg);
        ws->sendTXT(msg);
    }

    // Send done with final returns
    void done(JsonObject returns)
    {
        if (!ws)
            return;

        DynamicJsonDocument doc(1024);
        doc["type"] = "DONE";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();
        doc["returns"] = returns;

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] DONE >> " + msg);
        ws->sendTXT(msg);
    }

    // Send done without returns
    void done()
    {
        if (!ws)
            return;

        StaticJsonDocument<256> doc;
        doc["type"] = "DONE";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] DONE >> " + msg);
        ws->sendTXT(msg);
    }

    // Log a message
    void log(const String &message, const String &level = "INFO")
    {
        if (!ws)
            return;

        StaticJsonDocument<512> doc;
        doc["type"] = "LOG";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();
        doc["message"] = message;
        doc["level"] = level;

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] LOG >> " + msg);
        ws->sendTXT(msg);
    }

    // Report progress (0.0 - 1.0)
    void progress(float value)
    {
        if (!ws)
            return;

        StaticJsonDocument<256> doc;
        doc["type"] = "PROGRESS";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();
        doc["progress"] = value;

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] PROGRESS >> " + msg);
        ws->sendTXT(msg);
    }

    // Send critical error
    void critical(const String &error)
    {
        if (!ws)
            return;

        StaticJsonDocument<512> doc;
        doc["type"] = "CRITICAL";
        doc["assignation"] = assignation;
        doc["id"] = replyGenerateUUID4();
        doc["error"] = error;

        String msg;
        serializeJson(doc, msg);
        Serial.println("[REPLY] CRITICAL >> " + msg);
        ws->sendTXT(msg);
    }

    const String &getAssignation() const { return assignation; }
};

#endif // REPLY_CHANNEL_H
