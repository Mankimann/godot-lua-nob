godot.print("Hallo aus Lua 5.4 in Godot")

godot_node.name = "NodeRenamedFromLua"
godot.print("Node name via Lua:", godot_node.name)

local v = Vector2(12, 30)
godot.print("Vector2 table:", v.x, v.y)

local function compute_score(base, multiplier)
    return base * multiplier + 7
end

return {
    ok = true,
    score = compute_score(10, 4),
    node_name = godot_node.name,
    vector = v,
    type_name = godot.variant_type(v),
}
