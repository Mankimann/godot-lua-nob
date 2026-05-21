@tool
extends EditorInspectorPlugin

const LuaScriptSlot = preload("res://addons/godot_lua_editor/lua_script_slot.gd")

var editor_plugin: EditorPlugin


func setup(p_editor_plugin: EditorPlugin) -> void:
    editor_plugin = p_editor_plugin


func _can_handle(object: Object) -> bool:
    return object is Node


func _parse_begin(object: Object) -> void:
    if editor_plugin == null or not (object is Node):
        return

    var slot := LuaScriptSlot.new()
    slot.setup(editor_plugin, object as Node)
    add_custom_control(slot)
