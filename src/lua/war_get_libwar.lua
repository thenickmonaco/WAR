-------------------------------------------------------------------------------
--
-- WAR - make music with vim motions
-- Copyright (C) 2025 Monaco
--
-- This file is part of WAR 1.0 software.
-- WAR 1.0 software is licensed under the GNU Affero General Public License
-- version 3, with the following modification: attribution to the original
-- author is waived.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
--
-- For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
--
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- src/lua/war_get_libwar.lua
-------------------------------------------------------------------------------

local function run_cmd(cmd)
    local handle = io.popen(cmd, "r")
    if not handle then return nil, "failed to run command" end
    local output = handle:read("*a")
    local ok, reason, code = handle:close()
    if not ok then
        local err = output ~= "" and output or (reason or "") .. (code and (" " .. tostring(code)) or "")
        return nil, err, code
    end
    return output, nil, code
end

local function get_pipewire_cflags()
    local out, err = run_cmd("pkg-config --cflags libpipewire-0.3")
    if not out or out == "" then
        return "-I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT", err
    end
    return out:gsub("\n", " ")
end

local function get_pipewire_libs()
    local out, err = run_cmd("pkg-config --libs libpipewire-0.3")
    if not out or out == "" then
        return "-lpipewire-0.3", err
    end
    return out:gsub("\n", " ")
end

local function preprocess_header(header_path)
    local pipewire_cflags, pw_err = get_pipewire_cflags()
    if (not pipewire_cflags or pipewire_cflags == "") and pw_err then
        error("Failed to get pipewire cflags: " .. pw_err)
    end

    local cmd = table.concat({
        "gcc -E",
        "-D_GNU_SOURCE -std=c99",
        "-I src -I include",
        "-I /usr/include/libdrm",
        "-I /usr/include/freetype2",
        pipewire_cflags or "",
        header_path,
        "2>&1",
    }, " ")

    local content, err = run_cmd(cmd)
    if not content then
        error("Header preprocessing failed: " .. (err or "unknown error"))
    end
    return content
end

local function extract_fsm_context(content)
    local pattern = "typedef%s+struct%s+war_fsm_context%s*%{([^}]+)%}"
    return content:match(pattern)
end

local function extract_function_union(content)
    local pattern = "typedef%s+union%s+war_function_union%s*%{([^}]+)%}"
    return content:match(pattern)
end

local function extract_env_struct(content)
    local pattern = "struct%s+war_env%s*%{([^}]+)%}"
    return content:match(pattern)
end

local function generate_fsm_ffi(fsm_body, union_body, env_body)
    local output = {}
    -- forward declarations for pointer types used in war_env
    table.insert(output, "typedef struct war_window_render_context war_window_render_context;")
    table.insert(output, "typedef struct war_atomics war_atomics;")
    table.insert(output, "typedef struct war_color_context war_color_context;")
    table.insert(output, "typedef struct war_lua_context war_lua_context;")
    table.insert(output, "typedef struct war_views war_views;")
    table.insert(output, "typedef struct war_play_context war_play_context;")
    table.insert(output, "typedef struct war_capture_context war_capture_context;")
    table.insert(output, "typedef struct war_command_context war_command_context;")
    table.insert(output, "typedef struct war_status_context war_status_context;")
    table.insert(output, "typedef struct war_undo_tree war_undo_tree;")
    table.insert(output, "typedef struct war_note_quads war_note_quads;")
    table.insert(output, "typedef struct war_pool war_pool;")
    table.insert(output, "typedef struct war_vulkan_context war_vulkan_context;")
    table.insert(output, "typedef struct war_wav war_wav;")
    table.insert(output, "typedef struct war_fsm_context war_fsm_context;")
    table.insert(output, "typedef struct war_env war_env;")

    table.insert(output, "typedef union war_function_union {")
    table.insert(output, "    void (*c)(war_env* env);")
    table.insert(output, "    int lua;")
    table.insert(output, "} war_function_union;")

    table.insert(output, "typedef struct war_fsm_context {")
    local simplified_fsm = fsm_body:gsub("struct xkb_%w+%*", "void*")
    table.insert(output, simplified_fsm)
    table.insert(output, "\n} war_fsm_context;")

    if env_body then
        table.insert(output, "typedef struct war_env {")
        table.insert(output, env_body)
        table.insert(output, "\n} war_env;")
    end

    return table.concat(output, "\n")
end

local function write_fsm_ffi(ffi_content)
    os.execute("mkdir -p build")
    local file = assert(io.open("build/fsm_ffi.lua", "w"))
    file:write("return [[\n")
    file:write(ffi_content)
    file:write("\n]]\n")
    file:close()
end

local function compile_fsm_shared_library()
    local pipewire_cflags, pw_cflags_err = get_pipewire_cflags()
    local pipewire_libs, pw_libs_err = get_pipewire_libs()

    if (not pipewire_cflags or pipewire_cflags == "") and pw_cflags_err then
        io.stderr:write("Warning: using fallback pipewire cflags (" .. pw_cflags_err .. ")\n")
    end
    if (not pipewire_libs or pipewire_libs == "") and pw_libs_err then
        io.stderr:write("Warning: using fallback pipewire libs (" .. pw_libs_err .. ")\n")
    end

    local cmd = table.concat({
        "gcc -fPIC -shared",
        "-D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99",
        "-I src -I include",
        "-I /usr/include/libdrm",
        "-I /usr/include/freetype2",
        pipewire_cflags or "",
        "-o build/libwar.so",
        "src/war_main.c",
        "-lvulkan -ldrm -lm -lluajit-5.1 -lxkbcommon -lasound -lpthread -lfreetype",
        pipewire_libs or "",
        "2>&1",
    }, " ")

    print("Compiling FSM shared library...")
    local out, err = run_cmd(cmd)
    if not out then
        io.stderr:write(err or "unknown error", "\n")
        return false
    end
    return true
end

local function main()
    print("Generating FFI for war_fsm_context and war_env...")
    local content = preprocess_header("src/h/war_data.h")
    local fsm_body = extract_fsm_context(content)
    local union_body = extract_function_union(content)
    local env_body = extract_env_struct(content)
    if fsm_body and union_body and env_body then
        local ffi_def = generate_fsm_ffi(fsm_body, union_body, env_body)
        write_fsm_ffi(ffi_def)
        local success = compile_fsm_shared_library()
        if success then
            print("✅ Generated build/fsm_ffi.lua")
            print("✅ Generated build/libwar.so")
            print("🎯 Ready for FSM access in Lua")
        else
            print("❌ Compilation failed")
        end
    else
        print("❌ war_fsm_context or war_function_union or war_env not found")
    end
end

main()
