#include <godot_lua/lua_script_instance.hpp>
#include <godot_lua/lua_script.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static bool lua_result_is_error(const Variant &p_result) {
    if (p_result.get_type() != Variant::DICTIONARY) {
        return false;
    }
    Dictionary error = p_result;
    return error.has("message") || error.has("status");
}

void LuaScriptInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize", "owner", "script"), &LuaScriptInstance::initialize);
    ClassDB::bind_method(D_METHOD("initialize_with_properties", "owner", "script", "initial_properties"), &LuaScriptInstance::initialize_with_properties);
    ClassDB::bind_method(D_METHOD("initialize_from_file", "owner", "path", "initial_properties"), &LuaScriptInstance::initialize_from_file, DEFVAL(Dictionary()));
    ClassDB::bind_method(D_METHOD("reload", "keep_properties"), &LuaScriptInstance::reload, DEFVAL(true));
    ClassDB::bind_method(D_METHOD("is_ready"), &LuaScriptInstance::is_ready);
    ClassDB::bind_method(D_METHOD("get_owner_object"), &LuaScriptInstance::get_owner_object);
    ClassDB::bind_method(D_METHOD("get_state"), &LuaScriptInstance::get_state);
    ClassDB::bind_method(D_METHOD("get_script_resource"), &LuaScriptInstance::get_script_resource);
    ClassDB::bind_method(D_METHOD("diagnostics"), &LuaScriptInstance::diagnostics);
    ClassDB::bind_method(D_METHOD("add_module_root", "root"), &LuaScriptInstance::add_module_root);
    ClassDB::bind_method(D_METHOD("get_module_roots"), &LuaScriptInstance::get_module_roots);
    ClassDB::bind_method(D_METHOD("call_method", "method", "args"), &LuaScriptInstance::call_method, DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("has_method", "method"), &LuaScriptInstance::has_method);
    ClassDB::bind_method(D_METHOD("set_script_property", "name", "value"), &LuaScriptInstance::set_script_property);
    ClassDB::bind_method(D_METHOD("get_script_property", "name"), &LuaScriptInstance::get_script_property);
    ClassDB::bind_method(D_METHOD("get_script_properties"), &LuaScriptInstance::get_script_properties);
    ClassDB::bind_method(D_METHOD("notification", "name", "args"), &LuaScriptInstance::notification, DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("ready_notification"), &LuaScriptInstance::ready_notification);
    ClassDB::bind_method(D_METHOD("process_notification", "delta"), &LuaScriptInstance::process_notification);
    ClassDB::bind_method(D_METHOD("physics_process_notification", "delta"), &LuaScriptInstance::physics_process_notification);
    ClassDB::bind_method(D_METHOD("input_notification", "event"), &LuaScriptInstance::input_notification);
    ClassDB::bind_method(D_METHOD("unhandled_input_notification", "event"), &LuaScriptInstance::unhandled_input_notification);
    ClassDB::bind_method(D_METHOD("exit_tree_notification"), &LuaScriptInstance::exit_tree_notification);
}

LuaScriptInstance::LuaScriptInstance() {
}

Error LuaScriptInstance::initialize(Object *p_owner, const Ref<LuaScript> &p_script) {
    return initialize_with_properties(p_owner, p_script, Dictionary());
}

Error LuaScriptInstance::initialize_with_properties(Object *p_owner, const Ref<LuaScript> &p_script, const Dictionary &p_initial_properties) {
    owner = p_owner;
    script = p_script;
    ready = false;
    properties.clear();

    if (script.is_null() || !script->_is_valid()) {
        return ERR_INVALID_PARAMETER;
    }

    state.instantiate();
    state->open_with_policy(LuaState::POLICY_GAMEPLAY);
    state->set_global("self", Variant(owner));
    state->set_global("owner", Variant(owner));

    if (!script->get_path_hint().is_empty()) {
        String base_dir = script->get_path_hint().get_base_dir();
        if (!base_dir.is_empty()) {
            state->add_module_root(base_dir);
        }
    }

    Array keys = p_initial_properties.keys();
    for (int i = 0; i < keys.size(); i++) {
        StringName name = keys[i];
        set_script_property(name, p_initial_properties[keys[i]]);
    }

    Variant result = state->do_string(script->_get_source_code(), script->get_path_hint().is_empty() ? String("LuaScript") : script->get_path_hint());
    if (lua_result_is_error(result)) {
        Dictionary error = result;
        UtilityFunctions::push_error(String("Lua script initialization failed: ") + String(error.get("message", "unknown error")));
        state.unref();
        return ERR_SCRIPT_FAILED;
    }

    ready = true;
    return OK;
}

Error LuaScriptInstance::initialize_from_file(Object *p_owner, const String &p_path, const Dictionary &p_initial_properties) {
    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::push_error(String("Lua script file not found: ") + p_path);
        return ERR_FILE_NOT_FOUND;
    }

    Ref<LuaScript> file_script;
    file_script.instantiate();
    file_script->set_path_hint(p_path);
    file_script->_set_source_code(FileAccess::get_file_as_string(p_path));
    Error reload_error = file_script->_reload(false);
    if (reload_error != OK) {
        return reload_error;
    }
    return initialize_with_properties(p_owner, file_script, p_initial_properties);
}

Error LuaScriptInstance::reload(bool p_keep_properties) {
    if (script.is_null()) {
        return ERR_UNCONFIGURED;
    }
    Dictionary preserved = p_keep_properties ? properties : Dictionary();
    Error reload_error = script->_reload(p_keep_properties);
    if (reload_error != OK) {
        return reload_error;
    }
    return initialize_with_properties(owner, script, preserved);
}

bool LuaScriptInstance::is_ready() const {
    return ready;
}

Object *LuaScriptInstance::get_owner_object() const {
    return owner;
}

Ref<LuaState> LuaScriptInstance::get_state() const {
    return state;
}

Ref<LuaScript> LuaScriptInstance::get_script_resource() const {
    return script;
}

Dictionary LuaScriptInstance::diagnostics() const {
    Dictionary result;
    result["ready"] = ready;
    result["owner"] = Variant(owner);
    result["script_path"] = script.is_valid() ? script->get_path_hint() : String();
    result["methods"] = script.is_valid() ? script->get_discovered_methods() : Dictionary();
    result["properties"] = properties;
    result["state"] = state.is_valid() ? state->diagnostics() : Dictionary();
    return result;
}

void LuaScriptInstance::add_module_root(const String &p_root) {
    if (state.is_valid()) {
        state->add_module_root(p_root);
    }
}

Array LuaScriptInstance::get_module_roots() const {
    if (state.is_valid()) {
        return state->get_module_roots();
    }
    return Array();
}

Variant LuaScriptInstance::call_method(const StringName &p_method, const Array &p_args) {
    if (!ready || state.is_null()) {
        return Variant();
    }
    return state->call_global(String(p_method), p_args);
}

bool LuaScriptInstance::has_method(const StringName &p_method) const {
    return script.is_valid() && script->_has_method(p_method);
}

void LuaScriptInstance::set_script_property(const StringName &p_name, const Variant &p_value) {
    properties[p_name] = p_value;
    if (state.is_valid()) {
        state->set_global(String(p_name), p_value);
    }
}

Variant LuaScriptInstance::get_script_property(const StringName &p_name) const {
    if (properties.has(p_name)) {
        return properties[p_name];
    }
    if (state.is_valid()) {
        return state->get_global(String(p_name));
    }
    return Variant();
}

Dictionary LuaScriptInstance::get_script_properties() const {
    return properties;
}

void LuaScriptInstance::notification(const StringName &p_name, const Array &p_args) {
    if (has_method(p_name)) {
        Variant result = call_method(p_name, p_args);
        if (lua_result_is_error(result)) {
            Dictionary error = result;
            UtilityFunctions::push_error(String("Lua method failed: ") + String(p_name) + String(": ") + String(error.get("message", "unknown error")));
        }
    }
}

void LuaScriptInstance::ready_notification() {
    notification("_ready");
}

void LuaScriptInstance::process_notification(double p_delta) {
    Array args;
    args.push_back(p_delta);
    notification("_process", args);
}

void LuaScriptInstance::physics_process_notification(double p_delta) {
    Array args;
    args.push_back(p_delta);
    notification("_physics_process", args);
}

void LuaScriptInstance::input_notification(const Variant &p_event) {
    Array args;
    args.push_back(p_event);
    notification("_input", args);
}

void LuaScriptInstance::unhandled_input_notification(const Variant &p_event) {
    Array args;
    args.push_back(p_event);
    notification("_unhandled_input", args);
}

void LuaScriptInstance::exit_tree_notification() {
    notification("_exit_tree");
    ready = false;
    if (state.is_valid()) {
        state->close();
    }
}

} // namespace godot
