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
} Build_Config;

static const char *lua_sources[] = {
    "thirdparty/lua/src/lapi.c",
    "thirdparty/lua/src/lauxlib.c",
    "thirdparty/lua/src/lbaselib.c",
    "thirdparty/lua/src/lcode.c",
    "thirdparty/lua/src/lcorolib.c",
    "thirdparty/lua/src/lctype.c",
    "thirdparty/lua/src/ldblib.c",
    "thirdparty/lua/src/ldebug.c",
    "thirdparty/lua/src/ldo.c",
    "thirdparty/lua/src/ldump.c",
    "thirdparty/lua/src/lfunc.c",
    "thirdparty/lua/src/lgc.c",
    "thirdparty/lua/src/linit.c",
    "thirdparty/lua/src/liolib.c",
    "thirdparty/lua/src/llex.c",
    "thirdparty/lua/src/lmathlib.c",
    "thirdparty/lua/src/lmem.c",
    "thirdparty/lua/src/loadlib.c",
    "thirdparty/lua/src/lobject.c",
    "thirdparty/lua/src/lopcodes.c",
    "thirdparty/lua/src/loslib.c",
    "thirdparty/lua/src/lparser.c",
    "thirdparty/lua/src/lstate.c",
    "thirdparty/lua/src/lstring.c",
    "thirdparty/lua/src/lstrlib.c",
    "thirdparty/lua/src/ltable.c",
    "thirdparty/lua/src/ltablib.c",
    "thirdparty/lua/src/ltm.c",
    "thirdparty/lua/src/lundump.c",
    "thirdparty/lua/src/lutf8lib.c",
    "thirdparty/lua/src/lvm.c",
    "thirdparty/lua/src/lzio.c",
};

static const char *godot_cpp_core_dirs[] = {
    GODOT_CPP_DIR "/src/classes",
    GODOT_CPP_DIR "/src/core",
    GODOT_CPP_DIR "/src/variant",
};

static const char *extension_sources[] = {
    "src/register_types.cpp",
    "src/runtime/lua_error.cpp",
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
        "-Ithirdparty/lua/src",
        "-Ithirdparty/godot-cpp/include",
        "-Ithirdparty/godot-cpp/gen/include",
        "-Ithirdparty/godot-cpp/gdextension",
        "-DGDEXTENSION",
        "-Wno-unused-parameter",
        "-Wno-missing-field-initializers");
    if (cfg.mode == MODE_DEBUG) {
        nob_cmd_append(cmd, "-O0", "-g", "-DDEBUG_ENABLED", "-DDEV_ENABLED");
    } else {
        nob_cmd_append(cmd, "-O2", "-DNDEBUG");
    }
}

static bool compile_c(Build_Config cfg, const char *group, const char *src, Nob_File_Paths *objects) {
    const char *obj = object_path_for(group, src);
    nob_da_append(objects, obj);
    int rebuild = nob_needs_rebuild1(obj, src);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, cfg.cc, "-std=c99", "-fPIC", "-DLUA_COMPAT_5_3", "-Ithirdparty/lua/src");
    if (cfg.mode == MODE_DEBUG) nob_cmd_append(&cmd, "-O0", "-g");
    else nob_cmd_append(&cmd, "-O2", "-DNDEBUG");
    nob_cmd_append(&cmd, "-c", src, "-o", obj);
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

static bool link_extension(Build_Config cfg, Nob_File_Paths extension_objects, const char *godotcpp, const char *lua) {
    Nob_File_Paths deps = {0};
    for (size_t i = 0; i < extension_objects.count; ++i) nob_da_append(&deps, extension_objects.items[i]);
    nob_da_append(&deps, godotcpp);
    nob_da_append(&deps, lua);

    const char *out = shared_library_path(cfg);
    int rebuild = nob_needs_rebuild(out, deps.items, deps.count);
    if (rebuild < 0) return false;
    if (!rebuild) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, cfg.cxx, "-shared", "-o", out);
    for (size_t i = 0; i < extension_objects.count; ++i) nob_cmd_append(&cmd, extension_objects.items[i]);
    nob_cmd_append(&cmd, godotcpp, lua);
    if (strcmp(cfg.platform, "linux") == 0) {
        nob_cmd_append(&cmd, "-ldl", "-lm", "-pthread");
    }
    return nob_cmd_run(&cmd);
}

static bool build_all(Build_Config cfg) {
    ensure_dirs();
    if (!ensure_godot_cpp_bindings()) return false;

    Nob_File_Paths lua_objects = {0};
    for (size_t i = 0; i < NOB_ARRAY_LEN(lua_sources); ++i) {
        if (!compile_c(cfg, "lua", lua_sources[i], &lua_objects)) return false;
    }
    const char *lua_lib = LIB_DIR "/liblua54.a";
    if (!archive_static(cfg, lua_lib, lua_objects)) return false;

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

    if (!link_extension(cfg, extension_objects, godotcpp_lib, lua_lib)) return false;
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
    return nob_cmd_run(&cmd);
}

static void usage(const char *program) {
    nob_log(NOB_INFO, "Usage: %s [build|clean] [debug|release] [platform=linux|macos|windows] [arch=x86_64|arm64]", program);
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
    };
    const char *command = argc > 0 ? nob_shift(argv, argc) : "build";

    while (argc > 0) {
        const char *arg = nob_shift(argv, argc);
        if (strcmp(arg, "debug") == 0) cfg.mode = MODE_DEBUG;
        else if (strcmp(arg, "release") == 0) cfg.mode = MODE_RELEASE;
        else if (strncmp(arg, "platform=", 9) == 0) cfg.platform = arg + 9;
        else if (strncmp(arg, "arch=", 5) == 0) cfg.arch = arg + 5;
        else {
            usage(program);
            nob_log(NOB_ERROR, "unknown argument: %s", arg);
            return 1;
        }
    }

    if (strcmp(command, "build") == 0) {
        return build_all(cfg) ? 0 : 1;
    }
    if (strcmp(command, "clean") == 0) {
        return clean() ? 0 : 1;
    }
    usage(program);
    nob_log(NOB_ERROR, "unknown command: %s", command);
    return 1;
}
