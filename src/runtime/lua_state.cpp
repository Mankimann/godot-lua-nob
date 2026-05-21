#include <godot_lua/lua_state.hpp>
#include <godot_lua/lua_error.hpp>
#include <godot_lua/lua_variant_bridge.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

namespace godot {

static int traceback_handler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
            return 1;
        }
        msg = "<non-string Lua error>";
    }
    luaL_traceback(L, L, msg, 1);
    return 1;
}

LuaState::LuaState() {
    L = luaL_newstate();
}

LuaState::~LuaState() {
    close();
}

void LuaState::_bind_methods() {
    ClassDB::bind_method(D_METHOD("is_valid"), &LuaState::is_valid);
    ClassDB::bind_method(D_METHOD("is_sandboxed"), &LuaState::is_sandboxed);
    ClassDB::bind_method(D_METHOD("open_libraries", "sandboxed"), &LuaState::open_libraries, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("close"), &LuaState::close);
    ClassDB::bind_method(D_METHOD("reset"), &LuaState::reset);
    ClassDB::bind_method(D_METHOD("do_string", "code", "chunk_name"), &LuaState::do_string, DEFVAL("chunk"));
    ClassDB::bind_method(D_METHOD("do_file", "path"), &LuaState::do_file);
    ClassDB::bind_method(D_METHOD("get_global", "name"), &LuaState::get_global);
    ClassDB::bind_method(D_METHOD("set_global", "name", "value"), &LuaState::set_global);
    ClassDB::bind_method(D_METHOD("call_global", "name", "args"), &LuaState::call_global, DEFVAL(Array()));
    ClassDB::bind_method(D_METHOD("diagnostics"), &LuaState::diagnostics);
}

bool LuaState::is_valid() const {
    return L != nullptr;
}

bool LuaState::is_sandboxed() const {
    return sandboxed;
}

void LuaState::open_libraries(bool p_sandboxed) {
    if (!L) {
        L = luaL_newstate();
    }
    if (!libraries_opened) {
        luaL_openlibs(L);
        libraries_opened = true;
    }
    sandboxed = p_sandboxed;
    lua_bridge::install(L, sandboxed);
}

void LuaState::close() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
    libraries_opened = false;
    sandboxed = false;
}

void LuaState::reset() {
    close();
    L = luaL_newstate();
}

Variant LuaState::make_error(int p_status, const String &p_prefix) const {
    Ref<LuaError> err;
    err.instantiate();
    err->set_status(p_status);
    String message = L ? lua_bridge::read_lua_error(L, -1) : String("Lua state is closed");
    if (!p_prefix.is_empty()) {
        message = p_prefix + ": " + message;
    }
    err->set_message(message);
    err->set_traceback(message);
    if (L && lua_gettop(L) > 0) {
        lua_pop(L, 1);
    }
    return err;
}

Variant LuaState::finish_protected_call(int p_status, int p_result_count) {
    if (p_status != LUA_OK) {
        return make_error(p_status);
    }
    if (p_result_count <= 0) {
        return Variant();
    }
    if (p_result_count == 1) {
        Variant value = lua_bridge::read_variant(L, -1);
        lua_pop(L, 1);
        return value;
    }
    Array results;
    int first = lua_gettop(L) - p_result_count + 1;
    for (int i = 0; i < p_result_count; ++i) {
        results.push_back(lua_bridge::read_variant(L, first + i));
    }
    lua_pop(L, p_result_count);
    return results;
}

Variant LuaState::do_string(const String &p_code, const String &p_chunk_name) {
    if (!L) {
        return make_error(LUA_ERRERR, "do_string");
    }
    if (!libraries_opened) {
        open_libraries(false);
    }

    CharString code = p_code.utf8();
    CharString chunk = ("=" + p_chunk_name).utf8();

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback_handler);
    int error_handler = lua_gettop(L);

    int status = luaL_loadbufferx(L, code.get_data(), code.length(), chunk.get_data(), "t");
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
        err->set_message("Lua file not found: " + p_path);
        return err;
    }
    String source = FileAccess::get_file_as_string(p_path);
    return do_string(source, p_path);
}

Variant LuaState::get_global(const String &p_name) {
    if (!L) {
        return Variant();
    }
    CharString name = p_name.utf8();
    lua_getglobal(L, name.get_data());
    Variant value = lua_bridge::read_variant(L, -1);
    lua_pop(L, 1);
    return value;
}

void LuaState::set_global(const String &p_name, const Variant &p_value) {
    if (!L) {
        return;
    }
    CharString name = p_name.utf8();
    lua_bridge::push_variant(L, p_value);
    lua_setglobal(L, name.get_data());
}

Variant LuaState::call_global(const String &p_name, const Array &p_args) {
    if (!L) {
        return make_error(LUA_ERRERR, "call_global");
    }
    if (!libraries_opened) {
        open_libraries(false);
    }
    CharString name = p_name.utf8();
    lua_getglobal(L, name.get_data());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        Ref<LuaError> err;
        err.instantiate();
        err->set_status(LUA_ERRRUN);
        err->set_message("Lua global is not a function: " + p_name);
        return err;
    }

    lua_pushcfunction(L, traceback_handler);
    int error_handler = lua_gettop(L) - 1;
    lua_insert(L, error_handler);

    for (int64_t i = 0; i < p_args.size(); ++i) {
        lua_bridge::push_variant(L, p_args[i]);
    }
    int status = lua_pcall(L, (int)p_args.size(), LUA_MULTRET, error_handler);
    int result_count = lua_gettop(L) - error_handler;
    lua_remove(L, error_handler);
    if (status != LUA_OK) {
        return make_error(status, "runtime");
    }
    return finish_protected_call(status, result_count);
}

Dictionary LuaState::diagnostics() const {
    Dictionary d;
    d["valid"] = L != nullptr;
    d["libraries_opened"] = libraries_opened;
    d["sandboxed"] = sandboxed;
    d["lua_version"] = String(LUA_VERSION);
    d["lua_release"] = String(LUA_RELEASE);
    if (L) {
        d["stack_top"] = lua_gettop(L);
    }
    return d;
}

} // namespace godot
