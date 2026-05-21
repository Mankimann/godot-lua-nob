#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class LuaError : public RefCounted {
    GDCLASS(LuaError, RefCounted)

    int status = 0;
    String message;
    String traceback;

protected:
    static void _bind_methods();

public:
    LuaError() = default;
    ~LuaError() override = default;

    void set_status(int p_status);
    int get_status() const;

    void set_message(const String &p_message);
    String get_message() const;

    void set_traceback(const String &p_traceback);
    String get_traceback() const;

    String to_string() const;
};

} // namespace godot
