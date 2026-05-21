#include <godot_lua/register_types.hpp>
#include <godot_lua/lua_callable.hpp>
#include <godot_lua/lua_error.hpp>
#include <godot_lua/lua_state.hpp>

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace godot {

void initialize_godot_lua_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    GDREGISTER_CLASS(LuaError);
    GDREGISTER_CLASS(LuaCallable);
    GDREGISTER_CLASS(LuaState);
}

void uninitialize_godot_lua_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

} // namespace godot

extern "C" {

GDExtensionBool GDE_EXPORT godot_lua_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(godot::initialize_godot_lua_module);
    init_obj.register_terminator(godot::uninitialize_godot_lua_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}

}
