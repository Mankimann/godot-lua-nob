#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include <lua.h>
}

namespace godot {

class LuaState : public RefCounted {
    GDCLASS(LuaState, RefCounted)

    lua_State *L = nullptr;
    bool libraries_opened = false;
    bool sandboxed = false;

    Variant make_error(int p_status, const String &p_prefix = String()) const;
    Variant finish_protected_call(int p_status, int p_result_count);

protected:
    static void _bind_methods();

public:
    LuaState();
    ~LuaState() override;

    bool is_valid() const;
    bool is_sandboxed() const;

    void open_libraries(bool p_sandboxed = false);
    void close();
    void reset();

    Variant do_string(const String &p_code, const String &p_chunk_name = "chunk");
    Variant do_file(const String &p_path);

    Variant get_global(const String &p_name);
    void set_global(const String &p_name, const Variant &p_value);

    Variant call_global(const String &p_name, const Array &p_args = Array());

    Dictionary diagnostics() const;

    lua_State *get_lua_state() const { return L; }
};

} // namespace godot
