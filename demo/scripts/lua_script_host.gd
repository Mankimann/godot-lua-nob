class_name LuaScriptHost
extends Node

signal lua_ready(instance: LuaScriptInstance)
signal lua_error(message: String, code: int)

@export_file("*.lua") var lua_script_path: String = "res://scripts/native_node.lua"
@export var initial_properties: Dictionary = {}
@export var auto_ready: bool = true
@export var forward_process: bool = true
@export var forward_physics_process: bool = false
@export var forward_input: bool = false
@export var forward_unhandled_input: bool = false

var lua_instance: LuaScriptInstance

func _ready() -> void:
    if not load_lua():
        return
    if auto_ready:
        lua_instance.ready_notification()
    lua_ready.emit(lua_instance)

func _process(delta: float) -> void:
    if forward_process and is_lua_ready():
        lua_instance.process_notification(delta)

func _physics_process(delta: float) -> void:
    if forward_physics_process and is_lua_ready():
        lua_instance.physics_process_notification(delta)

func _input(event: InputEvent) -> void:
    if forward_input and is_lua_ready():
        lua_instance.input_notification(event)

func _unhandled_input(event: InputEvent) -> void:
    if forward_unhandled_input and is_lua_ready():
        lua_instance.unhandled_input_notification(event)

func _exit_tree() -> void:
    if is_lua_ready():
        lua_instance.exit_tree_notification()

func load_lua(path: String = lua_script_path) -> bool:
    lua_script_path = path
    lua_instance = LuaScriptInstance.new()
    var err := lua_instance.initialize_from_file(self, lua_script_path, initial_properties)
    if err != OK:
        var message := "Could not initialize Lua script '%s' (error %s)." % [lua_script_path, err]
        push_error(message)
        lua_error.emit(message, err)
        return false
    return true

func reload_lua(keep_properties: bool = true) -> bool:
    if not lua_instance:
        return load_lua()
    var err := lua_instance.reload(keep_properties)
    if err != OK:
        var message := "Could not reload Lua script '%s' (error %s)." % [lua_script_path, err]
        push_error(message)
        lua_error.emit(message, err)
        return false
    return true

func is_lua_ready() -> bool:
    return lua_instance != null and lua_instance.is_ready()

func call_lua(method: StringName, args: Array = []) -> Variant:
    if not is_lua_ready():
        return null
    return lua_instance.call_method(method, args)

func set_lua_property(name: StringName, value: Variant) -> void:
    if is_lua_ready():
        lua_instance.set_script_property(name, value)

func get_lua_property(name: StringName) -> Variant:
    if not is_lua_ready():
        return null
    return lua_instance.get_script_property(name)

func lua_diagnostics() -> Dictionary:
    if not is_lua_ready():
        return {}
    return lua_instance.diagnostics()
