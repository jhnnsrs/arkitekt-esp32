#ifndef FUNCTION_BUILDER_H
#define FUNCTION_BUILDER_H

#include <ArduinoJson.h>
#include "port_builder.h"

/*
 * Fluent API for building function definitions
 *
 * Example:
 * auto def = FunctionBuilder("toggle_led", "Toggles an LED")
 *     .addBoolArg("state", "LED state")
 *     .addIntReturn("pin", "Pin number")
 *     .addBoolReturn("state", "New state")
 *     .build();
 */

class FunctionBuilder
{
private:
    String name;
    String description;
    DynamicJsonDocument *argsDoc;
    DynamicJsonDocument *returnsDoc;
    JsonArray args;
    JsonArray returns;

public:
    FunctionBuilder(const String &funcName, const String &desc, size_t argsSize = 1024, size_t returnsSize = 512)
        : name(funcName), description(desc)
    {
        argsDoc = new DynamicJsonDocument(argsSize);
        returnsDoc = new DynamicJsonDocument(returnsSize);
        args = argsDoc->to<JsonArray>();
        returns = returnsDoc->to<JsonArray>();
    }

    ~FunctionBuilder()
    {
        // Note: Don't delete docs here as they need to persist
    }

    // Add argument methods
    FunctionBuilder &addIntArg(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createIntPort(args, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addFloatArg(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createFloatPort(args, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addStringArg(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createStringPort(args, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addBoolArg(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createBoolPort(args, key, label, description, nullable);
        return *this;
    }

    // Add return methods
    FunctionBuilder &addIntReturn(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createIntPort(returns, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addFloatReturn(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createFloatPort(returns, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addStringReturn(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createStringPort(returns, key, label, description, nullable);
        return *this;
    }

    FunctionBuilder &addBoolReturn(const String &key, const String &label = "", const String &description = "", bool nullable = false)
    {
        PortBuilder::createBoolPort(returns, key, label, description, nullable);
        return *this;
    }

    // Set defaults
    FunctionBuilder &withDefaultInt(const String &key, int value)
    {
        // Find the arg and set default
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                arg["default"] = value;
                break;
            }
        }
        return *this;
    }

    FunctionBuilder &withDefaultFloat(const String &key, float value)
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                arg["default"] = value;
                break;
            }
        }
        return *this;
    }

    FunctionBuilder &withDefaultString(const String &key, const String &value)
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                arg["default"] = value;
                break;
            }
        }
        return *this;
    }

    FunctionBuilder &withDefaultBool(const String &key, bool value)
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                arg["default"] = value;
                break;
            }
        }
        return *this;
    }

    // Add slider widget
    FunctionBuilder &withSlider(const String &key, float min, float max, float step = 1.0)
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                PortBuilder::addSliderWidget(arg, min, max, step);
                break;
            }
        }
        return *this;
    }

    // Add choice widget
    FunctionBuilder &withChoice(const String &key, const String &label, const String &value, const String &description = "")
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                PortBuilder::addChoice(arg, label, value, description);
                break;
            }
        }
        return *this;
    }

    FunctionBuilder &withChoiceWidget(const String &key)
    {
        for (JsonObject arg : args)
        {
            if (arg["key"] == key)
            {
                PortBuilder::addChoiceWidget(arg);
                break;
            }
        }
        return *this;
    }

    // Get the arrays for registration
    JsonArray &getArgs() { return args; }
    JsonArray &getReturns() { return returns; }

    // Get the documents (need to keep them alive!)
    DynamicJsonDocument *getArgsDoc() { return argsDoc; }
    DynamicJsonDocument *getReturnsDoc() { return returnsDoc; }

    String getName() { return name; }
    String getDescription() { return description; }
};

#endif // FUNCTION_BUILDER_H
