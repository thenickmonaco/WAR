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
-- src/lua/get_libwar.lua
-------------------------------------------------------------------------------

local function preprocess_header(header_path)
    local cmd = string.format("gcc -E -I src -I include %s", header_path)
    local handle = io.popen(cmd, "r")
    local content = handle:read("*a")
    handle:close()
    return content
end

local function extract_fsm_context(content)
    local pattern = "typedef%s+struct%s+war_fsm_context%s*%{([^}]+)%}"
    local body = content:match(pattern)
    return body
end

local function extract_function_union(content)
    local pattern = "typedef%s+union%s+war_function_union%s*%{([^}]+)%}"
    local body = content:match(pattern)
    return body
end

local function generate_fsm_ffi(fsm_body, union_body)
    local output = {}
    
    -- Add function union (EXACT structure)
    table.insert(output, "typedef union war_function_union {")
    table.insert(output, "    void (*c)(war_env* env);")
    table.insert(output, "    int lua;")
    table.insert(output, "} war_function_union;")
    
    -- Add main struct with simplified types
    table.insert(output, "\n-- FSM context struct")
    table.insert(output, "typedef struct war_fsm_context {")
    
    -- Replace XKB types with void*
    local simplified_body = fsm_body
    simplified_body = simplified_body:gsub("struct xkb_%w+%*", "void*")
    
    table.insert(output, simplified_body)
    table.insert(output, "\n} war_fsm_context;")
    
    return table.concat(output, "\n")
end

local function write_fsm_ffi(ffi_content)
    os.execute("mkdir -p build")
    local file = io.open("build/fsm_ffi.lua", "w")
    file:write("return [[\n")
    file:write(ffi_content)
    file:write("\n]]\n")
    file:close()
end

local function compile_fsm_shared_library()
    local cmd = string.format(
        "gcc -fPIC -shared -D_GNU_SOURCE -Wall -Wextra -O3 -march=native -std=c99 " ..
        "-I src -I include " ..
        "-o build/libwar.so src/war_main.c " ..
        "-lm -lluajit-5.1 -lxkbcommon -lpthread"
    )
    
    print("Compiling FSM shared library...")
    return os.execute(cmd) == 0
end

local function main()
    print("Generating FFI for war_fsm_context...")
    
    local content = preprocess_header("src/h/war_data.h")
    local fsm_body = extract_fsm_context(content)
    local union_body = extract_function_union(content)
    
    if fsm_body and union_body then
        local ffi_def = generate_fsm_ffi(fsm_body, union_body)
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
        print("❌ war_fsm_context or war_function_union not found")
    end
end

main()
