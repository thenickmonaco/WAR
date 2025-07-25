//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_macros.h
//-----------------------------------------------------------------------------

#ifndef WAR_MACROS_H
#define WAR_MACROS_H

#include "h/war_data.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define obj_op_index(obj, op) ((obj) * max_opcodes + (op))

#define keysym_mode_modifier_state_index(keysym, mode, keystate, mod)          \
    ((keysym) + max_keysyms * ((mode) + max_modes * ((keystate) +              \
                                                     max_keystates * (mod))))

// COMMENT OPTIMIZE: Duff's Device + SIMD (intrinsics)

static inline uint64_t get_monotonic_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static inline int32_t to_fixed(float f) { return (int32_t)(f * 256.0f); }

static inline uint32_t pad_to_scale(float value, uint32_t scale) {
    uint32_t rounded = (uint32_t)(value + 0.5f);
    return (rounded + scale - 1) / scale * scale;
}

static inline uint64_t read_le64(const uint8_t* p) {
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline uint32_t read_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t read_le16(const uint8_t* p) {
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

static inline void write_le64(uint8_t* p, uint64_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32);
    p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48);
    p[7] = (uint8_t)(v >> 56);
}

static inline void write_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static inline void
cmd_increment_row(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *row += numeric_prefix;
}

static inline void
cmd_decrement_row(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *row -= numeric_prefix;
}

static inline void
cmd_increment_col(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *col += numeric_prefix;
}

static inline void
cmd_decrement_col(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *col -= numeric_prefix;
}

static inline void
cmd_goto_row_0(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *row = 0;
}

static inline void
cmd_goto_col_0(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *col = 0;
}

static inline void
cmd_goto_row_end(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *row = 4;
}

static inline void
cmd_goto_col_end(uint32_t* col, uint32_t* row, uint32_t numeric_prefix) {
    *col = 4;
}

// Returns matched command, and sets *out_matched_length
static inline void (*war_match_sequence_in_trie(war_key_trie_pool* pool,
                                                war_key_event* input_seq,
                                                size_t input_len,
                                                size_t* out_matched_length))(
    uint32_t*, uint32_t*, uint32_t) {
    war_key_trie_node* node = &pool->nodes[0];
    void (*matched_command)(uint32_t*, uint32_t*, uint32_t) = NULL;
    size_t matched_len = 0;

    for (size_t len = 0; len < input_len; len++) {
        uint32_t keysym = input_seq[len].keysym;
        uint8_t mod = input_seq[len].mod;

        war_key_trie_node* child = NULL;
        for (size_t c = 0; c < node->child_count; c++) {
            war_key_trie_node* candidate = node->children[c];
            if (candidate->keysym == keysym && candidate->mod == mod) {
                child = candidate;
                break;
            }
        }

        if (!child) {
            break; // no further match
        }

        node = child;

        if (node->is_terminal) {
            matched_command =
                (void (*)(uint32_t*, uint32_t*, uint32_t))node->command;
            matched_len = len + 1;
        }
    }

    if (out_matched_length) { *out_matched_length = matched_len; }
    return matched_command;
}

#endif // WAR_MACROS_H
