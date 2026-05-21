#include <godot_lua/lua_callable.hpp>
#include <godot_lua/lua_variant_bridge.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/vector.hpp>

extern "C" {
#include <lauxlib.h>
}

namespace godot {

static Vector<Ref<LuaCallable>> lua_callable_keepalive;

static int lua_callback_traceback(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr) msg = "<non-string Lua callback error>";
    luaL_traceback(L, L, msg, 1);
    return 1;
}

LuaCallable::LuaCallable() = default;

LuaCallable::~LuaCallable() {
    if (L && registry_ref != LUA_NOREF && registry_ref != LUA_REFNIL) {
        luaL_unref(L, LUA_REGISTRYINDEX, registry_ref);
    }
    registry_ref = LUA_NOREF;
    L = nullptr;
}

void LuaCallable::_bind_methods() {
    ClassDB::bind_method(D_METHOD("invoke", "args"), &LuaCallable::invoke, DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("is_bound"), &LuaCallable::is_bound);
}

void LuaCallable::bind(lua_State *p_state, int p_function_index) {
    L = p_state;
    if (!L) return;
    if (registry_ref != LUA_NOREF && registry_ref != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, registry_ref);
    lua_pushvalue(L, p_function_index);
    registry_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

Variant LuaCallable::invoke(const Array &p_args) {
    if (!L || registry_ref == LUA_NOREF || registry_ref == LUA_REFNIL) return Variant();

    lua_pushcfunction(L, lua_callback_traceback);
    int error_handler = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, registry_ref);
    for (int64_t i = 0; i < p_args.size(); ++i) lua_bridge::push_variant(L, p_args[i]);

    int status = lua_pcall(L, (int)p_args.size(), LUA_MULTRET, error_handler);
    int result_count = lua_gettop(L) - error_handler;
    lua_remove(L, error_handler);
    if (status != LUA_OK) {
        String error = lua_bridge::read_lua_error(L, -1);
        lua_pop(L, 1);
        return error;
    }
    if (result_count <= 0) return Variant();
    if (result_count == 1) {
        Variant value = lua_bridge::read_variant(L, -1);
        lua_pop(L, 1);
        return value;
    }
    Array results;
    int first = lua_gettop(L) - result_count + 1;
    for (int i = 0; i < result_count; ++i) results.push_back(lua_bridge::read_variant(L, first + i));
    lua_pop(L, result_count);
    return results;
}

bool LuaCallable::is_bound() const {
    return L != nullptr && registry_ref != LUA_NOREF && registry_ref != LUA_REFNIL;
}

Ref<LuaCallable> make_lua_callable(lua_State *L, int p_function_index) {
    Ref<LuaCallable> cb;
    cb.instantiate();
    cb->bind(L, p_function_index);
    return cb;
}

void keep_lua_callable_alive(const Ref<LuaCallable> &p_callable) {
    if (p_callable.is_valid()) lua_callable_keepalive.push_back(p_callable);
}

void clear_lua_callable_keepalive() {
    lua_callable_keepalive.clear();
}

} // namespace godot
