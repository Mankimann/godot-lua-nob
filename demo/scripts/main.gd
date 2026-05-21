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

    var native_script := LuaScript.new()
    native_script.load_from_file("res://scripts/native_node.lua")

    var native_instance := LuaScriptInstance.new()
    var err := native_instance.initialize_from_file(self, "res://scripts/native_node.lua", {"spawn_count": 1})
    if err != OK:
        push_error("LuaScriptInstance initialization failed: %s" % err)
        return

    native_instance.ready_notification()
    native_instance.process_notification(0.016)
    print(native_instance.call_method("greet", ["Godot"]))
    print("Native LuaScript diagnostics: ", native_instance.diagnostics())

    var direct_node := Node.new()
    direct_node.name = "DirectLuaAttachedNode"
    var direct_script := LuaScript.new()
    direct_script.load_from_file("res://scripts/native_node.lua")
    direct_node.set_script(direct_script)
    direct_node.set_process(true)
    direct_node.set_physics_process(true)
    add_child(direct_node)
    print("Direct Lua attach call: ", direct_node.call("greet", "DirectAttach"))

    var host := LuaScriptHost.new()
    host.lua_script_path = "res://scripts/native_node.lua"
    host.forward_process = false
    add_child(host)
    print("LuaScriptHost fallback added: ", host.name)
