#define NOB_IMPLEMENTATION
#include "thirdparty/nob/nob.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUILD_DIR "build"
#define OBJ_DIR "build/obj"
#define LIB_DIR "build/lib"
#define OUT_DIR "demo/addons/godot_lua/bin"
#define GODOT_CPP_DIR "thirdparty/godot-cpp"
#define GODOT_CPP_API GODOT_CPP_DIR "/gdextension/extension_api.json"
#define GODOT_CPP_GEN_OBJECT GODOT_CPP_DIR "/gen/include/godot_cpp/classes/object.hpp"
#define LUAJIT_DIR "thirdparty/luajit"
#define LUAJIT_SRC LUAJIT_DIR "/src"
#define LUAJIT_STATIC LUAJIT_SRC "/libluajit.a"

typedef enum Build_Mode {
    MODE_DEBUG,
    MODE_RELEASE,
} Build_Mode;

typedef struct Build_Config {
    Build_Mode mode;
    const char *platform;
    const char *arch;
    const char *cxx;
    const char *cc;
    const char *ar;
    bool luajit_lua52compat;
    bool luajit_disable_ffi;
} Build_Config;

static const char *godot_cpp_core_dirs[] = {
    GODOT_CPP_DIR "/src/classes",
    GODOT_CPP_DIR "/src/core",
    GODOT_CPP_DIR "/src/variant",
};

static const char *extension_sources[] = {
    "src/register_types.cpp",
    "src/runtime/lua_error.cpp",
    "src/runtime/lua_callable.cpp",
    "src/runtime/lua_state.cpp",
    "src/bindings/lua_variant_bridge.cpp",
};

static bool ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

static const char *object_path_for(const char *group, const char *src) {
    char *sanitized = nob_temp_sprintf("%s_%s", group, src);
    for (char *p = sanitized; *p; ++p) {
        if (!isalnum((unsigned char)*p)) *p = '_';
    }
    return nob_temp_sprintf(OBJ_DIR "/%s.o", sanitized);
}

static bool collect_sources_recursive(const char *dir, const char *suffix, Nob_File_Paths *out) {
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir(dir, &children)) return false;

    for (size_t i = 0; i < children.count; ++i) {
        const char *child = children.items[i];
        if (strcmp(child, ".") == 0 || strcmp(child, "..") == 0) continue;

        const char *path = nob_temp_sprintf("%s/%s", dir, child);
        Nob_File_Type type = nob_get_file_type(path);
        if (type == NOB_FILE_DIRECTORY) {
            if (!collect_sources_recursive(path, suffix, out)) return false;
        } else if (type == NOB_FILE_REGULAR && ends_with(path, suffix)) {
            nob_da_append(out, path);
        }
    }
    return true;
}

static bool ensure_godot_cpp_bindings(void) {
    int needs = nob_needs_rebuild1(GODOT_CPP_GEN_OBJECT, GODOT_CPP_API);
    if (needs < 0) return false;
    if (!needs) return true;

    nob_log(NOB_INFO, "generating godot-cpp bindings from %s", GODOT_CPP_API);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "python3.11", "tools/generate_godot_cpp.py");
    return nob_cmd_run(&cmd);
}

static void ensure_dirs(void) {
    nob_mkdir_if_not_exists(BUILD_DIR);
    nob_mkdir_if_not_exists(OBJ_DIR);
    nob_mkdir_if_not_exists(LIB_DIR);
    nob_mkdir_if_not_exists("demo/addons");
    nob_mkdir_if_not_exists("demo/addons/godot_lua");
    nob_mkdir_if_not_exists(OUT_DIR);
}

static void add_common_cpp_flags(Nob_Cmd *cmd, Build_Config cfg) {
    nob_cmd_append(cmd,
        "-std=c++17", "-fPIC",
        "-Iinclude",
        "-Ithirdparty/luajit/src",
        "-Ithirdparty/godot-cpp/include",
        "-Ithirdparty/godot-cpp/gen/include",
        "-Ithirdparty/godot-cpp/gdextension",
        "-DGDEXTENSION",
        "-DLUAJIT_ENABLE_LUA52COMPAT",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers");
    if (cfg.mode == MODE_DEBUG) {
        nob_cmd_append(cmd, "-O0", "-g", "-DDEBUG_ENABLED", "-DDEV_ENABLED");
    } else {
        nob_cmd_append(cmd, "-O2", "-DNDEBUG");
    }
}

static bool build_luajit(Build_Config cfg) {
    int rebuild = nob_needs_rebuild1(LUAJIT_STATIC, LUAJIT_SRC "/luajit.c");
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    nob_log(NOB_INFO, "building optimized LuaJIT static library");
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd,
        "make", "-C", LUAJIT_SRC,
        "BUILDMODE=static",
        nob_temp_sprintf("CC=%s", cfg.cc),
        nob_temp_sprintf("HOST_CC=%s", cfg.cc),
        nob_temp_sprintf("STATIC_CC=%s -fPIC", cfg.cc),
        nob_temp_sprintf("DYNAMIC_CC=%s -fPIC", cfg.cc),
        "TARGET_CFLAGS=-fPIC",
        cfg.mode == MODE_DEBUG ? "CCDEBUG=-g" : "CCDEBUG=",
        cfg.mode == MODE_DEBUG ? "CCOPT=-O0 -fomit-frame-pointer" : "CCOPT=-O2 -fomit-frame-pointer",
        cfg.luajit_lua52compat ? "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT" : "XCFLAGS=");
    if (cfg.luajit_disable_ffi) {
        nob_cmd_append(&cmd, "XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT -DLUAJIT_DISABLE_FFI");
    }
    return nob_cmd_run(&cmd);
}

static bool compile_cpp(Build_Config cfg, const char *group, const char *src, Nob_File_Paths *objects) {
    const char *obj = object_path_for(group, src);
    nob_da_append(objects, obj);
    int rebuild = nob_needs_rebuild1(obj, src);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, cfg.cxx);
    add_common_cpp_flags(&cmd, cfg);
    nob_cmd_append(&cmd, "-c", src, "-o", obj);
    return nob_cmd_run(&cmd);
}

static bool archive_static(Build_Config cfg, const char *out, Nob_File_Paths objects) {
    int rebuild = nob_needs_rebuild(out, objects.items, objects.count);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, cfg.ar, "rcs", out);
    for (size_t i = 0; i < objects.count; ++i) nob_cmd_append(&cmd, objects.items[i]);
    return nob_cmd_run(&cmd);
}

static const char *shared_library_path(Build_Config cfg) {
    const char *kind = cfg.mode == MODE_DEBUG ? "debug" : "release";
    if (strcmp(cfg.platform, "windows") == 0) {
        return nob_temp_sprintf(OUT_DIR "/godot_lua.%s.%s.%s.dll", cfg.platform, kind, cfg.arch);
    }
    if (strcmp(cfg.platform, "macos") == 0) {
        return nob_temp_sprintf(OUT_DIR "/libgodot_lua.%s.%s.%s.dylib", cfg.platform, kind, cfg.arch);
    }
    return nob_temp_sprintf(OUT_DIR "/libgodot_lua.%s.%s.%s.so", cfg.platform, kind, cfg.arch);
}

static bool link_extension(Build_Config cfg, Nob_File_Paths extension_objects, const char *godotcpp, const char *luajit) {
    Nob_File_Paths deps = {0};
    for (size_t i = 0; i < extension_objects.count; ++i) nob_da_append(&deps, extension_objects.items[i]);
    nob_da_append(&deps, godotcpp);
    nob_da_append(&deps, luajit);

    const char *out = shared_library_path(cfg);
    int rebuild = nob_needs_rebuild(out, deps.items, deps.count);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, cfg.cxx, "-shared", "-o", out);
    for (size_t i = 0; i < extension_objects.count; ++i) nob_cmd_append(&cmd, extension_objects.items[i]);
    nob_cmd_append(&cmd, godotcpp, luajit);
    if (strcmp(cfg.platform, "linux") == 0) {
        nob_cmd_append(&cmd, "-ldl", "-lm", "-pthread");
    } else if (strcmp(cfg.platform, "macos") == 0) {
        nob_cmd_append(&cmd, "-pagezero_size", "10000", "-image_base", "100000000");
    }
    return nob_cmd_run(&cmd);
}

static bool build_all(Build_Config cfg) {
    ensure_dirs();
    if (!ensure_godot_cpp_bindings()) return false;
    if (!build_luajit(cfg)) return false;

    Nob_File_Paths godotcpp_sources = {0};
    for (size_t i = 0; i < NOB_ARRAY_LEN(godot_cpp_core_dirs); ++i) {
        if (!collect_sources_recursive(godot_cpp_core_dirs[i], ".cpp", &godotcpp_sources)) return false;
    }
    if (!collect_sources_recursive(GODOT_CPP_DIR "/gen/src", ".cpp", &godotcpp_sources)) return false;

    Nob_File_Paths godotcpp_objects = {0};
    for (size_t i = 0; i < godotcpp_sources.count; ++i) {
        if (!compile_cpp(cfg, "godotcpp", godotcpp_sources.items[i], &godotcpp_objects)) return false;
    }
    const char *godotcpp_lib = LIB_DIR "/libgodot-cpp.a";
    if (!archive_static(cfg, godotcpp_lib, godotcpp_objects)) return false;

    Nob_File_Paths extension_objects = {0};
    for (size_t i = 0; i < NOB_ARRAY_LEN(extension_sources); ++i) {
        if (!compile_cpp(cfg, "extension", extension_sources[i], &extension_objects)) return false;
    }

    if (!link_extension(cfg, extension_objects, godotcpp_lib, LUAJIT_STATIC)) return false;
    nob_log(NOB_INFO, "built %s", shared_library_path(cfg));
    return true;
}

static bool clean(void) {
    Nob_Cmd cmd = {0};
#ifdef _WIN32
    nob_cmd_append(&cmd, "cmd", "/C", "if exist build rmdir /S /Q build");
#else
    nob_cmd_append(&cmd, "rm", "-rf", BUILD_DIR);
#endif
    if (!nob_cmd_run(&cmd)) return false;

    Nob_Cmd lua = {0};
    nob_cmd_append(&lua, "make", "-C", LUAJIT_SRC, "clean");
    return nob_cmd_run(&lua);
}

static void usage(const char *program) {
    nob_log(NOB_INFO, "Usage: %s [build|clean] [debug|release] [platform=linux|macos|windows] [arch=x86_64|arm64] [luajit-no-lua52compat] [luajit-disable-ffi]", program);
    nob_log(NOB_INFO, "Environment: CC, CXX and AR override the compiler toolchain. Example: CXX='zig c++' CC='zig cc' ./nob build release");
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift(argv, argc);
    Build_Config cfg = {
        .mode = MODE_DEBUG,
        .platform = "linux",
        .arch = "x86_64",
        .cxx = getenv("CXX") ? getenv("CXX") : "c++",
        .cc = getenv("CC") ? getenv("CC") : "cc",
        .ar = getenv("AR") ? getenv("AR") : "ar",
        .luajit_lua52compat = true,
        .luajit_disable_ffi = false,
    };
    const char *command = argc > 0 ? nob_shift(argv, argc) : "build";

    while (argc > 0) {
        const char *arg = nob_shift(argv, argc);
        if (strcmp(arg, "debug") == 0) cfg.mode = MODE_DEBUG;
        else if (strcmp(arg, "release") == 0) cfg.mode = MODE_RELEASE;
        else if (strncmp(arg, "platform=", 9) == 0) cfg.platform = arg + 9;
        else if (strncmp(arg, "arch=", 5) == 0) cfg.arch = arg + 5;
        else if (strcmp(arg, "luajit-no-lua52compat") == 0) cfg.luajit_lua52compat = false;
        else if (strcmp(arg, "luajit-disable-ffi") == 0) cfg.luajit_disable_ffi = true;
        else {
            usage(program);
            nob_log(NOB_ERROR, "unknown argument: %s", arg);
            return 1;
        }
    }

    if (strcmp(command, "build") == 0) return build_all(cfg) ? 0 : 1;
    if (strcmp(command, "clean") == 0) return clean() ? 0 : 1;
    usage(program);
    nob_log(NOB_ERROR, "unknown command: %s", command);
    return 1;
}
