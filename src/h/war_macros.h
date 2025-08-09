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
#include "h/war_debug_macros.h"

#include <math.h>
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

static inline rgba_t unpack_abgr(uint32_t hex_color) {
    rgba_t color;
    color.r = ((hex_color >> 0) & 0xFF) / 255.0f;  // Red
    color.g = ((hex_color >> 8) & 0xFF) / 255.0f;  // Green
    color.b = ((hex_color >> 16) & 0xFF) / 255.0f; // Blue
    color.a = ((hex_color >> 24) & 0xFF) / 255.0f; // Alpha
    return color;
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

static inline uint32_t clamp_add_uint32(uint32_t a, uint32_t b) {
    uint64_t sum = (uint64_t)a + b;
    // create mask: 0 if no overflow, all 1s if overflow
    uint64_t mask = -(sum > UINT32_MAX);
    return (uint32_t)((sum & ~mask) | (UINT32_MAX & mask));
}

static inline uint32_t clamp_subtract_uint32(uint32_t a, uint32_t b) {
    uint32_t diff = a - b;
    uint32_t mask = -(a >= b);
    return diff & mask;
}

static inline uint32_t clamp_multiply_uint32(uint32_t a, uint32_t b) {
    uint64_t prod = (uint64_t)a * (uint64_t)b;
    return (prod > UINT32_MAX) ? UINT32_MAX : (uint32_t)prod;
}

static inline uint32_t clamp_to_uint32_max(uint64_t val) {
    return (val > UINT32_MAX) ? UINT32_MAX : (uint32_t)val;
}

static inline uint32_t abs_diff_uint32(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}

static inline void cmd_increment_row(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->row_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->row = clamp_add_uint32(ctx->row, increment);
    if (ctx->row > ctx->top_row - ctx->scroll_margin_rows) {
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        ctx->bottom_row = clamp_add_uint32(ctx->bottom_row, increment);
        ctx->top_row = clamp_add_uint32(ctx->top_row, increment);
        uint32_t new_viewport_height = ctx->top_row - ctx->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_decrement_row(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->row_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->row = clamp_subtract_uint32(ctx->row, increment);
    if (ctx->row < ctx->bottom_row + ctx->scroll_margin_rows) {
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, increment);
        ctx->top_row = clamp_subtract_uint32(ctx->top_row, increment);
        uint32_t new_viewport_height = ctx->top_row - ctx->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx->top_row = clamp_add_uint32(ctx->top_row, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_increment_col(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->col_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->col = clamp_add_uint32(ctx->col, increment);
    if (ctx->col > ctx->right_col - ctx->scroll_margin_cols) {
        uint32_t viewport_width = ctx->right_col - ctx->left_col;
        ctx->left_col = clamp_add_uint32(ctx->left_col, increment);
        ctx->right_col = clamp_add_uint32(ctx->right_col, increment);
        uint32_t new_viewport_width = ctx->right_col - ctx->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx->left_col = clamp_subtract_uint32(ctx->left_col, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_decrement_col(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->col_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->col = clamp_subtract_uint32(ctx->col, increment);
    if (ctx->col < ctx->left_col + ctx->scroll_margin_cols) {
        uint32_t viewport_width = ctx->right_col - ctx->left_col;
        ctx->left_col = clamp_subtract_uint32(ctx->left_col, increment);
        ctx->right_col = clamp_subtract_uint32(ctx->right_col, increment);
        uint32_t new_viewport_width = ctx->right_col - ctx->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx->right_col = clamp_add_uint32(ctx->right_col, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_leap_increment_row(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->row_leap_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->row = clamp_add_uint32(ctx->row, increment);
    if (ctx->row > ctx->top_row - ctx->scroll_margin_rows) {
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        ctx->bottom_row = clamp_add_uint32(ctx->bottom_row, increment);
        ctx->top_row = clamp_add_uint32(ctx->top_row, increment);
        uint32_t new_viewport_height = ctx->top_row - ctx->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_leap_decrement_row(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->row_leap_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->row = clamp_subtract_uint32(ctx->row, increment);
    if (ctx->row < ctx->bottom_row + ctx->scroll_margin_rows) {
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, increment);
        ctx->top_row = clamp_subtract_uint32(ctx->top_row, increment);
        uint32_t new_viewport_height = ctx->top_row - ctx->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx->top_row = clamp_add_uint32(ctx->top_row, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_leap_increment_col(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->col_leap_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->col = clamp_add_uint32(ctx->col, increment);
    if (ctx->col > ctx->right_col - ctx->scroll_margin_cols) {
        uint32_t viewport_width = ctx->right_col - ctx->left_col;
        ctx->left_col = clamp_add_uint32(ctx->left_col, increment);
        ctx->right_col = clamp_add_uint32(ctx->right_col, increment);
        uint32_t new_viewport_width = ctx->right_col - ctx->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx->left_col = clamp_subtract_uint32(ctx->left_col, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_leap_decrement_col(war_input_cmd_context* ctx) {
    uint32_t increment = ctx->col_leap_increment;
    if (ctx->numeric_prefix) {
        increment = clamp_multiply_uint32(increment, ctx->numeric_prefix);
    }
    ctx->col = clamp_subtract_uint32(ctx->col, increment);
    if (ctx->col < ctx->left_col + ctx->scroll_margin_cols) {
        uint32_t viewport_width = ctx->right_col - ctx->left_col;
        ctx->left_col = clamp_subtract_uint32(ctx->left_col, increment);
        ctx->right_col = clamp_subtract_uint32(ctx->right_col, increment);
        uint32_t new_viewport_width = ctx->right_col - ctx->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx->right_col = clamp_add_uint32(ctx->right_col, diff);
        }
    }
    ctx->numeric_prefix = 0;
}

static inline void cmd_goto_bottom_row(war_input_cmd_context* ctx) {
    if (ctx->numeric_prefix) {
        ctx->row = ctx->numeric_prefix;
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        uint32_t distance = viewport_height / 2;
        ctx->bottom_row = clamp_subtract_uint32(ctx->row, distance);
        ctx->top_row = clamp_add_uint32(ctx->row, distance);
        uint32_t new_viewport_height =
            clamp_subtract_uint32(ctx->top_row, ctx->bottom_row);
        if (new_viewport_height < viewport_height) {
            uint32_t diff =
                clamp_subtract_uint32(viewport_height, new_viewport_height);
            uint32_t sum = clamp_add_uint32(ctx->top_row, diff);
            if (sum < UINT32_MAX) {
                ctx->top_row = sum;
            } else {
                ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, diff);
            }
        }
        ctx->numeric_prefix = 0;
        return;
    }
    ctx->row = ctx->bottom_row;
    ctx->numeric_prefix = 0;
}

static inline void cmd_goto_left_col(war_input_cmd_context* ctx) {
    if (ctx->numeric_prefix) {
        ctx->numeric_prefix = ctx->numeric_prefix * 10;
        return;
    }
    ctx->col = ctx->left_col;
    ctx->numeric_prefix = 0;
}

static inline void cmd_goto_top_row(war_input_cmd_context* ctx) {
    if (ctx->numeric_prefix) {
        ctx->row = ctx->numeric_prefix;
        uint32_t viewport_height = ctx->top_row - ctx->bottom_row;
        uint32_t distance = viewport_height / 2;
        ctx->bottom_row = clamp_subtract_uint32(ctx->row, distance);
        ctx->top_row = clamp_add_uint32(ctx->row, distance);
        uint32_t new_viewport_height =
            clamp_subtract_uint32(ctx->top_row, ctx->bottom_row);
        if (new_viewport_height < viewport_height) {
            uint32_t diff =
                clamp_subtract_uint32(viewport_height, new_viewport_height);
            uint32_t sum = clamp_add_uint32(ctx->top_row, diff);
            if (sum < UINT32_MAX) {
                ctx->top_row = sum;
            } else {
                ctx->bottom_row = clamp_subtract_uint32(ctx->bottom_row, diff);
            }
        }
        ctx->numeric_prefix = 0;
        return;
    }
    ctx->row = ctx->top_row;
    ctx->numeric_prefix = 0;
}

static inline void cmd_goto_right_col(war_input_cmd_context* ctx) {
    if (ctx->numeric_prefix) {
        ctx->col = ctx->numeric_prefix;
        uint32_t viewport_width = ctx->right_col - ctx->left_col;
        uint32_t distance = viewport_width / 2;
        ctx->left_col = clamp_subtract_uint32(ctx->col, distance);
        ctx->right_col = clamp_add_uint32(ctx->col, distance);
        uint32_t new_viewport_width =
            clamp_subtract_uint32(ctx->right_col, ctx->left_col);
        if (new_viewport_width < viewport_width) {
            uint32_t diff =
                clamp_subtract_uint32(viewport_width, new_viewport_width);
            uint32_t sum = clamp_add_uint32(ctx->right_col, diff);
            if (sum < UINT32_MAX) {
                ctx->right_col = sum;
            } else {
                ctx->left_col = clamp_subtract_uint32(ctx->left_col, diff);
            }
        }
        ctx->numeric_prefix = 0;
        return;
    }
    ctx->col = ctx->right_col;
    ctx->numeric_prefix = 0;
}

static inline void cmd_append_1_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 1);
}

static inline void cmd_append_2_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 2);
}

static inline void cmd_append_3_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 3);
}

static inline void cmd_append_4_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 4);
}

static inline void cmd_append_5_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 5);
}

static inline void cmd_append_6_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 6);
}

static inline void cmd_append_7_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 7);
}

static inline void cmd_append_8_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 8);
}

static inline void cmd_append_9_to_numeric_prefix(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = clamp_multiply_uint32(ctx->numeric_prefix, 10);
    ctx->numeric_prefix = clamp_add_uint32(ctx->numeric_prefix, 9);
}

static inline void cmd_zoom_in(war_input_cmd_context* ctx) {
    // ctx->zoom_scale += ctx->zoom_increment;
    // if (ctx->zoom_scale > 5.0f) { ctx->zoom_scale = 5.0f; }
    // ctx->panning_x = ctx->anchor_ndc_x - ctx->anchor_x * ctx->zoom_scale;
    // ctx->panning_y = ctx->anchor_ndc_y - ctx->anchor_y * ctx->zoom_scale;
}

static inline void cmd_zoom_out(war_input_cmd_context* ctx) {
    // ctx->zoom_scale -= ctx->zoom_increment;
    // if (ctx->zoom_scale < 0.1f) { ctx->zoom_scale = 0.1f; }
    // ctx->panning_x = ctx->anchor_ndc_x - ctx->anchor_x * ctx->zoom_scale;
    // ctx->panning_y = ctx->anchor_ndc_y - ctx->anchor_y * ctx->zoom_scale;
}

static inline void cmd_zoom_in_leap(war_input_cmd_context* ctx) {
    // ctx->zoom_scale += ctx->zoom_leap_increment;
    // if (ctx->zoom_scale > 5.0f) { ctx->zoom_scale = 5.0f; }
    // ctx->panning_x = ctx->anchor_ndc_x - ctx->anchor_x * ctx->zoom_scale;
    // ctx->panning_y = ctx->anchor_ndc_y - ctx->anchor_y * ctx->zoom_scale;
}

static inline void cmd_zoom_out_leap(war_input_cmd_context* ctx) {
    // ctx->zoom_scale -= ctx->zoom_leap_increment;
    // if (ctx->zoom_scale < 0.1f) { ctx->zoom_scale = 0.1f; }
    // ctx->panning_x = ctx->anchor_ndc_x - ctx->anchor_x * ctx->zoom_scale;
    // ctx->panning_y = ctx->anchor_ndc_y - ctx->anchor_y * ctx->zoom_scale;
}

static inline void cmd_reset_zoom(war_input_cmd_context* ctx) {
    ctx->zoom_scale = 1.0f;
    ctx->panning_x = (2.0f * ctx->num_cols_for_line_numbers * ctx->cell_width) /
                     ctx->physical_width;
    ctx->panning_y =
        -(2.0f * ctx->num_rows_for_status_bars * ctx->cell_height) /
        ctx->physical_height;
    ctx->left_col = 0;
    ctx->bottom_row = 0;
    ctx->right_col =
        (uint32_t)((ctx->physical_width -
                    ((float)ctx->num_cols_for_line_numbers * ctx->cell_width)) /
                   ctx->cell_width) -
        1;
    ctx->top_row =
        (uint32_t)((ctx->physical_height -
                    ((float)ctx->num_rows_for_status_bars * ctx->cell_height)) /
                   ctx->cell_height) -
        1;
}

static inline void cmd_escape(war_input_cmd_context* ctx) {
    ctx->numeric_prefix = 0;
}

// Returns matched command, and sets *out_matched_length
static inline void (*war_match_sequence_in_trie(war_key_trie_pool* pool,
                                                war_key_event* input_seq,
                                                size_t input_len,
                                                size_t* out_matched_length))(
    war_input_cmd_context*) {
    war_key_trie_node* node = &pool->nodes[0];
    void (*matched_command)(war_input_cmd_context*) = NULL;
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
            matched_command = (void (*)(war_input_cmd_context*))node->command;
            matched_len = len + 1;
        }
    }

    if (out_matched_length) { *out_matched_length = matched_len; }
    return matched_command;
}

#endif // WAR_MACROS_H
