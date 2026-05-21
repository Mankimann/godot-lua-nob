#pragma once

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include <lua.h>
}

namespace godot::lua_bridge {

enum InstallFlags {
    INSTALL_SANDBOXED = 1 << 0,
    INSTALL_ALLOW_FFI = 1 << 1,
    INSTALL_ALLOW_BYTECODE = 1 << 2,
    INSTALL_ALLOW_NATIVE_LOADLIB = 1 << 3,
};

void install(lua_State *L, int flags, const Array &module_roots = Array());
void configure_package(lua_State *L, const Array &module_roots, int flags);
void push_variant(lua_State *L, const Variant &value);
Variant read_variant(lua_State *L, int index, int depth = 0);
String read_lua_error(lua_State *L, int index);

} // namespace godot::lua_bridge
