extends Node

func _ready() -> void:
    var lua := LuaState.new()
    lua.open_with_policy(LuaState.POLICY_GAMEPLAY)
    lua.add_module_root("res://scripts")
    lua.jit_optimize("hotloop=56,hotexit=10")
    lua.set_global("godot_node", self)

    var result = lua.do_file("res://scripts/hello.lua")
    if result is LuaError:
        push_error(result.to_string())
        return

    print("Lua returned: ", result)

    var tick = lua.call_global("lua_tick", [0.016])
    print("Lua tick result: ", tick)

    var connected = lua.call_global("connect_demo", [self])
    print("Lua signal connection: ", connected)

    print("Lua diagnostics: ", lua.diagnostics())
