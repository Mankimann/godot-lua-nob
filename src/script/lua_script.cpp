#include <godot_lua/lua_script.hpp>
#include <godot_lua/lua_script_instance.hpp>
#include <godot_lua/lua_script_language.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <gdextension_interface.h>

namespace godot {

static Dictionary make_method_info(const StringName &p_name, int32_t p_line, int32_t p_arg_count = -1) {
    Dictionary info;
    info["name"] = p_name;
    info["args"] = Array();
    info["default_args"] = Array();
    info["flags"] = 0;
    info["id"] = 0;
    info["return"] = Dictionary();
    info["line"] = p_line;
    info["argument_count"] = p_arg_count;
    return info;
}

static String strip_lua_comment(const String &p_line) {
    int comment = p_line.find("--");
    if (comment >= 0) {
        return p_line.substr(0, comment);
    }
    return p_line;

}

struct LuaNativeScriptInstanceData {
    Ref<LuaScriptInstance> wrapper;
    Ref<LuaScript> script;
    Object *owner = nullptr;
};

static LuaNativeScriptInstanceData *lua_native_data(GDExtensionScriptInstanceDataPtr p_instance) {
    return reinterpret_cast<LuaNativeScriptInstanceData *>(p_instance);
}

static void lua_variant_copy_to(GDExtensionVariantPtr r_return, const Variant &p_value) {
    internal::gdextension_interface_variant_new_copy(r_return, p_value._native_ptr());
}

static GDExtensionBool lua_script_instance_set(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        return false;
    }
    StringName name = *reinterpret_cast<const StringName *>(p_name);
    Variant value;
    internal::gdextension_interface_variant_new_copy(value._native_ptr(), p_value);
    data->wrapper->set_script_property(name, value);
    return true;
}

static GDExtensionBool lua_script_instance_get(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        return false;
    }
    StringName name = *reinterpret_cast<const StringName *>(p_name);
    Variant value = data->wrapper->get_script_property(name);
    lua_variant_copy_to(r_ret, value);
    return value.get_type() != Variant::NIL;
}

static const GDExtensionPropertyInfo *lua_script_instance_get_property_list(GDExtensionScriptInstanceDataPtr p_instance, uint32_t *r_count) {
    if (r_count) {
        *r_count = 0;
    }
    return nullptr;
}

static void lua_script_instance_free_property_list(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionPropertyInfo *p_list, uint32_t p_count) {
}

static GDExtensionVariantType lua_script_instance_get_property_type(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionBool *r_is_valid) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        if (r_is_valid) {
            *r_is_valid = false;
        }
        return GDEXTENSION_VARIANT_TYPE_NIL;
    }
    StringName name = *reinterpret_cast<const StringName *>(p_name);
    Variant value = data->wrapper->get_script_property(name);
    bool valid = value.get_type() != Variant::NIL;
    if (r_is_valid) {
        *r_is_valid = valid;
    }
    return static_cast<GDExtensionVariantType>(value.get_type());
}

static GDExtensionBool lua_script_instance_property_can_revert(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name) {
    return false;
}

static GDExtensionBool lua_script_instance_property_get_revert(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {
    return false;
}

static GDExtensionObjectPtr lua_script_instance_get_owner(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    return data && data->owner ? data->owner->_owner : nullptr;
}

static void lua_script_instance_get_property_state(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionScriptInstancePropertyStateAdd p_add_func, void *p_userdata) {
}

static const GDExtensionMethodInfo *lua_script_instance_get_method_list(GDExtensionScriptInstanceDataPtr p_instance, uint32_t *r_count) {
    if (r_count) {
        *r_count = 0;
    }
    return nullptr;
}

static void lua_script_instance_free_method_list(GDExtensionScriptInstanceDataPtr p_instance, const GDExtensionMethodInfo *p_list, uint32_t p_count) {
}

static GDExtensionBool lua_script_instance_has_method(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        return false;
    }
    StringName name = *reinterpret_cast<const StringName *>(p_name);
    return data->wrapper->has_method(name);
}

static GDExtensionInt lua_script_instance_get_method_argument_count(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionBool *r_is_valid) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->script.is_null()) {
        if (r_is_valid) {
            *r_is_valid = false;
        }
        return -1;
    }
    StringName name = *reinterpret_cast<const StringName *>(p_name);
    Variant count = data->script->_get_script_method_argument_count(name);
    bool valid = count.get_type() != Variant::NIL;
    if (r_is_valid) {
        *r_is_valid = valid;
    }
    return valid ? int64_t(count) : -1;
}

static void lua_script_instance_call(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionConstStringNamePtr p_method, const GDExtensionConstVariantPtr *p_args, GDExtensionInt p_argument_count, GDExtensionVariantPtr r_return, GDExtensionCallError *r_error) {
    if (r_error) {
        r_error->error = GDEXTENSION_CALL_OK;
        r_error->argument = 0;
        r_error->expected = GDEXTENSION_VARIANT_TYPE_NIL;
    }

    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        if (r_error) {
            r_error->error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
        }
        lua_variant_copy_to(r_return, Variant());
        return;
    }

    StringName method = *reinterpret_cast<const StringName *>(p_method);
    Array args;
    for (GDExtensionInt i = 0; i < p_argument_count; i++) {
        Variant arg;
        internal::gdextension_interface_variant_new_copy(arg._native_ptr(), p_args[i]);
        args.push_back(arg);
    }

    Variant ret = data->wrapper->call_method(method, args);
    lua_variant_copy_to(r_return, ret);
}

static void lua_script_instance_notification2(GDExtensionScriptInstanceDataPtr p_instance, int32_t p_what, GDExtensionBool p_reversed) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->wrapper.is_null()) {
        return;
    }
    switch (p_what) {
        case 13: // Node::NOTIFICATION_READY
            data->wrapper->ready_notification();
            break;
        case 17: // Node::NOTIFICATION_PROCESS
            data->wrapper->process_notification(0.0);
            break;
        case 16: // Node::NOTIFICATION_PHYSICS_PROCESS
            data->wrapper->physics_process_notification(0.0);
            break;
        case 11: // Node::NOTIFICATION_EXIT_TREE
            data->wrapper->exit_tree_notification();
            break;
        default:
            break;
    }
}

static void lua_script_instance_to_string(GDExtensionScriptInstanceDataPtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr r_out) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    String text = data && data->script.is_valid() ? String("<LuaScriptInstance:") + data->script->get_path_hint() + String(">") : String("<LuaScriptInstance>");
    internal::gdextension_interface_string_new_with_latin1_chars(r_out, text.utf8().get_data());
    if (r_is_valid) {
        *r_is_valid = true;
    }
}

static Ref<Script> lua_script_instance_get_script_ref(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (!data || data->script.is_null()) {
        return Ref<Script>();
    }
    return data->script;
}

static GDExtensionObjectPtr lua_script_instance_get_script(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    return data && data->script.is_valid() ? data->script->_owner : nullptr;
}

static GDExtensionBool lua_script_instance_is_placeholder(GDExtensionScriptInstanceDataPtr p_instance) {
    return false;
}

static void *lua_script_instance_get(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    return data && data->wrapper.is_valid() ? data->wrapper.ptr() : nullptr;
}

static GDExtensionScriptLanguagePtr lua_script_instance_get_language(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaScriptLanguage *language = LuaScriptLanguage::get_singleton();
    return language ? language->_owner : nullptr;
}

static void lua_script_instance_free(GDExtensionScriptInstanceDataPtr p_instance) {
    LuaNativeScriptInstanceData *data = lua_native_data(p_instance);
    if (data) {
        if (data->wrapper.is_valid()) {
            data->wrapper->exit_tree_notification();
        }
        memdelete(data);
    }
}

static GDExtensionScriptInstanceInfo3 lua_script_instance_info = {
    lua_script_instance_set,
    lua_script_instance_get,
    lua_script_instance_get_property_list,
    lua_script_instance_free_property_list,
    nullptr,
    lua_script_instance_property_can_revert,
    lua_script_instance_property_get_revert,
    lua_script_instance_get_owner,
    lua_script_instance_get_property_state,
    lua_script_instance_get_method_list,
    lua_script_instance_free_method_list,
    lua_script_instance_get_property_type,
    nullptr,
    lua_script_instance_has_method,
    lua_script_instance_get_method_argument_count,
    lua_script_instance_call,
    lua_script_instance_notification2,
    lua_script_instance_to_string,
    nullptr,
    nullptr,
    lua_script_instance_get_script,
    lua_script_instance_is_placeholder,
    nullptr,
    nullptr,
    lua_script_instance_get_language,
    lua_script_instance_free,
};

static String parse_function_name_from_line(const String &p_line) {
    String line = strip_lua_comment(p_line).strip_edges();
    if (line.begins_with("local function ")) {
        line = line.substr(15, line.length() - 15).strip_edges();
    } else if (line.begins_with("function ")) {
        line = line.substr(9, line.length() - 9).strip_edges();
    } else {
        int eq_function = line.find("= function");
        if (eq_function < 0) {
            eq_function = line.find("=function");
        }
        if (eq_function >= 0) {
            line = line.substr(0, eq_function).strip_edges();
        } else {
            return String();
        }
    }

    int paren = line.find("(");
    if (paren >= 0) {
        line = line.substr(0, paren).strip_edges();
    }

    int dot = line.rfind(".");
    int colon = line.rfind(":");
    int sep = dot > colon ? dot : colon;
    if (sep >= 0) {
        line = line.substr(sep + 1, line.length() - sep - 1).strip_edges();
    }
    return line;
}

static int count_function_args(const String &p_line) {
    int open = p_line.find("(");
    int close = p_line.find(")", open + 1);
    if (open < 0 || close < open) {
        return -1;
    }
    String args = p_line.substr(open + 1, close - open - 1).strip_edges();
    if (args.is_empty()) {
        return 0;
    }
    return args.split(",").size();
}

void LuaScript::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_path_hint", "path"), &LuaScript::set_path_hint);
    ClassDB::bind_method(D_METHOD("get_path_hint"), &LuaScript::get_path_hint);
    ClassDB::bind_method(D_METHOD("load_from_file", "path"), &LuaScript::load_from_file);
    ClassDB::bind_method(D_METHOD("get_discovered_methods"), &LuaScript::get_discovered_methods);
}

LuaScript::LuaScript() {
}

void LuaScript::set_path_hint(const String &p_path) {
    source_path = p_path;
}

String LuaScript::get_path_hint() const {
    return source_path;
}

Error LuaScript::load_from_file(const String &p_path) {
    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::push_error(String("Lua script file not found: ") + p_path);
        return ERR_FILE_NOT_FOUND;
    }

    source_path = p_path;
    source_code = FileAccess::get_file_as_string(p_path);
    analyze_source();
    return valid ? OK : ERR_PARSE_ERROR;
}

Dictionary LuaScript::get_discovered_methods() const {
    return methods;
}

void LuaScript::analyze_source() {
    methods.clear();
    valid = !source_code.is_empty();
    tool_script = false;

    PackedStringArray lines = source_code.split("\n");
    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i].strip_edges();
        if (line.begins_with("--@tool") || line.begins_with("---@tool")) {
            tool_script = true;
        }

        String method_name = parse_function_name_from_line(line);
        if (!method_name.is_empty()) {
            methods[StringName(method_name)] = make_method_info(StringName(method_name), i, count_function_args(line));
        }
    }
}

bool LuaScript::_editor_can_reload_from_file() {
    return true;
}

void LuaScript::_placeholder_erased(void *p_placeholder) {
}

bool LuaScript::_can_instantiate() const {
    return valid;
}

Ref<Script> LuaScript::_get_base_script() const {
    return Ref<Script>();
}

StringName LuaScript::_get_global_name() const {
    if (!source_path.is_empty()) {
        return StringName(source_path.get_file().get_basename());
    }
    return StringName();
}

bool LuaScript::_inherits_script(const Ref<Script> &p_script) const {
    return false;
}

StringName LuaScript::_get_instance_base_type() const {
    return StringName("Node");
}

void *LuaScript::_instance_create(Object *p_for_object) const {
    if (!p_for_object || !valid) {
        return nullptr;
    }

    LuaNativeScriptInstanceData *data = memnew(LuaNativeScriptInstanceData);
    data->owner = p_for_object;
    data->script = Ref<LuaScript>(const_cast<LuaScript *>(this));
    data->wrapper.instantiate();

    Error err = data->wrapper->initialize(p_for_object, data->script);
    if (err != OK) {
        UtilityFunctions::push_error(String("Failed to initialize native Lua script instance: ") + String::num_int64(err));
        memdelete(data);
        return nullptr;
    }

    return internal::gdextension_interface_script_instance_create3(&lua_script_instance_info, data);
}

void *LuaScript::_placeholder_instance_create(Object *p_for_object) const {
    return nullptr;
}

bool LuaScript::_instance_has(Object *p_object) const {
    if (!p_object) {
        return false;
    }
    LuaScriptLanguage *language = LuaScriptLanguage::get_singleton();
    return language && internal::gdextension_interface_object_get_script_instance(p_object->_owner, language->_owner) != nullptr;
}

bool LuaScript::_has_source_code() const {
    return true;
}

String LuaScript::_get_source_code() const {
    return source_code;
}

void LuaScript::_set_source_code(const String &p_code) {
    source_code = p_code;
    analyze_source();
}

Error LuaScript::_reload(bool p_keep_state) {
    analyze_source();
    return valid ? OK : ERR_PARSE_ERROR;
}

StringName LuaScript::_get_doc_class_name() const {
    return StringName("LuaScript");
}

TypedArray<Dictionary> LuaScript::_get_documentation() const {
    return TypedArray<Dictionary>();
}

String LuaScript::_get_class_icon_path() const {
    return String();
}

bool LuaScript::_has_method(const StringName &p_method) const {
    return methods.has(p_method);
}

bool LuaScript::_has_static_method(const StringName &p_method) const {
    return false;
}

Variant LuaScript::_get_script_method_argument_count(const StringName &p_method) const {
    if (!methods.has(p_method)) {
        return Variant();
    }
    Dictionary info = methods[p_method];
    return info.get("argument_count", -1);
}

Dictionary LuaScript::_get_method_info(const StringName &p_method) const {
    if (!methods.has(p_method)) {
        return Dictionary();
    }
    return methods[p_method];
}

bool LuaScript::_is_tool() const {
    return tool_script;
}

bool LuaScript::_is_valid() const {
    return valid;
}

bool LuaScript::_is_abstract() const {
    return false;
}

ScriptLanguage *LuaScript::_get_language() const {
    return LuaScriptLanguage::get_singleton();
}

bool LuaScript::_has_script_signal(const StringName &p_signal) const {
    return false;
}

TypedArray<Dictionary> LuaScript::_get_script_signal_list() const {
    return TypedArray<Dictionary>();
}

bool LuaScript::_has_property_default_value(const StringName &p_property) const {
    return false;
}

Variant LuaScript::_get_property_default_value(const StringName &p_property) const {
    return Variant();
}

void LuaScript::_update_exports() {
    analyze_source();
}

TypedArray<Dictionary> LuaScript::_get_script_method_list() const {
    TypedArray<Dictionary> list;
    Array keys = methods.keys();
    for (int i = 0; i < keys.size(); i++) {
        list.push_back(methods[keys[i]]);
    }
    return list;
}

TypedArray<Dictionary> LuaScript::_get_script_property_list() const {
    return TypedArray<Dictionary>();
}

int32_t LuaScript::_get_member_line(const StringName &p_member) const {
    if (!methods.has(p_member)) {
        return -1;
    }
    Dictionary info = methods[p_member];
    return int32_t(info.get("line", -1));
}

Dictionary LuaScript::_get_constants() const {
    return Dictionary();
}

TypedArray<StringName> LuaScript::_get_members() const {
    TypedArray<StringName> members;
    Array keys = methods.keys();
    for (int i = 0; i < keys.size(); i++) {
        members.push_back(keys[i]);
    }
    return members;
}

bool LuaScript::_is_placeholder_fallback_enabled() const {
    return false;
}

Variant LuaScript::_get_rpc_config() const {
    return Dictionary();
}

} // namespace godot
