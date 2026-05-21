#pragma once

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include <lua.h>
}

namespace godot::lua_bridge {

void install(lua_State *L, bool sandboxed);
void push_variant(lua_State *L, const Variant &value);
Variant read_variant(lua_State *L, int index, int depth = 0);
String read_lua_error(lua_State *L, int index);

} // namespace godot::lua_bridge
