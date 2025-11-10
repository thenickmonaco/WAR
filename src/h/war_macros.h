//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Monaco
//
// This file is part of WAR 1.0 software.
// WAR 1.0 software is licensed under the GNU Affero General Public License
// version 3, with the following modification: attribution to the original
// author is waived.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_macros.h
//-----------------------------------------------------------------------------

#ifndef WAR_MACROS_H
#define WAR_MACROS_H

#include "h/war_data.h"
#include "h/war_debug_macros.h"

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

// COMMENT OPTIMIZE: Duff's Device + SIMD (intrinsics)

#define ALIGN32(p) (uint8_t*)(((uintptr_t)(p) + 31) & ~((uintptr_t)31))

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

#define obj_op_index(obj, op) ((obj) * max_opcodes + (op))

#define STR(x) #x

static inline int war_load_lua(war_lua_context* ctx_lua, const char* lua_file) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, lua_file) != LUA_OK) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return -1;
    }

    lua_getglobal(L, "ctx_lua");
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "ctx_lua not a table\n");
        lua_close(L);
        return -1;
    }

#define LOAD_INT(field)                                                                                                                       \
    lua_getfield(L, -1, #field);                                                                                                              \
    if (lua_type(L, -1) == LUA_TNUMBER) {                                                                                                     \
        ctx_lua->field = (int)lua_tointeger(L, -1);                                                                                           \
        call_carmack("ctx_lua: %s = %d", #field, ctx_lua->field);                                                                             \
    }                                                                                                                                         \
    lua_pop(L, 1);

    // audio
    LOAD_INT(A_SAMPLE_RATE)
    LOAD_INT(A_CHANNEL_COUNT)
    LOAD_INT(A_NOTE_COUNT)
    LOAD_INT(A_LAYER_COUNT)
    LOAD_INT(A_LAYERS_IN_RAM)
    LOAD_INT(A_USERDATA)
    LOAD_INT(A_BASE_FREQUENCY)
    LOAD_INT(A_BASE_NOTE)
    LOAD_INT(A_EDO)
    LOAD_INT(A_NOTES_MAX)
    LOAD_INT(A_CACHE_SIZE)
    LOAD_INT(A_PATH_LIMIT)
    LOAD_INT(A_WARMUP_FRAMES_FACTOR)
    // window render
    LOAD_INT(WR_VIEWS_SAVED)
    LOAD_INT(WR_WARPOON_TEXT_COLS)
    LOAD_INT(WR_STATES)
    LOAD_INT(WR_SEQUENCE_COUNT)
    LOAD_INT(WR_SEQUENCE_LENGTH_MAX)
    LOAD_INT(WR_MODE_COUNT)
    LOAD_INT(WR_KEYSYM_COUNT)
    LOAD_INT(WR_MOD_COUNT)
    LOAD_INT(WR_NOTE_QUADS_MAX)
    LOAD_INT(WR_STATUS_BAR_COLS_MAX)
    LOAD_INT(WR_TEXT_QUADS_MAX)
    LOAD_INT(WR_QUADS_MAX)
    LOAD_INT(WR_WAYLAND_MSG_BUFFER_SIZE)
    LOAD_INT(WR_WAYLAND_MAX_OBJECTS)
    LOAD_INT(WR_WAYLAND_MAX_OP_CODES)
    LOAD_INT(WR_UNDO_NODES_MAX)
    LOAD_INT(WR_TIMESTAMP_LENGTH_MAX)
    LOAD_INT(WR_CURSOR_BLINK_DURATION_US)
    LOAD_INT(WR_REPEAT_DELAY_US)
    LOAD_INT(WR_REPEAT_RATE_US)
    LOAD_INT(WR_UNDO_NOTES_BATCH_MAX)
    LOAD_INT(WR_INPUT_SEQUENCE_LENGTH_MAX)
    LOAD_INT(VK_ATLAS_HEIGHT)
    LOAD_INT(VK_ATLAS_WIDTH)
    // pool
    LOAD_INT(POOL_ALIGNMENT)
    // cmd
    LOAD_INT(CMD_COUNT)
    // pc
    LOAD_INT(PC_BUFFER_SIZE)

#undef LOAD_INT

#define LOAD_FLOAT(field)                                                                                                                     \
    lua_getfield(L, -1, #field);                                                                                                              \
    if (lua_type(L, -1) == LUA_TNUMBER) {                                                                                                     \
        ctx_lua->field = (float)lua_tonumber(L, -1);                                                                                          \
        call_carmack("ctx_lua: %s = %f", #field, ctx_lua->field);                                                                             \
    }                                                                                                                                         \
    lua_pop(L, 1);

    LOAD_FLOAT(A_DEFAULT_ATTACK)
    LOAD_FLOAT(A_DEFAULT_SUSTAIN)
    LOAD_FLOAT(A_DEFAULT_RELEASE)
    LOAD_FLOAT(A_DEFAULT_GAIN)
    LOAD_FLOAT(VK_FONT_PIXEL_HEIGHT)
    LOAD_FLOAT(DEFAULT_ALPHA_SCALE);
    LOAD_FLOAT(DEFAULT_CURSOR_ALPHA_SCALE);
    LOAD_FLOAT(DEFAULT_PLAYBACK_BAR_THICKNESS);
    LOAD_FLOAT(DEFAULT_TEXT_FEATHER);
    LOAD_FLOAT(DEFAULT_TEXT_THICKNESS);
    LOAD_FLOAT(WINDOWED_TEXT_FEATHER);
    LOAD_FLOAT(WINDOWED_TEXT_THICKNESS);
    LOAD_FLOAT(DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE);
    LOAD_FLOAT(DEFAULT_WINDOWED_ALPHA_SCALE);

#undef LOAD_FLOAT

#define LOAD_DOUBLE(field)                                                                                                                    \
    lua_getfield(L, -1, #field);                                                                                                              \
    if (lua_type(L, -1) == LUA_TNUMBER) {                                                                                                     \
        ctx_lua->field = (double)lua_tonumber(L, -1);                                                                                         \
        call_carmack("ctx_lua: %s = %f", #field, ctx_lua->field);                                                                             \
    }                                                                                                                                         \
    lua_pop(L, 1);

    LOAD_DOUBLE(A_DEFAULT_COLUMNS_PER_BEAT)
    LOAD_DOUBLE(A_BPM)
    LOAD_DOUBLE(A_SAMPLE_DURATION)
    LOAD_DOUBLE(WR_FPS)

#undef LOAD_DOUBLE

    lua_close(L);
    return 0;
}

static inline size_t war_get_pool_a_size(war_pool* pool, war_lua_context* ctx_lua, const char* lua_file) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, lua_file) != LUA_OK) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }

    lua_getglobal(L, "pool_a");
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "pool_a not a table\n");
        lua_close(L);
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) { // iterate pool_a entries
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "type");
            const char* type = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "count");
            size_t count = (size_t)lua_tointeger(L, -1);
            lua_pop(L, 1);

            size_t type_size = 0;

            if (strcmp(type, "uint8_t") == 0)
                type_size = sizeof(uint8_t);
            else if (strcmp(type, "uint64_t") == 0)
                type_size = sizeof(uint64_t);
            else if (strcmp(type, "int16_t") == 0)
                type_size = sizeof(int16_t);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "float") == 0)
                type_size = sizeof(float);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "int32_t") == 0)
                type_size = sizeof(int32_t);
            else if (strcmp(type, "void*") == 0)
                type_size = sizeof(void*);
            else if (strcmp(type, "war_audio_context") == 0)
                type_size = sizeof(war_audio_context);
            else if (strcmp(type, "war_cache") == 0)
                type_size = sizeof(war_cache);
            else if (strcmp(type, "char*") == 0)
                type_size = sizeof(char*);
            else if (strcmp(type, "char") == 0)
                type_size = sizeof(char);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t**") == 0)
                type_size = sizeof(int16_t**);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "war_notes") == 0) {
                type_size = sizeof(war_notes);
            } else if (strcmp(type, "bool") == 0) {
                type_size = sizeof(bool);
            } else {
                fprintf(stderr, "Unknown pool_a type: %s\n", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);

    lua_close(L);
    call_carmack("pool_a size: %zu", total_size);
    return total_size;
}

static inline size_t war_get_pool_wr_size(war_pool* pool, war_lua_context* ctx_lua, const char* lua_file) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, lua_file) != LUA_OK) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 0;
    }

    lua_getglobal(L, "pool_wr");
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "pool_wr not a table\n");
        lua_close(L);
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) { // iterate pool_wr entries
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "type");
            const char* type = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "count");
            size_t count = (size_t)lua_tointeger(L, -1);
            lua_pop(L, 1);

            size_t type_size = 0;

            if (strcmp(type, "uint8_t") == 0)
                type_size = sizeof(uint8_t);
            else if (strcmp(type, "uint16_t") == 0)
                type_size = sizeof(uint16_t);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "uint64_t") == 0)
                type_size = sizeof(uint64_t);
            else if (strcmp(type, "int16_t") == 0)
                type_size = sizeof(int16_t);
            else if (strcmp(type, "int32_t") == 0)
                type_size = sizeof(int32_t);
            else if (strcmp(type, "float") == 0)
                type_size = sizeof(float);
            else if (strcmp(type, "double") == 0)
                type_size = sizeof(double);
            else if (strcmp(type, "void*") == 0)
                type_size = sizeof(void*);
            else if (strcmp(type, "char") == 0)
                type_size = sizeof(char);
            else if (strcmp(type, "char*") == 0)
                type_size = sizeof(char*);
            else if (strcmp(type, "bool") == 0)
                type_size = sizeof(bool);
            else if (strcmp(type, "war_undo_node*") == 0)
                type_size = sizeof(war_undo_node*);
            else if (strcmp(type, "war_undo_node") == 0)
                type_size = sizeof(war_undo_node);

            /* --- WR-specific structs --- */
            else if (strcmp(type, "war_fsm_state") == 0)
                type_size = sizeof(war_fsm_state);
            else if (strcmp(type, "war_quad_vertex") == 0)
                type_size = sizeof(war_quad_vertex);
            else if (strcmp(type, "war_note_quads") == 0)
                type_size = sizeof(war_note_quads);
            else if (strcmp(type, "war_text_vertex") == 0)
                type_size = sizeof(war_text_vertex);
            else if (strcmp(type, "war_audio_context") == 0)
                type_size = sizeof(war_audio_context);
            else if (strcmp(type, "war_undo_tree") == 0)
                type_size = sizeof(war_undo_tree);
            else if (strcmp(type, "war_payload_union") == 0)
                type_size = sizeof(war_payload_union);

            /* --- Pointer variants (optional but useful if you define them in
             * Lua) --- */
            else if (strcmp(type, "uint8_t*") == 0)
                type_size = sizeof(uint8_t*);
            else if (strcmp(type, "uint16_t*") == 0)
                type_size = sizeof(uint16_t*);
            else if (strcmp(type, "uint32_t*") == 0)
                type_size = sizeof(uint32_t*);
            else if (strcmp(type, "void**") == 0)
                type_size = sizeof(void**);

            else {
                fprintf(stderr, "Unknown pool_wr type: %s\n", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);

    lua_close(L);
    call_carmack("pool_wr size: %zu", total_size);
    return total_size;
}

static inline void* war_pool_alloc(war_pool* pool, size_t size) {
    size = ALIGN_UP(size, pool->pool_alignment);
    if (pool->pool_ptr + size > (uint8_t*)pool->pool + pool->pool_size) {
        call_carmack("war_pool_alloc not big enough! %zu bytes", size);
        abort();
    }
    void* ptr = pool->pool_ptr;
    pool->pool_ptr += size;
    return ptr;
}

static inline void war_get_top_text(war_window_render_context* ctx_wr, war_lua_context* ctx_lua, char* tmp_str, char* prompt) {
    memset(ctx_wr->text_top_status_bar, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    memset(tmp_str, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    if (getcwd(tmp_str, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX)) != NULL) {
        memcpy(ctx_wr->text_top_status_bar, tmp_str + 1, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    }
    memset(tmp_str, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    snprintf(tmp_str, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX), "%.0f,%.0f", ctx_wr->cursor_pos_y, ctx_wr->cursor_pos_x);
    memcpy(ctx_wr->text_top_status_bar + ctx_wr->text_status_bar_end_index, tmp_str, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    memset(tmp_str, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
}

static inline void war_get_middle_text(
    war_window_render_context* ctx_wr, war_views* views, war_atomics* atomics, war_lua_context* ctx_lua, char* tmp_str, char* prompt) {
    memset(ctx_wr->text_middle_status_bar, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    switch (ctx_wr->mode) {
    case MODE_NORMAL:
        if (atomic_load(&atomics->repeat_section)) {
            double start_frames = (double)atomic_load(&atomics->repeat_start_frames);
            double end_frames = (double)atomic_load(&atomics->repeat_end_frames);
            double bpm = atomic_load(&ctx_lua->A_BPM);
            double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
            uint32_t grid_start = (uint32_t)((start_frames * bpm * 4.0) / (60.0 * sample_rate));
            uint32_t grid_end = (uint32_t)((end_frames * bpm * 4.0) / (60.0 * sample_rate));
            memset(tmp_str, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
            snprintf(tmp_str, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX), "R:%u,%u", grid_start, grid_end);
            memcpy(
                ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_middle_index, tmp_str, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
        }
        break;
    case MODE_VISUAL:
        memcpy(ctx_wr->text_middle_status_bar, "-- VISUAL --", sizeof("-- VISUAL --"));
        break;
    case MODE_VIEWS:
        if (views->warpoon_mode == MODE_VISUAL_LINE) {
            memcpy(ctx_wr->text_middle_status_bar, "-- VIEWS -- -- VISUAL LINE --", sizeof("-- VIEWS -- -- VISUAL LINE --"));
            break;
        }
        memcpy(ctx_wr->text_middle_status_bar, "-- VIEWS --", sizeof("-- VIEWS --"));
        break;
    case MODE_COMMAND: {
        memset(tmp_str, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
        uint32_t max_cols = atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX);
        if (ctx_wr->num_chars_in_prompt > 0) {
            snprintf(tmp_str, ctx_wr->num_chars_in_sequence + ctx_wr->num_chars_in_prompt + 3, "%s: %s", prompt, ctx_wr->input_sequence);
            memcpy(ctx_wr->text_middle_status_bar, tmp_str, ctx_wr->num_chars_in_prompt + ctx_wr->num_chars_in_sequence + 3);
            break;
        }
        snprintf(tmp_str, ctx_wr->num_chars_in_sequence + 2, ":%s", ctx_wr->input_sequence);
        memcpy(ctx_wr->text_middle_status_bar, tmp_str, ctx_wr->num_chars_in_sequence + 2);
        break;
    }
    case MODE_MIDI: {
        switch (atomic_load(&atomics->state)) {
        case AUDIO_CMD_MIDI_RECORD_WAIT:
            memcpy(ctx_wr->text_middle_status_bar, "-- MIDI RECORD WAIT --", sizeof("-- MIDI RECORD WAIT --"));
            break;
        case AUDIO_CMD_MIDI_RECORD:
            memcpy(ctx_wr->text_middle_status_bar, "-- MIDI RECORD --", sizeof("-- MIDI RECORD --"));
            break;
        case AUDIO_CMD_MIDI_RECORD_MAP:
            memcpy(ctx_wr->text_middle_status_bar, "-- MIDI RECORD MAP --", sizeof("-- MIDI RECORD MAP --"));
            break;
        default:
            memcpy(ctx_wr->text_middle_status_bar, "-- MIDI --", sizeof("-- MIDI --"));
            break;
        }
        uint8_t loop = atomic_load(&atomics->loop);
        uint8_t midi_toggle = ctx_wr->midi_toggle;
        if (loop && midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_middle_index, "LOOP TOGGLE", sizeof("LOOP TOGGLE"));
        } else if (loop && !midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_middle_index, "LOOP", sizeof("LOOP"));
        } else if (!loop && midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_middle_index, "TOGGLE", sizeof("TOGGLE"));
        }
        break;
    }
    case MODE_RECORD:
        switch (atomic_load(&atomics->state)) {
        case AUDIO_CMD_RECORD_WAIT:
            memcpy(ctx_wr->text_middle_status_bar, "-- RECORD WAIT --", sizeof("-- RECORD WAIT --"));
            break;
        case AUDIO_CMD_RECORD:
            memcpy(ctx_wr->text_middle_status_bar, "-- RECORD --", sizeof("-- RECORD --"));
            break;
        case AUDIO_CMD_RECORD_MAP:
            memcpy(ctx_wr->text_middle_status_bar, "-- RECORD MAP --", sizeof("-- RECORD MAP --"));
            break;
        }
        break;
    default:
        break;
    }
    if (ctx_wr->cursor_blink_state && ctx_wr->mode != MODE_COMMAND) {
        memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_end_index, ctx_wr->input_sequence, ctx_wr->num_chars_in_sequence);
        uint32_t size = (ctx_wr->cursor_blink_state == CURSOR_BLINK) ? sizeof("BLINK") : sizeof("BPM");
        memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_end_index + 2, (size == sizeof("BLINK")) ? "BLINK" : "BPM", size);
        return;
    }
    if (ctx_wr->mode != MODE_COMMAND) {
        int offset = ctx_wr->text_status_bar_end_index;
        memcpy(ctx_wr->text_middle_status_bar + offset, ctx_wr->input_sequence, ctx_wr->num_chars_in_sequence);
    }
}

static inline void war_get_bottom_text(war_window_render_context* ctx_wr, war_lua_context* ctx_lua, char* tmp_str, char* prompt) {
    memset(ctx_wr->text_bottom_status_bar, 0, atomic_load(&ctx_lua->WR_STATUS_BAR_COLS_MAX));
    memcpy(ctx_wr->text_bottom_status_bar, "[WAR] 1:roll*", sizeof("[WAR] 1:roll*"));
}

static inline void war_get_local_time(char* timestamp, war_lua_context* ctx_lua) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, atomic_load(&ctx_lua->WR_TIMESTAMP_LENGTH_MAX), "%H:%M:%S, %m-%d-%Y", tm_info);
}

static inline void war_get_warpoon_text(war_views* views) {
    for (uint32_t i = 0; i < views->views_count; i++) {
        strncpy(views->warpoon_text[i], "", MAX_WARPOON_TEXT_COLS);
        snprintf(views->warpoon_text[i],
                 MAX_WARPOON_TEXT_COLS,
                 "%d,%d [%d,%d]",
                 views->row[i],
                 views->col[i],
                 views->bottom_row[i],
                 views->left_col[i]);
        call_carmack("i_views: %u", i);
        call_carmack("views row: %u", views->row[i]);
        call_carmack("views col: %u", views->col[i]);
        call_carmack("views bottom_row: %u", views->bottom_row[i]);
        call_carmack("views left_col: %u", views->left_col[i]);
    }
}

static inline void war_warpoon_delete_at_i(war_views* views, uint32_t i_delete) {
    if (i_delete >= views->views_count) return;
    uint32_t last = views->views_count - 1;
    // Shift all SoA arrays to the left to fill the gap
    for (uint32_t j = i_delete; j < last; j++) {
        views->col[j] = views->col[j + 1];
        views->row[j] = views->row[j + 1];
        views->left_col[j] = views->left_col[j + 1];
        views->right_col[j] = views->right_col[j + 1];
        views->bottom_row[j] = views->bottom_row[j + 1];
        views->top_row[j] = views->top_row[j + 1];
    }
    views->views_count--;
}

static inline void war_warpoon_shift_up(war_views* views) {
    if (views->warpoon_row + 1 > views->warpoon_max_row) { return; }

    uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
    if ((int)i_views - 1 < 0 || i_views > views->views_count - 1) { return; }

    uint32_t tmp_col = views->col[i_views - 1];
    uint32_t tmp_row = views->row[i_views - 1];
    uint32_t tmp_left_col = views->left_col[i_views - 1];
    uint32_t tmp_bottom_row = views->bottom_row[i_views - 1];
    uint32_t tmp_right_col = views->right_col[i_views - 1];
    uint32_t tmp_top_row = views->top_row[i_views - 1];
    views->col[i_views - 1] = views->col[i_views];
    views->row[i_views - 1] = views->row[i_views];
    views->left_col[i_views - 1] = views->left_col[i_views];
    views->bottom_row[i_views - 1] = views->bottom_row[i_views];
    views->right_col[i_views - 1] = views->right_col[i_views];
    views->top_row[i_views - 1] = views->top_row[i_views];
    views->col[i_views] = tmp_col;
    views->row[i_views] = tmp_row;
    views->left_col[i_views] = tmp_left_col;
    views->bottom_row[i_views] = tmp_bottom_row;
    views->right_col[i_views] = tmp_right_col;
    views->top_row[i_views] = tmp_top_row;
}

static inline void war_warpoon_shift_down(war_views* views) {
    if (views->warpoon_row == 0) return; // already at bottom in your indexing logic

    uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
    if (i_views + 1 >= views->views_count) return;

    uint32_t tmp_col = views->col[i_views + 1];
    uint32_t tmp_row = views->row[i_views + 1];
    uint32_t tmp_left_col = views->left_col[i_views + 1];
    uint32_t tmp_bottom_row = views->bottom_row[i_views + 1];
    uint32_t tmp_right_col = views->right_col[i_views + 1];
    uint32_t tmp_top_row = views->top_row[i_views + 1];

    views->col[i_views + 1] = views->col[i_views];
    views->row[i_views + 1] = views->row[i_views];
    views->left_col[i_views + 1] = views->left_col[i_views];
    views->bottom_row[i_views + 1] = views->bottom_row[i_views];
    views->right_col[i_views + 1] = views->right_col[i_views];
    views->top_row[i_views + 1] = views->top_row[i_views];

    views->col[i_views] = tmp_col;
    views->row[i_views] = tmp_row;
    views->left_col[i_views] = tmp_left_col;
    views->bottom_row[i_views] = tmp_bottom_row;
    views->right_col[i_views] = tmp_right_col;
    views->top_row[i_views] = tmp_top_row;
}

// --------------------------
// Writer: WR -> Audio (to_a)
static inline bool war_pc_to_a(war_producer_consumer* pc, uint32_t header, uint32_t payload_size, const void* payload) {
    uint32_t total_size = 8 + payload_size; // header(4) + size(4) + payload
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    // free bytes calculation (circular buffer)
    uint32_t free_bytes = (PC_BUFFER_SIZE + read_index - write_index - 1) & (PC_BUFFER_SIZE - 1);
    if (free_bytes < total_size) return false;
    // --- write header (4) + size (4) allowing split ---
    uint32_t cont_bytes = PC_BUFFER_SIZE - write_index;
    if (cont_bytes >= 8) {
        // contiguous place for header+size
        *(uint32_t*)(pc->to_a + write_index) = header;
        *(uint32_t*)(pc->to_a + write_index + 4) = payload_size;
    } else {
        // split header+size across end -> wrap
        if (cont_bytes >= 4) {
            *(uint32_t*)(pc->to_a + write_index) = header;
            *(uint32_t*)pc->to_a = payload_size;
        } else {
            *(uint16_t*)(pc->to_a + write_index) = (uint16_t)header;
            *(uint16_t*)(pc->to_a + write_index + 2) = (uint16_t)(header >> 16);
            *(uint32_t*)pc->to_a = payload_size;
        }
    }
    // --- write payload (may be zero length) allowing split ---
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (PC_BUFFER_SIZE - 1);
        uint32_t first_chunk = PC_BUFFER_SIZE - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_a + payload_write_pos, payload, payload_size);
        } else {
            // wrap
            memcpy(pc->to_a + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_a, (const uint8_t*)payload + first_chunk, payload_size - first_chunk);
        }
    }
    // commit write index
    pc->i_to_a = (write_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Reader: Audio <- WR (from_wr)
static inline bool war_pc_from_wr(war_producer_consumer* pc, uint32_t* out_header, uint32_t* out_size, void* out_payload) {
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    uint32_t used_bytes = (PC_BUFFER_SIZE + write_index - read_index) & (PC_BUFFER_SIZE - 1);
    if (used_bytes < 8) return false; // need at least header+size
    // read header+size (handle split)
    uint32_t cont_bytes = PC_BUFFER_SIZE - read_index;
    if (cont_bytes >= 8) {
        *out_header = *(uint32_t*)(pc->to_a + read_index);
        *out_size = *(uint32_t*)(pc->to_a + read_index + 4);
    } else {
        if (cont_bytes >= 4) {
            *out_header = *(uint32_t*)(pc->to_a + read_index);
            *out_size = *(uint32_t*)pc->to_a;
        } else {
            uint16_t low = *(uint16_t*)(pc->to_a + read_index);
            uint16_t high = *(uint16_t*)(pc->to_a + read_index + 2);
            *out_header = (uint32_t)high << 16 | low;
            *out_size = *(uint32_t*)pc->to_a;
        }
    }
    uint32_t total_size = 8 + *out_size;
    if (used_bytes < total_size) return false; // not all payload present yet
    // read payload (if any)
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (PC_BUFFER_SIZE - 1);
        uint32_t first_chunk = PC_BUFFER_SIZE - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_a + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_a + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk, pc->to_a, *out_size - first_chunk);
        }
    }
    // commit read index
    pc->i_from_wr = (read_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Writer: Main -> WR (to_wr)
static inline bool war_pc_to_wr(war_producer_consumer* pc, uint32_t header, uint32_t payload_size, const void* payload) {
    uint32_t total_size = 8 + payload_size;
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t free_bytes = (PC_BUFFER_SIZE + read_index - write_index - 1) & (PC_BUFFER_SIZE - 1);
    if (free_bytes < total_size) return false;
    // header+size
    uint32_t cont_bytes = PC_BUFFER_SIZE - write_index;
    if (cont_bytes >= 8) {
        *(uint32_t*)(pc->to_wr + write_index) = header;
        *(uint32_t*)(pc->to_wr + write_index + 4) = payload_size;
    } else {
        if (cont_bytes >= 4) {
            *(uint32_t*)(pc->to_wr + write_index) = header;
            *(uint32_t*)pc->to_wr = payload_size;
        } else {
            *(uint16_t*)(pc->to_wr + write_index) = (uint16_t)header;
            *(uint16_t*)(pc->to_wr + write_index + 2) = (uint16_t)(header >> 16);
            *(uint32_t*)pc->to_wr = payload_size;
        }
    }
    // payload
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (PC_BUFFER_SIZE - 1);
        uint32_t first_chunk = PC_BUFFER_SIZE - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_wr + payload_write_pos, payload, payload_size);
        } else {
            memcpy(pc->to_wr + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_wr, (const uint8_t*)payload + first_chunk, payload_size - first_chunk);
        }
    }
    pc->i_to_wr = (write_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Reader: WR <- Main (from_a)
static inline bool war_pc_from_a(war_producer_consumer* pc, uint32_t* out_header, uint32_t* out_size, void* out_payload) {
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t used_bytes = (PC_BUFFER_SIZE + write_index - read_index) & (PC_BUFFER_SIZE - 1);
    if (used_bytes < 8) return false;
    uint32_t cont_bytes = PC_BUFFER_SIZE - read_index;
    if (cont_bytes >= 8) {
        *out_header = *(uint32_t*)(pc->to_wr + read_index);
        *out_size = *(uint32_t*)(pc->to_wr + read_index + 4);
    } else {
        if (cont_bytes >= 4) {
            *out_header = *(uint32_t*)(pc->to_wr + read_index);
            *out_size = *(uint32_t*)pc->to_wr;
        } else {
            uint16_t low = *(uint16_t*)(pc->to_wr + read_index);
            uint16_t high = *(uint16_t*)(pc->to_wr + read_index + 2);
            *out_header = (uint32_t)high << 16 | low;
            *out_size = *(uint32_t*)pc->to_wr;
        }
    }
    uint32_t total_size = 8 + *out_size;
    if (used_bytes < total_size) return false;
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (PC_BUFFER_SIZE - 1);
        uint32_t first_chunk = PC_BUFFER_SIZE - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_wr + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_wr + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk, pc->to_wr, *out_size - first_chunk);
        }
    }
    pc->i_from_a = (read_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

static inline uint64_t war_get_monotonic_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static inline war_rgba_t war_unpack_abgr(uint32_t hex_color) {
    war_rgba_t color;
    color.r = ((hex_color >> 0) & 0xFF) / 255.0f;  // Red
    color.g = ((hex_color >> 8) & 0xFF) / 255.0f;  // Green
    color.b = ((hex_color >> 16) & 0xFF) / 255.0f; // Blue
    color.a = ((hex_color >> 24) & 0xFF) / 255.0f; // Alpha
    return color;
}

static inline int32_t war_to_fixed(float f) { return (int32_t)(f * 256.0f); }

static inline uint32_t war_pad_to_scale(float value, uint32_t scale) {
    uint32_t rounded = (uint32_t)(value + 0.5f);
    return (rounded + scale - 1) / scale * scale;
}

static inline uint64_t war_read_le64(const uint8_t* p) {
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static inline uint32_t war_read_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t war_read_le16(const uint8_t* p) { return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8); }

static inline void war_write_le64(uint8_t* p, uint64_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32);
    p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48);
    p[7] = (uint8_t)(v >> 56);
}

static inline void war_write_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void war_write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static inline uint32_t war_clamp_add_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t sum = (uint64_t)a + b;
    uint64_t mask = -(sum > max_value);
    return (uint32_t)((sum & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t war_clamp_subtract_uint32(uint32_t a, uint32_t b, uint32_t min_value) {
    uint32_t diff = a - b;
    uint32_t underflow_mask = -(a < b);
    uint32_t below_min_mask = -(diff < min_value);
    uint32_t clamped_diff = (diff & ~below_min_mask) | (min_value & below_min_mask);
    return (clamped_diff & ~underflow_mask) | (min_value & underflow_mask);
}

static inline uint32_t war_clamp_multiply_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t prod = (uint64_t)a * (uint64_t)b;
    uint64_t mask = -(prod > max_value);
    return (uint32_t)((prod & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t war_clamp_uint32(uint32_t a, uint32_t min_value, uint32_t max_value) {
    a = a < min_value ? min_value : a;
    a = a > max_value ? max_value : a;
    return a;
}

static inline uint64_t war_align64(uint64_t value) { return (value + 63) & ~63ULL; }

static inline uint16_t war_normalize_keysym(xkb_keysym_t ks) {
    if ((ks >= XKB_KEY_a && ks <= XKB_KEY_z) || (ks >= XKB_KEY_0 && ks <= XKB_KEY_9)) { return (uint16_t)ks; }
    switch (ks) {
    case XKB_KEY_Escape:
        return KEYSYM_ESCAPE;
    case XKB_KEY_apostrophe:
        return KEYSYM_APOSTROPHE;
    case XKB_KEY_BackSpace:
        return KEYSYM_BACKSPACE;
    case XKB_KEY_Left:
        return KEYSYM_LEFT;
    case XKB_KEY_Up:
        return KEYSYM_UP;
    case XKB_KEY_Right:
        return KEYSYM_RIGHT;
    case XKB_KEY_Down:
        return KEYSYM_DOWN;
    case XKB_KEY_Return:
        return KEYSYM_RETURN;
    case XKB_KEY_space:
        return KEYSYM_SPACE;
    case XKB_KEY_Tab:
        return KEYSYM_TAB;
    case XKB_KEY_minus:
        return KEYSYM_MINUS;
    case XKB_KEY_comma:
        return KEYSYM_COMMA;
    case XKB_KEY_equal:
        return KEYSYM_EQUAL;
    case XKB_KEY_plus:
        return KEYSYM_PLUS;
    case XKB_KEY_bracketleft:
        return KEYSYM_LEFTBRACKET;
    case XKB_KEY_bracketright:
        return KEYSYM_RIGHTBRACKET;
    case XKB_KEY_colon:
        return KEYSYM_SEMICOLON;
    case 65056:
        return KEYSYM_TAB;
    case XKB_KEY_A:
        return XKB_KEY_a;
    case XKB_KEY_B:
        return XKB_KEY_b;
    case XKB_KEY_C:
        return XKB_KEY_c;
    case XKB_KEY_D:
        return XKB_KEY_d;
    case XKB_KEY_E:
        return XKB_KEY_e;
    case XKB_KEY_F:
        return XKB_KEY_f;
    case XKB_KEY_G:
        return XKB_KEY_g;
    case XKB_KEY_H:
        return XKB_KEY_h;
    case XKB_KEY_I:
        return XKB_KEY_i;
    case XKB_KEY_J:
        return XKB_KEY_j;
    case XKB_KEY_K:
        return XKB_KEY_k;
    case XKB_KEY_L:
        return XKB_KEY_l;
    case XKB_KEY_M:
        return XKB_KEY_m;
    case XKB_KEY_N:
        return XKB_KEY_n;
    case XKB_KEY_O:
        return XKB_KEY_o;
    case XKB_KEY_P:
        return XKB_KEY_p;
    case XKB_KEY_Q:
        return XKB_KEY_q;
    case XKB_KEY_R:
        return XKB_KEY_r;
    case XKB_KEY_S:
        return XKB_KEY_s;
    case XKB_KEY_T:
        return XKB_KEY_t;
    case XKB_KEY_U:
        return XKB_KEY_u;
    case XKB_KEY_V:
        return XKB_KEY_v;
    case XKB_KEY_W:
        return XKB_KEY_w;
    case XKB_KEY_X:
        return XKB_KEY_x;
    case XKB_KEY_Y:
        return XKB_KEY_y;
    case XKB_KEY_Z:
        return XKB_KEY_z;
    case XKB_KEY_exclam:
        return XKB_KEY_1;
    case XKB_KEY_at:
        return XKB_KEY_2;
    case XKB_KEY_numbersign:
        return XKB_KEY_3;
    case XKB_KEY_dollar:
        return XKB_KEY_4;
    case XKB_KEY_percent:
        return XKB_KEY_5;
    case XKB_KEY_asciicircum:
        return XKB_KEY_6;
    case XKB_KEY_ampersand:
        return XKB_KEY_7;
    case XKB_KEY_asterisk:
        return XKB_KEY_8;
    case XKB_KEY_parenleft:
        return XKB_KEY_9;
    case XKB_KEY_parenright:
        return XKB_KEY_0;
    default:
        return KEYSYM_DEFAULT; // fallback / unknown
    }
}

static inline void war_get_frame_duration_us(war_window_render_context* ctx_wr) {
    const double microsecond_conversion = 1000000;
    ctx_wr->frame_duration_us = (uint64_t)round((1.0 / (double)ctx_wr->FPS) * microsecond_conversion);
}

static inline char war_keysym_to_char(xkb_keysym_t ks, uint8_t mod) {
    char lowercase = ks;
    int mod_shift_difference = (mod == MOD_SHIFT ? 32 : 0);
    // Letters a-z or A-Z -> always lowercase
    if (ks >= XKB_KEY_a && ks <= XKB_KEY_z) return (char)ks - mod_shift_difference;
    if (ks >= XKB_KEY_A && ks <= XKB_KEY_Z) return (char)(ks - XKB_KEY_A + 'a');

    // Numbers 0-9
    if (ks >= XKB_KEY_0 && ks <= XKB_KEY_9 && mod == 0) return (char)ks;

    switch (ks) {
    case KEYSYM_SPACE:
        return ' ';
    case KEYSYM_APOSTROPHE:
        return '\'';
    case KEYSYM_COMMA:
        return ',';
    case KEYSYM_MINUS:
        return '-';
    }

    return 0; // non-printable / special keys
}

static inline int war_num_digits(uint32_t n) {
    int digits = 0;
    do {
        digits++;
        n /= 10;
    } while (n != 0);
    return digits;
}

int war_compare_desc_uint32(const void* a, const void* b) {
    uint32_t f_a = *(const uint32_t*)a;
    uint32_t f_b = *(const uint32_t*)b;

    if (f_b > f_a) return 1;
    if (f_b < f_a) return -1;
    return 0;
}

static inline void war_wl_surface_set_opaque_region(int fd, uint32_t wl_surface_id, uint32_t wl_region_id) {
    uint8_t set_opaque_region[12];
    war_write_le32(set_opaque_region, wl_surface_id);
    war_write_le16(set_opaque_region + 4, 4);
    war_write_le16(set_opaque_region + 6, 12);
    war_write_le32(set_opaque_region + 8, wl_region_id);
    // dump_bytes("wl_surface::set_opaque_region request", set_opaque_region,
    // 12);
    ssize_t set_opaque_region_written = write(fd, set_opaque_region, 12);
    assert(set_opaque_region_written == 12);
}

static inline void war_make_text_quad(war_text_vertex* text_vertices,
                                      uint16_t* text_indices,
                                      uint32_t* text_vertices_count,
                                      uint32_t* text_indices_count,
                                      float bottom_left_pos[3],
                                      float span[2],
                                      uint32_t color,
                                      war_glyph_info* glyph_info,
                                      float thickness,
                                      float feather,
                                      uint32_t flags) {
    text_vertices[*text_vertices_count] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_pos[0], bottom_left_pos[1], bottom_left_pos[2]},
        .uv = {glyph_info->uv_x0, glyph_info->uv_y1},
        .glyph_size = {glyph_info->width, glyph_info->height},
        .glyph_bearing = {glyph_info->bearing_x, glyph_info->bearing_y},
        .ascent = glyph_info->ascent,
        .descent = glyph_info->descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
        .flags = flags,
    };
    text_vertices[*text_vertices_count + 1] = (war_text_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1], bottom_left_pos[2]},
        .uv = {glyph_info->uv_x1, glyph_info->uv_y1},
        .glyph_size = {glyph_info->width, glyph_info->height},
        .glyph_bearing = {glyph_info->bearing_x, glyph_info->bearing_y},
        .ascent = glyph_info->ascent,
        .descent = glyph_info->descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
        .flags = flags,
    };
    text_vertices[*text_vertices_count + 2] = (war_text_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .uv = {glyph_info->uv_x1, glyph_info->uv_y0},
        .glyph_size = {glyph_info->width, glyph_info->height},
        .glyph_bearing = {glyph_info->bearing_x, glyph_info->bearing_y},
        .ascent = glyph_info->ascent,
        .descent = glyph_info->descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
        .flags = flags,
    };
    text_vertices[*text_vertices_count + 3] = (war_text_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .uv = {glyph_info->uv_x0, glyph_info->uv_y0},
        .color = color,
        .glyph_size = {glyph_info->width, glyph_info->height},
        .glyph_bearing = {glyph_info->bearing_x, glyph_info->bearing_y},
        .ascent = glyph_info->ascent,
        .descent = glyph_info->descent,
        .thickness = thickness,
        .feather = feather,
        .flags = flags,
    };
    text_indices[*text_indices_count] = *text_vertices_count;
    text_indices[*text_indices_count + 1] = *text_vertices_count + 1;
    text_indices[*text_indices_count + 2] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 3] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 4] = *text_vertices_count + 3;
    text_indices[*text_indices_count + 5] = *text_vertices_count;
    (*text_vertices_count) += 4;
    (*text_indices_count) += 6;
}

static inline void
war_make_blank_text_quad(war_text_vertex* text_vertices, uint16_t* text_indices, uint32_t* text_vertices_count, uint32_t* text_indices_count) {
    text_vertices[*text_vertices_count] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
        .flags = 0,
    };
    text_vertices[*text_vertices_count + 1] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
        .flags = 0,
    };
    text_vertices[*text_vertices_count + 2] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
        .flags = 0,
    };
    text_vertices[*text_vertices_count + 3] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
        .flags = 0,
    };
    text_indices[*text_indices_count] = *text_vertices_count;
    text_indices[*text_indices_count + 1] = *text_vertices_count + 1;
    text_indices[*text_indices_count + 2] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 3] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 4] = *text_vertices_count + 3;
    text_indices[*text_indices_count + 5] = *text_vertices_count;
    (*text_vertices_count) += 4;
    (*text_indices_count) += 6;
}

static inline void war_make_quad(war_quad_vertex* quad_vertices,
                                 uint16_t* quad_indices,
                                 uint32_t* vertices_count,
                                 uint32_t* indices_count,
                                 float bottom_left_pos[3],
                                 float span[2],
                                 uint32_t color,
                                 float outline_thickness,
                                 uint32_t outline_color,
                                 float line_thickness[2],
                                 uint32_t flags) {
    quad_vertices[*vertices_count] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_pos[0], bottom_left_pos[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_vertices[*vertices_count + 1] = (war_quad_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_indices[*indices_count] = *vertices_count;
    quad_indices[*indices_count + 1] = *vertices_count + 1;
    quad_indices[*indices_count + 2] = *vertices_count + 2;
    quad_indices[*indices_count + 3] = *vertices_count + 2;
    quad_indices[*indices_count + 4] = *vertices_count + 3;
    quad_indices[*indices_count + 5] = *vertices_count;
    (*vertices_count) += 4;
    (*indices_count) += 6;
}

static inline void war_make_transparent_quad(war_quad_vertex* transparent_quad_vertices,
                                             uint16_t* transparent_quad_indices,
                                             uint32_t* vertices_count,
                                             uint32_t* indices_count,
                                             float bottom_left_pos[3],
                                             float span[2],
                                             uint32_t color,
                                             float outline_thickness,
                                             uint32_t outline_color,
                                             float line_thickness[2],
                                             uint32_t flags) {
    transparent_quad_vertices[*vertices_count] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_pos[0], bottom_left_pos[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_vertices[*vertices_count + 1] = (war_quad_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + span[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0], bottom_left_pos[1] + span[1], bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_indices[*indices_count] = *vertices_count;
    transparent_quad_indices[*indices_count + 1] = *vertices_count + 1;
    transparent_quad_indices[*indices_count + 2] = *vertices_count + 2;
    transparent_quad_indices[*indices_count + 3] = *vertices_count + 2;
    transparent_quad_indices[*indices_count + 4] = *vertices_count + 3;
    transparent_quad_indices[*indices_count + 5] = *vertices_count;
    (*vertices_count) += 4;
    (*indices_count) += 6;
}

static inline void
war_make_blank_quad(war_quad_vertex* quad_vertices, uint16_t* quad_indices, uint32_t* vertices_count, uint32_t* indices_count) {
    quad_vertices[*vertices_count] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .span = {0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 1] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .span = {0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .span = {0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .span = {0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_indices[*indices_count] = *vertices_count;
    quad_indices[*indices_count + 1] = *vertices_count + 1;
    quad_indices[*indices_count + 2] = *vertices_count + 2;
    quad_indices[*indices_count + 3] = *vertices_count + 2;
    quad_indices[*indices_count + 4] = *vertices_count + 3;
    quad_indices[*indices_count + 5] = *vertices_count;
    (*vertices_count) += 4;
    (*indices_count) += 6;
}

static inline uint32_t war_gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static inline uint32_t war_lcm(uint32_t a, uint32_t b) { return a / war_gcd(a, b) * b; }

static inline float war_midi_to_frequency(float midi_note) { return 440.0f * pow(2.0f, (midi_note - 69) / 12.0f); }

static inline float war_sine_phase_increment(war_audio_context* ctx_a, float frequency) {
    return (2.0f * M_PI * frequency) / (float)ctx_a->sample_rate;
}

static inline void war_undo_tree_add_note(
    war_undo_node* node, war_lua_context* ctx_lua, war_note_quads* note_quads, war_producer_consumer* pc, uint8_t* tmp_payload) {
    call_carmack("undo_add_note");
    war_note_quad note_quad = node->payload.add_note.note_quad;
    war_note note = node->payload.add_note.note;
    // --- Binary search for existing note ---
    uint32_t left = 0;
    uint32_t right = note_quads->count;
    bool already_in = false;
    while (left < right) {
        uint32_t mid = left + (right - left) / 2;
        if (note_quads->id[mid] == note.id) {
            note_quads->alive[mid] = 1; // revive existing note
            already_in = true;
            war_pc_to_a(pc, AUDIO_CMD_REVIVE_NOTE, sizeof(mid), &mid);
            break;
        } else if (note_quads->id[mid] < note.id) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    if (already_in) { return; } // nothing more to do
    uint32_t insert_idx = left;
    uint32_t note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
    // --- Compaction if needed ---
    if (note_quads->count + 1 >= note_quads_max) {
        war_pc_to_a(pc, AUDIO_CMD_COMPACT, 0, NULL);
        uint32_t write_idx = 0;
        uint32_t new_insert_idx = 0xFFFFFFFF; // invalid sentinel
        for (uint32_t read_idx = 0; read_idx < note_quads->count; read_idx++) {
            if (note_quads->alive[read_idx]) {
                if (write_idx != read_idx) {
                    // Copy all fields
                    note_quads->pos_x[write_idx] = note_quads->pos_x[read_idx];
                    note_quads->pos_y[write_idx] = note_quads->pos_y[read_idx];
                    note_quads->size_x[write_idx] = note_quads->size_x[read_idx];
                    note_quads->size_x_numerator[write_idx] = note_quads->size_x_numerator[read_idx];
                    note_quads->size_x_denominator[write_idx] = note_quads->size_x_denominator[read_idx];
                    note_quads->navigation_x[write_idx] = note_quads->navigation_x[read_idx];
                    note_quads->navigation_x_numerator[write_idx] = note_quads->navigation_x_numerator[read_idx];
                    note_quads->navigation_x_denominator[write_idx] = note_quads->navigation_x_denominator[read_idx];
                    note_quads->color[write_idx] = note_quads->color[read_idx];
                    note_quads->outline_color[write_idx] = note_quads->outline_color[read_idx];
                    note_quads->gain[write_idx] = note_quads->gain[read_idx];
                    note_quads->voice[write_idx] = note_quads->voice[read_idx];
                    note_quads->alive[write_idx] = note_quads->alive[read_idx];
                    note_quads->id[write_idx] = note_quads->id[read_idx];
                }
                // Track new insert index
                if (note_quads->id[write_idx] < note.id) { new_insert_idx = write_idx + 1; }
                write_idx++;
            }
        }
        if (write_idx >= note_quads_max) {
            call_carmack("TODO: implement spillover swapfile");
            return;
        };
        note_quads->count = write_idx;
        // Update insert_idx after compaction
        insert_idx = (new_insert_idx != 0xFFFFFFFF) ? new_insert_idx : note_quads->count;
    }
    // --- Shift elements forward to make space ---
    for (uint32_t i = note_quads->count; i > insert_idx; i--) {
        note_quads->pos_x[i] = note_quads->pos_x[i - 1];
        note_quads->pos_y[i] = note_quads->pos_y[i - 1];
        note_quads->size_x[i] = note_quads->size_x[i - 1];
        note_quads->size_x_numerator[i] = note_quads->size_x_numerator[i - 1];
        note_quads->size_x_denominator[i] = note_quads->size_x_denominator[i - 1];
        note_quads->navigation_x[i] = note_quads->navigation_x[i - 1];
        note_quads->navigation_x_numerator[i] = note_quads->navigation_x_numerator[i - 1];
        note_quads->navigation_x_denominator[i] = note_quads->navigation_x_denominator[i - 1];
        note_quads->color[i] = note_quads->color[i - 1];
        note_quads->outline_color[i] = note_quads->outline_color[i - 1];
        note_quads->gain[i] = note_quads->gain[i - 1];
        note_quads->voice[i] = note_quads->voice[i - 1];
        note_quads->alive[i] = note_quads->alive[i - 1];
        note_quads->id[i] = note_quads->id[i - 1];
    }
    // --- Insert new note ---
    note_quads->pos_x[insert_idx] = note_quad.pos_x;
    note_quads->pos_y[insert_idx] = note_quad.pos_y;
    note_quads->size_x[insert_idx] = note_quad.size_x;
    note_quads->size_x_numerator[insert_idx] = note_quad.size_x_numerator;
    note_quads->size_x_denominator[insert_idx] = note_quad.size_x_denominator;
    note_quads->navigation_x[insert_idx] = note_quad.navigation_x;
    note_quads->navigation_x_numerator[insert_idx] = note_quad.navigation_x_numerator;
    note_quads->navigation_x_denominator[insert_idx] = note_quad.navigation_x_denominator;
    note_quads->color[insert_idx] = note_quad.color;
    note_quads->outline_color[insert_idx] = note_quad.outline_color;
    note_quads->gain[insert_idx] = note_quad.gain;
    note_quads->voice[insert_idx] = note_quad.voice;
    note_quads->alive[insert_idx] = 1;
    note_quads->id[insert_idx] = note_quad.id;
    note_quads->count++;
    *(war_note*)tmp_payload = note;
    *(uint32_t*)(tmp_payload + sizeof(war_note)) = insert_idx;
    war_pc_to_a(pc, AUDIO_CMD_INSERT_NOTE, sizeof(note) + sizeof(insert_idx), tmp_payload);
}

static inline void war_undo_tree_delete_note(war_undo_node* node, war_note_quads* note_quads, war_producer_consumer* pc) {
    call_carmack("undo_delete_note");
    int32_t delete_idx = -1;
    for (uint32_t i = 0; i < note_quads->count; i++) {
        assert(i < note_quads->count); // bounds check
        if (note_quads->id[i] == node->payload.delete_note.note.id) {
            note_quads->alive[i] = 0;
            delete_idx = i;
            break;
        }
    }
    war_pc_to_a(pc, AUDIO_CMD_DELETE_NOTE, sizeof(int32_t), &delete_idx);
}

static inline void
war_undo_tree_delete_notes(war_undo_node* node, war_note_quads* note_quads, war_producer_consumer* pc, uint8_t* tmp_payload) {
    call_carmack("undo_delete_notes");
    uint32_t delete_count = node->payload.delete_notes.count;
    war_note_quad* note_quad = node->payload.delete_notes.note_quad;
    uint32_t count = 0;
    for (int32_t i = note_quads->count - 1; i >= 0 && count < delete_count; i--) {
        for (uint32_t k = 0; k < delete_count; k++) {
            if (note_quads->alive[i] == 0 || note_quads->hidden[i]) { continue; }
            if (note_quads->id[i] != note_quad[k].id) { continue; }
            note_quads->alive[i] = 0;
            *(uint32_t*)(tmp_payload + sizeof(uint32_t) * (count + 1)) = i;
            count++;
        }
    }
    *(uint32_t*)(tmp_payload) = count;
    war_pc_to_a(pc, AUDIO_CMD_DELETE_NOTES, sizeof(uint32_t) * (count + 1), tmp_payload);
}

static inline void war_undo_tree_add_notes(
    war_undo_node* node, war_note_quads* note_quads, war_producer_consumer* pc, uint8_t* tmp_payload, war_lua_context* ctx_lua) {
    call_carmack("undo_add_notes");
    war_note* note = node->payload.add_notes.note;
    war_note_quad* note_quad = node->payload.add_notes.note_quad;
    uint32_t count = node->payload.add_notes.count;
    // --- Binary search for existing note ---
    uint32_t already_in_count = 0;
    uint32_t insert_indices_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t left = 0;
        uint32_t right = note_quads->count;
        while (left < right) {
            uint32_t mid = left + (right - left) / 2;
            if (note_quads->id[mid] == note[i].id) {
                note_quads->alive[mid] = 1; // revive existing note
                *(uint32_t*)(tmp_payload + (already_in_count + 1) * sizeof(uint32_t)) = mid;
                already_in_count++;
                break;
            } else if (note_quads->id[mid] < note[i].id) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
        *(uint32_t*)(tmp_payload + (count + 1 + insert_indices_count) * sizeof(uint32_t)) = left; // not sture if right
        insert_indices_count++;
    }
    *(uint32_t*)tmp_payload = already_in_count;
    *(uint32_t*)(tmp_payload + count * sizeof(uint32_t)) = insert_indices_count;
    if (already_in_count == count) {
        war_pc_to_a(pc, AUDIO_CMD_REVIVE_NOTES, sizeof(uint32_t) * (already_in_count + 1), tmp_payload);
        return;
    } else if (already_in_count > 0) {
        war_pc_to_a(pc, AUDIO_CMD_REVIVE_NOTES, sizeof(uint32_t) * (already_in_count + 1), tmp_payload);
    }
    uint32_t* insert_indices = (uint32_t*)(tmp_payload + (count + 1) * sizeof(uint32_t));
    uint32_t note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
    // --- Compaction if needed ---
    if (note_quads->count + insert_indices_count >= note_quads_max) { // Check against actual new notes count
        war_pc_to_a(pc, AUDIO_CMD_COMPACT, 0, NULL);
        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < note_quads->count; read_idx++) {
            if (note_quads->alive[read_idx]) {
                if (write_idx != read_idx) {
                    // Copy all fields (same as original)
                    note_quads->pos_x[write_idx] = note_quads->pos_x[read_idx];
                    note_quads->pos_y[write_idx] = note_quads->pos_y[read_idx];
                    note_quads->size_x[write_idx] = note_quads->size_x[read_idx];
                    note_quads->size_x_numerator[write_idx] = note_quads->size_x_numerator[read_idx];
                    note_quads->size_x_denominator[write_idx] = note_quads->size_x_denominator[read_idx];
                    note_quads->navigation_x[write_idx] = note_quads->navigation_x[read_idx];
                    note_quads->navigation_x_numerator[write_idx] = note_quads->navigation_x_numerator[read_idx];
                    note_quads->navigation_x_denominator[write_idx] = note_quads->navigation_x_denominator[read_idx];
                    note_quads->color[write_idx] = note_quads->color[read_idx];
                    note_quads->outline_color[write_idx] = note_quads->outline_color[read_idx];
                    note_quads->gain[write_idx] = note_quads->gain[read_idx];
                    note_quads->voice[write_idx] = note_quads->voice[read_idx];
                    note_quads->alive[write_idx] = note_quads->alive[read_idx];
                    note_quads->id[write_idx] = note_quads->id[read_idx];
                }
                write_idx++;
            }
        }
        if (write_idx + insert_indices_count >= note_quads_max) {
            call_carmack("TODO: implement spillover swapfile");
            return; // Or return, depending on context
        }
        note_quads->count = write_idx;

        // Update all insert_indices after compaction
        for (uint32_t i = 0; i < insert_indices_count; i++) {
            uint32_t left = 0;
            uint32_t right = note_quads->count;
            while (left < right) {
                uint32_t mid = left + (right - left) / 2;
                if (note_quads->id[mid] < note[i].id) {
                    left = mid + 1;
                } else {
                    right = mid;
                }
            }
            insert_indices[i] = left;
        }
    }
    // After compaction... issues here probably
    // Collect new notes (not already-in) and their indices
    uint32_t new_notes_count = 0;
    uint32_t* new_note_indices = (uint32_t*)(tmp_payload + (count + 1) * sizeof(uint32_t));             // Reuse space
    uint32_t* final_insert_indices = (uint32_t*)(tmp_payload + (count + 1 + count) * sizeof(uint32_t)); // After potential max
    for (uint32_t i = 0; i < count; i++) {
        bool is_already_in = false;
        for (uint32_t j = 0; j < already_in_count; j++) {
            if (*(uint32_t*)(tmp_payload + (j + 1) * sizeof(uint32_t)) ==
                i) { // Check if i is in already_in (assuming payload tracks original indices)
                is_already_in = true;
                break;
            }
        }
        if (!is_already_in) {
            new_note_indices[new_notes_count] = i;
            final_insert_indices[new_notes_count] = insert_indices[i];
            new_notes_count++;
        }
    }
    insert_indices_count = new_notes_count; // Update count
    *(uint32_t*)(tmp_payload + count * sizeof(uint32_t)) = insert_indices_count;
    // Sort new notes by descending insert index
    for (uint32_t p = 0; p < new_notes_count - 1; p++) {
        for (uint32_t q = p + 1; q < new_notes_count; q++) {
            if (final_insert_indices[p] < final_insert_indices[q]) {
                // Swap
                uint32_t temp_idx = final_insert_indices[p];
                uint32_t temp_note = new_note_indices[p];
                final_insert_indices[p] = final_insert_indices[q];
                new_note_indices[p] = new_note_indices[q];
                final_insert_indices[q] = temp_idx;
                new_note_indices[q] = temp_note;
            }
        }
    }
    // Insert in descending order
    for (uint32_t k = 0; k < new_notes_count; k++) {
        uint32_t insert_idx = final_insert_indices[k];
        uint32_t note_idx = new_note_indices[k];

        // Shift elements forward
        for (uint32_t i = note_quads->count; i > insert_idx; i--) {
            note_quads->pos_x[i] = note_quads->pos_x[i - 1];
            note_quads->pos_y[i] = note_quads->pos_y[i - 1];
            note_quads->size_x[i] = note_quads->size_x[i - 1];
            note_quads->size_x_numerator[i] = note_quads->size_x_numerator[i - 1];
            note_quads->size_x_denominator[i] = note_quads->size_x_denominator[i - 1];
            note_quads->navigation_x[i] = note_quads->navigation_x[i - 1];
            note_quads->navigation_x_numerator[i] = note_quads->navigation_x_numerator[i - 1];
            note_quads->navigation_x_denominator[i] = note_quads->navigation_x_denominator[i - 1];
            note_quads->color[i] = note_quads->color[i - 1];
            note_quads->outline_color[i] = note_quads->outline_color[i - 1];
            note_quads->gain[i] = note_quads->gain[i - 1];
            note_quads->voice[i] = note_quads->voice[i - 1];
            note_quads->alive[i] = note_quads->alive[i - 1];
            note_quads->id[i] = note_quads->id[i - 1];
        }

        // Insert new note
        note_quads->pos_x[insert_idx] = note_quad[note_idx].pos_x;
        note_quads->pos_y[insert_idx] = note_quad[note_idx].pos_y;
        note_quads->size_x[insert_idx] = note_quad[note_idx].size_x;
        note_quads->size_x_numerator[insert_idx] = note_quad[note_idx].size_x_numerator;
        note_quads->size_x_denominator[insert_idx] = note_quad[note_idx].size_x_denominator;
        note_quads->navigation_x[insert_idx] = note_quad[note_idx].navigation_x;
        note_quads->navigation_x_numerator[insert_idx] = note_quad[note_idx].navigation_x_numerator;
        note_quads->navigation_x_denominator[insert_idx] = note_quad[note_idx].navigation_x_denominator;
        note_quads->color[insert_idx] = note_quad[note_idx].color;
        note_quads->outline_color[insert_idx] = note_quad[note_idx].outline_color;
        note_quads->gain[insert_idx] = note_quad[note_idx].gain;
        note_quads->voice[insert_idx] = note_quad[note_idx].voice;
        note_quads->alive[insert_idx] = 1;
        note_quads->id[insert_idx] = note_quad[note_idx].id;
        note_quads->count++;

        // Send insert command
        *(war_note*)(tmp_payload) = note[note_idx];
        *(uint32_t*)(tmp_payload + sizeof(war_note)) = insert_idx;
        war_pc_to_a(pc, AUDIO_CMD_INSERT_NOTE, sizeof(war_note) + sizeof(uint32_t), tmp_payload);
    }
}

static inline void war_undo_tree_add_notes_same(
    war_undo_node* node, war_note_quads* note_quads, war_producer_consumer* pc, uint8_t* tmp_payload, war_lua_context* ctx_lua) {
    call_carmack("undo_add_notes_same");
    war_note note = node->payload.add_notes_same.note;
    war_note_quad note_quad = node->payload.add_notes_same.note_quad;
    uint64_t* ids = node->payload.add_notes_same.ids;
    uint32_t count = node->payload.add_notes_same.count;
    for (uint32_t i = 0; i < count; i++) {
        war_note_quad nq = note_quad;
        nq.id = ids[i];
        war_note n = note;
        n.id = ids[i];
        // --- Binary search for existing note ---
        uint32_t left = 0;
        uint32_t right = note_quads->count;
        bool already_in = false;
        while (left < right) {
            uint32_t mid = left + (right - left) / 2;
            if (note_quads->id[mid] == n.id) {
                note_quads->alive[mid] = 1; // revive existing note
                already_in = true;
                war_pc_to_a(pc, AUDIO_CMD_REVIVE_NOTE, sizeof(mid), &mid);
                break;
            } else if (note_quads->id[mid] < n.id) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
        if (already_in) { continue; } // nothing more to do
        uint32_t insert_idx = left;
        uint32_t note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
        // --- Compaction if needed ---
        if (note_quads->count + 1 >= note_quads_max) {
            war_pc_to_a(pc, AUDIO_CMD_COMPACT, 0, NULL);
            uint32_t write_idx = 0;
            uint32_t new_insert_idx = 0xFFFFFFFF; // invalid sentinel
            for (uint32_t read_idx = 0; read_idx < note_quads->count; read_idx++) {
                if (note_quads->alive[read_idx]) {
                    if (write_idx != read_idx) {
                        // Copy all fields
                        note_quads->pos_x[write_idx] = note_quads->pos_x[read_idx];
                        note_quads->pos_y[write_idx] = note_quads->pos_y[read_idx];
                        note_quads->size_x[write_idx] = note_quads->size_x[read_idx];
                        note_quads->size_x_numerator[write_idx] = note_quads->size_x_numerator[read_idx];
                        note_quads->size_x_denominator[write_idx] = note_quads->size_x_denominator[read_idx];
                        note_quads->navigation_x[write_idx] = note_quads->navigation_x[read_idx];
                        note_quads->navigation_x_numerator[write_idx] = note_quads->navigation_x_numerator[read_idx];
                        note_quads->navigation_x_denominator[write_idx] = note_quads->navigation_x_denominator[read_idx];
                        note_quads->color[write_idx] = note_quads->color[read_idx];
                        note_quads->outline_color[write_idx] = note_quads->outline_color[read_idx];
                        note_quads->gain[write_idx] = note_quads->gain[read_idx];
                        note_quads->voice[write_idx] = note_quads->voice[read_idx];
                        note_quads->alive[write_idx] = note_quads->alive[read_idx];
                        note_quads->id[write_idx] = note_quads->id[read_idx];
                    }
                    // Track new insert index
                    if (note_quads->id[write_idx] < n.id) { new_insert_idx = write_idx + 1; }
                    write_idx++;
                }
            }
            if (write_idx >= note_quads_max) {
                call_carmack("TODO: implement spillover swapfile");
                continue;
            };
            note_quads->count = write_idx;
            // Update insert_idx after compaction
            insert_idx = (new_insert_idx != 0xFFFFFFFF) ? new_insert_idx : note_quads->count;
        }
        // --- Shift elements forward to make space ---
        for (uint32_t j = note_quads->count; j > insert_idx; j--) {
            note_quads->pos_x[j] = note_quads->pos_x[j - 1];
            note_quads->pos_y[j] = note_quads->pos_y[j - 1];
            note_quads->size_x[j] = note_quads->size_x[j - 1];
            note_quads->size_x_numerator[j] = note_quads->size_x_numerator[j - 1];
            note_quads->size_x_denominator[j] = note_quads->size_x_denominator[j - 1];
            note_quads->navigation_x[j] = note_quads->navigation_x[j - 1];
            note_quads->navigation_x_numerator[j] = note_quads->navigation_x_numerator[j - 1];
            note_quads->navigation_x_denominator[j] = note_quads->navigation_x_denominator[j - 1];
            note_quads->color[j] = note_quads->color[j - 1];
            note_quads->outline_color[j] = note_quads->outline_color[j - 1];
            note_quads->gain[j] = note_quads->gain[j - 1];
            note_quads->voice[j] = note_quads->voice[j - 1];
            note_quads->alive[j] = note_quads->alive[j - 1];
            note_quads->id[j] = note_quads->id[j - 1];
        }
        // --- Insert new note ---
        note_quads->pos_x[insert_idx] = nq.pos_x;
        note_quads->pos_y[insert_idx] = nq.pos_y;
        note_quads->size_x[insert_idx] = nq.size_x;
        note_quads->size_x_numerator[insert_idx] = nq.size_x_numerator;
        note_quads->size_x_denominator[insert_idx] = nq.size_x_denominator;
        note_quads->navigation_x[insert_idx] = nq.navigation_x;
        note_quads->navigation_x_numerator[insert_idx] = nq.navigation_x_numerator;
        note_quads->navigation_x_denominator[insert_idx] = nq.navigation_x_denominator;
        note_quads->color[insert_idx] = nq.color;
        note_quads->outline_color[insert_idx] = nq.outline_color;
        note_quads->gain[insert_idx] = nq.gain;
        note_quads->voice[insert_idx] = nq.voice;
        note_quads->alive[insert_idx] = 1;
        note_quads->id[insert_idx] = nq.id;
        note_quads->count++;
        *(war_note*)(tmp_payload) = n;
        *(uint32_t*)(tmp_payload + sizeof(war_note)) = insert_idx;
        war_pc_to_a(pc, AUDIO_CMD_INSERT_NOTE, sizeof(n) + sizeof(insert_idx), tmp_payload);
    }
}

static inline void
war_undo_tree_delete_notes_same(war_undo_node* node, war_note_quads* note_quads, war_producer_consumer* pc, uint8_t* tmp_payload) {
    call_carmack("undo_delete_notes_same");
    uint64_t* ids = node->payload.delete_notes_same.ids;
    uint32_t delete_count = node->payload.delete_notes_same.count;
    uint32_t count = 0;
    for (int32_t i = note_quads->count - 1; i >= 0 && count < delete_count; i--) {
        if (note_quads->alive[i] == 0 || note_quads->hidden[i]) { continue; }
        if (note_quads->id[i] != ids[delete_count - count - 1]) { continue; }
        note_quads->alive[i] = 0;
        *(uint32_t*)(tmp_payload + sizeof(uint32_t) * (count + 1)) = i;
        count++;
    }
    *(uint32_t*)tmp_payload = count;
    war_pc_to_a(pc, AUDIO_CMD_DELETE_NOTES_SAME, sizeof(uint32_t) * (count + 1), tmp_payload);
}

#endif // WAR_MACROS_H
