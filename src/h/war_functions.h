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
// src/h/war_functions.h
//-----------------------------------------------------------------------------

#ifndef WAR_FUNCTIONS_H
#define WAR_FUNCTIONS_H

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

#define FSM_3D_INDEX(state, keysym, mod)                                       \
    ((state) * (ctx_fsm->keysym_count * ctx_fsm->mod_count) +                  \
     (keysym) * ctx_fsm->mod_count + (mod))
// For: next_state, key_down, key_last_event_us

#define FSM_2D_MODE(state, mode) ((state) * ctx_fsm->mode_count + (mode))
// For: is_terminal, is_prefix, handle_release, handle_repeat, handle_timeout,
// function, type

#define FSM_3D_NAME(state, mode)                                               \
    ((state) * (ctx_fsm->mode_count * ctx_fsm->name_limit) +                   \
     (mode) * ctx_fsm->name_limit)
// For: name (when accessing the start of a name string)

static inline int war_load_lua_config(war_lua_context* ctx_lua,
                                      const char* lua_file) {
    if (luaL_dofile(ctx_lua->L, lua_file) != LUA_OK) {
        call_terry_davis("Lua error: %s", lua_tostring(ctx_lua->L, -1));
        return -1;
    }

    lua_getglobal(ctx_lua->L, "ctx_lua");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_terry_davis("ctx_lua not a table");
        return -1;
    }

#define LOAD_INT(field)                                                        \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (int)lua_tointeger(ctx_lua->L, -1);                   \
        call_terry_davis("ctx_lua: %s = %d", #field, ctx_lua->field);          \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    // audio
    LOAD_INT(A_SAMPLE_RATE)
    LOAD_INT(A_CHANNEL_COUNT)
    LOAD_INT(A_NOTE_COUNT)
    LOAD_INT(A_LAYER_COUNT)
    LOAD_INT(A_LAYERS_IN_RAM)
    LOAD_INT(A_PLAY_DATA_SIZE)
    LOAD_INT(A_CAPTURE_DATA_SIZE)
    LOAD_INT(A_BASE_FREQUENCY)
    LOAD_INT(A_BASE_NOTE)
    LOAD_INT(A_BYTES_NEEDED)
    LOAD_INT(A_EDO)
    LOAD_INT(A_NOTES_MAX)
    LOAD_INT(A_CACHE_SIZE)
    LOAD_INT(A_PATH_LIMIT)
    LOAD_INT(A_WARMUP_FRAMES_FACTOR)
    LOAD_INT(ROLL_POSITION_X_Y)
    LOAD_INT(A_SCHED_FIFO_PRIORITY)
    // window render
    LOAD_INT(WR_VIEWS_SAVED)
    LOAD_INT(WR_WARPOON_TEXT_COLS)
    LOAD_INT(WR_STATES)
    LOAD_INT(WR_SEQUENCE_COUNT)
    LOAD_INT(WR_SEQUENCE_LENGTH_MAX)
    LOAD_INT(WR_FN_NAME_LIMIT)
    LOAD_INT(WR_MODE_COUNT)
    LOAD_INT(WR_KEYSYM_COUNT)
    LOAD_INT(WR_CALLBACK_SIZE)
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
    LOAD_INT(PC_CONTROL_BUFFER_SIZE)
    LOAD_INT(PC_PLAY_BUFFER_SIZE)
    LOAD_INT(PC_CAPTURE_BUFFER_SIZE)
    LOAD_INT(A_BUILDER_DATA_SIZE)

#undef LOAD_INT

#define LOAD_FLOAT(field)                                                      \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (float)lua_tonumber(ctx_lua->L, -1);                  \
        call_terry_davis("ctx_lua: %s = %f", #field, ctx_lua->field);          \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    LOAD_FLOAT(A_DEFAULT_ATTACK)
    LOAD_FLOAT(A_DEFAULT_SUSTAIN)
    LOAD_FLOAT(A_DEFAULT_RELEASE)
    LOAD_FLOAT(A_DEFAULT_GAIN)
    LOAD_FLOAT(VK_FONT_PIXEL_HEIGHT)
    LOAD_FLOAT(DEFAULT_BOLD_TEXT_THICKNESS)
    LOAD_FLOAT(DEFAULT_BOLD_TEXT_FEATHER)
    LOAD_FLOAT(DEFAULT_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_CURSOR_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_PLAYBACK_BAR_THICKNESS)
    LOAD_FLOAT(WR_CAPTURE_THRESHOLD)
    LOAD_FLOAT(DEFAULT_TEXT_FEATHER)
    LOAD_FLOAT(DEFAULT_TEXT_THICKNESS)
    LOAD_FLOAT(WINDOWED_TEXT_FEATHER)
    LOAD_FLOAT(WINDOWED_TEXT_THICKNESS)
    LOAD_FLOAT(DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_WINDOWED_ALPHA_SCALE)
    LOAD_FLOAT(WR_COLOR_STEP)

#undef LOAD_FLOAT

#define LOAD_DOUBLE(field)                                                     \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (double)lua_tonumber(ctx_lua->L, -1);                 \
        call_terry_davis("ctx_lua: %s = %f", #field, ctx_lua->field);          \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    LOAD_DOUBLE(A_DEFAULT_COLUMNS_PER_BEAT)
    LOAD_DOUBLE(A_TARGET_SAMPLES_FACTOR)
    LOAD_DOUBLE(A_BPM)
    LOAD_DOUBLE(A_SAMPLE_DURATION)
    LOAD_DOUBLE(WR_FPS)
    LOAD_DOUBLE(WR_PLAY_CALLBACK_FPS)
    LOAD_DOUBLE(WR_CAPTURE_CALLBACK_FPS)

#undef LOAD_DOUBLE

    // #define LOAD_STRING(field) \
    //     lua_getfield(ctx_lua->L, -1, #field); \
    //     if (lua_isstring(ctx_lua->L, -1)) { \
    //         char* str = strdup(lua_tostring(ctx_lua->L, -1)); \
    //         if (str) { \
    //             atomic_store(&ctx_lua->field, str); \
    //             call_terry_davis( \
    //                 "ctx_lua: %s = %s", #field,
    //                 atomic_load(&ctx_lua->field));     \
    //         } \
    //     } \ lua_pop(ctx_lua->L, 1);
    //
    //     LOAD_STRING(CWD)

    // #undef LOAD_STRING
    return 0;
}

static inline size_t war_get_pool_a_size(war_pool* pool,
                                         war_lua_context* ctx_lua,
                                         const char* lua_file) {
    lua_getglobal(ctx_lua->L, "pool_a");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_terry_davis("pool_a not a table");
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(ctx_lua->L);
    while (lua_next(ctx_lua->L, -2) != 0) { // iterate pool_a entries
        if (lua_istable(ctx_lua->L, -1)) {
            lua_getfield(ctx_lua->L, -1, "type");
            const char* type = lua_tostring(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            lua_getfield(ctx_lua->L, -1, "count");
            size_t count = (size_t)lua_tointeger(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

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
            else if (strcmp(type, "char*") == 0)
                type_size = sizeof(char*);
            else if (strcmp(type, "char") == 0)
                type_size = sizeof(char);
            else if (strcmp(type, "war_midi_context") == 0)
                type_size = sizeof(war_midi_context);
            else if (strcmp(type, "war_pipewire_context") == 0)
                type_size = sizeof(war_pipewire_context);
            else if (strcmp(type, "ssize_t") == 0)
                type_size = sizeof(ssize_t);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t**") == 0)
                type_size = sizeof(int16_t**);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "int") == 0)
                type_size = sizeof(int);
            else if (strcmp(type, "size_t") == 0)
                type_size = sizeof(size_t);
            else if (strcmp(type, "war_riff_header") == 0)
                type_size = sizeof(war_riff_header);
            else if (strcmp(type, "war_fmt_chunk") == 0)
                type_size = sizeof(war_fmt_chunk);
            else if (strcmp(type, "war_data_chunk") == 0)
                type_size = sizeof(war_data_chunk);
            else if (strcmp(type, "war_notes") == 0) {
                type_size = sizeof(war_notes);
            } else if (strcmp(type, "bool") == 0) {
                type_size = sizeof(bool);
            } else {
                call_terry_davis("Unknown pool_a type: %s", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(ctx_lua->L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);
    call_terry_davis("pool_a size: %zu", total_size);
    return total_size;
}

static inline size_t war_get_pool_wr_size(war_pool* pool,
                                          war_lua_context* ctx_lua,
                                          const char* lua_file) {
    lua_getglobal(ctx_lua->L, "pool_wr");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_terry_davis("pool_wr not a table");
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(ctx_lua->L);
    while (lua_next(ctx_lua->L, -2) != 0) { // iterate pool_wr entries
        if (lua_istable(ctx_lua->L, -1)) {
            lua_getfield(ctx_lua->L, -1, "type");
            const char* type = lua_tostring(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            lua_getfield(ctx_lua->L, -1, "count");
            size_t count = (size_t)lua_tointeger(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

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
            else if (strcmp(type, "int") == 0)
                type_size = sizeof(int);
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
            else if (strcmp(type, "war_fsm_context") == 0)
                type_size = sizeof(war_fsm_context);
            else if (strcmp(type, "war_quad_vertex") == 0)
                type_size = sizeof(war_quad_vertex);
            else if (strcmp(type, "war_note_quads") == 0)
                type_size = sizeof(war_note_quads);
            else if (strcmp(type, "void (*)(war_env*)") == 0)
                type_size = sizeof(void (*)(war_env*));
            else if (strcmp(type, "war_text_vertex") == 0)
                type_size = sizeof(war_text_vertex);
            else if (strcmp(type, "war_status_context") == 0)
                type_size = sizeof(war_status_context);
            else if (strcmp(type, "war_capture_context") == 0)
                type_size = sizeof(war_capture_context);
            else if (strcmp(type, "war_command_context") == 0)
                type_size = sizeof(war_command_context);
            else if (strcmp(type, "war_play_context") == 0)
                type_size = sizeof(war_play_context);
            else if (strcmp(type, "war_audio_context") == 0)
                type_size = sizeof(war_audio_context);
            else if (strcmp(type, "war_cache_wav") == 0)
                type_size = sizeof(war_cache_wav);
            else if (strcmp(type, "war_map_wav") == 0)
                type_size = sizeof(war_map_wav);
            else if (strcmp(type, "war_wav") == 0)
                type_size = sizeof(war_wav);
            else if (strcmp(type, "war_env") == 0)
                type_size = sizeof(war_env);
            else if (strcmp(type, "war_color_context") == 0)
                type_size = sizeof(war_color_context);
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
                call_terry_davis("Unknown pool_wr type: %s", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(ctx_lua->L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);
    call_terry_davis("pool_wr size: %zu", total_size);
    return total_size;
}

static inline void* war_pool_alloc(war_pool* pool, size_t size) {
    size = ALIGN_UP(size, pool->pool_alignment);
    if (pool->pool_ptr + size > (uint8_t*)pool->pool + pool->pool_size) {
        call_terry_davis("war_pool_alloc not big enough! %zu bytes", size);
        abort();
    }
    void* ptr = pool->pool_ptr;
    pool->pool_ptr += size;
    return ptr;
}

static inline void war_layer_flux(war_window_render_context* ctx_wr,
                                  war_atomics* atomics,
                                  war_play_context* ctx_play,
                                  war_color_context* ctx_color) {
    uint64_t layer = ctx_play->note_layers[(int)ctx_wr->cursor_pos_y];
    atomic_store(&atomics->layer, layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0: {
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->white_hex;
        ctx_wr->color_note_outline_default = ctx_color->full_white_hex;
        break;
    }
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = ctx_color->colors[active];
        ctx_wr->color_cursor_transparent = ctx_color->colors[active];
        ctx_wr->color_note_outline_default = ctx_color->white_hex;
        ctx_wr->layers_active[0] = (active + 1) + '0';
        break;
    }
    default: {
        int count = 0;
        while (layer) {
            int active = __builtin_ctzll(layer);
            ctx_wr->layers_active[count++] = (active + 1) + '0';
            layer &= layer - 1;
        }
        ctx_wr->color_cursor = ctx_color->full_white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->white_hex;
        ctx_wr->color_note_outline_default = ctx_color->white_hex;
        break;
    }
    }
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
        call_terry_davis("i_views: %u", i);
        call_terry_davis("views row: %u", views->row[i]);
        call_terry_davis("views col: %u", views->col[i]);
        call_terry_davis("views bottom_row: %u", views->bottom_row[i]);
        call_terry_davis("views left_col: %u", views->left_col[i]);
    }
}

static inline void war_warpoon_delete_at_i(war_views* views,
                                           uint32_t i_delete) {
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
    if (views->warpoon_row == 0)
        return; // already at bottom in your indexing logic

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
static inline uint8_t war_pc_to_a(war_producer_consumer* pc,
                                  uint32_t header,
                                  uint32_t payload_size,
                                  const void* payload) {
    uint32_t total_size = 8 + payload_size; // header(4) + size(4) + payload
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    // free bytes calculation (circular buffer)
    uint32_t free_bytes =
        (pc->size + read_index - write_index - 1) & (pc->size - 1);
    if (free_bytes < total_size) return 0;
    // --- write header (4) + size (4) allowing split ---
    uint32_t cont_bytes = pc->size - write_index;
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
        uint32_t payload_write_pos = (write_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_a + payload_write_pos, payload, payload_size);
        } else {
            // wrap
            memcpy(pc->to_a + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_a,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }
    // commit write index
    pc->i_to_a = (write_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Reader: Audio <- WR (from_wr)
static inline uint8_t war_pc_from_wr(war_producer_consumer* pc,
                                     uint32_t* out_header,
                                     uint32_t* out_size,
                                     void* out_payload) {
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    uint32_t used_bytes =
        (pc->size + write_index - read_index) & (pc->size - 1);
    if (used_bytes < 8) return 0; // need at least header+size
    // read header+size (handle split)
    uint32_t cont_bytes = pc->size - read_index;
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
    if (used_bytes < total_size) return 0; // not all payload present yet
    // read payload (if any)
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_a + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_a + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_a,
                   *out_size - first_chunk);
        }
    }
    // commit read index
    pc->i_from_wr = (read_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Writer: Main -> WR (to_wr)
static inline uint8_t war_pc_to_wr(war_producer_consumer* pc,
                                   uint32_t header,
                                   uint32_t payload_size,
                                   const void* payload) {
    uint32_t total_size = 8 + payload_size;
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t free_bytes =
        (pc->size + read_index - write_index - 1) & (pc->size - 1);
    if (free_bytes < total_size) return 0;
    // header+size
    uint32_t cont_bytes = pc->size - write_index;
    if (cont_bytes >= 8) {
        *(uint32_t*)(pc->to_wr + write_index) = header;
        *(uint32_t*)(pc->to_wr + write_index + 4) = payload_size;
    } else {
        if (cont_bytes >= 4) {
            *(uint32_t*)(pc->to_wr + write_index) = header;
            *(uint32_t*)pc->to_wr = payload_size;
        } else {
            *(uint16_t*)(pc->to_wr + write_index) = (uint16_t)header;
            *(uint16_t*)(pc->to_wr + write_index + 2) =
                (uint16_t)(header >> 16);
            *(uint32_t*)pc->to_wr = payload_size;
        }
    }
    // payload
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_wr + payload_write_pos, payload, payload_size);
        } else {
            memcpy(pc->to_wr + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_wr,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }
    pc->i_to_wr = (write_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Reader: WR <- Main (from_a)
static inline uint8_t war_pc_from_a(war_producer_consumer* pc,
                                    uint32_t* out_header,
                                    uint32_t* out_size,
                                    void* out_payload) {
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t used_bytes =
        (pc->size + write_index - read_index) & (pc->size - 1);
    if (used_bytes < 8) return 0;
    uint32_t cont_bytes = pc->size - read_index;
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
    if (used_bytes < total_size) return 0;
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_wr + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_wr + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_wr,
                   *out_size - first_chunk);
        }
    }
    pc->i_from_a = (read_index + total_size) & (pc->size - 1);
    return 1;
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
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline uint32_t war_read_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t war_read_le16(const uint8_t* p) {
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

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

static inline uint32_t
war_clamp_add_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t sum = (uint64_t)a + b;
    uint64_t mask = -(sum > max_value);
    return (uint32_t)((sum & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
war_clamp_subtract_uint32(uint32_t a, uint32_t b, uint32_t min_value) {
    uint32_t diff = a - b;
    uint32_t underflow_mask = -(a < b);
    uint32_t below_min_mask = -(diff < min_value);
    uint32_t clamped_diff =
        (diff & ~below_min_mask) | (min_value & below_min_mask);
    return (clamped_diff & ~underflow_mask) | (min_value & underflow_mask);
}

static inline uint32_t
war_clamp_multiply_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t prod = (uint64_t)a * (uint64_t)b;
    uint64_t mask = -(prod > max_value);
    return (uint32_t)((prod & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
war_clamp_uint32(uint32_t a, uint32_t min_value, uint32_t max_value) {
    a = a < min_value ? min_value : a;
    a = a > max_value ? max_value : a;
    return a;
}

static inline uint64_t war_align64(uint64_t value) {
    return (value + 63) & ~63ULL;
}

static inline uint16_t war_normalize_keysym(xkb_keysym_t ks) {
    if ((ks >= XKB_KEY_a && ks <= XKB_KEY_z) ||
        (ks >= XKB_KEY_0 && ks <= XKB_KEY_9)) {
        return (uint16_t)ks;
    }
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
    case XKB_KEY_semicolon:
        return KEYSYM_SEMICOLON;
    case XKB_KEY_colon:
        return KEYSYM_SEMICOLON;
    case XKB_KEY_underscore:
        return KEYSYM_UNDERSCORE;
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
    case XKB_KEY_KP_0:
    case XKB_KEY_KP_Insert:
        return XKB_KEY_0;
    case XKB_KEY_KP_1:
    case XKB_KEY_KP_End:
        return XKB_KEY_1;
    case XKB_KEY_KP_2:
    case XKB_KEY_KP_Down:
        return XKB_KEY_2;
    case XKB_KEY_KP_3:
    case XKB_KEY_KP_Next:
        return XKB_KEY_3;
    case XKB_KEY_KP_4:
    case XKB_KEY_KP_Left:
        return XKB_KEY_4;
    case XKB_KEY_KP_5:
    case XKB_KEY_KP_Begin:
        return XKB_KEY_5;
    case XKB_KEY_KP_6:
    case XKB_KEY_KP_Right:
        return XKB_KEY_6;
    case XKB_KEY_KP_7:
    case XKB_KEY_KP_Home:
        return XKB_KEY_7;
    case XKB_KEY_KP_8:
    case XKB_KEY_KP_Up:
        return XKB_KEY_8;
    case XKB_KEY_KP_9:
    case XKB_KEY_KP_Prior:
        return XKB_KEY_9;
    default:
        return KEYSYM_DEFAULT; // fallback / unknown
    }
}

static inline int war_keysym_to_int(xkb_keysym_t ks, uint8_t mod) {
    int lowercase = ks;
    int mod_shift_difference = (mod == MOD_SHIFT ? 32 : 0);
    // Letters a-z or A-Z -> always lowercase
    if (ks >= XKB_KEY_a && ks <= XKB_KEY_z)
        return (int)ks - mod_shift_difference;
    if (ks >= XKB_KEY_A && ks <= XKB_KEY_Z) return (int)(ks - XKB_KEY_A + 'a');

    // Numbers 0-9
    if (ks >= XKB_KEY_0 && ks <= XKB_KEY_9 && mod == 0) return (int)ks;

    switch (ks) {
    case KEYSYM_SPACE:
        return ' ';
    case KEYSYM_APOSTROPHE:
        return '\'';
    case KEYSYM_COMMA:
        return ',';
    case KEYSYM_MINUS:
        return '-';
    case KEYSYM_UNDERSCORE:
        return '_';
    case KEYSYM_RETURN:
        return '\n';
    case KEYSYM_ESCAPE:
        return 27;
    case KEYSYM_UP:
        return 1;
    case KEYSYM_DOWN:
        return 2;
    case KEYSYM_LEFT:
        return 3;
    case KEYSYM_RIGHT:
        return 4;
    case KEYSYM_BACKSPACE:
        return 8;
    }

    return 0; // non-printable / special keys
}

static inline void war_command_reset(war_command_context* ctx_command,
                                     war_status_context* ctx_status) {
    memset(ctx_status->middle, 0, ctx_status->capacity);
    ctx_status->middle_size = 0;
    memset(ctx_command->text, 0, ctx_command->capacity);
    ctx_command->text_size = 0;
    ctx_command->text_write_index = 0;
    memset(ctx_command->input, 0, ctx_command->capacity);
    ctx_command->input_write_index = 0;
    ctx_command->input_read_index = 0;
    memset(ctx_command->prompt, 0, ctx_command->capacity);
    ctx_command->prompt_size = 0;
}

static inline void war_command_status(war_command_context* ctx_command,
                                      war_status_context* ctx_status) {
    snprintf(ctx_status->middle,
             ctx_command->prompt_size + 2 + ctx_command->text_size,
             "%s:%s",
             ctx_command->prompt,
             ctx_command->text);
    ctx_status->middle_size =
        ctx_command->prompt_size + 2 + ctx_command->text_size;
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

static inline void war_wl_surface_set_opaque_region(int fd,
                                                    uint32_t wl_surface_id,
                                                    uint32_t wl_region_id) {
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
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1],
                bottom_left_pos[2]},
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
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
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
        .pos = {bottom_left_pos[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
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

static inline void war_make_blank_text_quad(war_text_vertex* text_vertices,
                                            uint16_t* text_indices,
                                            uint32_t* text_vertices_count,
                                            uint32_t* text_indices_count) {
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
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1],
                bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
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

static inline void
war_make_transparent_quad(war_quad_vertex* transparent_quad_vertices,
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
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1],
                bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + span[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
        .span = {span[0], span[1]},
        .color = color,
        .outline_thickness = outline_thickness,
        .outline_color = outline_color,
        .line_thickness = {line_thickness[0], line_thickness[1]},
        .flags = flags,
    };
    transparent_quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0],
                bottom_left_pos[1] + span[1],
                bottom_left_pos[2]},
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

static inline void war_make_blank_quad(war_quad_vertex* quad_vertices,
                                       uint16_t* quad_indices,
                                       uint32_t* vertices_count,
                                       uint32_t* indices_count) {
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

static inline uint32_t war_lcm(uint32_t a, uint32_t b) {
    return a / war_gcd(a, b) * b;
}

static inline float war_midi_to_frequency(float midi_note) {
    return 440.0f * pow(2.0f, (midi_note - 69) / 12.0f);
}

static inline float war_sine_phase_increment(war_audio_context* ctx_a,
                                             float frequency) {
    return (2.0f * M_PI * frequency) / (float)ctx_a->sample_rate;
}

static inline uint8_t war_parse_token_to_keysym_mod(const char* token,
                                                    uint16_t* keysym_out,
                                                    uint8_t* mod_out) {
    if (!token || !keysym_out || !mod_out) { return 0; }
    *mod_out = 0;
    const char* key_str = token;
    char token_buf[64] = {0};
    if (token[0] == '<') {
        size_t len = strlen(token);
        if (len > 1 && token[len - 1] == '>') {
            size_t inner_len = len - 2;
            if (inner_len >= sizeof(token_buf))
                inner_len = sizeof(token_buf) - 1;
            memcpy(token_buf, token + 1, inner_len);
            token_buf[inner_len] = '\0';
            char key_part[64] = {0};
            char* saveptr = NULL;
            char* part = strtok_r(token_buf, "-", &saveptr);
            while (part) {
                if (strcasecmp(part, "C") == 0 ||
                    strcasecmp(part, "Ctrl") == 0 ||
                    strcasecmp(part, "Control") == 0) {
                    *mod_out |= MOD_CTRL;
                } else if (strcasecmp(part, "S") == 0 ||
                           strcasecmp(part, "Shift") == 0) {
                    *mod_out |= MOD_SHIFT;
                } else if (strcasecmp(part, "A") == 0 ||
                           strcasecmp(part, "Alt") == 0 ||
                           strcasecmp(part, "M") == 0 ||
                           strcasecmp(part, "Meta") == 0) {
                    *mod_out |= MOD_ALT;
                } else if (strcasecmp(part, "D") == 0 ||
                           strcasecmp(part, "Cmd") == 0 ||
                           strcasecmp(part, "Super") == 0 ||
                           strcasecmp(part, "Logo") == 0) {
                    *mod_out |= MOD_LOGO;
                } else {
                    strncpy(key_part, part, sizeof(key_part) - 1);
                    key_part[sizeof(key_part) - 1] = '\0';
                }
                part = strtok_r(NULL, "-", &saveptr);
            }
            if (key_part[0] == '\0' && inner_len > 0) {
                key_part[0] = token_buf[inner_len - 1];
                key_part[1] = '\0';
            }
            if (key_part[0] != '\0') { key_str = key_part; }
        }
    }
    xkb_keysym_t ks = XKB_KEY_NoSymbol;
    if (strcasecmp(key_str, "CR") == 0 || strcasecmp(key_str, "Enter") == 0 ||
        strcasecmp(key_str, "Return") == 0) {
        ks = XKB_KEY_Return;
    } else if (strcasecmp(key_str, "Esc") == 0 ||
               strcasecmp(key_str, "Escape") == 0) {
        ks = XKB_KEY_Escape;
    } else if (strcasecmp(key_str, "Space") == 0) {
        ks = XKB_KEY_space;
    } else if (strcasecmp(key_str, "Tab") == 0) {
        ks = XKB_KEY_Tab;
    } else if (strcasecmp(key_str, "BS") == 0 ||
               strcasecmp(key_str, "Backspace") == 0) {
        ks = XKB_KEY_BackSpace;
    } else if (strcasecmp(key_str, "Del") == 0 ||
               strcasecmp(key_str, "Delete") == 0) {
        ks = XKB_KEY_Delete;
    } else if (strcasecmp(key_str, "Insert") == 0) {
        ks = XKB_KEY_Insert;
    } else if (strcasecmp(key_str, "Home") == 0) {
        ks = XKB_KEY_Home;
    } else if (strcasecmp(key_str, "End") == 0) {
        ks = XKB_KEY_End;
    } else if (strcasecmp(key_str, "PageUp") == 0) {
        ks = XKB_KEY_Page_Up;
    } else if (strcasecmp(key_str, "PageDown") == 0) {
        ks = XKB_KEY_Page_Down;
    } else if (strcasecmp(key_str, "Up") == 0) {
        ks = XKB_KEY_Up;
    } else if (strcasecmp(key_str, "Down") == 0) {
        ks = XKB_KEY_Down;
    } else if (strcasecmp(key_str, "Left") == 0) {
        ks = XKB_KEY_Left;
    } else if (strcasecmp(key_str, "Right") == 0) {
        ks = XKB_KEY_Right;
    } else if (strcasecmp(key_str, "lt") == 0) {
        ks = XKB_KEY_less;
    } else if (strlen(key_str) == 1) {
        char c = key_str[0];
        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ks = XKB_KEY_0 + (c - '0');
            break;
        case '!':
            ks = XKB_KEY_1;
            *mod_out |= MOD_SHIFT;
            break;
        case '@':
            ks = XKB_KEY_2;
            *mod_out |= MOD_SHIFT;
            break;
        case '#':
            ks = XKB_KEY_3;
            *mod_out |= MOD_SHIFT;
            break;
        case '$':
            ks = XKB_KEY_4;
            *mod_out |= MOD_SHIFT;
            break;
        case '%':
            ks = XKB_KEY_5;
            *mod_out |= MOD_SHIFT;
            break;
        case '^':
            ks = XKB_KEY_6;
            *mod_out |= MOD_SHIFT;
            break;
        case '&':
            ks = XKB_KEY_7;
            *mod_out |= MOD_SHIFT;
            break;
        case '*':
            ks = XKB_KEY_8;
            *mod_out |= MOD_SHIFT;
            break;
        case '(':
            ks = XKB_KEY_9;
            *mod_out |= MOD_SHIFT;
            break;
        case ')':
            ks = XKB_KEY_0;
            *mod_out |= MOD_SHIFT;
            break;
        case '_':
            ks = XKB_KEY_minus;
            *mod_out |= MOD_SHIFT;
            break;
        case '-':
            ks = XKB_KEY_minus;
            break;
        case '+':
            ks = XKB_KEY_equal;
            *mod_out |= MOD_SHIFT;
            break;
        case '=':
            ks = XKB_KEY_equal;
            break;
        case ':':
            ks = XKB_KEY_semicolon;
            *mod_out |= MOD_SHIFT;
            break;
        case ';':
            ks = XKB_KEY_semicolon;
            break;
        case '?':
            ks = XKB_KEY_slash;
            *mod_out |= MOD_SHIFT;
            break;
        case '/':
            ks = XKB_KEY_slash;
            break;
        case '>':
            ks = XKB_KEY_period;
            *mod_out |= MOD_SHIFT;
            break;
        case '.':
            ks = XKB_KEY_period;
            break;
        case '<':
            ks = XKB_KEY_comma;
            *mod_out |= MOD_SHIFT;
            break;
        case ',':
            ks = XKB_KEY_comma;
            break;
        default:
            ks = xkb_keysym_from_name(key_str, XKB_KEYSYM_NO_FLAGS);
            break;
        }
    } else {
        ks = xkb_keysym_from_name(key_str, XKB_KEYSYM_NO_FLAGS);
    }
    if (ks == XKB_KEY_NoSymbol) { return 0; }
    if (strlen(key_str) == 1 && key_str[0] >= 'A' && key_str[0] <= 'Z' &&
        !(*mod_out & MOD_SHIFT)) {
        *mod_out |= MOD_SHIFT;
    }
    *keysym_out = war_normalize_keysym(ks);
    return 1;
}

static inline uint16_t war_find_prefix_state(char** prefixes,
                                             uint16_t prefix_count,
                                             const char* prefix) {
    for (uint16_t i = 0; i < prefix_count; i++) {
        if (strcmp(prefixes[i], prefix) == 0) { return i; }
    }
    return UINT16_MAX;
}

static inline void war_fsm_execute_command(war_env* env,
                                           war_fsm_context* ctx_fsm,
                                           uint16_t state_index) {
    if (!ctx_fsm || state_index >= ctx_fsm->state_count) { return; }
    size_t mode_idx =
        (size_t)state_index * ctx_fsm->mode_count + ctx_fsm->current_mode;
    if (ctx_fsm->type[mode_idx] != ctx_fsm->FUNCTION_C) { return; }
    void (*fn)(war_env*) = ctx_fsm->function[mode_idx].c;
    if (fn) { fn(env); }
}

#endif // WAR_FUNCTIONS_H
