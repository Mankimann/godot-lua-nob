extends Node

func _ready() -> void:
    var lua := LuaState.new()
    lua.open_libraries(true)
    lua.set_global("godot_node", self)

    var result = lua.do_file("res://scripts/hello.lua")
    if result is LuaError:
        push_error(result.to_string())
        return

    print("Lua returned: ", result)
    print("Lua diagnostics: ", lua.diagnostics())
