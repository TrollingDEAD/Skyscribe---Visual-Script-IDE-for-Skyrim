#pragma once

#include "graph/PinType.h"
#include <imgui.h>

namespace graph {

// Returns the display colour for a given pin type.
inline ImVec4 PinColor(PinType t) {
    switch (t) {
    case PinType::Exec:           return ImVec4(1.00f, 1.00f, 1.00f, 1.0f); // white
    case PinType::Bool:           return ImVec4(0.85f, 0.15f, 0.15f, 1.0f); // red
    case PinType::Int:            return ImVec4(0.25f, 0.65f, 1.00f, 1.0f); // blue
    case PinType::Float:          return ImVec4(0.40f, 0.90f, 0.40f, 1.0f); // green
    case PinType::String:         return ImVec4(1.00f, 0.70f, 0.20f, 1.0f); // orange
    case PinType::ObjectRef:      return ImVec4(0.65f, 0.40f, 1.00f, 1.0f); // purple
    case PinType::Actor:          return ImVec4(0.80f, 0.55f, 1.00f, 1.0f); // light purple
    case PinType::Quest:          return ImVec4(1.00f, 0.85f, 0.30f, 1.0f); // gold
    case PinType::Form:           return ImVec4(0.55f, 0.85f, 0.85f, 1.0f); // cyan
    case PinType::Array_Bool:     return ImVec4(0.85f, 0.30f, 0.30f, 1.0f);
    case PinType::Array_Int:      return ImVec4(0.20f, 0.50f, 0.85f, 1.0f);
    case PinType::Array_Float:    return ImVec4(0.30f, 0.75f, 0.30f, 1.0f);
    case PinType::Array_String:   return ImVec4(0.85f, 0.55f, 0.15f, 1.0f);
    case PinType::Array_ObjectRef:return ImVec4(0.50f, 0.30f, 0.85f, 1.0f);
    case PinType::Unknown:
    default:                      return ImVec4(0.50f, 0.50f, 0.50f, 1.0f); // grey
    }
}

} // namespace graph
