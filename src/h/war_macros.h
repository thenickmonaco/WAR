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

#define LOAD_INT(field)                                                        \
    lua_getfield(L, -1, #field);                                               \
    if (lua_type(L, -1) == LUA_TNUMBER) {                                      \
        ctx_lua->field = (int)lua_tointeger(L, -1);                            \
        call_carmack("ctx_lua: %s = %d", #field, ctx_lua->field);              \
    }                                                                          \
    lua_pop(L, 1);

    // audio
    LOAD_INT(A_SAMPLE_RATE)
    LOAD_INT(A_SAMPLE_DURATION)
    LOAD_INT(A_CHANNEL_COUNT)
    LOAD_INT(A_NOTE_COUNT)
    LOAD_INT(A_SAMPLES_PER_NOTE)
    LOAD_INT(A_BPM)
    LOAD_INT(A_BASE_FREQUENCY)
    LOAD_INT(A_BASE_NOTE)
    LOAD_INT(A_EDO)
    LOAD_INT(A_NOTES_MAX);
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
    // pool
    LOAD_INT(POOL_ALIGNMENT)
    // cmd
    LOAD_INT(CMD_COUNT)
    // pc
    LOAD_INT(PC_BUFFER_SIZE)

#undef LOAD_INT

    lua_close(L);
    return 0;
}

static inline size_t war_get_pool_a_size(war_pool* pool,
                                         war_lua_context* ctx_lua,
                                         const char* lua_file) {
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
            else if (strcmp(type, "war_samples") == 0)
                type_size = sizeof(war_samples);
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

static inline size_t war_get_pool_wr_size(war_pool* pool,
                                          war_lua_context* ctx_lua,
                                          const char* lua_file) {
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
            else if (strcmp(type, "bool") == 0) {
                type_size = sizeof(bool);
            }

            /* --- WR-specific structs --- */
            else if (strcmp(type, "war_fsm_state") == 0)
                type_size = sizeof(war_fsm_state);
            else if (strcmp(type, "war_quad_vertex") == 0)
                type_size = sizeof(war_quad_vertex);
            else if (strcmp(type, "war_text_vertex") == 0)
                type_size = sizeof(war_text_vertex);
            else if (strcmp(type, "war_audio_context") == 0)
                type_size = sizeof(war_audio_context);
            else if (strcmp(type, "war_samples") == 0)
                type_size = sizeof(war_samples);

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

static inline void war_get_top_text(war_window_render_context* ctx_wr) {
    memset(ctx_wr->text_top_status_bar, 0, MAX_STATUS_BAR_COLS);
    memcpy(ctx_wr->text_top_status_bar,
           "projects/demo.war",
           sizeof("projects/test.war"));
    ctx_wr->text_top_status_bar_count += sizeof("projects/test.war");
    char tmp[MAX_STATUS_BAR_COLS];
    memset(tmp, 0, MAX_STATUS_BAR_COLS);
    snprintf(tmp,
             MAX_STATUS_BAR_COLS,
             "%.0f,%.0f",
             ctx_wr->cursor_pos_y,
             ctx_wr->cursor_pos_x);
    memcpy(ctx_wr->text_top_status_bar + ctx_wr->text_status_bar_end_index,
           tmp,
           sizeof(ctx_wr->text_top_status_bar));
}

static inline void war_get_middle_text(war_window_render_context* ctx_wr,
                                       war_views* views,
                                       war_atomics* atomics) {
    memset(ctx_wr->text_middle_status_bar, 0, MAX_STATUS_BAR_COLS);
    switch (ctx_wr->mode) {
    case MODE_VISUAL:
        memcpy(ctx_wr->text_middle_status_bar,
               "-- VISUAL --",
               sizeof("-- VISUAL --"));
        break;
    case MODE_VIEWS:
        if (views->warpoon_mode == MODE_VISUAL_LINE) {
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- VIEWS -- -- VISUAL LINE --",
                   sizeof("-- VIEWS -- -- VISUAL LINE --"));
            break;
        }
        memcpy(ctx_wr->text_middle_status_bar,
               "-- VIEWS --",
               sizeof("-- VIEWS --"));
        break;
    case MODE_COMMAND:
        memcpy(ctx_wr->text_middle_status_bar, ":", sizeof(":"));
        break;
    case MODE_MIDI: {
        switch (atomic_load(&atomics->state)) {
        case AUDIO_CMD_MIDI_RECORD_WAIT:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- MIDI RECORD WAIT --",
                   sizeof("-- MIDI RECORD WAIT --"));
            break;
        case AUDIO_CMD_MIDI_RECORD:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- MIDI RECORD --",
                   sizeof("-- MIDI RECORD --"));
            break;
        case AUDIO_CMD_MIDI_RECORD_MAP:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- MIDI RECORD MAP --",
                   sizeof("-- MIDI RECORD MAP --"));
            break;
        default:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- MIDI --",
                   sizeof("-- MIDI --"));
            break;
        }
        uint8_t loop = atomic_load(&atomics->loop);
        uint8_t midi_toggle = ctx_wr->midi_toggle;
        if (loop && midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar +
                       ctx_wr->text_status_bar_middle_index,
                   "LOOP TOGGLE",
                   sizeof("LOOP TOGGLE"));
        } else if (loop && !midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar +
                       ctx_wr->text_status_bar_middle_index,
                   "LOOP",
                   sizeof("LOOP"));
        } else if (!loop && midi_toggle) {
            memcpy(ctx_wr->text_middle_status_bar +
                       ctx_wr->text_status_bar_middle_index,
                   "TOGGLE",
                   sizeof("TOGGLE"));
        }
        break;
    }
    case MODE_RECORD:
        switch (atomic_load(&atomics->state)) {
        case AUDIO_CMD_RECORD_WAIT:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- RECORD WAIT --",
                   sizeof("-- RECORD WAIT --"));
            break;
        case AUDIO_CMD_RECORD:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- RECORD --",
                   sizeof("-- RECORD --"));
            break;
        case AUDIO_CMD_RECORD_MAP:
            memcpy(ctx_wr->text_middle_status_bar,
                   "-- RECORD MAP --",
                   sizeof("-- RECORD MAP --"));
            break;
        }
        break;
    default:
        break;
    }
    if (ctx_wr->cursor_blink_state) {
        memcpy(ctx_wr->text_middle_status_bar +
                   ctx_wr->text_status_bar_end_index,
               ctx_wr->input_sequence,
               ctx_wr->num_chars_in_sequence);
        uint32_t size = (ctx_wr->cursor_blink_state == CURSOR_BLINK) ?
                            sizeof("BLINK") :
                            sizeof("BPM");
        memcpy(ctx_wr->text_middle_status_bar +
                   ctx_wr->text_status_bar_end_index + 2,
               (size == sizeof("BLINK")) ? "BLINK" : "BPM",
               size);
        return;
    }
    memcpy(ctx_wr->text_middle_status_bar + ctx_wr->text_status_bar_end_index,
           ctx_wr->input_sequence,
           ctx_wr->num_chars_in_sequence);
}

static inline void war_get_bottom_text(war_window_render_context* ctx_wr) {
    memset(ctx_wr->text_bottom_status_bar, 0, MAX_STATUS_BAR_COLS);
    memcpy(ctx_wr->text_bottom_status_bar,
           "[WAR] 1:roll*",
           sizeof("[WAR] 1:roll*"));
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
static inline bool war_pc_to_a(war_producer_consumer* pc,
                               uint32_t header,
                               uint32_t payload_size,
                               const void* payload) {
    uint32_t total_size = 8 + payload_size; // header(4) + size(4) + payload
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;

    // free bytes calculation (circular buffer)
    uint32_t free_bytes =
        (PC_BUFFER_SIZE + read_index - write_index - 1) & (PC_BUFFER_SIZE - 1);
    if (free_bytes < total_size) return false;

    // --- write header (4) + size (4) allowing split ---
    uint8_t tmp_hdr[8];
    memcpy(tmp_hdr, &header, 4);
    memcpy(tmp_hdr + 4, &payload_size, 4);

    uint32_t cont_bytes = PC_BUFFER_SIZE - write_index;

    if (cont_bytes >= 8) {
        // contiguous place for header+size
        memcpy(pc->to_a + write_index, tmp_hdr, 8);
    } else {
        // split header+size across end -> wrap
        memcpy(pc->to_a + write_index, tmp_hdr, cont_bytes);
        memcpy(pc->to_a, tmp_hdr + cont_bytes, 8 - cont_bytes);
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
            memcpy(pc->to_a,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }

    // commit write index
    pc->i_to_a = (write_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Reader: Audio <- WR (from_wr)
static inline bool war_pc_from_wr(war_producer_consumer* pc,
                                  uint32_t* out_header,
                                  uint32_t* out_size,
                                  void* out_payload) {
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;

    uint32_t used_bytes =
        (PC_BUFFER_SIZE + write_index - read_index) & (PC_BUFFER_SIZE - 1);
    if (used_bytes < 8) return false; // need at least header+size

    // read header+size (handle split)
    uint32_t cont_bytes = PC_BUFFER_SIZE - read_index;
    if (cont_bytes >= 8) {
        memcpy(out_header, pc->to_a + read_index, 4);
        memcpy(out_size, pc->to_a + read_index + 4, 4);
    } else {
        uint8_t tmp[8];
        memcpy(tmp, pc->to_a + read_index, cont_bytes);
        memcpy(tmp + cont_bytes, pc->to_a, 8 - cont_bytes);
        memcpy(out_header, tmp, 4);
        memcpy(out_size, tmp + 4, 4);
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
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_a,
                   *out_size - first_chunk);
        }
    }

    // commit read index
    pc->i_from_wr = (read_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Writer: Main -> WR (to_wr)
static inline bool war_pc_to_wr(war_producer_consumer* pc,
                                uint32_t header,
                                uint32_t payload_size,
                                const void* payload) {
    uint32_t total_size = 8 + payload_size;
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;

    uint32_t free_bytes =
        (PC_BUFFER_SIZE + read_index - write_index - 1) & (PC_BUFFER_SIZE - 1);
    if (free_bytes < total_size) return false;

    // header+size
    uint8_t tmp_hdr[8];
    memcpy(tmp_hdr, &header, 4);
    memcpy(tmp_hdr + 4, &payload_size, 4);

    uint32_t cont_bytes = PC_BUFFER_SIZE - write_index;
    if (cont_bytes >= 8) {
        memcpy(pc->to_wr + write_index, tmp_hdr, 8);
    } else {
        memcpy(pc->to_wr + write_index, tmp_hdr, cont_bytes);
        memcpy(pc->to_wr, tmp_hdr + cont_bytes, 8 - cont_bytes);
    }

    // payload
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (PC_BUFFER_SIZE - 1);
        uint32_t first_chunk = PC_BUFFER_SIZE - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_wr + payload_write_pos, payload, payload_size);
        } else {
            memcpy(pc->to_wr + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_wr,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }

    pc->i_to_wr = (write_index + total_size) & (PC_BUFFER_SIZE - 1);
    return true;
}

// --------------------------
// Reader: WR <- Main (from_a)
static inline bool war_pc_from_a(war_producer_consumer* pc,
                                 uint32_t* out_header,
                                 uint32_t* out_size,
                                 void* out_payload) {
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;

    uint32_t used_bytes =
        (PC_BUFFER_SIZE + write_index - read_index) & (PC_BUFFER_SIZE - 1);
    if (used_bytes < 8) return false;

    uint32_t cont_bytes = PC_BUFFER_SIZE - read_index;
    if (cont_bytes >= 8) {
        memcpy(out_header, pc->to_wr + read_index, 4);
        memcpy(out_size, pc->to_wr + read_index + 4, 4);
    } else {
        uint8_t tmp[8];
        memcpy(tmp, pc->to_wr + read_index, cont_bytes);
        memcpy(tmp + cont_bytes, pc->to_wr, 8 - cont_bytes);
        memcpy(out_header, tmp, 4);
        memcpy(out_size, tmp + 4, 4);
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
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_wr,
                   *out_size - first_chunk);
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
        call_carmack("default: %u", ks);
        return KEYSYM_DEFAULT; // fallback / unknown
    }
}

static inline void
war_get_frame_duration_us(war_window_render_context* ctx_wr) {
    const double microsecond_conversion = 1000000;
    ctx_wr->frame_duration_us =
        (uint64_t)round((1.0 / (double)ctx_wr->FPS) * microsecond_conversion);
}

static inline char war_keysym_to_char(xkb_keysym_t ks, uint8_t mod) {
    char lowercase = ks;
    int mod_shift_difference = (mod == MOD_SHIFT ? 32 : 0);
    // Letters a-z or A-Z -> always lowercase
    if (ks >= XKB_KEY_a && ks <= XKB_KEY_z)
        return (char)ks - mod_shift_difference;
    if (ks >= XKB_KEY_A && ks <= XKB_KEY_Z) return (char)(ks - XKB_KEY_A + 'a');

    // Numbers 0-9
    if (ks >= XKB_KEY_0 && ks <= XKB_KEY_9 && mod == 0) return (char)ks;

    switch (ks) {
    case KEYSYM_SPACE:
        return '_';
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

static inline void war_notes_add(war_note_quads* note_quads,
                                 uint32_t* note_quads_count,
                                 war_producer_consumer* pc,
                                 war_window_render_context* ctx_wr,
                                 war_lua_context* ctx_lua,
                                 uint32_t color,
                                 uint32_t outline_color,
                                 float gain,
                                 uint32_t voice,
                                 uint32_t hidden,
                                 uint32_t mute) {
    assert(*note_quads_count < max_note_quads);
    note_quads->timestamp[*note_quads_count] = ctx_wr->now;
    note_quads->pos_x[*note_quads_count] = ctx_wr->cursor_pos_x;
    note_quads->pos_y[*note_quads_count] = ctx_wr->cursor_pos_y;
    note_quads->navigation_x[*note_quads_count] = ctx_wr->cursor_navigation_x;
    note_quads->size_x[*note_quads_count] = ctx_wr->cursor_size_x;
    note_quads->navigation_x_numerator[*note_quads_count] =
        ctx_wr->navigation_whole_number_col;
    note_quads->navigation_x_denominator[*note_quads_count] =
        ctx_wr->navigation_sub_cells_col;
    note_quads->size_x_numerator[*note_quads_count] =
        ctx_wr->cursor_width_whole_number;
    note_quads->size_x_denominator[*note_quads_count] =
        ctx_wr->cursor_width_sub_cells;
    note_quads->color[*note_quads_count] = color;
    note_quads->outline_color[*note_quads_count] = outline_color;
    note_quads->gain[*note_quads_count] = gain;
    note_quads->voice[*note_quads_count] = voice;
    note_quads->hidden[*note_quads_count] = hidden;
    note_quads->mute[*note_quads_count] = mute;
    (*note_quads_count)++;

    double d_start_sec =
        ctx_wr->cursor_pos_x * ((60.0 / atomic_load(&ctx_lua->A_BPM) / 4.0));
    uint64_t start_frames =
        (uint64_t)llround(d_start_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    double d_duration_sec = ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) *
                            ((double)ctx_wr->cursor_size_x);
    uint64_t duration_frames = (uint64_t)llround(
        d_duration_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    war_note_msg note_msg = {.note_start_frames = start_frames,
                             .note_duration_frames = duration_frames,
                             .note_sample_index = ctx_wr->cursor_pos_y,
                             .note_gain = 1.0f,
                             .note_attack = 0.0f,
                             .note_sustain = 1.0f,
                             .note_release = 0.0f};
    war_pc_to_a(pc, AUDIO_CMD_ADD_NOTE, sizeof(war_note_msg), &note_msg);
}

static inline void war_note_quads_delete(war_note_quads* note_quads,
                                         uint32_t* note_quads_count,
                                         war_producer_consumer* pc,
                                         war_window_render_context* ctx_wr) {
    if (*note_quads_count == 0) return;

    // Find the index of the freshest note at the given col/row
    uint64_t freshest_time = 0;
    int32_t freshest_index = -1;

    for (uint32_t i = 0; i < *note_quads_count; i++) {
        if (note_quads->pos_x[i] == ctx_wr->cursor_pos_x &&
            note_quads->pos_y[i] == ctx_wr->cursor_pos_y &&
            note_quads->timestamp[i] > freshest_time) {
            freshest_time = note_quads->timestamp[i];
            freshest_index = i;
        }
    }

    if (freshest_index == -1) return; // no note found

    // Swap-back delete the freshest note
    uint32_t last = *note_quads_count - 1;
    if ((uint32_t)freshest_index != last) {
        note_quads->timestamp[freshest_index] = note_quads->timestamp[last];
        note_quads->pos_x[freshest_index] = note_quads->pos_x[last];
        note_quads->pos_y[freshest_index] = note_quads->pos_y[last];
        note_quads->navigation_x[freshest_index] =
            note_quads->navigation_x[last];
        note_quads->size_x[freshest_index] = note_quads->size_x[last];
        note_quads->navigation_x_numerator[freshest_index] =
            note_quads->navigation_x_numerator[last];
        note_quads->navigation_x_denominator[freshest_index] =
            note_quads->navigation_x_denominator[last];
        note_quads->size_x_numerator[freshest_index] =
            note_quads->size_x_numerator[last];
        note_quads->size_x_denominator[freshest_index] =
            note_quads->size_x_denominator[last];
        note_quads->color[freshest_index] = note_quads->color[last];
        note_quads->outline_color[freshest_index] =
            note_quads->outline_color[last];
        note_quads->gain[freshest_index] = note_quads->gain[last];
        note_quads->voice[freshest_index] = note_quads->voice[last];
        note_quads->hidden[freshest_index] = note_quads->hidden[last];
        note_quads->mute[freshest_index] = note_quads->mute[last];
    }

    (*note_quads_count)--;

    // TODO: send delete command to audio thread
}

static inline void war_notes_delete_at_i(war_note_quads* note_quads,
                                         uint32_t* note_quads_count,
                                         war_producer_consumer* pc,
                                         war_lua_context* ctx_lua,
                                         uint32_t i_delete) {
    if (*note_quads_count == 0 || i_delete >= *note_quads_count) return;
    double d_start_sec = note_quads->pos_x[i_delete] *
                         ((60.0 / atomic_load(&ctx_lua->A_BPM) / 4.0));
    uint64_t start_frames =
        (uint64_t)llround(d_start_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    double d_duration_sec = ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) *
                            ((double)note_quads->size_x[i_delete]);
    uint64_t duration_frames = (uint64_t)llround(
        d_duration_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    war_note_msg note_msg = {.note_start_frames = start_frames,
                             .note_duration_frames = duration_frames,
                             .note_sample_index = note_quads->pos_y[i_delete],
                             .note_gain = 1.0f,
                             .note_attack = 0.0f,
                             .note_sustain = 1.0f,
                             .note_release = 0.0f};
    war_pc_to_a(pc, AUDIO_CMD_DELETE_NOTE, sizeof(note_msg), &note_msg);
    uint32_t last = *note_quads_count - 1;
    // Swap the element to delete with the last element
    uint64_t swapped_time = note_quads->timestamp[last];
    note_quads->timestamp[i_delete] = note_quads->timestamp[last];
    note_quads->pos_x[i_delete] = note_quads->pos_x[last];
    note_quads->pos_y[i_delete] = note_quads->pos_y[last];
    note_quads->navigation_x[i_delete] = note_quads->navigation_x[last];
    note_quads->size_x[i_delete] = note_quads->size_x[last];
    note_quads->navigation_x_numerator[i_delete] =
        note_quads->navigation_x_numerator[last];
    note_quads->navigation_x_denominator[i_delete] =
        note_quads->navigation_x_denominator[last];
    note_quads->size_x_numerator[i_delete] = note_quads->size_x_numerator[last];
    note_quads->size_x_denominator[i_delete] =
        note_quads->size_x_denominator[last];
    note_quads->color[i_delete] = note_quads->color[last];
    note_quads->outline_color[i_delete] = note_quads->outline_color[last];
    note_quads->gain[i_delete] = note_quads->gain[last];
    note_quads->voice[i_delete] = note_quads->voice[last];
    note_quads->hidden[i_delete] = note_quads->hidden[last];
    note_quads->mute[i_delete] = note_quads->mute[last];
    (*note_quads_count)--;
    int32_t i = i_delete;
    while (i > 0 && note_quads->timestamp[i] < note_quads->timestamp[i - 1]) {
        uint64_t tmp_time = note_quads->timestamp[i - 1];
        note_quads->timestamp[i - 1] = note_quads->timestamp[i];
        note_quads->timestamp[i] = tmp_time;
        double tmp_col = note_quads->pos_x[i - 1];
        note_quads->pos_x[i - 1] = note_quads->pos_x[i];
        note_quads->pos_x[i] = tmp_col;
        double tmp_row = note_quads->pos_y[i - 1];
        note_quads->pos_y[i - 1] = note_quads->pos_y[i];
        note_quads->pos_y[i] = tmp_row;
        double tmp_navigation_x = note_quads->navigation_x[i - 1];
        note_quads->navigation_x[i - 1] = note_quads->navigation_x[i];
        note_quads->navigation_x[i] = tmp_navigation_x;
        double tmp_size_x = note_quads->size_x[i - 1];
        note_quads->size_x[i - 1] = note_quads->size_x[i];
        note_quads->size_x[i] = tmp_size_x;
        uint32_t tmp_size_x_numerator = note_quads->size_x_numerator[i - 1];
        note_quads->size_x_numerator[i - 1] = note_quads->size_x_numerator[i];
        note_quads->size_x_numerator[i] = tmp_size_x_numerator;
        uint32_t tmp_size_x_denominator = note_quads->size_x_denominator[i - 1];
        note_quads->size_x_denominator[i - 1] =
            note_quads->size_x_denominator[i];
        note_quads->size_x_denominator[i] = tmp_size_x_denominator;
        uint32_t tmp_navigation_x_numerator =
            note_quads->navigation_x_numerator[i - 1];
        note_quads->navigation_x_numerator[i - 1] =
            note_quads->navigation_x_numerator[i];
        note_quads->navigation_x_numerator[i] = tmp_navigation_x_numerator;
        uint32_t tmp_navigation_x_denominator =
            note_quads->navigation_x_denominator[i - 1];
        note_quads->navigation_x_denominator[i - 1] =
            note_quads->navigation_x_denominator[i];
        note_quads->navigation_x_denominator[i] = tmp_navigation_x_denominator;
        uint32_t tmp_color = note_quads->color[i - 1];
        note_quads->color[i - 1] = note_quads->color[i];
        note_quads->color[i] = tmp_color;
        uint32_t tmp_outline = note_quads->outline_color[i - 1];
        note_quads->outline_color[i - 1] = note_quads->outline_color[i];
        note_quads->outline_color[i] = tmp_outline;
        uint8_t tmp_strength = note_quads->gain[i - 1];
        note_quads->gain[i - 1] = note_quads->gain[i];
        note_quads->gain[i] = tmp_strength;
        uint8_t tmp_voice = note_quads->voice[i - 1];
        note_quads->voice[i - 1] = note_quads->voice[i];
        note_quads->voice[i] = tmp_voice;
        uint8_t tmp_hidden = note_quads->hidden[i - 1];
        note_quads->hidden[i - 1] = note_quads->hidden[i];
        note_quads->hidden[i] = tmp_hidden;
        uint8_t tmp_mute = note_quads->mute[i - 1];
        note_quads->mute[i - 1] = note_quads->mute[i];
        note_quads->mute[i] = tmp_mute;
        i--;
    }
    i = i_delete;
    while (i < (int32_t)(*note_quads_count - 1) &&
           note_quads->timestamp[i] > note_quads->timestamp[i + 1]) {
        uint64_t tmp_time = note_quads->timestamp[i + 1];
        note_quads->timestamp[i + 1] = note_quads->timestamp[i];
        note_quads->timestamp[i] = tmp_time;
        double tmp_col = note_quads->pos_x[i + 1];
        note_quads->pos_x[i + 1] = note_quads->pos_x[i];
        note_quads->pos_x[i] = tmp_col;
        double tmp_row = note_quads->pos_y[i + 1];
        note_quads->pos_y[i + 1] = note_quads->pos_y[i];
        note_quads->pos_y[i] = tmp_row;
        double tmp_navigation_x = note_quads->navigation_x[i + 1];
        note_quads->navigation_x[i + 1] = note_quads->navigation_x[i];
        note_quads->navigation_x[i] = tmp_navigation_x;
        double tmp_size_x = note_quads->size_x[i + 1];
        note_quads->size_x[i + 1] = note_quads->size_x[i];
        note_quads->size_x[i] = tmp_size_x;
        uint32_t tmp_size_x_numerator = note_quads->size_x_numerator[i + 1];
        note_quads->size_x_numerator[i + 1] = note_quads->size_x_numerator[i];
        note_quads->size_x_numerator[i] = tmp_size_x_numerator;
        uint32_t tmp_size_x_denominator = note_quads->size_x_denominator[i + 1];
        note_quads->size_x_denominator[i + 1] =
            note_quads->size_x_denominator[i];
        note_quads->size_x_denominator[i] = tmp_size_x_denominator;
        uint32_t tmp_navigation_x_numerator =
            note_quads->navigation_x_numerator[i + 1];
        note_quads->navigation_x_numerator[i + 1] =
            note_quads->navigation_x_numerator[i];
        note_quads->navigation_x_numerator[i] = tmp_navigation_x_numerator;
        uint32_t tmp_navigation_x_denominator =
            note_quads->navigation_x_denominator[i + 1];
        note_quads->navigation_x_denominator[i + 1] =
            note_quads->navigation_x_denominator[i];
        note_quads->navigation_x_denominator[i] = tmp_navigation_x_denominator;
        uint32_t tmp_color = note_quads->color[i + 1];
        note_quads->color[i + 1] = note_quads->color[i];
        note_quads->color[i] = tmp_color;
        uint32_t tmp_outline = note_quads->outline_color[i + 1];
        note_quads->outline_color[i + 1] = note_quads->outline_color[i];
        note_quads->outline_color[i] = tmp_outline;
        uint8_t tmp_strength = note_quads->gain[i + 1];
        note_quads->gain[i + 1] = note_quads->gain[i];
        note_quads->gain[i] = tmp_strength;
        uint8_t tmp_voice = note_quads->voice[i + 1];
        note_quads->voice[i + 1] = note_quads->voice[i];
        note_quads->voice[i] = tmp_voice;
        uint8_t tmp_hidden = note_quads->hidden[i + 1];
        note_quads->hidden[i + 1] = note_quads->hidden[i];
        note_quads->hidden[i] = tmp_hidden;
        uint8_t tmp_mute = note_quads->mute[i + 1];
        note_quads->mute[i + 1] = note_quads->mute[i];
        note_quads->mute[i] = tmp_mute;
        i++;
    }
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

static inline void war_notes_trim_right_at_i(war_note_quads* note_quads,
                                             uint32_t* note_quads_count,
                                             war_window_render_context* ctx_wr,
                                             war_producer_consumer* pc,
                                             war_lua_context* ctx_lua,
                                             uint32_t i_trim) {
    if (*note_quads_count == 0 || i_trim >= *note_quads_count) return;
    double d_start_sec = note_quads->pos_x[i_trim] *
                         ((60.0 / atomic_load(&ctx_lua->A_BPM) / 4.0));
    uint64_t start_frames =
        (uint64_t)llround(d_start_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    double d_duration_sec = ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) *
                            ((double)note_quads->size_x[i_trim]);
    uint64_t duration_frames = (uint64_t)llround(
        d_duration_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    war_note_msg old_note_msg = {.note_start_frames = start_frames,
                                 .note_duration_frames = duration_frames,
                                 .note_sample_index = note_quads->pos_y[i_trim],
                                 .note_gain = 1.0f,
                                 .note_attack = 0.0f,
                                 .note_sustain = 1.0f,
                                 .note_release = 0.0f};
    double new_size_x = ctx_wr->cursor_pos_x - note_quads->pos_x[i_trim];
    double new_d_duration_sec =
        ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) * (new_size_x);
    uint64_t new_duration_frames = (uint64_t)llround(
        new_d_duration_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    uint8_t payload[sizeof(war_note_msg) + sizeof(uint64_t)];
    memcpy(payload, &old_note_msg, sizeof(war_note_msg));
    memcpy(
        payload + sizeof(war_note_msg), &new_duration_frames, sizeof(uint64_t));
    war_pc_to_a(pc, AUDIO_CMD_REPLACE_NOTE_DURATION, sizeof(payload), payload);
    note_quads->size_x[i_trim] = new_size_x;
}

static inline void war_notes_trim_left_at_i(war_note_quads* note_quads,
                                            uint32_t* note_quads_count,
                                            war_window_render_context* ctx_wr,
                                            war_producer_consumer* pc,
                                            war_lua_context* ctx_lua,
                                            uint32_t i_trim) {
    if (*note_quads_count == 0 || i_trim >= *note_quads_count) return;
    double d_start_sec = note_quads->pos_x[i_trim] *
                         ((60.0 / atomic_load(&ctx_lua->A_BPM) / 4.0));
    uint64_t start_frames =
        (uint64_t)llround(d_start_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    double d_duration_sec = ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) *
                            ((double)note_quads->size_x[i_trim]);
    uint64_t duration_frames = (uint64_t)llround(
        d_duration_sec * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    war_note_msg old_note_msg = {.note_start_frames = start_frames,
                                 .note_duration_frames = duration_frames,
                                 .note_sample_index = note_quads->pos_y[i_trim],
                                 .note_gain = 1.0f,
                                 .note_attack = 0.0f,
                                 .note_sustain = 1.0f,
                                 .note_release = 0.0f};
    double new_pos_x = ctx_wr->cursor_pos_x + ctx_wr->cursor_size_x;
    double new_size_x =
        note_quads->size_x[i_trim] - (new_pos_x - note_quads->pos_x[i_trim]);
    double new_d_pos_seconds =
        ((60.0 / atomic_load(&ctx_lua->A_BPM)) / 4.0) * (new_pos_x);
    uint64_t new_start_frames = (uint64_t)llround(
        new_d_pos_seconds * atomic_load(&ctx_lua->A_SAMPLE_RATE));
    uint64_t new_duration_frames =
        duration_frames - (new_start_frames - start_frames);
    war_note_msg new_note_msg = {
        .note_start_frames = new_start_frames,
        .note_duration_frames = new_duration_frames,
        .note_sample_index = note_quads->pos_y[i_trim],
    };
    uint8_t payload[sizeof(war_note_msg) * 2];
    memcpy(payload, &old_note_msg, sizeof(war_note_msg));
    memcpy(payload + sizeof(war_note_msg), &new_note_msg, sizeof(war_note_msg));
    war_pc_to_a(pc, AUDIO_CMD_REPLACE_NOTE, sizeof(payload), payload);
    note_quads->pos_x[i_trim] = new_pos_x;
    note_quads->size_x[i_trim] = new_size_x;
}

static inline void war_note_quads_in_view(war_note_quads* note_quads,
                                          uint32_t note_quads_count,
                                          war_window_render_context* ctx_wr,
                                          uint32_t* out_indices,
                                          uint32_t* out_indices_count) {
    for (uint32_t i = 0; i < note_quads_count; i++) {
        double leftover_x = fmodf(ctx_wr->physical_width, ctx_wr->cell_width) /
                            ctx_wr->cell_width;
        double note_pos_x = note_quads->pos_x[i];
        double note_span_x = note_quads->size_x[i];
        double note_pos_y = note_quads->pos_y[i];
        double note_pos_x_end = note_pos_x + note_span_x;
        double left_col = ctx_wr->left_col;
        double right_col = ctx_wr->right_col + leftover_x + 1;
        double bottom_row = ctx_wr->bottom_row;
        double top_row = ctx_wr->top_row;
        bool in_view = (note_pos_x <= right_col && note_pos_x_end > left_col) &&
                       (note_pos_y >= bottom_row && note_pos_y <= top_row);
        if (in_view) { out_indices[(*out_indices_count)++] = i; }
    }
}

static inline void
war_note_quads_outside_view(war_note_quads* note_quads,
                            uint32_t note_quads_count,
                            war_window_render_context* ctx_wr,
                            uint32_t* out_indices,
                            uint32_t* out_indices_count) {
    for (uint32_t i = 0; i < note_quads_count; i++) {
        double leftover_x = fmodf(ctx_wr->physical_width, ctx_wr->cell_width) /
                            ctx_wr->cell_width;
        double note_pos_x = note_quads->pos_x[i];
        double note_span_x = note_quads->size_x[i];
        double note_pos_y = note_quads->pos_y[i];
        double note_pos_x_end = note_pos_x + note_span_x;
        double left_col = ctx_wr->left_col;
        double right_col = ctx_wr->right_col + leftover_x + 1;
        double bottom_row = ctx_wr->bottom_row;
        double top_row = ctx_wr->top_row;
        bool outside_view =
            !((note_pos_x <= right_col && note_pos_x_end > left_col) &&
              (note_pos_y >= bottom_row && note_pos_y <= top_row));
        if (outside_view) { out_indices[(*out_indices_count)++] = i; }
    }
}

static inline void
war_note_quads_under_cursor(war_note_quads* note_quads,
                            uint32_t note_quads_count,
                            war_window_render_context* ctx_wr,
                            uint32_t* out_indices,
                            uint32_t* out_indices_count) {
    double cursor_pos_x = ctx_wr->cursor_pos_x;
    double cursor_span_x = ctx_wr->cursor_size_x;
    double cursor_pos_x_end = cursor_pos_x + cursor_span_x;
    double cursor_pos_y = ctx_wr->cursor_pos_y;
    double cursor_pos_y_end = cursor_pos_y + 1;
    for (uint32_t i = 0; i < note_quads_count; i++) {
        double note_pos_x = note_quads->pos_x[i];
        double note_span_x = note_quads->size_x[i];
        double note_pos_x_end = note_pos_x + note_span_x;
        double note_pos_y = note_quads->pos_y[i];
        double note_pos_y_end = note_pos_y + 1;
        bool under_cursor =
            (note_pos_x < cursor_pos_x_end && note_pos_x_end > cursor_pos_x) &&
            (note_pos_y == cursor_pos_y);
        if (under_cursor) { out_indices[(*out_indices_count)++] = i; }
    }
}

static inline void war_note_quads_in_row(war_note_quads* note_quads,
                                         uint32_t note_quads_count,
                                         war_window_render_context* ctx_wr,
                                         uint32_t* out_indices,
                                         uint32_t* out_indices_count) {
    uint32_t row = ctx_wr->cursor_pos_y;
    for (uint32_t i = 0; i < note_quads_count; i++) {
        if (row == note_quads->pos_y[i] && !note_quads->hidden[i]) {
            out_indices[(*out_indices_count)++] = i;
        }
    }
}

static inline void war_note_quads_in_col(war_note_quads* note_quads,
                                         uint32_t note_quads_count,
                                         war_window_render_context* ctx_wr,
                                         uint32_t* out_indices,
                                         uint32_t* out_indices_count) {
    double cursor_pos_x = ctx_wr->cursor_pos_x;
    double cursor_span_x = ctx_wr->cursor_size_x;
    double cursor_pos_x_end = cursor_pos_x + cursor_span_x;
    double cursor_pos_y = ctx_wr->cursor_pos_y;
    double cursor_pos_y_end = cursor_pos_y + 1;
    for (uint32_t i = 0; i < note_quads_count; i++) {
        double note_pos_x = note_quads->pos_x[i];
        double note_span_x = note_quads->size_x[i];
        double note_pos_x_end = note_pos_x + note_span_x;
        double note_pos_y = note_quads->pos_y[i];
        double note_pos_y_end = note_pos_y + 1;
        bool under_cursor =
            (note_pos_x < cursor_pos_x_end && note_pos_x_end > cursor_pos_x);
        if (under_cursor) { out_indices[(*out_indices_count)++] = i; }
    }
}

static inline float war_midi_to_frequency(float midi_note) {
    return 440.0f * pow(2.0f, (midi_note - 69) / 12.0f);
}

static inline float war_sine_phase_increment(war_audio_context* ctx_a,
                                             float frequency) {
    return (2.0f * M_PI * frequency) / (float)ctx_a->sample_rate;
}

#endif // WAR_MACROS_H
