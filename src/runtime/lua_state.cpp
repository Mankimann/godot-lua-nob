#include <godot_lua/lua_state.hpp>
#include <godot_lua/lua_error.hpp>
#include <godot_lua/lua_variant_bridge.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lualib.h>
}

namespace godot {

static int traceback_handler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr) msg = "<non-string Lua error>";
    luaL_traceback(L, L, msg, 1);
    return 1;
}

static int policy_to_flags(LuaState::RuntimePolicy policy, bool ffi_enabled, bool bytecode_enabled) {
    int flags = 0;
    switch (policy) {
        case LuaState::POLICY_TRUSTED:
            flags |= lua_bridge::INSTALL_ALLOW_NATIVE_LOADLIB;
            flags |= lua_bridge::INSTALL_ALLOW_BYTECODE;
            if (ffi_enabled) flags |= lua_bridge::INSTALL_ALLOW_FFI;
            break;
        case LuaState::POLICY_GAMEPLAY:
            flags |= lua_bridge::INSTALL_ALLOW_BYTECODE;
            if (ffi_enabled) flags |= lua_bridge::INSTALL_ALLOW_FFI;
            break;
        case LuaState::POLICY_MODDING:
            flags |= lua_bridge::INSTALL_SANDBOXED;
            if (bytecode_enabled) flags |= lua_bridge::INSTALL_ALLOW_BYTECODE;
            break;
        case LuaState::POLICY_SANDBOXED:
        default:
            flags |= lua_bridge::INSTALL_SANDBOXED;
            break;
    }
    if (bytecode_enabled) flags |= lua_bridge::INSTALL_ALLOW_BYTECODE;
    return flags;
}

LuaState::LuaState() {
    L = luaL_newstate();
    module_roots.push_back("res://scripts");
    module_roots.push_back("res://lua");
}

LuaState::~LuaState() {
    close();
}

void LuaState::_bind_methods() {
    ClassDB::bind_method(D_METHOD("is_valid"), &LuaState::is_valid);
    ClassDB::bind_method(D_METHOD("is_sandboxed"), &LuaState::is_sandboxed);
    ClassDB::bind_method(D_METHOD("open_libraries", "sandboxed"), &LuaState::open_libraries, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("open_with_policy", "policy"), &LuaState::open_with_policy, DEFVAL(POLICY_GAMEPLAY));
    ClassDB::bind_method(D_METHOD("close"), &LuaState::close);
    ClassDB::bind_method(D_METHOD("reset"), &LuaState::reset);
    ClassDB::bind_method(D_METHOD("set_policy", "policy"), &LuaState::set_policy);
    ClassDB::bind_method(D_METHOD("get_policy"), &LuaState::get_policy);
    ClassDB::bind_method(D_METHOD("set_ffi_enabled", "enabled"), &LuaState::set_ffi_enabled);
    ClassDB::bind_method(D_METHOD("is_ffi_enabled"), &LuaState::is_ffi_enabled);
    ClassDB::bind_method(D_METHOD("set_jit_enabled", "enabled"), &LuaState::set_jit_enabled);
    ClassDB::bind_method(D_METHOD("is_jit_enabled"), &LuaState::is_jit_enabled);
    ClassDB::bind_method(D_METHOD("jit_flush"), &LuaState::jit_flush);
    ClassDB::bind_method(D_METHOD("jit_optimize", "options"), &LuaState::jit_optimize, DEFVAL("hotloop=56,hotexit=10"));
    ClassDB::bind_method(D_METHOD("set_bytecode_enabled", "enabled"), &LuaState::set_bytecode_enabled);
    ClassDB::bind_method(D_METHOD("is_bytecode_enabled"), &LuaState::is_bytecode_enabled);
    ClassDB::bind_method(D_METHOD("add_module_root", "root"), &LuaState::add_module_root);
    ClassDB::bind_method(D_METHOD("clear_module_roots"), &LuaState::clear_module_roots);
    ClassDB::bind_method(D_METHOD("get_module_roots"), &LuaState::get_module_roots);
    ClassDB::bind_method(D_METHOD("do_string", "code", "chunk_name"), &LuaState::do_string, DEFVAL("chunk"));
    ClassDB::bind_method(D_METHOD("do_file", "path"), &LuaState::do_file);
    ClassDB::bind_method(D_METHOD("require_module", "name"), &LuaState::require_module);
    ClassDB::bind_method(D_METHOD("unload_module", "name"), &LuaState::unload_module);
    ClassDB::bind_method(D_METHOD("reload_changed_files"), &LuaState::reload_changed_files);
    ClassDB::bind_method(D_METHOD("get_global", "name"), &LuaState::get_global);
    ClassDB::bind_method(D_METHOD("set_global", "name", "value"), &LuaState::set_global);
    ClassDB::bind_method(D_METHOD("call_global", "name", "args"), &LuaState::call_global, DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("diagnostics"), &LuaState::diagnostics);

    BIND_ENUM_CONSTANT(POLICY_TRUSTED);
    BIND_ENUM_CONSTANT(POLICY_GAMEPLAY);
    BIND_ENUM_CONSTANT(POLICY_SANDBOXED);
    BIND_ENUM_CONSTANT(POLICY_MODDING);
}

bool LuaState::is_valid() const { return L != nullptr; }
bool LuaState::is_sandboxed() const { return sandboxed; }
int LuaState::get_policy() const { return (int)policy; }
bool LuaState::is_ffi_enabled() const { return ffi_enabled; }
bool LuaState::is_jit_enabled() const { return jit_enabled; }
bool LuaState::is_bytecode_enabled() const { return bytecode_enabled; }
Array LuaState::get_module_roots() const { return module_roots; }

void LuaState::install_runtime() {
    if (!L) return;
    sandboxed = policy == POLICY_SANDBOXED || policy == POLICY_MODDING;
    lua_bridge::install(L, policy_to_flags(policy, ffi_enabled, bytecode_enabled), module_roots);
    set_jit_enabled(jit_enabled);
}

void LuaState::apply_policy() {
    if (L && libraries_opened) install_runtime();
}

void LuaState::open_libraries(bool p_sandboxed) {
    open_with_policy(p_sandboxed ? POLICY_SANDBOXED : POLICY_GAMEPLAY);
}

void LuaState::open_with_policy(int p_policy) {
    if (!L) L = luaL_newstate();
    policy = (RuntimePolicy)p_policy;
    sandboxed = policy == POLICY_SANDBOXED || policy == POLICY_MODDING;
    if (policy == POLICY_TRUSTED) {
        ffi_enabled = true;
        bytecode_enabled = true;
    } else if (policy == POLICY_SANDBOXED || policy == POLICY_MODDING) {
        ffi_enabled = false;
        bytecode_enabled = false;
    }
    if (!libraries_opened) {
        luaL_openlibs(L);
        libraries_opened = true;
    }
    install_runtime();
}

void LuaState::close() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    libraries_opened = false;
    sandboxed = false;
    loaded_files.clear();
}

void LuaState::reset() {
    close();
    L = luaL_newstate();
    open_with_policy(policy);
}

void LuaState::set_policy(int p_policy) {
    policy = (RuntimePolicy)p_policy;
    apply_policy();
}

void LuaState::set_ffi_enabled(bool p_enabled) {
    ffi_enabled = p_enabled;
    apply_policy();
}

void LuaState::set_jit_enabled(bool p_enabled) {
    jit_enabled = p_enabled;
    if (!L || !libraries_opened) return;
    lua_getglobal(L, "require");
    lua_pushliteral(L, "jit");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return;
    }
    lua_getfield(L, -1, p_enabled ? "on" : "off");
    if (lua_isfunction(L, -1)) {
        lua_pushboolean(L, true);
        lua_pcall(L, 1, 0, 0);
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

Variant LuaState::jit_flush() {
    if (!L) return make_error(LUA_ERRERR, "jit_flush");
    return do_string("local jit=require('jit'); jit.flush(); return true", "jit_flush");
}

Variant LuaState::jit_optimize(const String &p_options) {
    if (!L) return make_error(LUA_ERRERR, "jit_optimize");
    String code = "local jit=require('jit'); jit.opt.start('" + p_options.replace("'", "") + "'); return true";
    return do_string(code, "jit_optimize");
}

void LuaState::set_bytecode_enabled(bool p_enabled) {
    bytecode_enabled = p_enabled;
    apply_policy();
}

void LuaState::add_module_root(const String &p_root) {
    if (!module_roots.has(p_root)) module_roots.push_back(p_root);
    apply_policy();
}

void LuaState::clear_module_roots() {
    module_roots.clear();
    apply_policy();
}

Variant LuaState::make_error(int p_status, const String &p_prefix) const {
    Ref<LuaError> err;
    err.instantiate();
    err->set_status(p_status);
    String message = L ? lua_bridge::read_lua_error(L, -1) : String("LuaJIT state is closed");
    if (!p_prefix.is_empty()) message = p_prefix + ": " + message;
    err->set_message(message);
    err->set_traceback(message);
    if (L && lua_gettop(L) > 0) lua_pop(L, 1);
    return err;
}

Variant LuaState::finish_protected_call(int p_status, int p_result_count) {
    if (p_status != LUA_OK) return make_error(p_status);
    if (p_result_count <= 0) return Variant();
    if (p_result_count == 1) {
        Variant value = lua_bridge::read_variant(L, -1);
        lua_pop(L, 1);
        return value;
    }
    Array results;
    int first = lua_gettop(L) - p_result_count + 1;
    for (int i = 0; i < p_result_count; ++i) results.push_back(lua_bridge::read_variant(L, first + i));
    lua_pop(L, p_result_count);
    return results;
}

Variant LuaState::do_string(const String &p_code, const String &p_chunk_name) {
    if (!L) return make_error(LUA_ERRERR, "do_string");
    if (!libraries_opened) open_with_policy(policy);

    CharString code = p_code.utf8();
    CharString chunk = ("=" + p_chunk_name).utf8();
    const char *mode = bytecode_enabled ? "bt" : "t";

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback_handler);
    int error_handler = lua_gettop(L);

    int status = luaL_loadbufferx(L, code.get_data(), code.length(), chunk.get_data(), mode);
    if (status != LUA_OK) {
        lua_remove(L, error_handler);
        return make_error(status, "load");
    }

    status = lua_pcall(L, 0, LUA_MULTRET, error_handler);
    int result_count = lua_gettop(L) - error_handler;
    lua_remove(L, error_handler);
    if (status != LUA_OK) {
        lua_settop(L, base + 1);
        return make_error(status, "runtime");
    }
    return finish_protected_call(status, result_count);
}

Variant LuaState::do_file(const String &p_path) {
    if (!FileAccess::file_exists(p_path)) {
        Ref<LuaError> err;
        err.instantiate();
        err->set_status(LUA_ERRFILE);
        err->set_message("LuaJIT file not found: " + p_path);
        return err;
    }
    String source = FileAccess::get_file_as_string(p_path);
    Variant result = do_string(source, p_path);
    if (!result.is_ref_counted()) loaded_files[p_path] = FileAccess::get_modified_time(p_path);
    return result;
}

Variant LuaState::require_module(const String &p_name) {
    if (!L) return make_error(LUA_ERRERR, "require_module");
    if (!libraries_opened) open_with_policy(policy);
    lua_getglobal(L, "require");
    CharString name = p_name.utf8();
    lua_pushstring(L, name.get_data());
    lua_pushcfunction(L, traceback_handler);
    int error_handler = lua_gettop(L) - 2;
    lua_insert(L, error_handler);
    int status = lua_pcall(L, 1, 1, error_handler);
    lua_remove(L, error_handler);
    if (status != LUA_OK) return make_error(status, "require");
    return finish_protected_call(status, 1);
}

void LuaState::unload_module(const String &p_name) {
    if (!L) return;
    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            CharString name = p_name.utf8();
            lua_pushnil(L);
            lua_setfield(L, -2, name.get_data());
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void LuaState::reload_changed_files() {
    Array keys = loaded_files.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        String path = keys[i];
        uint64_t old_time = loaded_files[path];
        uint64_t new_time = FileAccess::get_modified_time(path);
        if (new_time != old_time) {
            do_file(path);
            loaded_files[path] = new_time;
        }
    }
}

Variant LuaState::get_global(const String &p_name) {
    if (!L) return Variant();
    CharString name = p_name.utf8();
    lua_getglobal(L, name.get_data());
    Variant value = lua_bridge::read_variant(L, -1);
    lua_pop(L, 1);
    return value;
}

void LuaState::set_global(const String &p_name, const Variant &p_value) {
    if (!L) return;
    CharString name = p_name.utf8();
    lua_bridge::push_variant(L, p_value);
    lua_setglobal(L, name.get_data());
}

Variant LuaState::call_global(const String &p_name, const Array &p_args) {
    if (!L) return make_error(LUA_ERRERR, "call_global");
    if (!libraries_opened) open_with_policy(policy);
    CharString name = p_name.utf8();
    lua_getglobal(L, name.get_data());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        Ref<LuaError> err;
        err.instantiate();
        err->set_status(LUA_ERRRUN);
        err->set_message("LuaJIT global is not a function: " + p_name);
        return err;
    }

    lua_pushcfunction(L, traceback_handler);
    int error_handler = lua_gettop(L) - 1;
    lua_insert(L, error_handler);

    for (int64_t i = 0; i < p_args.size(); ++i) lua_bridge::push_variant(L, p_args[i]);
    int status = lua_pcall(L, (int)p_args.size(), LUA_MULTRET, error_handler);
    int result_count = lua_gettop(L) - error_handler;
    lua_remove(L, error_handler);
    if (status != LUA_OK) return make_error(status, "runtime");
    return finish_protected_call(status, result_count);
}

Dictionary LuaState::diagnostics() const {
    Dictionary d;
    d["valid"] = L != nullptr;
    d["libraries_opened"] = libraries_opened;
    d["sandboxed"] = sandboxed;
    d["policy"] = (int)policy;
    d["ffi_enabled"] = ffi_enabled;
    d["jit_enabled"] = jit_enabled;
    d["bytecode_enabled"] = bytecode_enabled;
    d["lua_runtime"] = String("LuaJIT");
    d["lua_version"] = String(LUA_VERSION);
    d["luajit_version"] = String(LUAJIT_VERSION);
    d["module_roots"] = module_roots;
    d["loaded_files"] = loaded_files;
    if (L) d["stack_top"] = lua_gettop(L);
    return d;
}

} // namespace godot
