#include <godot_lua/lua_variant_bridge.hpp>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

namespace godot::lua_bridge {

static constexpr const char *GODOT_OBJECT_MT = "godot_lua.object";
static constexpr int MAX_TABLE_DEPTH = 32;

struct LuaGodotObject {
    uint64_t object_id;
};

static String lua_to_string(lua_State *L, int index) {
    size_t len = 0;
    const char *text = lua_tolstring(L, index, &len);
    if (!text) {
        return String();
    }
    return String::utf8(text, (int)len);
}

String read_lua_error(lua_State *L, int index) {
    if (lua_isnil(L, index)) {
        return String("<nil>");
    }
    if (lua_isstring(L, index)) {
        return lua_to_string(L, index);
    }
    luaL_tolstring(L, index, nullptr);
    String result = lua_to_string(L, -1);
    lua_pop(L, 1);
    return result;
}

static Object *object_from_userdata(lua_State *L, int index) {
    LuaGodotObject *ud = static_cast<LuaGodotObject *>(luaL_testudata(L, index, GODOT_OBJECT_MT));
    if (!ud) {
        return nullptr;
    }
    return ObjectDB::get_instance(ud->object_id);
}

static int godot_object_tostring(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) {
        lua_pushliteral(L, "<GodotObject:freed>");
        return 1;
    }
    String s = vformat("<%s:%d>", object->get_class(), object->get_instance_id());
    CharString utf8 = s.utf8();
    lua_pushstring(L, utf8.get_data());
    return 1;
}

static int godot_object_call_method(lua_State *L) {
    Object *object = object_from_userdata(L, lua_upvalueindex(1));
    StringName method = lua_to_string(L, lua_upvalueindex(2));
    if (!object) {
        return luaL_error(L, "Godot object has been freed");
    }

    int argc = lua_gettop(L);
    Array args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(read_variant(L, i + 1));
    }

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

    Variant property = object->get(key);
    push_variant(L, property);
    return 1;
}

static int godot_object_newindex(lua_State *L) {
    Object *object = object_from_userdata(L, 1);
    if (!object) {
        return luaL_error(L, "Godot object has been freed");
    }
    StringName key = lua_to_string(L, 2);
    Variant value = read_variant(L, 3);
    object->set(key, value);
    return 0;
}

static void push_object(lua_State *L, Object *object) {
    if (!object) {
        lua_pushnil(L);
        return;
    }
    LuaGodotObject *ud = static_cast<LuaGodotObject *>(lua_newuserdatauv(L, sizeof(LuaGodotObject), 0));
    ud->object_id = object->get_instance_id();
    luaL_getmetatable(L, GODOT_OBJECT_MT);
    lua_setmetatable(L, -2);
}

void push_variant(lua_State *L, const Variant &value) {
    switch (value.get_type()) {
        case Variant::NIL:
            lua_pushnil(L);
            break;
        case Variant::BOOL:
            lua_pushboolean(L, (bool)value);
            break;
        case Variant::INT:
            lua_pushinteger(L, (lua_Integer)(int64_t)value);
            break;
        case Variant::FLOAT:
            lua_pushnumber(L, (lua_Number)(double)value);
            break;
        case Variant::STRING:
        case Variant::STRING_NAME: {
            String s = value;
            CharString utf8 = s.utf8();
            lua_pushlstring(L, utf8.get_data(), utf8.length());
        } break;
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
        case Variant::COLOR: {
            Color c = value;
            lua_newtable(L);
            lua_pushnumber(L, c.r); lua_setfield(L, -2, "r");
            lua_pushnumber(L, c.g); lua_setfield(L, -2, "g");
            lua_pushnumber(L, c.b); lua_setfield(L, -2, "b");
            lua_pushnumber(L, c.a); lua_setfield(L, -2, "a");
        } break;
        case Variant::ARRAY: {
            Array array = value;
            lua_newtable(L);
            for (int64_t i = 0; i < array.size(); ++i) {
                push_variant(L, array[i]);
                lua_rawseti(L, -2, (lua_Integer)i + 1);
            }
        } break;
        case Variant::DICTIONARY: {
            Dictionary dict = value;
            Array keys = dict.keys();
            lua_newtable(L);
            for (int64_t i = 0; i < keys.size(); ++i) {
                push_variant(L, keys[i]);
                push_variant(L, dict[keys[i]]);
                lua_settable(L, -3);
            }
        } break;
        case Variant::OBJECT:
            push_object(L, (Object *)value);
            break;
        default: {
            String s = value.stringify();
            CharString utf8 = s.utf8();
            lua_pushlstring(L, utf8.get_data(), utf8.length());
        } break;
    }
}

static Variant table_to_variant(lua_State *L, int index, int depth) {
    if (depth > MAX_TABLE_DEPTH) {
        return Variant();
    }
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }

    bool array_like = true;
    lua_Integer max_index = 0;
    size_t count = 0;

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (!lua_isinteger(L, -2)) {
            array_like = false;
        } else {
            lua_Integer k = lua_tointeger(L, -2);
            if (k < 1) {
                array_like = false;
            }
            if (k > max_index) {
                max_index = k;
            }
        }
        ++count;
        lua_pop(L, 1);
    }
    if ((size_t)max_index != count) {
        array_like = false;
    }

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
        case LUA_TNONE:
            return Variant();
        case LUA_TBOOLEAN:
            return (bool)lua_toboolean(L, index);
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                return (int64_t)lua_tointeger(L, index);
            }
            return (double)lua_tonumber(L, index);
        case LUA_TSTRING:
            return lua_to_string(L, index);
        case LUA_TTABLE:
            return table_to_variant(L, index, depth);
        case LUA_TUSERDATA: {
            Object *object = object_from_userdata(L, index);
            if (object) {
                return object;
            }
            return Variant();
        }
        default:
            return Variant();
    }
}

static int l_godot_print(lua_State *L) {
    int n = lua_gettop(L);
    String line;
    for (int i = 1; i <= n; ++i) {
        luaL_tolstring(L, i, nullptr);
        if (i > 1) {
            line += "\t";
        }
        line += lua_to_string(L, -1);
        lua_pop(L, 1);
    }
    UtilityFunctions::print(line);
    return 0;
}

static int l_vector2(lua_State *L) {
    lua_Number x = luaL_optnumber(L, 1, 0.0);
    lua_Number y = luaL_optnumber(L, 2, 0.0);
    push_variant(L, Vector2((real_t)x, (real_t)y));
    return 1;
}

static int l_vector3(lua_State *L) {
    lua_Number x = luaL_optnumber(L, 1, 0.0);
    lua_Number y = luaL_optnumber(L, 2, 0.0);
    lua_Number z = luaL_optnumber(L, 3, 0.0);
    push_variant(L, Vector3((real_t)x, (real_t)y, (real_t)z));
    return 1;
}

static int l_color(lua_State *L) {
    lua_Number r = luaL_optnumber(L, 1, 1.0);
    lua_Number g = luaL_optnumber(L, 2, 1.0);
    lua_Number b = luaL_optnumber(L, 3, 1.0);
    lua_Number a = luaL_optnumber(L, 4, 1.0);
    push_variant(L, Color((float)r, (float)g, (float)b, (float)a));
    return 1;
}

static int l_get_singleton(lua_State *L) {
    StringName name = lua_to_string(L, 1);
    Object *singleton = Engine::get_singleton()->get_singleton(name);
    push_object(L, singleton);
    return 1;
}

static int l_variant_type(lua_State *L) {
    Variant v = read_variant(L, 1);
    String name = Variant::get_type_name(v.get_type());
    CharString utf8 = name.utf8();
    lua_pushstring(L, utf8.get_data());
    return 1;
}

static void install_object_metatable(lua_State *L) {
    if (luaL_newmetatable(L, GODOT_OBJECT_MT)) {
        lua_pushcfunction(L, godot_object_index);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, godot_object_newindex);
        lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, godot_object_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
}

static void apply_sandbox(lua_State *L) {
    static const char *blocked[] = { "dofile", "loadfile", "collectgarbage" };
    for (const char *name : blocked) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        static const char *blocked_os[] = { "execute", "exit", "getenv", "remove", "rename", "setlocale", "tmpname" };
        for (const char *name : blocked_os) {
            lua_pushnil(L);
            lua_setfield(L, -2, name);
        }
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setglobal(L, "io");
    lua_pushnil(L);
    lua_setglobal(L, "package");
}

void install(lua_State *L, bool sandboxed) {
    install_object_metatable(L);

    lua_newtable(L);
    lua_pushcfunction(L, l_godot_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, l_get_singleton); lua_setfield(L, -2, "get_singleton");
    lua_pushcfunction(L, l_variant_type); lua_setfield(L, -2, "variant_type");
    lua_setglobal(L, "godot");

    lua_pushcfunction(L, l_vector2); lua_setglobal(L, "Vector2");
    lua_pushcfunction(L, l_vector3); lua_setglobal(L, "Vector3");
    lua_pushcfunction(L, l_color); lua_setglobal(L, "Color");

    if (sandboxed) {
        apply_sandbox(L);
    }
}

} // namespace godot::lua_bridge
