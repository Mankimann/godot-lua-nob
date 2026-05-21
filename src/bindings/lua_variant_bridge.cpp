#include <godot_lua/lua_variant_bridge.hpp>
#include <godot_lua/lua_callable.hpp>

#include <cmath>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector4.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

namespace godot::lua_bridge {

static constexpr const char *GODOT_OBJECT_MT = "godot_lua.object";
static constexpr const char *MODULE_ROOTS_REGISTRY_KEY = "godot_lua.module_roots";
static constexpr const char *INSTALL_FLAGS_REGISTRY_KEY = "godot_lua.install_flags";
static constexpr int MAX_TABLE_DEPTH = 64;

struct LuaGodotObject {
    uint64_t object_id;
};

static int abs_index(lua_State *L, int index) {
    if (index > 0 || index <= LUA_REGISTRYINDEX) return index;
    return lua_gettop(L) + index + 1;
}

static bool lua_number_is_integral(lua_Number n) {
    if (!std::isfinite((double)n)) return false;
    lua_Number rounded = std::floor(n);
    return rounded == n && n >= (lua_Number)INT64_MIN && n <= (lua_Number)INT64_MAX;
}

static String lua_to_string(lua_State *L, int index) {
    size_t len = 0;
    const char *text = lua_tolstring(L, index, &len);
    if (!text) return String();
    return String::utf8(text, (int)len);
}

static void push_string(lua_State *L, const String &value) {
    CharString utf8 = value.utf8();
    lua_pushlstring(L, utf8.get_data(), utf8.length());
}

String read_lua_error(lua_State *L, int index) {
    if (lua_isnil(L, index)) return String("<nil>");
    if (lua_isstring(L, index)) return lua_to_string(L, index);
    lua_getglobal(L, "tostring");
    lua_pushvalue(L, index < 0 ? index - 1 : index);
    if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
        String result = lua_to_string(L, -1);
        lua_pop(L, 1);
        return result;
    }
    lua_pop(L, 1);
    return String("<non-string Lua error>");
}

static Object *object_from_userdata(lua_State *L, int index) {
    LuaGodotObject *ud = static_cast<LuaGodotObject *>(luaL_testudata(L, index, GODOT_OBJECT_MT));
    if (!ud) return nullptr;
    return ObjectDB::get_instance(ud->object_id);
}

static int godot_object_tostring(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) {
        lua_pushliteral(L, "<GodotObject:freed>");
        return 1;
    }
    push_string(L, vformat("<%s:%d>", object->get_class(), object->get_instance_id()));
    return 1;
}

static int godot_object_call_method(lua_State *L) {
    Object *object = object_from_userdata(L, lua_upvalueindex(1));
    StringName method = lua_to_string(L, lua_upvalueindex(2));
    if (!object) return luaL_error(L, "Godot object has been freed");

    int argc = lua_gettop(L);
    Array args;
    for (int i = 0; i < argc; ++i) args.push_back(read_variant(L, i + 1));

    Variant result = object->callv(method, args);
    push_variant(L, result);
    return 1;
}

static int godot_object_index(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) {
        lua_pushnil(L);
        return 1;
    }
    StringName key = lua_to_string(L, 2);

    if (object->has_method(key)) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 2);
        lua_pushcclosure(L, godot_object_call_method, 2);
        return 1;
    }

    push_variant(L, object->get(key));
    return 1;
}

static int godot_object_newindex(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) return luaL_error(L, "Godot object has been freed");
    object->set(lua_to_string(L, 2), read_variant(L, 3));
    return 0;
}

static int godot_object_is_valid(lua_State *L) {
    lua_pushboolean(L, object_from_userdata(L, 1) != nullptr);
    return 1;
}

static void push_object(lua_State *L, Object *object) {
    if (!object) {
        lua_pushnil(L);
        return;
    }
    LuaGodotObject *ud = static_cast<LuaGodotObject *>(lua_newuserdata(L, sizeof(LuaGodotObject)));
    ud->object_id = object->get_instance_id();
    luaL_getmetatable(L, GODOT_OBJECT_MT);
    lua_setmetatable(L, -2);
}

void push_variant(lua_State *L, const Variant &value) {
    switch (value.get_type()) {
        case Variant::NIL: lua_pushnil(L); break;
        case Variant::BOOL: lua_pushboolean(L, (bool)value); break;
        case Variant::INT: lua_pushinteger(L, (lua_Integer)(int64_t)value); break;
        case Variant::FLOAT: lua_pushnumber(L, (lua_Number)(double)value); break;
        case Variant::STRING:
        case Variant::STRING_NAME:
        case Variant::NODE_PATH: push_string(L, String(value)); break;
        case Variant::VECTOR2: {
            Vector2 v = value;
            lua_newtable(L);
            lua_pushnumber(L, v.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, v.y); lua_setfield(L, -2, "y");
        } break;
        case Variant::VECTOR3: {
            Vector3 v = value;
            lua_newtable(L);
            lua_pushnumber(L, v.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, v.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, v.z); lua_setfield(L, -2, "z");
        } break;
        case Variant::VECTOR4: {
            Vector4 v = value;
            lua_newtable(L);
            lua_pushnumber(L, v.x); lua_setfield(L, -2, "x");
            lua_pushnumber(L, v.y); lua_setfield(L, -2, "y");
            lua_pushnumber(L, v.z); lua_setfield(L, -2, "z");
            lua_pushnumber(L, v.w); lua_setfield(L, -2, "w");
        } break;
        case Variant::COLOR: {
            Color c = value;
            lua_newtable(L);
            lua_pushnumber(L, c.r); lua_setfield(L, -2, "r");
            lua_pushnumber(L, c.g); lua_setfield(L, -2, "g");
            lua_pushnumber(L, c.b); lua_setfield(L, -2, "b");
            lua_pushnumber(L, c.a); lua_setfield(L, -2, "a");
        } break;
        case Variant::PACKED_BYTE_ARRAY: {
            PackedByteArray bytes = value;
            lua_pushlstring(L, reinterpret_cast<const char *>(bytes.ptr()), bytes.size());
        } break;
        case Variant::ARRAY: {
            Array array = value;
            lua_createtable(L, (int)array.size(), 0);
            for (int64_t i = 0; i < array.size(); ++i) {
                push_variant(L, array[i]);
                lua_rawseti(L, -2, (lua_Integer)i + 1);
            }
        } break;
        case Variant::DICTIONARY: {
            Dictionary dict = value;
            Array keys = dict.keys();
            lua_createtable(L, 0, (int)keys.size());
            for (int64_t i = 0; i < keys.size(); ++i) {
                push_variant(L, keys[i]);
                push_variant(L, dict[keys[i]]);
                lua_settable(L, -3);
            }
        } break;
        case Variant::OBJECT: push_object(L, (Object *)value); break;
        default: push_string(L, value.stringify()); break;
    }
}

static Variant table_to_vector_or_color(lua_State *L, int index) {
    index = abs_index(L, index);
    lua_getfield(L, index, "x"); lua_getfield(L, index, "y"); lua_getfield(L, index, "z"); lua_getfield(L, index, "w");
    bool has_x = lua_isnumber(L, -4);
    bool has_y = lua_isnumber(L, -3);
    bool has_z = lua_isnumber(L, -2);
    bool has_w = lua_isnumber(L, -1);
    lua_Number x = lua_tonumber(L, -4), y = lua_tonumber(L, -3), z = lua_tonumber(L, -2), w = lua_tonumber(L, -1);
    lua_pop(L, 4);
    if (has_x && has_y && has_z && has_w) return Vector4((real_t)x, (real_t)y, (real_t)z, (real_t)w);
    if (has_x && has_y && has_z) return Vector3((real_t)x, (real_t)y, (real_t)z);
    if (has_x && has_y) return Vector2((real_t)x, (real_t)y);

    lua_getfield(L, index, "r"); lua_getfield(L, index, "g"); lua_getfield(L, index, "b"); lua_getfield(L, index, "a");
    bool has_r = lua_isnumber(L, -4), has_g = lua_isnumber(L, -3), has_b = lua_isnumber(L, -2);
    lua_Number r = lua_tonumber(L, -4), g = lua_tonumber(L, -3), b = lua_tonumber(L, -2), a = luaL_optnumber(L, -1, 1.0);
    lua_pop(L, 4);
    if (has_r && has_g && has_b) return Color((float)r, (float)g, (float)b, (float)a);
    return Variant();
}

static Variant table_to_variant(lua_State *L, int index, int depth) {
    if (depth > MAX_TABLE_DEPTH) return Variant();
    index = abs_index(L, index);

    Variant typed = table_to_vector_or_color(L, index);
    if (typed.get_type() != Variant::NIL) return typed;

    bool array_like = true;
    lua_Integer max_index = 0;
    size_t count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_type(L, -2) != LUA_TNUMBER || !lua_number_is_integral(lua_tonumber(L, -2))) {
            array_like = false;
        } else {
            lua_Integer k = lua_tointeger(L, -2);
            if (k < 1) array_like = false;
            if (k > max_index) max_index = k;
        }
        ++count;
        lua_pop(L, 1);
    }
    if ((size_t)max_index != count) array_like = false;

    if (array_like) {
        Array array;
        for (lua_Integer i = 1; i <= max_index; ++i) {
            lua_rawgeti(L, index, i);
            array.push_back(read_variant(L, -1, depth + 1));
            lua_pop(L, 1);
        }
        return array;
    }

    Dictionary dict;
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        Variant key = read_variant(L, -2, depth + 1);
        Variant val = read_variant(L, -1, depth + 1);
        dict[key] = val;
        lua_pop(L, 1);
    }
    return dict;
}

Variant read_variant(lua_State *L, int index, int depth) {
    switch (lua_type(L, index)) {
        case LUA_TNIL:
        case LUA_TNONE: return Variant();
        case LUA_TBOOLEAN: return (bool)lua_toboolean(L, index);
        case LUA_TNUMBER: {
            lua_Number n = lua_tonumber(L, index);
            if (lua_number_is_integral(n)) return (int64_t)lua_tointeger(L, index);
            return (double)n;
        }
        case LUA_TSTRING: return lua_to_string(L, index);
        case LUA_TTABLE: return table_to_variant(L, index, depth);
        case LUA_TUSERDATA: {
            Object *object = object_from_userdata(L, index);
            if (object) return object;
            return Variant();
        }
        default: return Variant();
    }
}

static int l_godot_print(lua_State *L) {
    int n = lua_gettop(L);
    String line;
    lua_getglobal(L, "tostring");
    for (int i = 1; i <= n; ++i) {
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
            if (i > 1) line += "\t";
            line += lua_to_string(L, -1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    UtilityFunctions::print(line);
    return 0;
}

static int l_push_warning(lua_State *L) {
    UtilityFunctions::push_warning(lua_to_string(L, 1));
    return 0;
}

static int l_push_error(lua_State *L) {
    UtilityFunctions::push_error(lua_to_string(L, 1));
    return 0;
}

static int l_vector2(lua_State *L) { push_variant(L, Vector2((real_t)luaL_optnumber(L, 1, 0.0), (real_t)luaL_optnumber(L, 2, 0.0))); return 1; }
static int l_vector3(lua_State *L) { push_variant(L, Vector3((real_t)luaL_optnumber(L, 1, 0.0), (real_t)luaL_optnumber(L, 2, 0.0), (real_t)luaL_optnumber(L, 3, 0.0))); return 1; }
static int l_vector4(lua_State *L) { push_variant(L, Vector4((real_t)luaL_optnumber(L, 1, 0.0), (real_t)luaL_optnumber(L, 2, 0.0), (real_t)luaL_optnumber(L, 3, 0.0), (real_t)luaL_optnumber(L, 4, 0.0))); return 1; }
static int l_color(lua_State *L) { push_variant(L, Color((float)luaL_optnumber(L, 1, 1.0), (float)luaL_optnumber(L, 2, 1.0), (float)luaL_optnumber(L, 3, 1.0), (float)luaL_optnumber(L, 4, 1.0))); return 1; }

static int l_get_singleton(lua_State *L) {
    push_object(L, Engine::get_singleton()->get_singleton(lua_to_string(L, 1)));
    return 1;
}

static int l_load_resource(lua_State *L) {
    String path = lua_to_string(L, 1);
    String type_hint = lua_gettop(L) >= 2 ? lua_to_string(L, 2) : String();
    Ref<Resource> resource = ResourceLoader::get_singleton()->load(path, type_hint);
    push_variant(L, resource);
    return 1;
}

static int l_variant_type(lua_State *L) {
    Variant v = read_variant(L, 1);
    push_string(L, Variant::get_type_name(v.get_type()));
    return 1;
}

static int l_is_instance_valid(lua_State *L) {
    lua_pushboolean(L, object_from_userdata(L, 1) != nullptr);
    return 1;
}

static int l_callable(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    Ref<LuaCallable> callback = make_lua_callable(L, 1);
    keep_lua_callable_alive(callback);
    push_variant(L, callback);
    return 1;
}

static int l_connect(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) return luaL_error(L, "godot.connect expects a valid Godot object as first argument");
    StringName signal = lua_to_string(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    Ref<LuaCallable> callback = make_lua_callable(L, 3);
    keep_lua_callable_alive(callback);
    Callable callable(callback.ptr(), "invoke");
    Error err = object->connect(signal, callable);
    lua_pushboolean(L, err == OK);
    lua_pushinteger(L, err);
    return 2;
}

static String module_name_to_path(const String &name) {
    return name.replace(".", "/");
}

static bool try_load_lua_file(lua_State *L, const String &path, int flags) {
    if (!FileAccess::file_exists(path)) return false;
    String source = FileAccess::get_file_as_string(path);
    CharString code = source.utf8();
    CharString chunk = ("=" + path).utf8();
    const char *mode = (flags & INSTALL_ALLOW_BYTECODE) ? "bt" : "t";
    if (luaL_loadbufferx(L, code.get_data(), code.length(), chunk.get_data(), mode) != LUA_OK) {
        return true;
    }
    return true;
}

static int godot_lua_loader(lua_State *L) {
    String module = lua_to_string(L, 1);
    String rel = module_name_to_path(module);
    String errors;

    lua_getfield(L, LUA_REGISTRYINDEX, INSTALL_FLAGS_REGISTRY_KEY);
    int flags = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : 0;
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, MODULE_ROOTS_REGISTRY_KEY);
    if (lua_istable(L, -1)) {
        int roots = abs_index(L, -1);
        int count = (int)lua_objlen(L, roots);
        for (int i = 1; i <= count; ++i) {
            lua_rawgeti(L, roots, i);
            String root = lua_to_string(L, -1);
            lua_pop(L, 1);
            if (!root.ends_with("/")) root += "/";
            String candidates[2] = { root + rel + ".lua", root + rel + "/init.lua" };
            for (int c = 0; c < 2; ++c) {
                if (FileAccess::file_exists(candidates[c])) {
                    lua_pop(L, 1);
                    if (try_load_lua_file(L, candidates[c], flags)) return 1;
                }
                errors += "\n\tno Godot Lua file '" + candidates[c] + "'";
            }
        }
    }
    lua_pop(L, 1);
    push_string(L, errors);
    return 1;
}

void configure_package(lua_State *L, const Array &module_roots, int flags) {
    lua_pushinteger(L, flags);
    lua_setfield(L, LUA_REGISTRYINDEX, INSTALL_FLAGS_REGISTRY_KEY);

    lua_newtable(L);
    for (int64_t i = 0; i < module_roots.size(); ++i) {
        push_string(L, module_roots[i]);
        lua_rawseti(L, -2, (lua_Integer)i + 1);
    }
    lua_setfield(L, LUA_REGISTRYINDEX, MODULE_ROOTS_REGISTRY_KEY);

    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    if (!(flags & INSTALL_ALLOW_NATIVE_LOADLIB)) {
        lua_pushnil(L); lua_setfield(L, -2, "loadlib");
        lua_pushliteral(L, ""); lua_setfield(L, -2, "cpath");
    }

    lua_getfield(L, -1, "loaders");
    if (lua_istable(L, -1)) {
        int n = (int)lua_objlen(L, -1);
        for (int i = n + 1; i >= 2; --i) {
            lua_rawgeti(L, -1, i - 1);
            lua_rawseti(L, -2, i);
        }
        lua_pushcfunction(L, godot_lua_loader);
        lua_rawseti(L, -2, 2);
    }
    lua_pop(L, 1);

    lua_pop(L, 1);
}

static int blocked_module_loader(lua_State *L) {
    const char *name = lua_tostring(L, lua_upvalueindex(1));
    return luaL_error(L, "LuaJIT module '%s' is disabled by this LuaState policy", name ? name : "<unknown>");
}

static void block_module(lua_State *L, const char *name) {
    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "preload");
        if (lua_istable(L, -1)) {
            lua_pushstring(L, name);
            lua_pushstring(L, name);
            lua_pushcclosure(L, blocked_module_loader, 1);
            lua_settable(L, -3);
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            lua_setfield(L, -2, name);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setglobal(L, name);
}

static void install_object_metatable(lua_State *L) {
    if (luaL_newmetatable(L, GODOT_OBJECT_MT)) {
        lua_pushcfunction(L, godot_object_index); lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, godot_object_newindex); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, godot_object_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, godot_object_is_valid); lua_setfield(L, -2, "is_valid");
    }
    lua_pop(L, 1);
}

static void apply_sandbox(lua_State *L, int flags) {
    static const char *blocked_globals[] = { "dofile", "loadfile" };
    for (const char *name : blocked_globals) { lua_pushnil(L); lua_setglobal(L, name); }

    if (!(flags & INSTALL_ALLOW_BYTECODE)) {
        lua_pushnil(L); lua_setglobal(L, "loadstring");
    }

    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        static const char *blocked_os[] = { "execute", "exit", "getenv", "remove", "rename", "setlocale", "tmpname" };
        for (const char *name : blocked_os) { lua_pushnil(L); lua_setfield(L, -2, name); }
    }
    lua_pop(L, 1);

    lua_pushnil(L); lua_setglobal(L, "io");
    if (!(flags & INSTALL_ALLOW_FFI)) block_module(L, "ffi");
}

void install(lua_State *L, int flags, const Array &module_roots) {
    install_object_metatable(L);
    configure_package(L, module_roots, flags);

    lua_newtable(L);
    lua_pushcfunction(L, l_godot_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, l_push_warning); lua_setfield(L, -2, "push_warning");
    lua_pushcfunction(L, l_push_error); lua_setfield(L, -2, "push_error");
    lua_pushcfunction(L, l_get_singleton); lua_setfield(L, -2, "get_singleton");
    lua_pushcfunction(L, l_load_resource); lua_setfield(L, -2, "load_resource");
    lua_pushcfunction(L, l_variant_type); lua_setfield(L, -2, "variant_type");
    lua_pushcfunction(L, l_is_instance_valid); lua_setfield(L, -2, "is_instance_valid");
    lua_pushcfunction(L, l_callable); lua_setfield(L, -2, "callable");
    lua_pushcfunction(L, l_connect); lua_setfield(L, -2, "connect");
    lua_setglobal(L, "godot");

    lua_pushcfunction(L, l_vector2); lua_setglobal(L, "Vector2");
    lua_pushcfunction(L, l_vector3); lua_setglobal(L, "Vector3");
    lua_pushcfunction(L, l_vector4); lua_setglobal(L, "Vector4");
    lua_pushcfunction(L, l_color); lua_setglobal(L, "Color");

    if (flags & INSTALL_SANDBOXED) apply_sandbox(L, flags);
}

} // namespace godot::lua_bridge
