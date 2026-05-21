#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include <lua.h>
}

namespace godot {

class LuaCallable : public RefCounted {
    GDCLASS(LuaCallable, RefCounted)

    lua_State *L = nullptr;
    int registry_ref = LUA_NOREF;

protected:
    static void _bind_methods();

public:
    LuaCallable();
    ~LuaCallable() override;

    void bind(lua_State *p_state, int p_function_index);
    Variant invoke(const Array &p_args = Array());
    bool is_bound() const;
};

Ref<LuaCallable> make_lua_callable(lua_State *L, int p_function_index);
void keep_lua_callable_alive(const Ref<LuaCallable> &p_callable);
void clear_lua_callable_keepalive();

} // namespace godot
