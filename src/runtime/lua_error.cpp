#include <godot_lua/lua_error.hpp>

#include <godot_cpp/core/class_db.hpp>

namespace godot {

void LuaError::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_status", "status"), &LuaError::set_status);
    ClassDB::bind_method(D_METHOD("get_status"), &LuaError::get_status);
    ClassDB::bind_method(D_METHOD("set_message", "message"), &LuaError::set_message);
    ClassDB::bind_method(D_METHOD("get_message"), &LuaError::get_message);
    ClassDB::bind_method(D_METHOD("set_traceback", "traceback"), &LuaError::set_traceback);
    ClassDB::bind_method(D_METHOD("get_traceback"), &LuaError::get_traceback);
    ClassDB::bind_method(D_METHOD("to_string"), &LuaError::to_string);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "status"), "set_status", "get_status");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "set_message", "get_message");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "traceback"), "set_traceback", "get_traceback");
}

void LuaError::set_status(int p_status) {
    status = p_status;
}

int LuaError::get_status() const {
    return status;
}

void LuaError::set_message(const String &p_message) {
    message = p_message;
}

String LuaError::get_message() const {
    return message;
}

void LuaError::set_traceback(const String &p_traceback) {
    traceback = p_traceback;
}

String LuaError::get_traceback() const {
    return traceback;
}

String LuaError::to_string() const {
    if (traceback.is_empty()) {
        return vformat("LuaError(status=%d): %s", status, message);
    }
    return vformat("LuaError(status=%d): %s\n%s", status, message, traceback);
}

} // namespace godot
