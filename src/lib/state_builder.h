#ifndef STATE_BUILDER_H
#define STATE_BUILDER_H

#include <ArduinoJson.h>
#include "port_builder.h"

/*
 * Fluent API for building state definitions.
 * Documents are heap-allocated during building and ownership
 * transfers to StateDefinition on build().
 *
 * Example:
 *   auto def = StateBuilder("led_status", "LED Status")
 *       .portBool("on", "On", "LED on/off")
 *       .portInt("pin", "Pin", "GPIO pin")
 *       .build();
 */

class StateBuilder
{
private:
    String _name;
    String _description;
    DynamicJsonDocument *_portsDoc;
    JsonArray _ports;

public:
    StateBuilder(const String &name, const String &desc, size_t portsSize = 256)
        : _name(name), _description(desc)
    {
        _portsDoc = new DynamicJsonDocument(portsSize);
        _ports = _portsDoc->to<JsonArray>();
    }

    StateBuilder &portInt(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createIntPort(_ports, key, label, desc, nullable);
        return *this;
    }

    StateBuilder &portFloat(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createFloatPort(_ports, key, label, desc, nullable);
        return *this;
    }

    StateBuilder &portString(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createStringPort(_ports, key, label, desc, nullable);
        return *this;
    }

    StateBuilder &portBool(const String &key, const String &label = "", const String &desc = "", bool nullable = false)
    {
        PortBuilder::createBoolPort(_ports, key, label, desc, nullable);
        return *this;
    }

    // --- Build: transfers document ownership to the definition ---

    StateDefinition build()
    {
        StateDefinition def(_name, _name);
        def._portsDoc = _portsDoc;
        def.ports = _ports;
        // Builder relinquishes ownership
        _portsDoc = nullptr;
        return def;
    }
};

#endif // STATE_BUILDER_H
