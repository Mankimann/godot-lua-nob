#include <godot_lua/lua_script_language.hpp>
#include <godot_lua/lua_script.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace godot {

LuaScriptLanguage *LuaScriptLanguage::singleton = nullptr;

static Dictionary make_lua_method_info(const StringName &p_name, int32_t p_line = -1) {
    Dictionary info;
    info["name"] = p_name;
    info["args"] = Array();
    info["default_args"] = Array();
    info["flags"] = 0;
    info["id"] = 0;
    info["return"] = Dictionary();
    if (p_line >= 0) {
        info["line"] = p_line;
    }
    return info;
}

static int find_lua_function_line(const String &p_function, const String &p_code) {
    PackedStringArray lines = p_code.split("\n");
    const String needle_a = String("function ") + p_function + String("(");
    const String needle_b = p_function + String(" = function(");
    const String needle_c = p_function + String("=function(");

    for (int i = 0; i < lines.size(); i++) {
        String line = lines[i].strip_edges();
        if (line.begins_with("--")) {
            continue;
        }
        if (line.find(needle_a) >= 0 || line.find(needle_b) >= 0 || line.find(needle_c) >= 0) {
            return i;
        }
    }
    return -1;
}

void LuaScriptLanguage::_bind_methods() {
}

LuaScriptLanguage::LuaScriptLanguage() {
    if (singleton == nullptr) {
        singleton = this;
    }
}

LuaScriptLanguage::~LuaScriptLanguage() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

LuaScriptLanguage *LuaScriptLanguage::get_singleton() {
    return singleton;
}

LuaScriptLanguage *LuaScriptLanguage::create_singleton() {
    if (singleton == nullptr) {
        singleton = memnew(LuaScriptLanguage);
    }
    return singleton;
}

void LuaScriptLanguage::destroy_singleton() {
    if (singleton != nullptr) {
        LuaScriptLanguage *lang = singleton;
        singleton = nullptr;
        memdelete(lang);
    }
}

String LuaScriptLanguage::_get_name() const {
    return "Lua";
}

void LuaScriptLanguage::_init() {
}

String LuaScriptLanguage::_get_type() const {
    return "Lua";
}

String LuaScriptLanguage::_get_extension() const {
    return "lua";
}

void LuaScriptLanguage::_finish() {
}

PackedStringArray LuaScriptLanguage::_get_reserved_words() const {
    PackedStringArray words;
    const char *lua_words[] = {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while", nullptr
    };
    for (int i = 0; lua_words[i] != nullptr; i++) {
        words.push_back(lua_words[i]);
    }
    return words;
}

bool LuaScriptLanguage::_is_control_flow_keyword(const String &p_keyword) const {
    return p_keyword == "if" || p_keyword == "elseif" || p_keyword == "else" || p_keyword == "for" || p_keyword == "while" || p_keyword == "repeat" || p_keyword == "until" || p_keyword == "break" || p_keyword == "return";
}

PackedStringArray LuaScriptLanguage::_get_comment_delimiters() const {
    PackedStringArray delimiters;
    delimiters.push_back("--");
    delimiters.push_back("--[[ ]] ");
    return delimiters;
}

PackedStringArray LuaScriptLanguage::_get_doc_comment_delimiters() const {
    PackedStringArray delimiters;
    delimiters.push_back("---");
    return delimiters;
}

PackedStringArray LuaScriptLanguage::_get_string_delimiters() const {
    PackedStringArray delimiters;
    delimiters.push_back("\" \"");
    delimiters.push_back("' '");
    delimiters.push_back("[[ ]] ");
    return delimiters;
}

Ref<Script> LuaScriptLanguage::_make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
    Ref<LuaScript> script;
    script.instantiate();

    String code = p_template;
    if (code.is_empty()) {
        code = String("-- Lua script for ") + p_class_name + String("\n");
        code += String("-- Attach this script to a ") + p_base_class_name + String("-derived node.\n\n");
        code += "function _ready()\n";
        code += String("    godot.print('Lua script ready: ") + p_class_name + String("')\n");
        code += "end\n\n";
        code += "function _process(delta)\n";
        code += "    -- Per-frame gameplay logic.\n";
        code += "end\n";
    }
    script->_set_source_code(code);
    return script;
}

TypedArray<Dictionary> LuaScriptLanguage::_get_built_in_templates(const StringName &p_object) const {
    TypedArray<Dictionary> templates;
    Dictionary base;
    base["inherit"] = String(p_object);
    base["name"] = "Default";
    base["description"] = "LuaJIT gameplay script with _ready and _process hooks.";
    base["content"] = "function _ready()\n    godot.print('Lua ready')\nend\n\nfunction _process(delta)\nend\n";
    templates.push_back(base);
    return templates;
}

bool LuaScriptLanguage::_is_using_templates() {
    return true;
}

Dictionary LuaScriptLanguage::_validate(const String &p_script, const String &p_path, bool p_validate_functions, bool p_validate_errors, bool p_validate_warnings, bool p_validate_safe_lines) const {
    Dictionary result;
    Array errors;
    Array warnings;
    Array safe_lines;

    CharString utf8 = p_script.utf8();
    lua_State *L = luaL_newstate();
    int status = luaL_loadbuffer(L, utf8.get_data(), utf8.length(), p_path.utf8().get_data());
    if (status != LUA_OK) {
        Dictionary error;
        error["line"] = 0;
        error["column"] = 0;
        error["message"] = String(lua_tostring(L, -1));
        errors.push_back(error);
    }
    lua_close(L);

    result["valid"] = errors.is_empty();
    result["errors"] = errors;
    result["warnings"] = warnings;
    result["safe_lines"] = safe_lines;
    return result;
}

String LuaScriptLanguage::_validate_path(const String &p_path) const {
    if (!p_path.ends_with(".lua")) {
        return "Lua scripts must use the .lua extension.";
    }
    return String();
}

Object *LuaScriptLanguage::_create_script() const {
    return memnew(LuaScript);
}

bool LuaScriptLanguage::_has_named_classes() const {
    return false;
}

bool LuaScriptLanguage::_supports_builtin_mode() const {
    return true;
}

bool LuaScriptLanguage::_supports_documentation() const {
    return false;
}

bool LuaScriptLanguage::_can_inherit_from_file() const {
    return false;
}

int32_t LuaScriptLanguage::_find_function(const String &p_function, const String &p_code) const {
    return find_lua_function_line(p_function, p_code);
}

String LuaScriptLanguage::_make_function(const String &p_class_name, const String &p_function_name, const PackedStringArray &p_function_args) const {
    String code = String("function ") + p_function_name + String("(");
    for (int i = 0; i < p_function_args.size(); i++) {
        if (i > 0) {
            code += ", ";
        }
        code += p_function_args[i];
    }
    code += ")\n";
    code += String("    -- TODO: implement ") + p_function_name + String("\n");
    code += "end\n";
    return code;
}

bool LuaScriptLanguage::_can_make_function() const {
    return true;
}

Error LuaScriptLanguage::_open_in_external_editor(const Ref<Script> &p_script, int32_t p_line, int32_t p_column) {
    return ERR_UNAVAILABLE;
}

bool LuaScriptLanguage::_overrides_external_editor() {
    return false;
}

ScriptLanguage::ScriptNameCasing LuaScriptLanguage::_preferred_file_name_casing() const {
    return ScriptLanguage::SCRIPT_NAME_CASING_SNAKE_CASE;
}

Dictionary LuaScriptLanguage::_complete_code(const String &p_code, const String &p_path, Object *p_owner) const {
    Dictionary result;
    result["matches"] = Array();
    result["force"] = false;
    result["call_hint"] = String();
    return result;
}

Dictionary LuaScriptLanguage::_lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner) const {
    return Dictionary();
}

String LuaScriptLanguage::_auto_indent_code(const String &p_code, int32_t p_from_line, int32_t p_to_line) const {
    return p_code;
}

void LuaScriptLanguage::_add_global_constant(const StringName &p_name, const Variant &p_value) {
}

void LuaScriptLanguage::_add_named_global_constant(const StringName &p_name, const Variant &p_value) {
}

void LuaScriptLanguage::_remove_named_global_constant(const StringName &p_name) {
}

void LuaScriptLanguage::_thread_enter() {
}

void LuaScriptLanguage::_thread_exit() {
}

String LuaScriptLanguage::_debug_get_error() const {
    return String();
}

int32_t LuaScriptLanguage::_debug_get_stack_level_count() const {
    return 0;
}

int32_t LuaScriptLanguage::_debug_get_stack_level_line(int32_t p_level) const {
    return -1;
}

String LuaScriptLanguage::_debug_get_stack_level_function(int32_t p_level) const {
    return String();
}

String LuaScriptLanguage::_debug_get_stack_level_source(int32_t p_level) const {
    return String();
}

Dictionary LuaScriptLanguage::_debug_get_stack_level_locals(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) {
    return Dictionary();
}

Dictionary LuaScriptLanguage::_debug_get_stack_level_members(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) {
    return Dictionary();
}

void *LuaScriptLanguage::_debug_get_stack_level_instance(int32_t p_level) {
    return nullptr;
}

Dictionary LuaScriptLanguage::_debug_get_globals(int32_t p_max_subitems, int32_t p_max_depth) {
    return Dictionary();
}

String LuaScriptLanguage::_debug_parse_stack_level_expression(int32_t p_level, const String &p_expression, int32_t p_max_subitems, int32_t p_max_depth) {
    return String();
}

TypedArray<Dictionary> LuaScriptLanguage::_debug_get_current_stack_info() {
    return TypedArray<Dictionary>();
}

void LuaScriptLanguage::_reload_all_scripts() {
}

void LuaScriptLanguage::_reload_scripts(const Array &p_scripts, bool p_soft_reload) {
    for (int i = 0; i < p_scripts.size(); i++) {
        Ref<LuaScript> script = p_scripts[i];
        if (script.is_valid()) {
            script->_reload(p_soft_reload);
        }
    }
}

void LuaScriptLanguage::_reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
    Ref<LuaScript> script = p_script;
    if (script.is_valid()) {
        script->_reload(p_soft_reload);
    }
}

PackedStringArray LuaScriptLanguage::_get_recognized_extensions() const {
    PackedStringArray extensions;
    extensions.push_back("lua");
    return extensions;
}

TypedArray<Dictionary> LuaScriptLanguage::_get_public_functions() const {
    TypedArray<Dictionary> functions;
    functions.push_back(make_lua_method_info("_ready"));
    functions.push_back(make_lua_method_info("_process"));
    functions.push_back(make_lua_method_info("_physics_process"));
    functions.push_back(make_lua_method_info("_input"));
    functions.push_back(make_lua_method_info("_unhandled_input"));
    return functions;
}

Dictionary LuaScriptLanguage::_get_public_constants() const {
    return Dictionary();
}

TypedArray<Dictionary> LuaScriptLanguage::_get_public_annotations() const {
    return TypedArray<Dictionary>();
}

void LuaScriptLanguage::_profiling_start() {
}

void LuaScriptLanguage::_profiling_stop() {
}

void LuaScriptLanguage::_profiling_set_save_native_calls(bool p_enable) {
}

int32_t LuaScriptLanguage::_profiling_get_accumulated_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) {
    return 0;
}

int32_t LuaScriptLanguage::_profiling_get_frame_data(ScriptLanguageExtensionProfilingInfo *p_info_array, int32_t p_info_max) {
    return 0;
}

void LuaScriptLanguage::_frame() {
}

bool LuaScriptLanguage::_handles_global_class_type(const String &p_type) const {
    return p_type == "Lua";
}

Dictionary LuaScriptLanguage::_get_global_class_name(const String &p_path) const {
    Dictionary result;
    if (p_path.ends_with(".lua")) {
        result["name"] = p_path.get_file().get_basename();
        result["base_type"] = "Node";
        result["language"] = "Lua";
        result["path"] = p_path;
    }
    return result;
}

} // namespace godot
