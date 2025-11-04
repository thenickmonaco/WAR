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
-- src/lua/monaco/set.lua
-------------------------------------------------------------------------------

---@class ctx_lua
-- audio
---@field A_BASE_FREQUENCY number
---@field A_BASE_NOTE number
---@field A_EDO number
---@field A_SAMPLE_RATE number
---@field A_BPM number
---@field A_SAMPLE_DURATION number
---@field A_CHANNEL_COUNT number
---@field A_NOTE_COUNT number
---@field A_SAMPLES_PER_NOTE number
-- window render
---@field WR_VIEWS_SAVED number
---@field WR_WARPOON_TEXT_COLS number
---@field WR_STATES number
---@field WR_SEQUENCE_COUNT number
---@field WR_SEQUENCE_LENGTH_MAX number
---@field WR_MODE_COUNT number
---@field WR_KEYSYM_COUNT number
---@field WR_MOD_COUNT number
---@field WR_NOTE_QUADS_MAX number
---@field WR_STATUS_BAR_COLS_MAX number
---@field WR_TEXT_QUADS_MAX number
---@field WR_QUADS_MAX number
---@field WR_LEADER string
---@field WR_WAYLAND_MSG_BUFFER_SIZE number
---@field WR_WAYLAND_MAX_OBJECTS number
---@field WR_WAYLAND_MAX_OP_CODES number
-- pool
---@field POOL_ALIGNMENT number
---@diagnostic disable: lowercase-global
---@type ctx_lua
ctx_lua = {
    -- audio
    A_BASE_FREQUENCY            = 440,
    A_BASE_NOTE                 = 69, -- A4
    A_EDO                       = 12,
    A_SAMPLE_RATE               = 44100,
    A_BPM                       = 100.0,
    A_SAMPLE_DURATION           = 30.0,
    A_CHANNEL_COUNT             = 2,
    A_NOTE_COUNT                = 128,
    A_SAMPLES_PER_NOTE          = 128,
    A_NOTES_MAX                 = 20000,
    A_DEFAULT_ATTACK            = 0.0,
    A_DEFAULT_SUSTAIN           = 1.0,
    A_DEFAULT_RELEASE           = 0.0,
    A_DEFAULT_GAIN              = 1.0,
    A_DEFAULT_COLUMNS_PER_BEAT  = 4.0,
    -- window render
    WR_VIEWS_SAVED              = 13,
    WR_WARPOON_TEXT_COLS        = 25,
    WR_STATES                   = 256,
    WR_SEQUENCE_COUNT           = 0,
    WR_SEQUENCE_LENGTH_MAX      = 0,
    WR_MODE_COUNT               = 10,
    WR_KEYSYM_COUNT             = 512,
    WR_MOD_COUNT                = 16,
    WR_NOTE_QUADS_MAX           = 20000,
    WR_STATUS_BAR_COLS_MAX      = 200,
    WR_TEXT_QUADS_MAX           = 20000,
    WR_QUADS_MAX                = 20000,
    WR_LEADER                   = "<Space>",
    WR_WAYLAND_MSG_BUFFER_SIZE  = 4096,
    WR_WAYLAND_MAX_OBJECTS      = 1000,
    WR_WAYLAND_MAX_OP_CODES     = 20,
    WR_UNDO_NODES_MAX           = 10000,
    WR_TIMESTAMP_LENGTH_MAX     = 33,
    WR_REPEAT_DELAY_US          = 150000, -- 150000
    WR_REPEAT_RATE_US           = 40000,  -- 40000
    WR_CURSOR_BLINK_DURATION_US = 700000, -- 700000
    WR_UNDO_NOTES_BATCH_MAX     = 100,    -- <= 100
    WR_FPS                      = 240.0,
    -- pool
    POOL_ALIGNMENT              = 256,
    -- cmd
    CMD_COUNT                   = 32,
    -- pc
    PC_BUFFER_SIZE              = 32864, -- 32864
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
                cmd = "cmd_normal_k",
                mode = "normal",
            },
            {
                cmd = "cmd_views_k",
                mode = "views",
            },
            {
                cmd = "cmd_record_k",
                mode = "record",
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
                cmd = "cmd_normal_j",
                mode = "normal",
            },
            {
                cmd = "cmd_views_j",
                mode = "views",
            },
            {
                cmd = "cmd_record_j",
                mode = "record",
            },
        },
    },
    {
        sequences = {
            "r",
        },
        commands = {
            {
                cmd = "cmd_normal_r",
                mode = "normal",
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
                cmd = "cmd_normal_h",
                mode = "normal",
            },
            {
                cmd = "cmd_views_h",
                mode = "views",
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
                cmd = "cmd_normal_l",
                mode = "normal",
            },
            {
                cmd = "cmd_views_l",
                mode = "views",
            },
            {
                cmd = "cmd_midi_l",
                mode = "midi",
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
                cmd = "cmd_normal_alt_k",
                mode = "normal",
            },
            {
                cmd = "cmd_views_alt_k",
                mode = "views",
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
                cmd = "cmd_normal_alt_j",
                mode = "normal",
            },
            {
                cmd = "cmd_views_alt_j",
                mode = "views",
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
                cmd = "cmd_normal_alt_h",
                mode = "normal",
            },
            {
                cmd = "cmd_views_alt_h",
                mode = "views",
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
                cmd = "cmd_normal_alt_l",
                mode = "normal",
            },
            {
                cmd = "cmd_views_alt_l",
                mode = "views",
            },
        },
    },
    {
        sequences = {
            "0",
        },
        commands = {
            {
                cmd = "cmd_normal_0",
                mode = "normal",
            },
            {
                cmd = "cmd_record_0",
                mode = "record",
            },
        },
    },
    {
        sequences = {
            "$",
        },
        commands = {
            {
                cmd = "cmd_normal_$",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "G",
        },
        commands = {
            {
                cmd = "cmd_normal_G",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "gg",
        },
        commands = {
            {
                cmd = "cmd_normal_gg",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "1",
        },
        commands = {
            {
                cmd = "cmd_normal_1",
                mode = "normal",
            },
            {
                cmd = "cmd_record_1",
                mode = "record",
            },
            {
                cmd = "cmd_midi_1",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "2",
        },
        commands = {
            {
                cmd = "cmd_normal_2",
                mode = "normal",
            },
            {
                cmd = "cmd_record_2",
                mode = "record",
            },
            {
                cmd = "cmd_midi_2",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "3",
        },
        commands = {
            {
                cmd = "cmd_normal_3",
                mode = "normal",
            },
            {
                cmd = "cmd_record_3",
                mode = "record",
            },
            {
                cmd = "cmd_midi_3",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "4",
        },
        commands = {
            {
                cmd = "cmd_normal_4",
                mode = "normal",
            },
            {
                cmd = "cmd_record_4",
                mode = "record",
            },
            {
                cmd = "cmd_midi_4",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "5",
        },
        commands = {
            {
                cmd = "cmd_normal_5",
                mode = "normal",
            },
            {
                cmd = "cmd_record_5",
                mode = "record",
            },
            {
                cmd = "cmd_midi_5",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "6",
        },
        commands = {
            {
                cmd = "cmd_normal_6",
                mode = "normal",
            },
            {
                cmd = "cmd_record_6",
                mode = "record",
            },
            {
                cmd = "cmd_midi_6",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "7",
        },
        commands = {
            {
                cmd = "cmd_normal_7",
                mode = "normal",
            },
            {
                cmd = "cmd_record_7",
                mode = "record",
            },
            {
                cmd = "cmd_midi_7",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "8",
        },
        commands = {
            {
                cmd = "cmd_normal_8",
                mode = "normal",
            },
            {
                cmd = "cmd_record_8",
                mode = "record",
            },
            {
                cmd = "cmd_midi_8",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "9",
        },
        commands = {
            {
                cmd = "cmd_normal_9",
                mode = "normal",
            },
            {
                cmd = "cmd_record_9",
                mode = "record",
            },
            {
                cmd = "cmd_midi_9",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "9",
        },
        commands = {
            {
                cmd = "cmd_normal_9",
                mode = "normal",
            },
            {
                cmd = "cmd_record_9",
                mode = "record",
            },
            {
                cmd = "cmd_midi_9",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "<C-=>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_equal",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-->",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_minus",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-+>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_plus",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-A-=>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_alt_equal",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-A-->",
        },
        commands = {
            {
                cmd = "",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-0>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_0",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<Esc>",
        },
        commands = {
            {
                cmd = "cmd_normal_esc",
                mode = "normal",
            },
            {
                cmd = "cmd_views_esc",
                mode = "views",
            },
            {
                cmd = "cmd_record_esc",
                mode = "record",
            },
            {
                cmd = "cmd_midi_esc",
                mode = "midi",
            },
            {
                cmd = "cmd_command_esc",
                mode = "command",
            },
        },
    },
    {
        sequences = {
            "f",
        },
        commands = {
            {
                cmd = "cmd_normal_f",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "t",
        },
        commands = {
            {
                cmd = "cmd_normal_t",
                mode = "normal",
            },
            {
                cmd = "cmd_record_t",
                mode = "record",
            },
            {
                cmd = "cmd_midi_t",
                mode = "midi",
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
                cmd = "cmd_normal_x",
                mode = "normal",
            },
            {
                cmd = "cmd_midi_x",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "T",
        },
        commands = {
            {
                cmd = "cmd_normal_T",
                mode = "normal",
            },
            {
                cmd = "cmd_midi_T",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "F",
        },
        commands = {
            {
                cmd = "cmd_normal_F",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "gb",
        },
        commands = {
            {
                cmd = "cmd_normal_gb",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "gt",
        },
        commands = {
            {
                cmd = "cmd_normal_gt",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "gm",
        },
        commands = {
            {
                cmd = "cmd_normal_gm",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "s",
        },
        commands = {
            {
                cmd = "cmd_normal_s",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "z",
        },
        commands = {
            {
                cmd = "cmd_normal_z",
                mode = "normal",
            },
            {
                cmd = "cmd_views_z",
                mode = "views",
            },
        },
    },
    {
        sequences = {
            "<CR>",
        },
        commands = {
            {
                cmd = "cmd_normal_return",
                mode = "normal",
            },
            {
                cmd = "cmd_views_return",
                mode = "views",
            },
        },
    },
    {
        sequences = {
            "<leader>div",
        },
        commands = {
            {
                cmd = "cmd_normal_spacediv",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>dov",
        },
        commands = {
            {
                cmd = "cmd_normal_spacedov",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>diw",
        },
        commands = {
            {
                cmd = "cmd_normal_spacediw",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>da",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceda",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>hov",
        },
        commands = {
            {
                cmd = "cmd_normal_spacehov",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>hiv",
        },
        commands = {
            {
                cmd = "cmd_normal_spacehiv",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>ha",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceha",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>sov",
        },
        commands = {
            {
                cmd = "cmd_normal_spacesov",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>siv",
        },
        commands = {
            {
                cmd = "cmd_normal_spacesiv",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>sa",
        },
        commands = {
            {
                cmd = "cmd_normal_spacesa",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>mov",
        },
        commands = {
            {
                cmd = "cmd_normal_spacemov",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>miv",
        },
        commands = {
            {
                cmd = "cmd_normal_spacemiv",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>ma",
        },
        commands = {
            {
                cmd = "cmd_normal_spacema",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>umov",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceumov",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>umiv",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceumiv",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>uma",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceuma",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>a",
        },
        commands = {
            {
                cmd = "cmd_normal_spacea",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-g>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_g",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-t>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_t",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-n>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_n",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-s>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_s",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-m>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_m",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-y>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_y",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-z>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_z",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-q>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_q",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-e>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_e",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "a",
        },
        commands = {
            {
                cmd = "cmd_normal_a",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>1",
        },
        commands = {
            {
                cmd = "cmd_normal_space1",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>2",
        },
        commands = {
            {
                cmd = "cmd_normal_space2",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>3",
        },
        commands = {
            {
                cmd = "cmd_normal_space3",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>4",
        },
        commands = {
            {
                cmd = "cmd_normal_space4",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>5",
        },
        commands = {
            {
                cmd = "cmd_normal_space5",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>6",
        },
        commands = {
            {
                cmd = "cmd_normal_space6",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>7",
        },
        commands = {
            {
                cmd = "cmd_normal_space7",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>8",
        },
        commands = {
            {
                cmd = "cmd_normal_space8",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>9",
        },
        commands = {
            {
                cmd = "cmd_normal_space9",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>0",
        },
        commands = {
            {
                cmd = "cmd_normal_space0",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-1>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_1",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-2>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_2",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-3>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_3",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-4>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_4",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-5>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_5",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-6>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_6",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-7>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_7",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-8>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_8",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-9>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_9",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-0>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_0",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>d<leader>a",
        },
        commands = {
            {
                cmd = "cmd_normal_spacedspacea",
                mode = "normal",
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
                cmd = "cmd_normal_alt_K",
                mode = "normal",
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
                cmd = "cmd_normal_alt_J",
                mode = "normal",
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
                cmd = "cmd_normal_alt_H",
                mode = "normal",
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
                cmd = "cmd_normal_alt_L",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "d",
        },
        commands = {
            {
                cmd = "cmd_normal_d",
                mode = "normal",
            },
            {
                cmd = "cmd_views_d",
                mode = "views",
            },
        },
    },
    {
        sequences = {
            "m",
        },
        commands = {
            {
                cmd = "cmd_normal_m",
                mode = "normal",
            },
            {
                cmd = "cmd_midi_m",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "X",
        },
        commands = {
            {
                cmd = "cmd_normal_X",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "w",
        },
        commands = {
            {
                cmd = "cmd_normal_w",
                mode = "normal",
            },
            {
                cmd = "cmd_record_w",
                mode = "record",
            },
            {
                cmd = "cmd_midi_w",
                mode = "midi",
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
                cmd = "cmd_normal_W",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "e",
        },
        commands = {
            {
                cmd = "cmd_normal_e",
                mode = "normal",
            },
            {
                cmd = "cmd_record_e",
                mode = "record",
            },
            {
                cmd = "cmd_midi_e",
                mode = "midi",
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
                cmd = "cmd_normal_E",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "b",
        },
        commands = {
            {
                cmd = "cmd_normal_b",
                mode = "normal",
            },
            {
                cmd = "cmd_midi_b",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "<A-u>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_u",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-d>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_d",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "A",
        },
        commands = {
            {
                cmd = "cmd_normal_A",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-a>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_a",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-Esc>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_esc",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<A-A>",
        },
        commands = {
            {
                cmd = "cmd_normal_alt_A",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-a>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_a",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<Tab>",
        },
        commands = {
            {
                cmd = "cmd_normal_tab",
                mode = "normal",
            },
            {
                cmd = "cmd_normal_tab",
                mode = "record",
            },
        },
    },
    {
        sequences = {
            "<leader>hiw",
        },
        commands = {
            {
                cmd = "cmd_normal_spacehiw",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>siw",
        },
        commands = {
            {
                cmd = "cmd_normal_spacesiw",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<leader>umiw",
        },
        commands = {
            {
                cmd = "cmd_normal_spaceumiw",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "ga",
        },
        commands = {
            {
                cmd = "cmd_normal_ga",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<S-Tab>",
        },
        commands = {
            {
                cmd = "cmd_normal_shift_tab",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "V",
        },
        commands = {
            {
                cmd = "cmd_normal_V",
                mode = "normal",
            },
            {
                cmd = "cmd_views_V",
                mode = "views",
            },
        },
    },
    {
        sequences = {
            "K",
        },
        commands = {
            {
                cmd = "cmd_normal_K",
                mode = "normal",
            },
            {
                cmd = "cmd_views_K",
                mode = "views",
            },
            {
                cmd = "cmd_record_K",
                mode = "record",
            },
            {
                cmd = "cmd_midi_K",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "J",
        },
        commands = {
            {
                cmd = "cmd_normal_J",
                mode = "normal",
            },
            {
                cmd = "cmd_views_J",
                mode = "views",
            },
            {
                cmd = "cmd_record_J",
                mode = "record",
            },
            {
                cmd = "cmd_midi_J",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "<leader>m",
        },
        commands = {
            {
                cmd = "cmd_normal_spacem",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "B",
        },
        commands = {
            {
                cmd = "cmd_normal_B",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "q",
        },
        commands = {
            {
                cmd = "cmd_normal_q",
                mode = "normal",
            },
            {
                cmd = "cmd_record_q",
                mode = "record",
            },
            {
                cmd = "cmd_midi_q",
                mode = "midi",
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
                cmd = "cmd_normal_Q",
                mode = "normal",
            },
            {
                cmd = "cmd_record_Q",
                mode = "record",
            },
            {
                cmd = "cmd_midi_Q",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "r",
        },
        commands = {
            {
                cmd = "cmd_record_r",
                mode = "record",
            },
            {
                cmd = "cmd_midi_r",
                mode = "midi",
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "y",
        },
        commands = {
            {
                cmd = "cmd_record_y",
                mode = "record",
            },
            {
                cmd = "cmd_midi_y",
                mode = "midi",
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
                cmd = "cmd_record_u",
                mode = "record",
            },
            {
                cmd = "cmd_midi_u",
                mode = "midi",
                handle_release = 1,
            },
        },
    },
    {
        sequences = {
            "i",
        },
        commands = {
            {
                cmd = "cmd_record_i",
                mode = "record",
            },
            {
                cmd = "cmd_midi_i",
                mode = "midi",
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
                cmd = "cmd_record_o",
                mode = "record",
            },
            {
                cmd = "cmd_midi_o",
                mode = "midi",
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
                cmd = "cmd_record_p",
                mode = "record",
            },
            {
                cmd = "cmd_midi_p",
                mode = "midi",
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
                cmd = "cmd_record_leftbracket",
                mode = "record",
            },
            {
                cmd = "cmd_midi_leftbracket",
                mode = "midi",
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
                cmd = "cmd_record_rightbracket",
                mode = "record",
            },
            {
                cmd = "cmd_midi_rightbracket",
                mode = "midi",
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
                cmd = "cmd_record_minus",
                mode = "record",
            },
            {
                cmd = "cmd_midi_minus",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "c",
        },
        commands = {
            {
                cmd = "cmd_midi_c",
                mode = "midi",
            },
        },
    },
    {
        sequences = {
            "u",
        },
        commands = {
            {
                cmd = "cmd_normal_u",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<C-r>",
        },
        commands = {
            {
                cmd = "cmd_normal_ctrl_r",
                mode = "normal",
            },
        },
    },
    {
        sequences = {
            "<Space>",
        },
        commands = {
            {
                cmd = "cmd_record_space",
                mode = "record",
                handle_repeat = 0,
            },
            {
                cmd = "cmd_midi_space",
                mode = "midi",
                handle_repeat = 0,
            },
        },
    },
    {
        sequences = {
            "w<CR>",
        },
        commands = {
            {
                cmd = "cmd_command_w",
                mode = "command",
            },
        },
    },
    {
        sequences = {
            ":",
        },
        commands = {
            {
                cmd = "cmd_normal_colon",
                mode = "normal",
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
                    local merged = {
                        cmd            = cmd.cmd,
                        mode           = cmd.mode,
                        handle_release = cmd.handle_release or keymap_flags.handle_release,
                        handle_repeat  = cmd.handle_repeat or keymap_flags.handle_repeat,
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

pool_a = {
    -- Atoms
    { name = "atomics.notes_on",                         type = "uint8_t",           count = ctx_lua.A_NOTE_COUNT },
    { name = "atomics.notes_on_previous",                type = "uint8_t",           count = ctx_lua.A_NOTE_COUNT },

    -- Audio Context struct itself
    { name = "audio_context",                            type = "war_audio_context", count = 1 },
    { name = "audio_context.sample_frames",              type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "audio_context.sample_frames_duration",     type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "audio_context.sample_phase",               type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "audio_context.record_buffer",              type = "int16_t",           count = ctx_lua.A_SAMPLE_RATE * ctx_lua.A_SAMPLE_DURATION * ctx_lua.A_CHANNEL_COUNT },
    { name = "audio_context.resample_buffer",            type = "int16_t",           count = ctx_lua.A_SAMPLE_RATE * ctx_lua.A_SAMPLE_DURATION * ctx_lua.A_CHANNEL_COUNT },

    -- Sample Pool
    { name = "sample_pool",                              type = "int16_t",           count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLE_RATE * ctx_lua.A_SAMPLE_DURATION * ctx_lua.A_CHANNEL_COUNT },

    -- Samples struct
    { name = "samples",                                  type = "war_samples",       count = 1 },
    { name = "samples.samples",                          type = "int16_t*",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_start",             type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_duration",          type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames",                   type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_trim_start",        type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_trim_end",          type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_attack",                   type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_sustain",                  type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_release",                  type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_gain",                     type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.samples_active",                   type = "uint8_t",           count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "samples.notes_attack",                     type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_sustain",                    type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_release",                    type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_gain",                       type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_frames_start",               type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_frames_duration",            type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_frames_trim_start",          type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.notes_frames_trim_end",            type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "samples.samples_count",                    type = "uint32_t",          count = ctx_lua.A_NOTE_COUNT },

    -- Record Samples struct
    { name = "record_samples",                           type = "war_samples",       count = 1 },
    { name = "record_samples.samples",                   type = "int16_t*",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_start",      type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_duration",   type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames",            type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_trim_start", type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_trim_end",   type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_attack",            type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_sustain",           type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_release",           type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_gain",              type = "float",             count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_active",            type = "uint8_t",           count = ctx_lua.A_NOTE_COUNT * ctx_lua.A_SAMPLES_PER_NOTE },
    { name = "record_samples.notes_attack",              type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_sustain",             type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_release",             type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_gain",                type = "float",             count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_frames_start",        type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_frames_duration",     type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_frames_trim_start",   type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.notes_frames_trim_end",     type = "uint64_t",          count = ctx_lua.A_NOTE_COUNT },
    { name = "record_samples.samples_count",             type = "uint32_t",          count = ctx_lua.A_NOTE_COUNT },

    -- war_notes
    { name = "notes",                                    type = "war_notes",         count = 1 },
    { name = "notes.alive",                              type = "uint8_t",           count = ctx_lua.A_NOTES_MAX },
    { name = "notes.id",                                 type = "uint64_t",          count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_start_frames",                 type = "uint64_t",          count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_duration_frames",              type = "uint64_t",          count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_phase_increment",              type = "float",             count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_velocity",                     type = "float",             count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_sample_index",                 type = "uint32_t",          count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_attack",                       type = "float",             count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_sustain",                      type = "float",             count = ctx_lua.A_NOTES_MAX },
    { name = "notes.notes_release",                      type = "float",             count = ctx_lua.A_NOTES_MAX },

    -- war_notes undo batch
    { name = "notes",                                    type = "war_notes",         count = 1 },
    { name = "notes.alive",                              type = "uint8_t",           count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.id",                                 type = "uint64_t",          count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_start_frames",                 type = "uint64_t",          count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_duration_frames",              type = "uint64_t",          count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_phase_increment",              type = "float",             count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_velocity",                     type = "float",             count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_sample_index",                 type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_attack",                       type = "float",             count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_sustain",                      type = "float",             count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },
    { name = "notes.notes_release",                      type = "float",             count = ctx_lua.WR_UNDO_NODES_MAX * ctx_lua.WR_UNDO_NOTES_BATCH_MAX },

    -- Record indices and userdata
    { name = "record_samples_notes_indices",             type = "int32_t",           count = ctx_lua.A_NOTE_COUNT },
    { name = "userdata",                                 type = "void*",             count = 9 },

    -- payload, pc_audio, play_buffer, record_buffer
    { name = "pc_audio",                                 type = "void*",             count = ctx_lua.CMD_COUNT },
    { name = "payload",                                  type = "uint8_t",           count = ctx_lua.PC_BUFFER_SIZE },
    { name = "tmp_payload",                              type = "uint8_t",           count = ctx_lua.PC_BUFFER_SIZE },
    { name = "play_buffer",                              type = "uint8_t",           count = 1024 },
    { name = "record_buffer",                            type = "uint8_t",           count = 1024 },
}

pool_wr = {
    ---------------------------------------------------------------------------
    -- Views (war_views)
    ---------------------------------------------------------------------------
    { name = "views.col",                           type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.row",                           type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.left_col",                      type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.right_col",                     type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.bottom_row",                    type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.top_row",                       type = "uint32_t",          count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.warpoon_text",                  type = "char*",             count = ctx_lua.WR_VIEWS_SAVED },
    { name = "views.warpoon_text_rows",             type = "char",              count = ctx_lua.WR_VIEWS_SAVED * ctx_lua.WR_WARPOON_TEXT_COLS },

    ---------------------------------------------------------------------------
    -- FSM State Machine
    ---------------------------------------------------------------------------
    -- FSM root struct array
    { name = "fsm",                                 type = "war_fsm_state",     count = ctx_lua.WR_STATES },
    { name = "fsm.is_terminal",                     type = "uint8_t",           count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.is_prefix",                       type = "uint8_t",           count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.handle_release",                  type = "uint8_t",           count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.handle_repeat",                   type = "uint8_t",           count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.handle_timeout",                  type = "uint8_t",           count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.command",                         type = "void*",             count = ctx_lua.WR_STATES * ctx_lua.WR_MODE_COUNT },
    { name = "fsm.next_state",                      type = "uint16_t",          count = ctx_lua.WR_STATES * ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },

    ---------------------------------------------------------------------------
    -- Quads and Vertices
    ---------------------------------------------------------------------------
    { name = "quad_vertices",                       type = "war_quad_vertex",   count = ctx_lua.WR_QUADS_MAX },

    { name = "quad_indices",                        type = "uint16_t",          count = ctx_lua.WR_QUADS_MAX },

    { name = "transparent_quad_vertices",           type = "war_quad_vertex",   count = ctx_lua.WR_QUADS_MAX },

    { name = "transparent_quad_indices",            type = "uint16_t",          count = ctx_lua.WR_QUADS_MAX },

    { name = "text_vertices",                       type = "war_text_vertex",   count = ctx_lua.WR_TEXT_QUADS_MAX },

    { name = "text_indices",                        type = "uint16_t",          count = ctx_lua.WR_TEXT_QUADS_MAX },

    ---------------------------------------------------------------------------
    -- Status Bar Text Buffers
    ---------------------------------------------------------------------------
    { name = "text_top_status_bar",                 type = "char",              count = ctx_lua.WR_STATUS_BAR_COLS_MAX },
    { name = "text_middle_status_bar",              type = "char",              count = ctx_lua.WR_STATUS_BAR_COLS_MAX },
    { name = "text_bottom_status_bar",              type = "char",              count = ctx_lua.WR_STATUS_BAR_COLS_MAX },

    ---------------------------------------------------------------------------
    -- Note Quads (war_note_quads)
    ---------------------------------------------------------------------------
    { name = "note_quads.alive",                    type = "uint8_t",           count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.id",                       type = "uint64_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.pos_x",                    type = "double",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.pos_y",                    type = "double",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x",                   type = "double",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x",             type = "double",            count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x_numerator",   type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.navigation_x_denominator", type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x_numerator",         type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.size_x_denominator",       type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.color",                    type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.outline_color",            type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.gain",                     type = "float",             count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.voice",                    type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.hidden",                   type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },
    { name = "note_quads.mute",                     type = "uint32_t",          count = ctx_lua.WR_NOTE_QUADS_MAX },

    -- keydown, keylasteventus, msgbuffer, pc_window_render, payload
    { name = "key_down",                            type = "bool",              count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "key_last_event_us",                   type = "uint64_t",          count = ctx_lua.WR_KEYSYM_COUNT * ctx_lua.WR_MOD_COUNT },
    { name = "msg_buffer",                          type = "uint8_t",           count = ctx_lua.WR_WAYLAND_MSG_BUFFER_SIZE },
    { name = "obj_op",                              type = "void*",             count = ctx_lua.WR_WAYLAND_MAX_OBJECTS * ctx_lua.WR_WAYLAND_MAX_OP_CODES },
    { name = "pc_window_render",                    type = "void*",             count = ctx_lua.CMD_COUNT },
    { name = "payload",                             type = "uint8_t",           count = ctx_lua.PC_BUFFER_SIZE },
    { name = "tmp_payload",                         type = "uint8_t",           count = ctx_lua.PC_BUFFER_SIZE },

    -- undo tree
    { name = "undo_tree",                           type = "war_undo_tree",     count = 1 },
    { name = "undo_node.id",                        type = "uint64_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.seq_num",                   type = "uint64_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.branch_id",                 type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.command",                   type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.payload",                   type = "war_payload_union", count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.cursor_pos_x",              type = "double",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.cursor_pos_y",              type = "double",            count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.left_col",                  type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.right_col",                 type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.top_row",                   type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.bottom_row",                type = "uint32_t",          count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.timestamp",                 type = "char",              count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.parent",                    type = "war_undo_node*",    count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.next",                      type = "war_undo_node*",    count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.prev",                      type = "war_undo_node*",    count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.alt_next",                  type = "war_undo_node*",    count = ctx_lua.WR_UNDO_NODES_MAX },
    { name = "undo_node.alt_prev",                  type = "war_undo_node*",    count = ctx_lua.WR_UNDO_NODES_MAX },

    -- undo nodes batch payload
    { name = "note_quads",                          type = "war_note_quads",    count = 1 },
    { name = "note_quads.alive",                    type = "uint8_t",           count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.id",                       type = "uint64_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.pos_x",                    type = "double",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.pos_y",                    type = "double",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x",                   type = "double",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x",             type = "double",            count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x_numerator",   type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.navigation_x_denominator", type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x_numerator",         type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.size_x_denominator",       type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.color",                    type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.outline_color",            type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.gain",                     type = "float",             count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.voice",                    type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.hidden",                   type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
    { name = "note_quads.mute",                     type = "uint32_t",          count = ctx_lua.WR_UNDO_NOTES_BATCH_MAX * ctx_lua.WR_UNDO_NODES_MAX },
}
