---@class LuaNativeDemo: Node
---@extends Node

local frames = 0

godot.print("native_node.lua loaded for owner: ", self)

function _ready()
    godot.print("LuaNativeDemo._ready from LuaScriptInstance")
end

function _process(delta)
    frames = frames + 1
    if frames == 1 then
        godot.print("LuaNativeDemo._process first frame delta=", delta)
    end
    return frames
end

function greet(name)
    return "Hello " .. tostring(name) .. " from native LuaScript"
end
