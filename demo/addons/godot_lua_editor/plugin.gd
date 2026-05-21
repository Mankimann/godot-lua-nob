@tool
extends EditorPlugin

const LuaScriptImporter = preload("res://addons/godot_lua_editor/lua_script_importer.gd")
const LuaScriptInspectorPlugin = preload("res://addons/godot_lua_editor/lua_script_inspector_plugin.gd")

const MENU_ATTACH_LUA_SCRIPT := "Attach Lua Script to Selected Node..."

var _importer: EditorImportPlugin
var _inspector_plugin: EditorInspectorPlugin
var _file_dialog: EditorFileDialog
var _pending_nodes: Array[Node] = []


func _enter_tree() -> void:
    _importer = LuaScriptImporter.new()
    add_import_plugin(_importer)

    _inspector_plugin = LuaScriptInspectorPlugin.new()
    _inspector_plugin.setup(self)
    add_inspector_plugin(_inspector_plugin)

    add_tool_menu_item(MENU_ATTACH_LUA_SCRIPT, _show_attach_dialog)

    _file_dialog = EditorFileDialog.new()
    _file_dialog.title = "Attach Lua Script"
    _file_dialog.access = EditorFileDialog.ACCESS_RESOURCES
    _file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
    _file_dialog.filters = PackedStringArray(["*.lua ; Lua scripts"])
    _file_dialog.file_selected.connect(_attach_selected_lua_file)
    get_editor_interface().get_base_control().add_child(_file_dialog)


func _exit_tree() -> void:
    remove_tool_menu_item(MENU_ATTACH_LUA_SCRIPT)

    if _inspector_plugin != null:
        remove_inspector_plugin(_inspector_plugin)
        _inspector_plugin = null

    if _importer != null:
        remove_import_plugin(_importer)
        _importer = null

    if _file_dialog != null:
        _file_dialog.queue_free()
        _file_dialog = null

    _pending_nodes.clear()


func _show_attach_dialog() -> void:
    if not ClassDB.class_exists("LuaScript"):
        push_error("LuaScript is not registered. Build and load the godot_lua GDExtension first.")
        return

    _pending_nodes = get_editor_interface().get_selection().get_selected_nodes()
    if _pending_nodes.is_empty():
        push_warning("Select at least one Node before attaching a Lua script.")
        return

    _file_dialog.popup_centered_ratio(0.6)


func _attach_selected_lua_file(path: String) -> void:
    if _pending_nodes.is_empty():
        return

    var attach_pairs: Array[Dictionary] = []
    for node in _pending_nodes:
        if not is_instance_valid(node):
            continue
        var lua_script := _create_lua_script(path)
        if lua_script == null:
            _pending_nodes.clear()
            return
        attach_pairs.append({
            "node": node,
            "old_script": node.get_script(),
            "new_script": lua_script
        })

    if attach_pairs.is_empty():
        _pending_nodes.clear()
        return

    var undo_redo := get_undo_redo()
    undo_redo.create_action("Attach Lua Script")

    for pair in attach_pairs:
        undo_redo.add_do_method(pair["node"], "set_script", pair["new_script"])
        undo_redo.add_undo_method(pair["node"], "set_script", pair["old_script"])

    undo_redo.commit_action()
    _pending_nodes.clear()


func _create_lua_script(path: String) -> Resource:
    var lua_script := ClassDB.instantiate("LuaScript") as Resource
    if lua_script == null:
        push_error("Could not instantiate LuaScript.")
        return null

    var error := lua_script.call("load_from_file", path)
    if error != OK:
        push_error("Could not load Lua script: %s (error %s)" % [path, error])
        return null

    return lua_script
