#pragma once

#include <imgui.h>

namespace graph {

enum class PinType {
    Exec,
    Bool, Int, Float, String,
    ObjectRef, Actor, Quest, Form,
    Array_Bool, Array_Int, Array_Float, Array_String,
    Array_ObjectRef,
    Unknown
};

// Returns true if a wire from 'from' can connect to 'to'.
// E.g. Actor → ObjectRef is valid (subtype); Bool → Int is not.
inline bool IsCompatible(PinType from, PinType to) {
    if (from == to) return true;
    if (to == PinType::Exec || from == PinType::Exec) return false;
    // Actor is a subtype of ObjectRef
    if (from == PinType::Actor && to == PinType::ObjectRef) return true;
    // Quest and Form are also subtypes of ObjectRef
    if ((from == PinType::Quest || from == PinType::Form) &&
        to == PinType::ObjectRef) return true;
    return false;
}

} // namespace graph
