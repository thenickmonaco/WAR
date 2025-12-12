-------------------------------------------------------------------------------
--
-- WAR - make music with vim motions
-- Copyright (C) 2025 Nick Monaco
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
-- src/lua/war_main.lua
-------------------------------------------------------------------------------

war = {}

war.keymap = {}
function war.keymap.set(modes, keys, command, flags)
    local keymap_flags = {
        handle_release = 0,
        handle_repeat = 1,
        handle_timeout = 1,
    }
end

war.modes = {
    roll = 0,
    views = 1,
    capture = 2,
    midi = 3,
    command = 4,
    wav = 5,
}

war.function_types = {
    none = 0,
    c = 1,
    lua = 2,
}

---@class ctx_lua
---@type ctx_lua
ctx_lua = {
    -- audio
    A_BASE_FREQUENCY                    = 440,
    A_BASE_NOTE                         = 69, -- A4
    A_EDO                               = 12,
    A_SAMPLE_RATE                       = 44100,
    A_BPM                               = 100.0,
    A_SAMPLE_DURATION                   = 30.0,
    A_CHANNEL_COUNT                     = 2,
    A_NOTE_COUNT                        = 128,
    A_LAYERS_IN_RAM                     = 13,
    A_LAYER_COUNT                       = 9,
    A_PLAY_DATA_SIZE                    = 6,
    A_CAPTURE_DATA_SIZE                 = 6,
    A_WARMUP_FRAMES_FACTOR              = 1000, -- bigger value means less recording warmup frames
    A_NOTES_MAX                         = 20000,
    A_DEFAULT_ATTACK                    = 0.0,
    A_DEFAULT_SUSTAIN                   = 1.0,
    A_DEFAULT_RELEASE                   = 0.0,
    A_DEFAULT_GAIN                      = 1.0,
    A_DEFAULT_COLUMNS_PER_BEAT          = 4.0,
    A_CACHE_SIZE                        = 100,
    A_PATH_LIMIT                        = 4096,
    A_SCHED_FIFO_PRIORITY               = 10,
    A_BUILDER_DATA_SIZE                 = 1024,
    -- window render
    WR_VIEWS_SAVED                      = 13,
    WR_COLOR_STEP                       = 43.2,
    WR_WARPOON_TEXT_COLS                = 25,
    WR_STATES                           = 256,
    WR_SEQUENCE_COUNT                   = 0,
    WR_SEQUENCE_LENGTH_MAX              = 0,
    WR_INPUT_SEQUENCE_LENGTH_MAX        = 256,
    WR_MODE_COUNT                       = 11,
    WR_KEYSYM_COUNT                     = 512,
    WR_MOD_COUNT                        = 16,
    WR_NOTE_QUADS_MAX                   = 20000,
    WR_STATUS_BAR_COLS_MAX              = 400,
    WR_TEXT_QUADS_MAX                   = 20000,
    WR_QUADS_MAX                        = 20000,
    WR_LEADER                           = "<Space>",
    WR_WAYLAND_MSG_BUFFER_SIZE          = 4096,
    WR_WAYLAND_MAX_OBJECTS              = 1000,
    WR_WAYLAND_MAX_OP_CODES             = 20,
    WR_UNDO_NODES_MAX                   = 10000,
    WR_TIMESTAMP_LENGTH_MAX             = 33,
    WR_CAPTURE_THRESHOLD                = 0.0001,
    WR_REPEAT_DELAY_US                  = 150000, -- 150000
    WR_REPEAT_RATE_US                   = 40000,  -- 40000
    WR_CURSOR_BLINK_DURATION_US         = 700000, -- 700000
    WR_UNDO_NOTES_BATCH_MAX             = 100,    -- <= 100
    WR_FPS                              = 240.0,
    WR_PLAY_CALLBACK_FPS                = 173.0,
    WR_CAPTURE_CALLBACK_FPS             = 47.0,
    WR_CALLBACK_SIZE                    = 4192,
    A_BYTES_NEEDED                      = 2048,
    A_TARGET_SAMPLES_FACTOR             = 2.0,
    ROLL_POSITION_X_Y                   = 0,
    -- pool
    POOL_ALIGNMENT                      = 256,
    -- cmd
    CMD_COUNT                           = 1,
    -- pc
    PC_CONTROL_BUFFER_SIZE              = 65536, -- 2^16
    PC_PLAY_BUFFER_SIZE                 = 65536, -- 2^16
    PC_CAPTURE_BUFFER_SIZE              = 65536, -- 2^16
    -- vk
    VK_ATLAS_WIDTH                      = 8192,
    VK_ATLAS_HEIGHT                     = 8192,
    VK_FONT_PIXEL_HEIGHT                = 69.0,
    -- misc
    DEFAULT_ALPHA_SCALE                 = 0.2,
    DEFAULT_CURSOR_ALPHA_SCALE          = 0.6,
    DEFAULT_PLAYBACK_BAR_THICKNESS      = 0.05,
    DEFAULT_TEXT_FEATHER                = 0.0,
    DEFAULT_TEXT_THICKNESS              = 0.2,
    WINDOWED_TEXT_FEATHER               = 0.0,
    WINDOWED_TEXT_THICKNESS             = 0.2,
    DEFAULT_BOLD_TEXT_FEATHER           = 0.0,
    DEFAULT_BOLD_TEXT_THICKNESS         = 0.2,
    DEFAULT_WINDOWED_ALPHA_SCALE        = 0.02,
    DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE = 0.02,
    WR_FN_NAME_LIMIT                    = 4096,
}

pool_a = {
    { name = "ctx_pw",                      type = "war_pipewire_context", count = 1 },
    { name = "ctx_pw.play_builder_data",    type = "uint8_t",              count = ctx_lua.A_BUILDER_DATA_SIZE },
    { name = "ctx_pw.capture_builder_data", type = "uint8_t",              count = ctx_lua.A_BUILDER_DATA_SIZE },
    { name = "ctx_pw.play_data",            type = "void*",                count = ctx_lua.A_PLAY_DATA_SIZE },
    { name = "ctx_pw.capture_data",         type = "void*",                count = ctx_lua.A_CAPTURE_DATA_SIZE },
    { name = "control_payload",             type = "uint8_t",              count = ctx_lua.PC_CONTROL_BUFFER_SIZE },
    { name = "tmp_control_payload",         type = "uint8_t",              count = ctx_lua.PC_CONTROL_BUFFER_SIZE },
    { name = "pc_control_cmd",              type = "void*",                count = ctx_lua.CMD_COUNT },
    { name = "play_read_count",             type = "uint64_t",             count = 1 },
    { name = "play_last_read_time",         type = "uint64_t",             count = 1 },
    { name = "capture_read_count",          type = "uint64_t",             count = 1 },
    { name = "capture_last_read_time",      type = "uint64_t",             count = 1 },
}

pool_wr = {
    -- env
    { name = "env",                                 type = "war_env",             count = 1 },
    -- command context
    { name = "ctx_command",                         type = "war_command_context", count = 1 },
    { name = "ctx_command.input",                   type = "int",                 count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_command.text",                    type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_command.prompt",                  type = "char",                count = ctx_lua.A_PATH_LIMIT },
    -- status context
    { name = "ctx_status",                          type = "war_status_context",  count = 1 },
    { name = "ctx_status.top",                      type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_status.middle",                   type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_status.bottom",                   type = "char",                count = ctx_lua.A_PATH_LIMIT },
    -- char input
    { name = "char_input",                          type = "char",                count = ctx_lua.A_PATH_LIMIT * 2 },
    -- play context
    { name = "ctx_play",                            type = "war_play_context",    count = 1 },
    { name = "ctx_play.note_layers",                type = "uint64_t",            count = ctx_lua.A_NOTE_COUNT },
    -- capture context
    { name = "ctx_capture",                         type = "war_capture_context", count = 1 },
    -- capture_wav
    { name = "capture_wav",                         type = "war_file",            count = 1 },
    { name = "capture_wav.fname",                   type = "char",                count = ctx_lua.A_PATH_LIMIT },
    -- cache_wav
    { name = "cache",                               type = "war_cache",           count = 1 },
    { name = "cache.id",                            type = "uint64_t",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.timestamp",                     type = "uint64_t",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.map",                           type = "uint8_t*",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.type",                          type = "uint8_t",             count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.inode",                         type = "ino_t",               count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.device",                        type = "dev_t",               count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.fd_size",                       type = "uint64_t",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.memfd_size",                    type = "uint64_t",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.memfd_capacity",                type = "uint64_t",            count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.fd",                            type = "int",                 count = ctx_lua.A_CACHE_SIZE },
    { name = "cache.memfd",                         type = "int",                 count = ctx_lua.A_CACHE_SIZE },
    -- map_wav
    { name = "map_wav",                             type = "war_map_wav",         count = 1 },
    { name = "map_wav.id",                          type = "uint64_t",            count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_LAYER_COUNT },
    { name = "map_wav.fname",                       type = "char",                count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_LAYER_COUNT * ctx_lua.A_PATH_LIMIT },
    { name = "map_wav.fname_size",                  type = "uint32_t",            count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_LAYER_COUNT },
    { name = "map_wav.note",                        type = "uint32_t",            count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_LAYER_COUNT },
    { name = "map_wav.layer",                       type = "uint32_t",            count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_LAYER_COUNT },
    -- color
    { name = "ctx_color",                           type = "war_color_context",   count = 1 },
    -- layres
    { name = "layers_active",                       type = "char",                count = ctx_lua.A_LAYER_COUNT },
    -- Colors
    { name = "colors",                              type = "uint32_t",            count = ctx_lua.A_LAYER_COUNT },
    -- views
    { name = "views.col",                           type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.row",                           type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.left_col",                      type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.right_col",                     type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.bottom_row",                    type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.top_row",                       type = "uint32_t",            count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.warpoon_text",                  type = "char*",               count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.warpoon_text_rows",             type = "char",                count = ctx_lua.WR_VIEWS_SAVED * ctx_lua.WR_WARPOON_TEXT_COLS },
    -- FSM CONTEXT
    { name = "ctx_fsm",                             type = "war_fsm_context",     count = 1 },
    { name = "ctx_fsm.function",                    type = "war_function_union",  count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.function_type",               type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.name",                        type = "char",                count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT * ctx_lua.A_PATH_LIMIT },
    { name = "ctx_fsm.is_terminal",                 type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.handle_release",              type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.handle_timeout",              type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.handle_repeat",               type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.is_prefix",                   type = "uint8_t",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "ctx_fsm.next_state",                  type = "uint64_t",            count = ctx_lua.WR_STATES * ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "ctx_fsm.cwd",                         type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_fsm.current_file_path",           type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_fsm.ext",                         type = "char",                count = ctx_lua.A_PATH_LIMIT },
    { name = "ctx_fsm.key_down",                    type = "uint8_t",             count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "ctx_fsm.key_last_event_us",           type = "uint64_t",            count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    -- quads vertices
    { name = "quad_vertices",                       type = "war_quad_vertex",     count = ctx_lua.WR_QUADS_MAX },
    { name = "quad_indices",                        type = "uint16_t",            count = ctx_lua.WR_QUADS_MAX },
    { name = "transparent_quad_vertices",           type = "war_quad_vertex",     count = ctx_lua.WR_QUADS_MAX },
    { name = "transparent_quad_indices",            type = "uint16_t",            count = ctx_lua.WR_QUADS_MAX },
    { name = "text_vertices",                       type = "war_text_vertex",     count = ctx_lua.WR_TEXT_QUADS_MAX },
    { name = "text_indices",                        type = "uint16_t",            count = ctx_lua.WR_TEXT_QUADS_MAX },
    -- note quads
    { name = "note_quads.alive",                    type = "uint8_t",             count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.id",                       type = "uint64_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.pos_x",                    type = "double",              count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.pos_y",                    type = "double",              count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.layer",                    type = "uint64_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x",                   type = "double",              count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x",             type = "double",              count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x_numerator",   type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x_denominator", type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x_numerator",         type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x_denominator",       type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.color",                    type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.outline_color",            type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.gain",                     type = "float",               count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.voice",                    type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.hidden",                   type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.mute",                     type = "uint32_t",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    -- keydown, keylasteventus, msgbuffer, pc_window_render, payload, input sequence
    { name = "key_down",                            type = "bool",                count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "key_last_event_us",                   type = "uint64_t",            count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "msg_buffer",                          type = "uint8_t",             count = ctx_lua.WR_WAYLAND_MSG_BUFFER_SIZE },
    { name = "obj_op",                              type = "void*",               count = ctx_lua.WR_WAYLAND_MAX_OBJECTS * ctx_lua.WR_WAYLAND_MAX_OP_CODES },
    { name = "pc_control_cmd",                      type = "void*",               count = ctx_lua.CMD_COUNT },
    { name = "control_payload",                     type = "uint8_t",             count = ctx_lua.PC_CONTROL_BUFFER_SIZE },
    { name = "tmp_control_payload",                 type = "uint8_t",             count = ctx_lua.PC_CONTROL_BUFFER_SIZE },
    { name = "input_sequence",                      type = "char",                count = ctx_lua.WR_INPUT_SEQUENCE_LENGTH_MAX },
    { name = "prompt",                              type = "char",                count = ctx_lua.WR_INPUT_SEQUENCE_LENGTH_MAX },
    -- { name = "cwd",                                 type = "char",              count = ctx_lua.A_PATH_LIMIT },
    -- undo tree
    { name = "undo_tree",                           type = "war_undo_tree",       count = 1 },
    { name = "undo_node.id",                        type = "uint64_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.seq_num",                   type = "uint64_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.branch_id",                 type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.command",                   type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.payload",                   type = "war_payload_union",   count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.cursor_pos_x",              type = "double",              count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.cursor_pos_y",              type = "double",              count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.left_col",                  type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.right_col",                 type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.top_row",                   type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.bottom_row",                type = "uint32_t",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.timestamp",                 type = "char",                count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.parent",                    type = "war_undo_node*",      count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.next",                      type = "war_undo_node*",      count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.prev",                      type = "war_undo_node*",      count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.alt_next",                  type = "war_undo_node*",      count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.alt_prev",                  type = "war_undo_node*",      count = ctx_lua.WR_UNDO_NODES_MAX },
    -- undo nodes batch payload
    { name = "note_quads",                          type = "war_note_quads",      count = 1 },
    { name = "note_quads.alive",                    type = "uint8_t",             count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.id",                       type = "uint64_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.pos_x",                    type = "double",              count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.pos_y",                    type = "double",              count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x",                   type = "double",              count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x",             type = "double",              count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x_numerator",   type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x_denominator", type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x_numerator",         type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x_denominator",       type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.color",                    type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.outline_color",            type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.gain",                     type = "float",               count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.voice",                    type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.hidden",                   type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.mute",                     type = "uint32_t",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
}

keymap_flags = {
    handle_release = 0,
    handle_repeat = 1,
    handle_timeout = 1,
}
keymap = {
    -- | Key           | Neovim String    |
    -- | ------------- | ---------------- |
    -- | Up arrow      | `<Up>`           |
    -- | Down arrow    | `<Down>`         |
    -- | Left arrow    | `<Left>`         |
    -- | Right arrow   | `<Right>`        |
    -- | Enter/Return  | `<CR>`           |
    -- | Escape        | `<Esc>`          |
    -- | Tab           | `<Tab>`          |
    -- | Space         | `<Space>`        |
    -- | Backspace     | `<BS>`           |
    -- | Delete        | `<Del>`          |
    -- | Insert        | `<Insert>`       |
    -- | PageUp        | `<PageUp>`       |
    -- | PageDown      | `<PageDown>`     |
    -- | Home          | `<Home>`         |
    -- | End           | `<End>`          |
    -- | Function keys | `<F1>` … `<F12>` |

    -- | Modifier      | Prefix | Notes                                       |
    -- | ------------- | ------ | ------------------------------------------- |
    -- | Control       | `C-`   | Example: `<C-x>`                            |
    -- | Shift         | `S-`   | Example: `<S-Tab>`                          |
    -- | Alt/Meta      | `A-`   | Example: `<A-e>` or `<M-e>` (both accepted) |
    -- | Command (Mac) | `D-`   | Not official, but often used in GUIs        |

    -- Special Edge Cases
    -- <lt> represents the literal < key (since < starts special sequences).
    -- <C-CR> or <S-CR> for Ctrl+Enter or Shift+Enter.
    -- <NL> is also valid for newline (same as <CR>).
    {
        sequences = {
            "k",
            "<Up>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_up",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_up",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "j",
            "<Down>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_down",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_down",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "h",
            "<Left>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_left",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_left",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "l",
            "<Right>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_right",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_right",
                mode = war.modes.views,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_l",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "L",
        },
        commands = {
            {
                cmd = "war_roll_get_layer_from_row",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "i",
        },
        commands = {
            {
                cmd = "war_roll_map_layer_to_cursor_row",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_i",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "<A-Tab>",
        },
        commands = {
            {
                cmd = "war_roll_toggle_flux",
                mode = war.modes.roll,
                type = war.function_types.c
            },
        },
    },
    {
        sequences = {
            "<A-k>",
            "<A-Up>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_up_leap",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_up_leap",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-j>",
            "<A-Down>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_down_leap",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_down_leap",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-h>",
            "<A-Left>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_left_leap",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_left_leap",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-l>",
            "<A-Right>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_right_leap",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_cursor_right_leap",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "0",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_left_bound",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_0",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "$",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_right_bound_or_prefix_horizontal",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "G",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_bottom_bound_or_prefix_vertical",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "gg",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_top_bound_or_prefix_vertical",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "1",
        },
        commands = {
            {
                cmd = "war_roll_1",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_1",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_1",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "2",
        },
        commands = {
            {
                cmd = "war_roll_2",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_2",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_2",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "3",
        },
        commands = {
            {
                cmd = "war_roll_3",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_3",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_3",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "4",
        },
        commands = {
            {
                cmd = "war_roll_4",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_4",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_4",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "5",
        },
        commands = {
            {
                cmd = "war_roll_5",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_5",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_5",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "6",
        },
        commands = {
            {
                cmd = "war_roll_6",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_6",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_6",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "7",
        },
        commands = {
            {
                cmd = "war_roll_7",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_7",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_7",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "8",
        },
        commands = {
            {
                cmd = "war_roll_8",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_8",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_8",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "9",
        },
        commands = {
            {
                cmd = "war_roll_9",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_9",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_9",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-=>",
        },
        commands = {
            {
                cmd = "war_roll_zoom_in",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-->",
        },
        commands = {
            {
                cmd = "war_roll_zoom_out",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-+>",
        },
        commands = {
            {
                cmd = "war_roll_zoom_in_leap",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-0>",
        },
        commands = {
            {
                cmd = "war_roll_zoom_reset",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<Esc>",
        },
        commands = {
            {
                cmd = "war_previous_mode",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_previous_mode",
                mode = war.modes.views,
                type = war.function_types.c,
            },
            {
                cmd = "war_previous_mode",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_previous_mode",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
            {
                cmd = "war_previous_mode",
                mode = war.modes.wav,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "f",
        },
        commands = {
            {
                cmd = "war_roll_cursor_fat",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-f>",
        },
        commands = {
            {
                cmd = "war_roll_alt_f",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "t",
        },
        commands = {
            {
                cmd = "war_roll_cursor_thin",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_t",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_t",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "x",
        },
        commands = {
            {
                cmd = "war_roll_x",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_x",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "T",
        },
        commands = {
            {
                cmd = "war_roll_cursor_move_thin",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_T",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "F",
        },
        commands = {
            {
                cmd = "war_roll_cursor_move_fat",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "gb",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_bottom",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "gt",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_top",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "gd",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_wav",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "gm",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_middle",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "s",
        },
        commands = {
            {
                cmd = "war_roll_reset_cursor",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "S",
        },
        commands = {
            {
                cmd = "war_roll_shift_s",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "z",
        },
        commands = {
            {
                cmd = "war_roll_note_draw",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_enter",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<CR>",
        },
        commands = {
            {
                cmd = "war_roll_note_draw",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_enter",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>div",
        },
        commands = {
            {
                cmd = "war_roll_note_delete_in_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>dov",
        },
        commands = {
            {
                cmd = "war_roll_note_delete_outside_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>diw",
        },
        commands = {
            {
                cmd = "war_roll_note_delete_in_word",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>da",
        },
        commands = {
            {
                cmd = "war_roll_note_delete_all",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>hov",
        },
        commands = {
            {
                cmd = "war_roll_note_hide_outside_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>hiv",
        },
        commands = {
            {
                cmd = "war_roll_note_hide_in_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>ha",
        },
        commands = {
            {
                cmd = "war_roll_note_hide_all",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>sov",
        },
        commands = {
            {
                cmd = "war_roll_note_show_outside_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>siv",
        },
        commands = {
            {
                cmd = "war_roll_note_show_in_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>sa",
        },
        commands = {
            {
                cmd = "war_roll_note_show_all",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>mov",
        },
        commands = {
            {
                cmd = "war_roll_note_mute_outside_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>miv",
        },
        commands = {
            {
                cmd = "war_roll_note_mute_in_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>ma",
        },
        commands = {
            {
                cmd = "war_roll_note_mute_all",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>umov",
        },
        commands = {
            {
                cmd = "war_roll_note_unmute_outside_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>umiv",
        },
        commands = {
            {
                cmd = "war_roll_note_unmute_in_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>uma",
        },
        commands = {
            {
                cmd = "war_roll_note_unmute_all",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>a",
        },
        commands = {
            {
                cmd = "war_roll_views_save",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-g>",
        },
        commands = {
            {
                cmd = "war_roll_views_1",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-t>",
        },
        commands = {
            {
                cmd = "war_roll_views_2",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-n>",
        },
        commands = {
            {
                cmd = "war_roll_views_3",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-s>",
        },
        commands = {
            {
                cmd = "war_roll_views_4",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-m>",
        },
        commands = {
            {
                cmd = "war_roll_views_5",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-y>",
        },
        commands = {
            {
                cmd = "war_roll_views_6",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-z>",
        },
        commands = {
            {
                cmd = "war_roll_views_7",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-q>",
        },
        commands = {
            {
                cmd = "war_roll_views_8",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-e>",
        },
        commands = {
            {
                cmd = "war_views_mode",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "a",
        },
        commands = {
            {
                cmd = "war_roll_play",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>1",
        },
        commands = {
            {
                cmd = "war_roll_space1",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>2",
        },
        commands = {
            {
                cmd = "war_roll_space2",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>3",
        },
        commands = {
            {
                cmd = "war_roll_space3",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>4",
        },
        commands = {
            {
                cmd = "war_roll_space4",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>5",
        },
        commands = {
            {
                cmd = "war_roll_space5",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>6",
        },
        commands = {
            {
                cmd = "war_roll_space6",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>7",
        },
        commands = {
            {
                cmd = "war_roll_space7",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>8",
        },
        commands = {
            {
                cmd = "war_roll_space8",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>9",
        },
        commands = {
            {
                cmd = "war_roll_space9",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>0",
        },
        commands = {
            {
                cmd = "war_roll_space0",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-1>",
        },
        commands = {
            {
                cmd = "war_roll_layer_1",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_1",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-2>",
        },
        commands = {
            {
                cmd = "war_roll_layer_2",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_2",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-3>",
        },
        commands = {
            {
                cmd = "war_roll_layer_3",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_3",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-4>",
        },
        commands = {
            {
                cmd = "war_roll_layer_4",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_4",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-5>",
        },
        commands = {
            {
                cmd = "war_roll_layer_5",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_5",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-6>",
        },
        commands = {
            {
                cmd = "war_roll_layer_6",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_6",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-7>",
        },
        commands = {
            {
                cmd = "war_roll_layer_7",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_7",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-8>",
        },
        commands = {
            {
                cmd = "war_roll_layer_8",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_8",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-9>",
        },
        commands = {
            {
                cmd = "war_roll_layer_9",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_9",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-0>",
        },
        commands = {
            {
                cmd = "war_roll_get_layer_from_row",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_get_layer_from_row",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-1>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_1",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_1",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-2>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_2",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_2",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-3>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_3",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_3",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-4>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_4",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_4",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-5>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_5",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_5",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-6>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_6",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_6",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-7>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_7",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_7",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-8>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_8",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_8",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-9>",
        },
        commands = {
            {
                cmd = "war_roll_layer_toggle_9",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_layer_toggle_9",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-S-0>",
        },
        commands = {
            {
                cmd = "war_roll_get_layer_from_row",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_roll_get_layer_from_row",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>d<leader>a",
        },
        commands = {
            {
                cmd = "war_roll_spacedspacea",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-K>",
            "<A-S-Up>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_up_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-J>",
            "<A-S-Down>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_down_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-H>",
            "<A-S-Left>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_left_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-L>",
            "<A-S-Right>",
        },
        commands = {
            {
                cmd = "war_roll_cursor_right_view",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "d",
        },
        commands = {
            {
                cmd = "war_roll_note_delete",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_delete",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "m",
        },
        commands = {
            {
                cmd = "war_midi_mode",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_previous_mode",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "X",
        },
        commands = {
            {
                cmd = "war_roll_shift_x",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "w",
        },
        commands = {
            {
                cmd = "war_roll_w",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_w",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_w",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "W",
        },
        commands = {
            {
                cmd = "war_roll_W",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "e",
        },
        commands = {
            {
                cmd = "war_roll_e",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_e",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_e",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "E",
        },
        commands = {
            {
                cmd = "war_roll_E",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "b",
        },
        commands = {
            {
                cmd = "war_roll_b",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_b",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "A",
        },
        commands = {
            {
                cmd = "war_roll_A",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-a>",
        },
        commands = {
            {
                cmd = "war_roll_alt_a",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-Esc>",
        },
        commands = {
            {
                cmd = "war_roll_alt_esc",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<A-A>",
        },
        commands = {
            {
                cmd = "war_roll_alt_A",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-a>",
        },
        commands = {
            {
                cmd = "war_roll_ctrl_a",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<Tab>",
        },
        commands = {
            {
                cmd = "war_roll_tab",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_tab",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>hiw",
        },
        commands = {
            {
                cmd = "war_roll_note_hide_in_word",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>siw",
        },
        commands = {
            {
                cmd = "war_roll_note_show_in_word",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>umiw",
        },
        commands = {
            {
                cmd = "war_roll_note_unmute_in_word",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "ga",
        },
        commands = {
            {
                cmd = "war_roll_cursor_goto_play_bar",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<S-Tab>",
        },
        commands = {
            {
                cmd = "war_roll_hud_cycle",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "V",
        },
        commands = {
            {
                cmd = "war_roll_shift_v",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_visual_line",
                mode = war.modes.views,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "K",
        },
        commands = {
            {
                cmd = "war_roll_gain_up",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_shift_up",
                mode = war.modes.views,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_K",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_K",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "J",
        },
        commands = {
            {
                cmd = "war_roll_gain_down",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_views_shift_down",
                mode = war.modes.views,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_J",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_J",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<leader>m",
        },
        commands = {
            {
                cmd = "war_roll_note_mute",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "B",
        },
        commands = {
            {
                cmd = "war_roll_B",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "q",
        },
        commands = {
            {
                cmd = "war_roll_q",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_q",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_q",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "Q",
        },
        commands = {
            {
                cmd = "war_capture_mode",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_mode",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_mode",
                mode = war.modes.wav,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "r",
        },
        commands = {
            {
                cmd = "war_capture_r",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_r",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "u",
        },
        commands = {
            {
                cmd = "war_roll_u",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_capture_u",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_u",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "o",
        },
        commands = {
            {
                cmd = "war_capture_o",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_o",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "p",
        },
        commands = {
            {
                cmd = "war_capture_p",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_p",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "[",
        },
        commands = {
            {
                cmd = "war_capture_leftbracket",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_leftbracket",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "]",
        },
        commands = {
            {
                cmd = "war_capture_rightbracket",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_rightbracket",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "-",
        },
        commands = {
            {
                cmd = "war_capture_minus",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_midi_minus",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "c",
        },
        commands = {
            {
                cmd = "war_midi_c",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<C-r>",
        },
        commands = {
            {
                cmd = "war_roll_ctrl_r",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
        },
    },
    {
        sequences = {
            "<Space>",
        },
        commands = {
            {
                cmd = "war_capture_space",
                mode = war.modes.capture,
                type = war.function_types.c,
                handle_repeat = 0,
            },
            {
                cmd = "war_midi_space",
                mode = war.modes.midi,
                type = war.function_types.c,
                handle_repeat = 0,
            },
        },
    },
    {
        sequences = {
            ":",
        },
        commands = {
            {
                cmd = "war_command_mode",
                mode = war.modes.roll,
                type = war.function_types.c,
            },
            {
                cmd = "war_command_mode",
                mode = war.modes.views,
                type = war.function_types.c,
            },
            {
                cmd = "war_command_mode",
                mode = war.modes.capture,
                type = war.function_types.c,
            },
            {
                cmd = "war_command_mode",
                mode = war.modes.midi,
                type = war.function_types.c,
            },
            {
                cmd = "war_command_mode",
                mode = war.modes.wav,
                type = war.function_types.c,
            },
        },
    },
}

-- Preprocess keymap: replace <leader> with WR_LEADER
local function preprocess_leader(keymap)
    for _, entry in ipairs(keymap) do
        if type(entry) == "table" and entry.sequences then
            for i, seq in ipairs(entry.sequences) do
                entry.sequences[i] = seq:gsub("<leader>", ctx_lua.WR_LEADER)
            end
        end
    end
end

preprocess_leader(keymap)

-- Extract individual keys from a sequence string (handles <...> tokens)
local function parse_sequence(seq)
    local keys = {}
    local i = 1
    while i <= #seq do
        if seq:sub(i, i) == "<" then
            local closing = seq:find(">", i)
            if closing then
                table.insert(keys, seq:sub(i, closing))
                i = closing + 1
            else
                table.insert(keys, seq:sub(i, i))
                i = i + 1
            end
        else
            table.insert(keys, seq:sub(i, i))
            i = i + 1
        end
    end
    return keys
end

local function war_analyze_keymap(keymap)
    local seen = { [""] = true }
    local total_states = 1
    local max_len = 0

    for _, entry in ipairs(keymap) do
        if entry.sequences then
            for _, seq in ipairs(entry.sequences) do
                -- replace <leader> in sequence
                seq = seq:gsub("<leader>", ctx_lua.WR_LEADER)
                local keys = parse_sequence(seq)
                local prefix = ""
                for _, k in ipairs(keys) do
                    prefix = prefix .. k
                    if not seen[prefix] then
                        seen[prefix] = true
                        total_states = total_states + 1
                    end
                end
                if #keys > max_len then
                    max_len = #keys
                end
            end
        end
    end

    return total_states, max_len
end

ctx_lua.WR_STATES, ctx_lua.WR_SEQUENCE_LENGTH_MAX = war_analyze_keymap(keymap)
ctx_lua.WR_STATES = ctx_lua.WR_STATES * 2

print("Total FSM states needed:", ctx_lua.WR_STATES)
print("Max sequence length:", ctx_lua.WR_SEQUENCE_LENGTH_MAX)

-- Flatten keymap with parsed keys and merged command flags
local function war_flatten_keymap(keymap)
    local flattened = {}
    for _, entry in ipairs(keymap) do
        if entry.sequences then
            for _, seq in ipairs(entry.sequences) do
                local keys = parse_sequence(seq)

                local commands_with_flags = {}
                for _, cmd in ipairs(entry.commands) do
                    local merged_handle_repeat = keymap_flags.handle_repeat
                    if cmd.handle_repeat ~= nil then
                        merged_handle_repeat = cmd.handle_repeat
                    elseif #keys > 1 then
                        merged_handle_repeat = 0
                    end

                    local merged = {
                        cmd            = cmd.cmd,
                        mode           = cmd.mode,
                        type           = cmd.type,
                        handle_release = cmd.handle_release or keymap_flags.handle_release,
                        handle_repeat  = merged_handle_repeat,
                        handle_timeout = cmd.handle_timeout or keymap_flags.handle_timeout,
                    }
                    table.insert(commands_with_flags, merged)
                end

                table.insert(flattened, { keys = keys, commands = commands_with_flags })
            end
        end
    end
    return flattened
end

war_flattened = war_flatten_keymap(keymap)
