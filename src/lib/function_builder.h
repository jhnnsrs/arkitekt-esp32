#ifndef FUNCTION_BUILDER_H
#define FUNCTION_BUILDER_H

#include <ArduinoJson.h>
#include "port_builder.h"

/*
 * Fluent API for building function definitions.
 * Documents are heap-allocated during building and ownership
 * transfers to FunctionDefinition on build(). No global
 * persistent documents needed.
 *
 * Example:
 *   auto def = FunctionBuilder("toggle_led", "Toggles an LED")
 *       .argBool("state", "LED state")
 *       .returnInt("pin", "Pin number")
 *       .returnBool("state", "New state")
 *       .build();
 */

class FunctionBuilder
{
private:
    String _name;
    String _description;
    DynamicJsonDocument *_argsDoc;
    DynamicJsonDocument *_returnsDoc;
    JsonArray _args;
    JsonArray _returns;

public:
    FunctionBuilder(const String &name, const String &desc, size_t argsSize = 1024, size_t returnsSize = 512)
        : _name(name), _description(desc)
    {
        _argsDoc = new DynamicJsonDocument(argsSize);
        _returnsDoc = new DynamicJsonDocument(returnsSize);
        _args = _argsDoc->to<JsonArray>();
        _returns = _returnsDoc->to<JsonArray>();
    }

    // --- Argument ports ---

    FunctionBuilder &argInt(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createIntPort(_args, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &argFloat(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createFloatPort(_args, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &argString(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createStringPort(_args, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &argBool(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createBoolPort(_args, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &argStringChoice(const String &key, const String &label, const String &desc,
                                     std::initializer_list<std::pair<String, String>> choices)
    {
        JsonObject arg = PortBuilder::createStringPort(_args, key, label, desc);
        for (const auto &c : choices)
        {
            PortBuilder::addChoice(arg, c.first, c.second);
        }
        PortBuilder::addChoiceWidget(arg);
        return *this;
    }

    FunctionBuilder &argStructure(const String &key, const String &identifier, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createStructurePort(_args, key, identifier, label, desc, nullable);
        return *this;
    }

    // --- Return ports ---

    FunctionBuilder &returnInt(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createIntPort(_returns, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &returnFloat(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createFloatPort(_returns, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &returnString(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createStringPort(_returns, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &returnBool(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createBoolPort(_returns, key, label, desc, nullable);
        return *this;
    }

    FunctionBuilder &returnStructure(const String &key, const String &identifier, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createStructurePort(_returns, key, identifier, label, desc, nullable);
        return *this;
    }

    // --- Modifiers (apply to last-added or named arg) ---

    FunctionBuilder &withDefault(const String &key, int value)
    {
        for (JsonObject arg : _args) { if (arg["key"] == key) { arg["default"] = value; break; } }
        return *this;
    }

    FunctionBuilder &withDefault(const String &key, float value)
    {
        for (JsonObject arg : _args) { if (arg["key"] == key) { arg["default"] = value; break; } }
        return *this;
    }

    FunctionBuilder &withDefault(const String &key, const String &value)
    {
        for (JsonObject arg : _args) { if (arg["key"] == key) { arg["default"] = value; break; } }
        return *this;
    }

    FunctionBuilder &withDefault(const String &key, bool value)
    {
        for (JsonObject arg : _args) { if (arg["key"] == key) { arg["default"] = value; break; } }
        return *this;
    }

    FunctionBuilder &withSlider(const String &key, float min, float max, float step = 1.0)
    {
        for (JsonObject arg : _args) { if (arg["key"] == key) { PortBuilder::addSliderWidget(arg, min, max, step); break; } }
        return *this;
    }

    // --- Build: transfers document ownership to the definition ---

    FunctionDefinition build()
    {
        FunctionDefinition def(_name, _description);
        def._argsDoc = _argsDoc;
        def._returnsDoc = _returnsDoc;
        def.args = _args;
        def.returns = _returns;
        // Builder relinquishes ownership
        _argsDoc = nullptr;
        _returnsDoc = nullptr;
        return def;
    }
};

#endif // FUNCTION_BUILDER_H
