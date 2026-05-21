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

public:
    enum RuntimePolicy {
        POLICY_TRUSTED = 0,
        POLICY_GAMEPLAY = 1,
        POLICY_SANDBOXED = 2,
        POLICY_MODDING = 3,
    };

private:
    lua_State *L = nullptr;
    bool libraries_opened = false;
    bool sandboxed = false;
    RuntimePolicy policy = POLICY_GAMEPLAY;
    bool ffi_enabled = false;
    bool jit_enabled = true;
    bool bytecode_enabled = false;
    Array module_roots;
    Dictionary loaded_files;

    Variant make_error(int p_status, const String &p_prefix = String()) const;
    Variant finish_protected_call(int p_status, int p_result_count);
    void apply_policy();
    void install_runtime();

protected:
    static void _bind_methods();

public:
    LuaState();
    ~LuaState() override;

    bool is_valid() const;
    bool is_sandboxed() const;

    void open_libraries(bool p_sandboxed = false);
    void open_with_policy(int p_policy = POLICY_GAMEPLAY);
    void close();
    void reset();

    void set_policy(int p_policy);
    int get_policy() const;

    void set_ffi_enabled(bool p_enabled);
    bool is_ffi_enabled() const;

    void set_jit_enabled(bool p_enabled);
    bool is_jit_enabled() const;
    Variant jit_flush();
    Variant jit_optimize(const String &p_options = "hotloop=56,hotexit=10");

    void set_bytecode_enabled(bool p_enabled);
    bool is_bytecode_enabled() const;

    void add_module_root(const String &p_root);
    void clear_module_roots();
    Array get_module_roots() const;

    Variant do_string(const String &p_code, const String &p_chunk_name = "chunk");
    Variant do_file(const String &p_path);
    Variant require_module(const String &p_name);
    void unload_module(const String &p_name);
    void reload_changed_files();

    Variant get_global(const String &p_name);
    void set_global(const String &p_name, const Variant &p_value);

    Variant call_global(const String &p_name, const Array &p_args = Array());

    Dictionary diagnostics() const;

    lua_State *get_lua_state() const { return L; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::LuaState::RuntimePolicy);
