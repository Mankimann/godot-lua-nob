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
    native_script.set_path_hint("res://scripts/native_node.lua")
    native_script.set_source_code(FileAccess.get_file_as_string("res://scripts/native_node.lua"))
    native_script.reload(false)

    var native_instance := LuaScriptInstance.new()
    var err := native_instance.initialize(self, native_script)
    if err != OK:
        push_error("LuaScriptInstance initialization failed: %s" % err)
        return

    native_instance.ready_notification()
    print(native_instance.call_method("greet", ["Godot"]))
    print("Native LuaScript methods: ", native_script.get_discovered_methods())
