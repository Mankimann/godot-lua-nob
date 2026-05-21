#pragma once

#include <godot_lua/lua_state.hpp>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

class LuaScript;

class LuaScriptInstance : public RefCounted {
    GDCLASS(LuaScriptInstance, RefCounted)

    Object *owner = nullptr;
    Ref<LuaScript> script;
    Ref<LuaState> state;
    Dictionary properties;
    bool ready = false;

protected:
    static void _bind_methods();

public:
    LuaScriptInstance();
    ~LuaScriptInstance() override = default;

    Error initialize(Object *p_owner, const Ref<LuaScript> &p_script);
    Error initialize_with_properties(Object *p_owner, const Ref<LuaScript> &p_script, const Dictionary &p_initial_properties);
    Error initialize_from_file(Object *p_owner, const String &p_path, const Dictionary &p_initial_properties = Dictionary());
    Error reload(bool p_keep_properties = true);
    bool is_ready() const;

    Object *get_owner_object() const;
    Ref<LuaState> get_state() const;
    Ref<LuaScript> get_script_resource() const;
    Dictionary diagnostics() const;

    void add_module_root(const String &p_root);
    Array get_module_roots() const;

    Variant call_method(const StringName &p_method, const Array &p_args = Array());
    bool has_method(const StringName &p_method) const;

    void set_script_property(const StringName &p_name, const Variant &p_value);
    Variant get_script_property(const StringName &p_name) const;
    Dictionary get_script_properties() const;

    void notification(const StringName &p_name, const Array &p_args = Array());
    void ready_notification();
    void process_notification(double p_delta);
    void physics_process_notification(double p_delta);
    void input_notification(const Variant &p_event);
    void unhandled_input_notification(const Variant &p_event);
    void exit_tree_notification();
};

} // namespace godot
