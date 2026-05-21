@tool
extends VBoxContainer

var _editor_plugin: EditorPlugin
var _node: Node
var _path_edit: LineEdit
var _status_label: Label
var _file_dialog: EditorFileDialog


func setup(p_editor_plugin: EditorPlugin, p_node: Node) -> void:
    _editor_plugin = p_editor_plugin
    _node = p_node


func _ready() -> void:
    add_theme_constant_override("separation", 6)

    var title := Label.new()
    title.text = "Lua Script"
    title.add_theme_font_size_override("font_size", 14)
    add_child(title)

    var row := HBoxContainer.new()
    add_child(row)

    _path_edit = LineEdit.new()
    _path_edit.placeholder_text = "res://scripts/example.lua"
    _path_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    _path_edit.text_submitted.connect(_attach_from_text)
    row.add_child(_path_edit)

    var browse_button := Button.new()
    browse_button.text = "..."
    browse_button.tooltip_text = "Choose a Lua script file"
    browse_button.pressed.connect(_show_file_dialog)
    row.add_child(browse_button)

    var actions := HBoxContainer.new()
    add_child(actions)

    var attach_button := Button.new()
    attach_button.text = "Attach"
    attach_button.tooltip_text = "Attach the Lua file as this node's script"
    attach_button.pressed.connect(_attach_current_path)
    actions.add_child(attach_button)

    var reload_button := Button.new()
    reload_button.text = "Reload"
    reload_button.tooltip_text = "Reload and reattach the current Lua script"
    reload_button.pressed.connect(_reload_current_script)
    actions.add_child(reload_button)

    var clear_button := Button.new()
    clear_button.text = "Clear"
    clear_button.tooltip_text = "Remove the current script from this node"
    clear_button.pressed.connect(_clear_script)
    actions.add_child(clear_button)

    _status_label = Label.new()
    _status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    add_child(_status_label)

    _file_dialog = EditorFileDialog.new()
    _file_dialog.title = "Attach Lua Script"
    _file_dialog.access = EditorFileDialog.ACCESS_RESOURCES
    _file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
    _file_dialog.filters = PackedStringArray(["*.lua ; Lua scripts"])
    _file_dialog.file_selected.connect(_attach_selected_file)
    add_child(_file_dialog)

    _refresh_from_node()


func _refresh_from_node() -> void:
    if not is_instance_valid(_node):
        _set_status("Node is no longer valid.", true)
        return

    var script := _node.get_script()
    if _is_lua_script(script):
        var path := ""
        if script.has_method("get_path_hint"):
            path = str(script.call("get_path_hint"))
        _path_edit.text = path
        var suffix := "."
        if not path.is_empty():
            suffix = " (%s)" % path
        _set_status("Attached LuaScript" + suffix, false)
    elif script != null:
        _path_edit.text = ""
        _set_status("This node already has a non-Lua script: %s" % script.get_class(), true)
    else:
        _set_status("No script attached. Choose a .lua file and press Attach.", false)


func _show_file_dialog() -> void:
    _file_dialog.popup_centered_ratio(0.6)


func _attach_selected_file(path: String) -> void:
    _path_edit.text = path
    _attach_path(path)


func _attach_from_text(path: String) -> void:
    _attach_path(path)


func _attach_current_path() -> void:
    _attach_path(_path_edit.text.strip_edges())


func _reload_current_script() -> void:
    var path := _path_edit.text.strip_edges()
    if path.is_empty() and is_instance_valid(_node):
        var script: Resource = _node.get_script()
        if _is_lua_script(script) and script.has_method("get_path_hint"):
            path = str(script.call("get_path_hint"))
    _attach_path(path)


func _clear_script() -> void:
    if not is_instance_valid(_node):
        _set_status("Node is no longer valid.", true)
        return

    var old_script := _node.get_script()
    var undo_redo := _editor_plugin.get_undo_redo()
    undo_redo.create_action("Clear Lua Script")
    undo_redo.add_do_method(_node, "set_script", null)
    undo_redo.add_undo_method(_node, "set_script", old_script)
    undo_redo.add_do_method(self, "_refresh_from_node")
    undo_redo.add_undo_method(self, "_refresh_from_node")
    undo_redo.commit_action()


func _attach_path(path: String) -> void:
    if not is_instance_valid(_node):
        _set_status("Node is no longer valid.", true)
        return
    if path.is_empty():
        _set_status("Choose a .lua file first.", true)
        return
    if path.get_extension().to_lower() != "lua":
        _set_status("Expected a .lua file: %s" % path, true)
        return
    if not ClassDB.class_exists("LuaScript"):
        _set_status("LuaScript is not registered. Build and load the godot_lua GDExtension first.", true)
        return

    var lua_script := _create_lua_script(path)
    if lua_script == null:
        return

    var old_script := _node.get_script()
    var undo_redo := _editor_plugin.get_undo_redo()
    undo_redo.create_action("Attach Lua Script")
    undo_redo.add_do_method(_node, "set_script", lua_script)
    undo_redo.add_undo_method(_node, "set_script", old_script)
    undo_redo.add_do_method(self, "_refresh_from_node")
    undo_redo.add_undo_method(self, "_refresh_from_node")
    undo_redo.commit_action()


func _create_lua_script(path: String) -> Resource:
    var lua_script := ClassDB.instantiate("LuaScript") as Resource
    if lua_script == null:
        _set_status("Could not instantiate LuaScript.", true)
        return null

    var error := lua_script.call("load_from_file", path)
    if error != OK:
        _set_status("Could not load Lua script: %s (error %s)" % [path, error], true)
        return null

    return lua_script


func _is_lua_script(script: Variant) -> bool:
    if script == null:
        return false
    if script is Object and (script as Object).is_class("LuaScript"):
        return true
    return script.has_method("load_from_file") and script.has_method("get_path_hint")


func _set_status(message: String, is_error: bool) -> void:
    if _status_label == null:
        return
    _status_label.text = message
    if is_error:
        _status_label.add_theme_color_override("font_color", Color(1.0, 0.35, 0.25))
    else:
        _status_label.remove_theme_color_override("font_color")
