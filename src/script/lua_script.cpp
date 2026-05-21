#include <godot_lua/lua_script.hpp>
#include <godot_lua/lua_script_instance.hpp>
#include <godot_lua/lua_script_language.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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
    // GDExtension ScriptExtension::_instance_create must return an engine-owned
    // GDExtension script-instance handle created through the low-level C API.
    // The public LuaScriptInstance wrapper is exposed for host-controlled
    // lifecycle dispatch while the native ScriptInstance ABI is completed.
    return nullptr;
}

void *LuaScript::_placeholder_instance_create(Object *p_for_object) const {
    return nullptr;
}

bool LuaScript::_instance_has(Object *p_object) const {
    return false;
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
