@tool
extends EditorImportPlugin


func _get_importer_name() -> String:
    return "godot_lua.lua_script"


func _get_visible_name() -> String:
    return "Lua Script"


func _get_recognized_extensions() -> PackedStringArray:
    return PackedStringArray(["lua"])


func _get_save_extension() -> String:
    return "tres"


func _get_resource_type() -> String:
    return "LuaScript"


func _get_preset_count() -> int:
    return 1


func _get_preset_name(preset_index: int) -> String:
    return "Default"


func _get_priority() -> float:
    return 1.0


func _get_import_order() -> int:
    return 0


func _get_import_options(path: String, preset_index: int) -> Array[Dictionary]:
    return [
        {
            "name": "store_source_path",
            "default_value": true
        }
    ]


func _get_option_visibility(path: String, option_name: StringName, options: Dictionary) -> bool:
    return true


func _import(source_file: String, save_path: String, options: Dictionary, platform_variants: Array[String], gen_files: Array[String]) -> Error:
    if not ClassDB.class_exists("LuaScript"):
        push_error("LuaScript is not registered. Build and load the godot_lua GDExtension first.")
        return ERR_UNAVAILABLE

    var lua_script := ClassDB.instantiate("LuaScript") as Resource
    if lua_script == null:
        push_error("Could not instantiate LuaScript while importing %s." % source_file)
        return ERR_CANT_CREATE

    var error := lua_script.call("load_from_file", source_file)
    if error != OK:
        push_error("Could not import Lua script %s: error %s" % [source_file, error])
        return error

    if not options.get("store_source_path", true):
        lua_script.call("set_path_hint", "")

    var target_path := "%s.%s" % [save_path, _get_save_extension()]
    return ResourceSaver.save(lua_script, target_path)
