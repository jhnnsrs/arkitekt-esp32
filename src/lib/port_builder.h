#ifndef PORT_BUILDER_H
#define PORT_BUILDER_H

#include <ArduinoJson.h>

/*
 * Helper functions to build Port definitions according to Rekuest GraphQL schema
 *
 * These match the PortInput type from the schema:
 * - key: String!
 * - kind: PortKind! (INT, STRING, FLOAT, BOOL, STRUCTURE, LIST, DICT, etc.)
 * - label: String (optional)
 * - description: String (optional)
 * - nullable: Boolean! (default false)
 * - identifier: String (optional, for STRUCTURE kind)
 * - default: AnyDefault (optional)
 * - children: [PortInput!] (optional, for nested structures)
 * - choices: [ChoiceInput!] (optional)
 * - assignWidget: AssignWidgetInput (optional)
 * - returnWidget: ReturnWidgetInput (optional)
 * - validators: [ValidatorInput!] (optional)
 * - effects: [EffectInput!] (optional)
 * - descriptors: [DescriptorInput!] (optional)
 */

class PortBuilder
{
public:
    // Create a basic port
    static JsonObject createPort(JsonArray &ports, const String &key, const String &kind, bool nullable = false)
    {
        JsonObject port = ports.add<JsonObject>();
        port["key"] = key;
        port["kind"] = kind;
        port["nullable"] = nullable;
        return port;
    }

    // Create an INT port
    static JsonObject createIntPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "INT", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a FLOAT port
    static JsonObject createFloatPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "FLOAT", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a STRING port
    static JsonObject createStringPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "STRING", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a BOOL port
    static JsonObject createBoolPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "BOOL", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a DICT port
    static JsonObject createDictPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "DICT", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a LIST port
    static JsonObject createListPort(JsonArray &ports, const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "LIST", nullable);
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Create a STRUCTURE port
    static JsonObject createStructurePort(JsonArray &ports, const String &key, const String &identifier, const String &label = "", const String &description = "", bool nullable = false)
    {
        JsonObject port = createPort(ports, key, "STRUCTURE", nullable);
        port["identifier"] = identifier;
        if (label.length() > 0)
            port["label"] = label;
        if (description.length() > 0)
            port["description"] = description;
        return port;
    }

    // Add a slider widget to a port
    static void addSliderWidget(JsonObject &port, float min, float max, float step = 1.0)
    {
        JsonObject widget = port["widget"].to<JsonObject>();
        widget["kind"] = "SLIDER";
        widget["min"] = min;
        widget["max"] = max;
        widget["step"] = step;
    }

    // Add a string widget to a port
    static void addStringWidget(JsonObject &port, const String &placeholder = "", bool asParagraph = false)
    {
        JsonObject widget = port["widget"].to<JsonObject>();
        widget["kind"] = "STRING";
        widget["placeholder"] = placeholder;
        widget["asParagraph"] = asParagraph;
    }

    // Add choices to a port
    static void addChoice(JsonObject &port, const String &label, const String &value, const String &description = "")
    {
        JsonArray choices;
        if (!port.containsKey("choices"))
        {
            choices = port["choices"].to<JsonArray>();
        }
        else
        {
            choices = port["choices"].as<JsonArray>();
        }

        JsonObject choice = choices.add<JsonObject>();
        choice["label"] = label;
        choice["value"] = value;
        if (description.length() > 0)
        {
            choice["description"] = description;
        }
    }

    // Add a choice widget to a port
    static void addChoiceWidget(JsonObject &port)
    {
        JsonObject widget = port["widget"].to<JsonObject>();
        widget["kind"] = "CHOICE";
        // Choices should already be set on the port itself
    }

    // Set a default value on a port
    static void setDefault(JsonObject &port, int value)
    {
        port["default"] = value;
    }

    static void setDefault(JsonObject &port, float value)
    {
        port["default"] = value;
    }

    static void setDefault(JsonObject &port, const String &value)
    {
        port["default"] = value;
    }

    static void setDefault(JsonObject &port, bool value)
    {
        port["default"] = value;
    }

    // Add a descriptor to a port
    static void addDescriptor(JsonObject &port, const String &key, const String &value)
    {
        JsonArray descriptors;
        if (!port.containsKey("descriptors"))
        {
            descriptors = port["descriptors"].to<JsonArray>();
        }
        else
        {
            descriptors = port["descriptors"].as<JsonArray>();
        }

        JsonObject descriptor = descriptors.add<JsonObject>();
        descriptor["key"] = key;
        descriptor["value"] = value;
    }
};

#endif // PORT_BUILDER_H
