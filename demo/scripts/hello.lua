godot.print("Hallo aus LuaJIT in Godot")

godot.print("jit status:", jit and jit.status())

local math_util = require("game.math_util")
godot.print("math_util.add:", math_util.add(20, 22))

if godot_node then
    godot_node.name = "NodeRenamedFromLuaJIT"
    godot.print("Node name via LuaJIT:", godot_node.name)
end

local v = Vector2(10, 20)
godot.print("Vector2 from Lua:", v.x, v.y)

local engine = godot.get_singleton("Engine")
godot.print("Engine singleton valid:", godot.is_instance_valid(engine))

function lua_tick(delta)
    if delta then
        return delta * 2
    end
    return 0
end

function connect_demo(node)
    if not node then
        godot.push_warning("connect_demo called without a Godot node")
        return false
    end

    local ok, err = godot.connect(node, "tree_exiting", function()
        godot.print("Lua callback received Godot signal: tree_exiting")
    end)
    godot.print("signal connect result:", ok, err)
    return ok
end

return {
    ok = true,
    runtime = "LuaJIT",
    vector = v,
    add_result = math_util.add(10, 4),
    type_name = godot.variant_type(v),
}
