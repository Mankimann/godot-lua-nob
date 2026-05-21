#include <godot_lua/lua_script_instance.hpp>
#include <godot_lua/lua_script.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void LuaScriptInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize", "owner", "script"), &LuaScriptInstance::initialize);
    ClassDB::bind_method(D_METHOD("is_ready"), &LuaScriptInstance::is_ready);
    ClassDB::bind_method(D_METHOD("get_owner_object"), &LuaScriptInstance::get_owner_object);
    ClassDB::bind_method(D_METHOD("get_state"), &LuaScriptInstance::get_state);
    ClassDB::bind_method(D_METHOD("get_script_resource"), &LuaScriptInstance::get_script_resource);
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
}

LuaScriptInstance::LuaScriptInstance() {
}

Error LuaScriptInstance::initialize(Object *p_owner, const Ref<LuaScript> &p_script) {
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

    Variant result = state->do_string(script->_get_source_code(), script->get_path_hint().is_empty() ? String("LuaScript") : script->get_path_hint());
    if (result.get_type() == Variant::DICTIONARY) {
        Dictionary error = result;
        if (error.has("message")) {
            UtilityFunctions::push_error(String("Lua script initialization failed: ") + String(error["message"]));
            return ERR_SCRIPT_FAILED;
        }
    }

    ready = true;
    return OK;
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
        call_method(p_name, p_args);
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

} // namespace godot
