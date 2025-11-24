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
// src/h/war_commands.h
//-----------------------------------------------------------------------------

#ifndef WAR_COMMANDS_H
#define WAR_COMMANDS_H

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"

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
#include <sys/sendfile.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

static inline void war_roll_cursor_up(war_env* env) {
    call_carmack("war_roll_cursor_up");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    uint32_t scaled_whole = (increment * ctx_wr->navigation_whole_number_row) /
                            ctx_wr->navigation_sub_cells_row;
    uint32_t scaled_frac = (increment * ctx_wr->navigation_whole_number_row) %
                           ctx_wr->navigation_sub_cells_row;
    ctx_wr->cursor_pos_y = war_clamp_add_uint32(
        ctx_wr->cursor_pos_y, scaled_whole, ctx_wr->max_row);
    ctx_wr->sub_row =
        war_clamp_add_uint32(ctx_wr->sub_row, scaled_frac, ctx_wr->max_row);
    ctx_wr->cursor_pos_y =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y,
                             ctx_wr->sub_row / ctx_wr->navigation_sub_cells_row,
                             ctx_wr->max_row);
    ctx_wr->sub_row =
        war_clamp_uint32(ctx_wr->sub_row % ctx_wr->navigation_sub_cells_row,
                         ctx_wr->min_row,
                         ctx_wr->max_row);
    if (ctx_wr->cursor_pos_y > ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_add_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->max_row);
        ctx_wr->top_row =
            war_clamp_add_uint32(ctx_wr->top_row, increment, ctx_wr->max_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_down(war_env* env) {
    call_carmack("war_roll_cursor_down");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    uint32_t scaled_whole = (increment * ctx_wr->navigation_whole_number_row) /
                            ctx_wr->navigation_sub_cells_row;
    uint32_t scaled_frac = (increment * ctx_wr->navigation_whole_number_row) %
                           ctx_wr->navigation_sub_cells_row;
    ctx_wr->cursor_pos_y = war_clamp_add_uint32(
        ctx_wr->cursor_pos_y, scaled_whole, ctx_wr->max_row);
    ctx_wr->sub_row =
        war_clamp_add_uint32(ctx_wr->sub_row, scaled_frac, ctx_wr->max_row);
    ctx_wr->cursor_pos_y =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y,
                             ctx_wr->sub_row / ctx_wr->navigation_sub_cells_row,
                             ctx_wr->max_row);
    ctx_wr->sub_row =
        war_clamp_uint32(ctx_wr->sub_row % ctx_wr->navigation_sub_cells_row,
                         ctx_wr->min_row,
                         ctx_wr->max_row);
    if (ctx_wr->cursor_pos_y > ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_add_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->max_row);
        ctx_wr->top_row =
            war_clamp_add_uint32(ctx_wr->top_row, increment, ctx_wr->max_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_left(war_env* env) {
    call_carmack("war_roll_cursor_left");
    war_window_render_context* ctx_wr = env->ctx_wr;
    double initial = ctx_wr->cursor_pos_x;
    double increment =
        (double)ctx_wr->col_increment * ctx_wr->cursor_navigation_x;
    if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
    ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x - increment;
    if (ctx_wr->cursor_pos_x < ctx_wr->min_col) {
        ctx_wr->cursor_pos_x = ctx_wr->min_col;
    }
    double pan = initial - ctx_wr->cursor_pos_x;
    if (ctx_wr->cursor_pos_x < ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col =
            war_clamp_subtract_uint32(ctx_wr->left_col, pan, ctx_wr->min_col);
        ctx_wr->right_col =
            war_clamp_subtract_uint32(ctx_wr->right_col, pan, ctx_wr->min_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->right_col =
                war_clamp_add_uint32(ctx_wr->right_col, diff, ctx_wr->max_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_right(war_env* env) {
    call_carmack("war_roll_cursor_right");
    war_window_render_context* ctx_wr = env->ctx_wr;
    double initial = ctx_wr->cursor_pos_x;
    double increment =
        (double)ctx_wr->col_increment * ctx_wr->cursor_navigation_x;
    if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
    ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x + increment;
    if (ctx_wr->cursor_pos_x > ctx_wr->max_col) {
        ctx_wr->cursor_pos_x = ctx_wr->max_col;
    }
    double pan = ctx_wr->cursor_pos_x - initial;
    if (ctx_wr->cursor_pos_x > ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col =
            war_clamp_add_uint32(ctx_wr->left_col, pan, ctx_wr->max_col);
        ctx_wr->right_col =
            war_clamp_add_uint32(ctx_wr->right_col, pan, ctx_wr->max_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->left_col = war_clamp_subtract_uint32(
                ctx_wr->left_col, diff, ctx_wr->min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_goto_wav(war_env* env) {
    call_carmack("war_roll_cursor_goto_wav");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_WAV;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_gain_up(war_env* env) {
    call_carmack("war_roll_gain_up");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
    gain = fminf(gain, 1.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_gain_down(war_env* env) {
    call_carmack("war_roll_gain_down");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
    gain = fmaxf(gain, 0.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_up_leap(war_env* env) {
    call_carmack("war_roll_cursor_up_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment = ctx_wr->row_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    ctx_wr->cursor_pos_y =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y, increment, ctx_wr->max_row);
    if (ctx_wr->cursor_pos_y > ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_add_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->max_row);
        ctx_wr->top_row =
            war_clamp_add_uint32(ctx_wr->top_row, increment, ctx_wr->max_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_down_leap(war_env* env) {
    call_carmack("war_roll_cursor_down_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment = ctx_wr->row_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_y, increment, ctx_wr->min_row);
    if (ctx_wr->cursor_pos_y <
        ctx_wr->bottom_row + ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_subtract_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->min_row);
        ctx_wr->top_row = war_clamp_subtract_uint32(
            ctx_wr->top_row, increment, ctx_wr->min_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->top_row =
                war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_right_leap(war_env* env) {
    call_carmack("war_roll_cursor_right_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    double initial = ctx_wr->cursor_pos_x;
    double increment =
        (double)ctx_wr->col_leap_increment * ctx_wr->cursor_navigation_x;
    if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
    ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x + increment;
    if (ctx_wr->cursor_pos_x > ctx_wr->max_col) {
        ctx_wr->cursor_pos_x = ctx_wr->max_col;
    }
    double pan = ctx_wr->cursor_pos_x - initial;
    if (ctx_wr->cursor_pos_x > ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col =
            war_clamp_add_uint32(ctx_wr->left_col, pan, ctx_wr->max_col);
        ctx_wr->right_col =
            war_clamp_add_uint32(ctx_wr->right_col, pan, ctx_wr->max_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->left_col = war_clamp_subtract_uint32(
                ctx_wr->left_col, diff, ctx_wr->min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_left_leap(war_env* env) {
    call_carmack("war_roll_cursor_left_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    double initial = ctx_wr->cursor_pos_x;
    double increment =
        (double)ctx_wr->col_leap_increment * ctx_wr->cursor_navigation_x;
    if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
    ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x - increment;
    if (ctx_wr->cursor_pos_x < ctx_wr->min_col) {
        ctx_wr->cursor_pos_x = ctx_wr->min_col;
    }
    double pan = initial - ctx_wr->cursor_pos_x;
    if (ctx_wr->cursor_pos_x < ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col =
            war_clamp_subtract_uint32(ctx_wr->left_col, pan, ctx_wr->min_col);
        ctx_wr->right_col =
            war_clamp_subtract_uint32(ctx_wr->right_col, pan, ctx_wr->min_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->right_col =
                war_clamp_add_uint32(ctx_wr->right_col, diff, ctx_wr->max_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_map_layer_to_cursor_row(war_env* env) {
    call_carmack("war_roll_map_layer_to_cursor_row");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_play_context* ctx_play = env->ctx_play;
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_play->note_layers[(int)ctx_wr->cursor_pos_y] = layer;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_get_layer_from_row(war_env* env) {
    call_carmack("war_roll_get_layer_from_row");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_toggle_flux(war_env* env) {
    call_carmack("war_roll_toggle_flux");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->layer_flux = !ctx_wr->layer_flux;
    war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_up_view(war_env* env) {
    call_carmack("war_roll_cursor_up_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment =
        ctx_wr->viewport_rows - ctx_wr->num_rows_for_status_bars;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    ctx_wr->cursor_pos_y =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y, increment, ctx_wr->max_row);
    if (ctx_wr->cursor_pos_y > ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_add_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->max_row);
        ctx_wr->top_row =
            war_clamp_add_uint32(ctx_wr->top_row, increment, ctx_wr->max_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_down_view(war_env* env) {
    call_carmack("war_roll_cursor_down_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    uint32_t increment =
        ctx_wr->viewport_rows - ctx_wr->num_rows_for_status_bars;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
    }
    ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_y, increment, ctx_wr->min_row);
    if (ctx_wr->cursor_pos_y <
        ctx_wr->bottom_row + ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        ctx_wr->bottom_row = war_clamp_subtract_uint32(
            ctx_wr->bottom_row, increment, ctx_wr->min_row);
        ctx_wr->top_row = war_clamp_subtract_uint32(
            ctx_wr->top_row, increment, ctx_wr->min_row);
        uint32_t new_viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            ctx_wr->top_row =
                war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_left_view(war_env* env) {
    call_carmack("war_roll_cursor_left_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    uint32_t increment =
        ctx_wr->viewport_cols - ctx_wr->num_cols_for_line_numbers;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_col);
    }
    ctx_wr->cursor_pos_x = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_x, increment, ctx_wr->min_col);
    if (ctx_wr->cursor_pos_x < ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col = war_clamp_subtract_uint32(
            ctx_wr->left_col, increment, ctx_wr->min_col);
        ctx_wr->right_col = war_clamp_subtract_uint32(
            ctx_wr->right_col, increment, ctx_wr->min_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->right_col =
                war_clamp_add_uint32(ctx_wr->right_col, diff, ctx_wr->max_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_right_view(war_env* env) {
    call_carmack("war_roll_cursor_right_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    uint32_t increment =
        ctx_wr->viewport_cols - ctx_wr->num_cols_for_line_numbers;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, ctx_wr->max_col);
    }
    ctx_wr->cursor_pos_x =
        war_clamp_add_uint32(ctx_wr->cursor_pos_x, increment, ctx_wr->max_col);
    if (ctx_wr->cursor_pos_x > ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        ctx_wr->left_col =
            war_clamp_add_uint32(ctx_wr->left_col, increment, ctx_wr->max_col);
        ctx_wr->right_col =
            war_clamp_add_uint32(ctx_wr->right_col, increment, ctx_wr->max_col);
        uint32_t new_viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            ctx_wr->left_col = war_clamp_subtract_uint32(
                ctx_wr->left_col, diff, ctx_wr->min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_goto_left_bound(war_env* env) {
    call_carmack("war_roll_cursor_goto_left_bound");
    war_window_render_context* ctx_wr = env->ctx_wr;
    if (ctx_wr->numeric_prefix) {
        ctx_wr->numeric_prefix = ctx_wr->numeric_prefix * 10;
        return;
    }
    ctx_wr->cursor_pos_x = ctx_wr->left_col;
    ctx_wr->sub_col = 0;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_shift_v(war_env* env) {
    call_carmack("war_roll_shift_v");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_goto_play_bar(war_env* env) {
    call_carmack("war_roll_cursor_goto_play_bar");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_lua_context* ctx_lua = env->ctx_lua;
    uint32_t col = ((float)atomic_load(&atomics->play_clock) /
                    atomic_load(&ctx_lua->A_SAMPLE_RATE)) /
                   ((60.0f / atomic_load(&ctx_lua->A_BPM)) /
                    atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT));
    ctx_wr->cursor_pos_x =
        war_clamp_uint32(col, ctx_wr->min_col, ctx_wr->max_col);
    ctx_wr->sub_col = 0;
    uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
    uint32_t distance = viewport_width / 2;
    ctx_wr->left_col = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_x, distance, ctx_wr->min_col);
    ctx_wr->right_col =
        war_clamp_add_uint32(ctx_wr->cursor_pos_x, distance, ctx_wr->max_col);
    uint32_t new_viewport_width = war_clamp_subtract_uint32(
        ctx_wr->right_col, ctx_wr->left_col, ctx_wr->min_col);
    if (new_viewport_width < viewport_width) {
        uint32_t diff = war_clamp_subtract_uint32(
            viewport_width, new_viewport_width, ctx_wr->min_col);
        uint32_t sum =
            war_clamp_add_uint32(ctx_wr->right_col, diff, ctx_wr->max_col);
        if (sum < ctx_wr->max_col) {
            ctx_wr->right_col = sum;
        } else {
            ctx_wr->left_col = war_clamp_subtract_uint32(
                ctx_wr->left_col, diff, ctx_wr->min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void
war_roll_cursor_goto_right_bound_or_prefix_horizontal(war_env* env) {
    war_window_render_context* ctx_wr = env->ctx_wr;
    call_carmack("war_roll_cursor_goto_right_bound_or_prefix_horizontal");
    if (ctx_wr->numeric_prefix) {
        ctx_wr->cursor_pos_x = war_clamp_uint32(
            ctx_wr->numeric_prefix, ctx_wr->min_col, ctx_wr->max_col);
        ctx_wr->sub_col = 0;
        uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
        uint32_t distance = viewport_width / 2;
        ctx_wr->left_col = war_clamp_subtract_uint32(
            ctx_wr->cursor_pos_x, distance, ctx_wr->min_col);
        ctx_wr->right_col = war_clamp_add_uint32(
            ctx_wr->cursor_pos_x, distance, ctx_wr->max_col);
        uint32_t new_viewport_width = war_clamp_subtract_uint32(
            ctx_wr->right_col, ctx_wr->left_col, ctx_wr->min_col);
        if (new_viewport_width < viewport_width) {
            uint32_t diff = war_clamp_subtract_uint32(
                viewport_width, new_viewport_width, ctx_wr->min_col);
            uint32_t sum =
                war_clamp_add_uint32(ctx_wr->right_col, diff, ctx_wr->max_col);
            if (sum < ctx_wr->max_col) {
                ctx_wr->right_col = sum;
            } else {
                ctx_wr->left_col = war_clamp_subtract_uint32(
                    ctx_wr->left_col, diff, ctx_wr->min_col);
            }
        }
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_pos_x = ctx_wr->right_col;
    ctx_wr->sub_col = 0;
    ctx_wr->numeric_prefix = 0;
}

static inline void
war_roll_cursor_goto_bottom_bound_or_prefix_vertical(war_env* env) {
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    call_carmack("war_roll_cursor_goto_bottom_bound_or_prefix_vertical");
    if (ctx_wr->numeric_prefix) {
        ctx_wr->cursor_pos_y = war_clamp_uint32(
            ctx_wr->numeric_prefix, ctx_wr->min_row, ctx_wr->max_row);
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        uint32_t distance = viewport_height / 2;
        ctx_wr->bottom_row = war_clamp_subtract_uint32(
            ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
        ctx_wr->top_row = war_clamp_add_uint32(
            ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
        uint32_t new_viewport_height =
            war_clamp_subtract_uint32(ctx_wr->top_row, ctx_wr->bottom_row, 0);
        if (new_viewport_height < viewport_height) {
            uint32_t diff = war_clamp_subtract_uint32(
                viewport_height, new_viewport_height, 0);
            uint32_t sum =
                war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
            if (sum < ctx_wr->max_row) {
                ctx_wr->top_row = sum;
            } else {
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->bottom_row, diff, ctx_wr->min_row);
            }
        }
        if (ctx_wr->layer_flux) {
            war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
        }
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_pos_y = ctx_wr->bottom_row;
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void
war_roll_cursor_goto_top_bound_or_prefix_vertical(war_env* env) {
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    call_carmack("war_roll_cursor_goto_top_bound_or_prefix_vertical");
    if (ctx_wr->numeric_prefix) {
        ctx_wr->cursor_pos_y = war_clamp_uint32(
            ctx_wr->numeric_prefix, ctx_wr->min_row, ctx_wr->max_row);
        uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
        uint32_t distance = viewport_height / 2;
        ctx_wr->bottom_row = war_clamp_subtract_uint32(
            ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
        ctx_wr->top_row = war_clamp_add_uint32(
            ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
        uint32_t new_viewport_height = war_clamp_subtract_uint32(
            ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
        if (new_viewport_height < viewport_height) {
            uint32_t diff = war_clamp_subtract_uint32(
                viewport_height, new_viewport_height, ctx_wr->min_row);
            uint32_t sum =
                war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
            if (sum < ctx_wr->max_row) {
                ctx_wr->top_row = sum;
            } else {
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->bottom_row, diff, ctx_wr->min_row);
            }
        }
        if (ctx_wr->layer_flux) {
            war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
        }
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_pos_y = ctx_wr->top_row;
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_1(war_env* env) {
    call_carmack("war_roll_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 1, UINT32_MAX);
}

static inline void war_roll_2(war_env* env) {
    call_carmack("war_roll_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 2, UINT32_MAX);
}

static inline void war_roll_3(war_env* env) {
    call_carmack("war_roll_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 3, UINT32_MAX);
}

static inline void war_roll_4(war_env* env) {
    call_carmack("war_roll_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 4, UINT32_MAX);
}

static inline void war_roll_5(war_env* env) {
    call_carmack("war_roll_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 5, UINT32_MAX);
}

static inline void war_roll_6(war_env* env) {
    call_carmack("war_roll_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 6, UINT32_MAX);
}

static inline void war_roll_7(war_env* env) {
    call_carmack("war_roll_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 7, UINT32_MAX);
}

static inline void war_roll_8(war_env* env) {
    call_carmack("war_roll_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 8, UINT32_MAX);
}

static inline void war_roll_9(war_env* env) {
    call_carmack("war_roll_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix =
        war_clamp_multiply_uint32(ctx_wr->numeric_prefix, 10, UINT32_MAX);
    ctx_wr->numeric_prefix =
        war_clamp_add_uint32(ctx_wr->numeric_prefix, 9, UINT32_MAX);
}

static inline void war_roll_r(war_env* env) {
    call_carmack("war_roll_r");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_zoom_in(war_env* env) {
    call_carmack("war_roll_zoom_in");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->zoom_scale += ctx_wr->zoom_increment;
    if (ctx_wr->zoom_scale > 5.0f) { ctx_wr->zoom_scale = 5.0f; }
    float viewport_cols_f =
        ctx_wr->physical_width / (ctx_wr->cell_width * ctx_wr->zoom_scale);
    float viewport_rows_f =
        ctx_wr->physical_height / (ctx_wr->cell_height * ctx_wr->zoom_scale);
    int viewport_cols = fmax(5, (int)round(viewport_cols_f));
    int viewport_rows = fmax(5, (int)round(viewport_rows_f));
    ctx_wr->viewport_cols = viewport_cols;
    ctx_wr->viewport_rows = viewport_rows;
    ctx_wr->right_col = fmin(ctx_wr->max_col,
                             ctx_wr->left_col + viewport_cols - 1 -
                                 ctx_wr->num_cols_for_line_numbers);
    ctx_wr->top_row = fmin(ctx_wr->max_row,
                           ctx_wr->bottom_row + viewport_rows - 1 -
                               ctx_wr->num_rows_for_status_bars);
    ctx_wr->numeric_prefix = 0;
    return;
}

static inline void war_roll_zoom_out(war_env* env) {
    call_carmack("war_roll_zoom_out");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->zoom_scale -= ctx_wr->zoom_increment;
    if (ctx_wr->zoom_scale <= 0.1f) { ctx_wr->zoom_scale = 0.1f; }
    float viewport_cols_f =
        ctx_wr->physical_width / (ctx_wr->cell_width * ctx_wr->zoom_scale);
    float viewport_rows_f =
        ctx_wr->physical_height / (ctx_wr->cell_height * ctx_wr->zoom_scale);
    int viewport_cols = fmax(5, (int)round(viewport_cols_f));
    int viewport_rows = fmax(5, (int)round(viewport_rows_f));
    ctx_wr->viewport_cols = viewport_cols;
    ctx_wr->viewport_rows = viewport_rows;
    ctx_wr->right_col = fmin(ctx_wr->max_col,
                             ctx_wr->left_col + viewport_cols - 1 -
                                 ctx_wr->num_cols_for_line_numbers);
    ctx_wr->top_row = fmin(ctx_wr->max_row,
                           ctx_wr->bottom_row + viewport_rows - 1 -
                               ctx_wr->num_rows_for_status_bars);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_zoom_in_leap(war_env* env) {
    call_carmack("war_roll_zoom_in_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    // ctx_wr.zoom_scale +=
    // ctx_wr.zoom_leap_increment; if
    // (ctx_wr.zoom_scale > 5.0f) {
    // ctx_wr.zoom_scale = 5.0f; }
    // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
    // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
    // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
    // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_zoom_out_leap(war_env* env) {
    call_carmack("war_roll_zoom_out_leap");
    war_window_render_context* ctx_wr = env->ctx_wr;
    // ctx_wr.zoom_scale -=
    // ctx_wr.zoom_leap_increment; if
    // (ctx_wr.zoom_scale < 0.1f) {
    // ctx_wr.zoom_scale = 0.1f; }
    // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
    // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
    // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
    // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_zoom_reset(war_env* env) {
    call_carmack("war_roll_zoom_reset");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_vulkan_context* ctx_vk = env->ctx_vk;
    ctx_wr->zoom_scale = 1.0f;
    ctx_wr->viewport_cols =
        (uint32_t)(ctx_wr->physical_width / ctx_vk->cell_width);
    ctx_wr->viewport_rows =
        (uint32_t)(ctx_wr->physical_height / ctx_vk->cell_height);
    ctx_wr->right_col = fmin(ctx_wr->max_col,
                             ctx_wr->left_col + ctx_wr->default_viewport_cols -
                                 1 - ctx_wr->num_cols_for_line_numbers);
    ctx_wr->top_row = fmin(ctx_wr->max_row,
                           ctx_wr->bottom_row + ctx_wr->default_viewport_rows -
                               1 - ctx_wr->num_rows_for_status_bars);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_esc(war_env* env) {
    call_carmack("war_roll_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    // if (timeout_state_index) {
    //     if (fsm[timeout_state_index].command[ctx_wr.mode]) {
    //         goto* fsm[timeout_state_index].command[ctx_wr.mode];
    //         timeout_state_index = 0;
    //     }
    //     return;
    // }
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
    return;
}

static inline void war_roll_shift_s(war_env* env) {
    call_carmack("war_roll_shift_s");
}

static inline void war_roll_reset_cursor(war_env* env) {
    call_carmack("war_roll_reset_cursor");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->cursor_width_sub_cells = 1;
    ctx_wr->cursor_width_whole_number = 1;
    ctx_wr->cursor_width_sub_col = 1;
    ctx_wr->navigation_whole_number_col = 1;
    ctx_wr->navigation_sub_cells_col = 1;
    ctx_wr->cursor_size_x = 1.0;
    ctx_wr->cursor_navigation_x = 1.0;
    uint32_t whole_pos_x = llround(ctx_wr->cursor_pos_x);
    ctx_wr->cursor_pos_x = whole_pos_x;
    ctx_wr->numeric_prefix = 0;
    return;
}

static inline void war_roll_cursor_fat(war_env* env) {
    call_carmack("war_roll_cursor_fat");
    war_window_render_context* ctx_wr = env->ctx_wr;
    if (ctx_wr->numeric_prefix) {
        ctx_wr->cursor_width_whole_number = ctx_wr->numeric_prefix;
        ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                                ctx_wr->cursor_width_sub_cells;
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_width_whole_number = 1;
    ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                            ctx_wr->cursor_width_sub_cells;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_thin(war_env* env) {
    call_carmack("war_roll_cursor_thin");
    war_window_render_context* ctx_wr = env->ctx_wr;
    if (ctx_wr->numeric_prefix) {
        ctx_wr->cursor_width_sub_cells = ctx_wr->numeric_prefix;
        ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                                ctx_wr->cursor_width_sub_cells;
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_width_sub_cells = 1;
    ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                            ctx_wr->cursor_width_sub_cells;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_move_thin(war_env* env) {
    call_carmack("war_roll_cursor_move_thin");
    war_window_render_context* ctx_wr = env->ctx_wr;
    if (ctx_wr->numeric_prefix) {
        ctx_wr->navigation_sub_cells_col = ctx_wr->numeric_prefix;
        ctx_wr->cursor_navigation_x =
            (double)ctx_wr->navigation_whole_number_col /
            ctx_wr->navigation_sub_cells_col;
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->navigation_sub_cells_col = 1;
    ctx_wr->cursor_navigation_x = (double)ctx_wr->navigation_whole_number_col /
                                  ctx_wr->navigation_sub_cells_col;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_move_fat(war_env* env) {
    call_carmack("war_roll_cursor_move_fat");
    war_window_render_context* ctx_wr = env->ctx_wr;
    if (ctx_wr->numeric_prefix) {
        ctx_wr->navigation_whole_number_col = ctx_wr->numeric_prefix;
        ctx_wr->cursor_navigation_x =
            (double)ctx_wr->navigation_whole_number_col /
            ctx_wr->navigation_sub_cells_col;
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->navigation_whole_number_col = 1;
    ctx_wr->cursor_navigation_x = (double)ctx_wr->navigation_whole_number_col /
                                  ctx_wr->navigation_sub_cells_col;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_f(war_env* env) {
    call_carmack("war_roll_alt_f");
}

static inline void war_roll_cursor_goto_bottom(war_env* env) {
    call_carmack("war_window_render_context");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->cursor_pos_y = ctx_wr->min_row;
    uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
    uint32_t distance = viewport_height / 2;
    ctx_wr->bottom_row = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
    ctx_wr->top_row =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
    uint32_t new_viewport_height = war_clamp_subtract_uint32(
        ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
    if (new_viewport_height < viewport_height) {
        uint32_t diff = war_clamp_subtract_uint32(
            viewport_height, new_viewport_height, ctx_wr->min_row);
        uint32_t sum =
            war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
        if (sum < ctx_wr->max_row) {
            ctx_wr->top_row = sum;
        } else {
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_goto_top(war_env* env) {
    call_carmack("war_roll_cursor_goto_top");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->cursor_pos_y = ctx_wr->max_row;
    uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
    uint32_t distance = viewport_height / 2;
    ctx_wr->bottom_row = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
    ctx_wr->top_row =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
    uint32_t new_viewport_height = war_clamp_subtract_uint32(
        ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
    if (new_viewport_height < viewport_height) {
        uint32_t diff = war_clamp_subtract_uint32(
            viewport_height, new_viewport_height, ctx_wr->min_row);
        uint32_t sum =
            war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
        if (sum < ctx_wr->max_row) {
            ctx_wr->top_row = sum;
        } else {
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_goto_middle(war_env* env) {
    call_carmack("war_window_render_context");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->cursor_pos_y = 60;
    uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
    uint32_t distance = viewport_height / 2;
    ctx_wr->bottom_row = war_clamp_subtract_uint32(
        ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
    ctx_wr->top_row =
        war_clamp_add_uint32(ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
    uint32_t new_viewport_height = war_clamp_subtract_uint32(
        ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
    if (new_viewport_height < viewport_height) {
        uint32_t diff = war_clamp_subtract_uint32(
            viewport_height, new_viewport_height, ctx_wr->min_row);
        uint32_t sum =
            war_clamp_add_uint32(ctx_wr->top_row, diff, ctx_wr->max_row);
        if (sum < ctx_wr->max_row) {
            ctx_wr->top_row = sum;
        } else {
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->bottom_row, diff, ctx_wr->min_row);
        }
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_draw(war_env* env) {
    call_carmack("war_roll_note_draw");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_lua_context* ctx_lua = env->ctx_lua;
    war_undo_tree* undo_tree = env->undo_tree;
    war_note_quads* note_quads = env->note_quads;
    war_pool* pool_wr = env->pool_wr;
    uint64_t id = atomic_fetch_add(&atomics->note_next_id, 1);
    war_note_quad note_quad = {
        .alive = 1,
        .id = id,
        .pos_x = ctx_wr->cursor_pos_x,
        .pos_y = ctx_wr->cursor_pos_y,
        .layer = atomic_load(&atomics->layer),
        .size_x = ctx_wr->cursor_size_x,
        .size_x_numerator = ctx_wr->cursor_width_whole_number,
        .size_x_denominator = ctx_wr->cursor_width_sub_cells,
        .navigation_x = ctx_wr->cursor_navigation_x,
        .navigation_x_numerator = ctx_wr->navigation_whole_number_col,
        .navigation_x_denominator = ctx_wr->navigation_sub_cells_col,
        .color = ctx_wr->color_cursor,
        .outline_color = ctx_wr->color_note_outline_default,
        .gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN),
        .voice = 0,
    };
    double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
    double bpm = atomic_load(&ctx_lua->A_BPM);
    double frames_per_beat = sample_rate * 60.0 / bpm;
    double columns_per_beat = atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
    war_note note;
    double start_beats = note_quad.pos_x / columns_per_beat;
    note.note_start_frames = (uint64_t)(start_beats * frames_per_beat + 0.5);
    double duration_beats = note_quad.size_x / columns_per_beat;
    note.note_duration_frames =
        (uint64_t)(duration_beats * frames_per_beat + 0.5);
    note.note = note_quad.pos_y;
    note.layer = note_quad.layer;
    note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
    note.note_sustain = atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
    note.note_release = atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
    note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
    note.note_phase_increment = 0;
    note.alive = note_quad.alive;
    note.id = note_quad.id;
    uint32_t note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
    uint32_t undo_notes_batch_max =
        atomic_load(&ctx_lua->WR_UNDO_NOTES_BATCH_MAX);
    if (ctx_wr->numeric_prefix) {
        // add notes
        if (ctx_wr->numeric_prefix >= undo_notes_batch_max) {
            // swapfile logic (memmove probably)
            ctx_wr->numeric_prefix = 0;
        }
        // --- Compaction if needed ---
        if (note_quads->count + ctx_wr->numeric_prefix >= note_quads_max) {
            uint32_t write_idx = 0;
            for (uint32_t read_idx = 0; read_idx < note_quads->count;
                 read_idx++) {
                if (note_quads->alive[read_idx]) {
                    if (write_idx != read_idx) {
                        // Copy all fields
                        note_quads->pos_x[write_idx] =
                            note_quads->pos_x[read_idx];
                        note_quads->pos_y[write_idx] =
                            note_quads->pos_y[read_idx];
                        note_quads->layer[write_idx] =
                            note_quads->layer[read_idx];
                        note_quads->size_x[write_idx] =
                            note_quads->size_x[read_idx];
                        note_quads->size_x_numerator[write_idx] =
                            note_quads->size_x_numerator[read_idx];
                        note_quads->size_x_denominator[write_idx] =
                            note_quads->size_x_denominator[read_idx];
                        note_quads->navigation_x[write_idx] =
                            note_quads->navigation_x[read_idx];
                        note_quads->navigation_x_numerator[write_idx] =
                            note_quads->navigation_x_numerator[read_idx];
                        note_quads->navigation_x_denominator[write_idx] =
                            note_quads->navigation_x_denominator[read_idx];
                        note_quads->color[write_idx] =
                            note_quads->color[read_idx];
                        note_quads->outline_color[write_idx] =
                            note_quads->outline_color[read_idx];
                        note_quads->gain[write_idx] =
                            note_quads->gain[read_idx];
                        note_quads->voice[write_idx] =
                            note_quads->voice[read_idx];
                        note_quads->alive[write_idx] =
                            note_quads->alive[read_idx];
                        note_quads->id[write_idx] = note_quads->id[read_idx];
                    }
                    write_idx++;
                }
            }
            if (write_idx >= note_quads_max) {
                call_carmack("TODO: implement spillover swapfile");
                ctx_wr->numeric_prefix = 0;
                return;
            };
            note_quads->count = write_idx;
        }
        war_undo_node* node = war_pool_alloc(pool_wr, sizeof(war_undo_node));
        node->id = undo_tree->next_id++;
        node->seq_num = undo_tree->next_seq_num++;
        node->command = CMD_ADD_NOTES_SAME;
        node->payload.delete_notes_same.note = note;
        node->payload.delete_notes_same.note_quad = note_quad;
        node->payload.delete_notes_same.ids =
            war_pool_alloc(pool_wr, sizeof(uint64_t) * ctx_wr->numeric_prefix);
        node->payload.delete_notes_same.count = ctx_wr->numeric_prefix;
        node->cursor_pos_x = ctx_wr->cursor_pos_x;
        node->cursor_pos_y = ctx_wr->cursor_pos_y;
        node->left_col = ctx_wr->left_col;
        node->right_col = ctx_wr->right_col;
        node->top_row = ctx_wr->top_row;
        node->bottom_row = ctx_wr->bottom_row;
        node->parent = NULL;
        node->next = NULL;
        node->prev = NULL;
        node->alt_next = NULL;
        node->alt_prev = NULL;
        if (!undo_tree->root) {
            node->branch_id = undo_tree->next_branch_id++;
            undo_tree->root = node;
            undo_tree->current = node;
        } else if (!undo_tree->current) {
            node->branch_id = undo_tree->next_branch_id++;
            node->parent = NULL;
            undo_tree->root = node;
            undo_tree->current = node;
        } else {
            war_undo_node* cur = undo_tree->current;
            if (cur->next) { cur->next = NULL; }
            node->parent = cur;
            node->branch_id = cur->branch_id;
            cur->next = node;
            node->prev = cur;
            undo_tree->current = node;
        }
        // batch add
        for (uint32_t i = 0; i < ctx_wr->numeric_prefix; i++) {
            note_quads->pos_x[note_quads->count + i] = note_quad.pos_x;
            note_quads->pos_y[note_quads->count + i] = note_quad.pos_y;
            note_quads->layer[note_quads->count + i] = note_quad.layer;
            note_quads->size_x[note_quads->count + i] = note_quad.size_x;
            note_quads->size_x_numerator[note_quads->count + i] =
                note_quad.size_x_numerator;
            note_quads->size_x_denominator[note_quads->count + i] =
                note_quad.size_x_denominator;
            note_quads->navigation_x[note_quads->count + i] =
                note_quad.navigation_x;
            note_quads->navigation_x_numerator[note_quads->count + i] =
                note_quad.navigation_x_numerator;
            note_quads->navigation_x_denominator[note_quads->count + i] =
                note_quad.navigation_x_denominator;
            note_quads->color[note_quads->count + i] = note_quad.color;
            note_quads->outline_color[note_quads->count + i] =
                note_quad.outline_color;
            note_quads->gain[note_quads->count + i] = note_quad.gain;
            note_quads->voice[note_quads->count + i] = note_quad.voice;
            note_quads->alive[note_quads->count + i] = note_quad.alive;
            note_quads->id[note_quads->count + i] = id;
            node->payload.delete_notes_same.ids[i] = id;
            id = atomic_fetch_add(&atomics->note_next_id, 1);
        }
        note_quads->count += ctx_wr->numeric_prefix;
        // note, count, ids
        ctx_wr->numeric_prefix = 0;
        return;
    }
    //-------------------------------------------------------------
    // ADD SINGLE NOTE
    //-------------------------------------------------------------
    // --- Compaction if needed ---
    if (note_quads->count + 1 >= note_quads_max) {
        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < note_quads->count; read_idx++) {
            if (note_quads->alive[read_idx]) {
                if (write_idx != read_idx) {
                    // Copy all fields
                    note_quads->pos_x[write_idx] = note_quads->pos_x[read_idx];
                    note_quads->pos_y[write_idx] = note_quads->pos_y[read_idx];
                    note_quads->layer[write_idx] = note_quads->layer[read_idx];
                    note_quads->size_x[write_idx] =
                        note_quads->size_x[read_idx];
                    note_quads->size_x_numerator[write_idx] =
                        note_quads->size_x_numerator[read_idx];
                    note_quads->size_x_denominator[write_idx] =
                        note_quads->size_x_denominator[read_idx];
                    note_quads->navigation_x[write_idx] =
                        note_quads->navigation_x[read_idx];
                    note_quads->navigation_x_numerator[write_idx] =
                        note_quads->navigation_x_numerator[read_idx];
                    note_quads->navigation_x_denominator[write_idx] =
                        note_quads->navigation_x_denominator[read_idx];
                    note_quads->color[write_idx] = note_quads->color[read_idx];
                    note_quads->outline_color[write_idx] =
                        note_quads->outline_color[read_idx];
                    note_quads->gain[write_idx] = note_quads->gain[read_idx];
                    note_quads->voice[write_idx] = note_quads->voice[read_idx];
                    note_quads->alive[write_idx] = note_quads->alive[read_idx];
                    note_quads->id[write_idx] = note_quads->id[read_idx];
                }
                write_idx++;
            }
        }
        assert(write_idx < note_quads_max);
        note_quads->count = write_idx;
    }
    note_quads->pos_x[note_quads->count] = note_quad.pos_x;
    note_quads->pos_y[note_quads->count] = note_quad.pos_y;
    note_quads->layer[note_quads->count] = note_quad.layer;
    note_quads->size_x[note_quads->count] = note_quad.size_x;
    note_quads->size_x_numerator[note_quads->count] =
        note_quad.size_x_numerator;
    note_quads->size_x_denominator[note_quads->count] =
        note_quad.size_x_denominator;
    note_quads->navigation_x[note_quads->count] = note_quad.navigation_x;
    note_quads->navigation_x_numerator[note_quads->count] =
        note_quad.navigation_x_numerator;
    note_quads->navigation_x_denominator[note_quads->count] =
        note_quad.navigation_x_denominator;
    note_quads->color[note_quads->count] = note_quad.color;
    note_quads->outline_color[note_quads->count] = note_quad.outline_color;
    note_quads->gain[note_quads->count] = note_quad.gain;
    note_quads->voice[note_quads->count] = note_quad.voice;
    note_quads->alive[note_quads->count] = note_quad.alive;
    note_quads->id[note_quads->count] = note_quad.id;
    note_quads->count++;
    war_undo_node* node = war_pool_alloc(pool_wr, sizeof(war_undo_node));
    node->id = undo_tree->next_id++;
    node->seq_num = undo_tree->next_seq_num++;
    node->command = CMD_ADD_NOTE;
    node->payload.delete_note.note = note;
    node->payload.delete_note.note_quad = note_quad;
    node->cursor_pos_x = ctx_wr->cursor_pos_x;
    node->cursor_pos_y = ctx_wr->cursor_pos_y;
    node->left_col = ctx_wr->left_col;
    node->right_col = ctx_wr->right_col;
    node->top_row = ctx_wr->top_row;
    node->bottom_row = ctx_wr->bottom_row;
    node->parent = NULL;
    node->next = NULL;
    node->prev = NULL;
    node->alt_next = NULL;
    node->alt_prev = NULL;
    if (!undo_tree->root) {
        node->branch_id = undo_tree->next_branch_id++;
        undo_tree->root = node;
        undo_tree->current = node;
    } else if (!undo_tree->current) {
        node->branch_id = undo_tree->next_branch_id++;
        node->parent = NULL;
        undo_tree->root = node;
        undo_tree->current = node;
    } else {
        war_undo_node* cur = undo_tree->current;
        if (cur->next) { cur->next = NULL; }
        node->parent = cur;
        node->branch_id = cur->branch_id;
        cur->next = node;
        node->prev = cur;
        undo_tree->current = node;
    }
    ctx_wr->numeric_prefix = 0;
    return;
}

static inline void war_roll_x(war_env* env) {
    call_carmack("war_roll_x");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_shift_x(war_env* env) {
    call_carmack("war_roll_shift_x");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_delete(war_env* env) {
    // TODO: spillover swapfile
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_lua_context* ctx_lua = env->ctx_lua;
    war_undo_tree* undo_tree = env->undo_tree;
    war_note_quads* note_quads = env->note_quads;
    war_pool* pool_wr = env->pool_wr;
    call_carmack("war_roll_note_delete");
    if (note_quads->count == 0) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    uint64_t layer = atomic_load(&atomics->layer);
    if (ctx_wr->numeric_prefix) {
        // TODO: spillover swapfile
        // TODO batch delete
        uint32_t delete_count = 0;
        uint32_t undo_notes_batch_max =
            atomic_load(&ctx_lua->WR_UNDO_NOTES_BATCH_MAX);
        war_undo_node* node;
        for (int32_t i = note_quads->count - 1; i >= 0; i--) {
            if (note_quads->alive[i] == 0 || note_quads->hidden[i] ||
                note_quads->layer[i] != layer) {
                continue;
            }
            double cursor_pos_x = ctx_wr->cursor_pos_x;
            double cursor_pos_y = ctx_wr->cursor_pos_y;
            double cursor_end_x = cursor_pos_x + ctx_wr->cursor_size_x;
            double note_pos_x = note_quads->pos_x[i];
            double note_pos_y = note_quads->pos_y[i];
            double note_end_x = note_pos_x + note_quads->size_x[i];
            if (cursor_pos_y != note_pos_y || cursor_pos_x >= note_end_x ||
                cursor_end_x <= note_pos_x) {
                continue;
            }
            if (delete_count == 0) {
                node = war_pool_alloc(pool_wr, sizeof(war_undo_node));
                node->id = undo_tree->next_id++;
                node->seq_num = undo_tree->next_seq_num++;
                node->command = CMD_DELETE_NOTES;
                node->payload.add_notes.note = war_pool_alloc(
                    pool_wr, sizeof(war_note) * undo_notes_batch_max);
                node->payload.add_notes.note_quad = war_pool_alloc(
                    pool_wr, sizeof(war_note_quad) * undo_notes_batch_max);
                node->cursor_pos_x = ctx_wr->cursor_pos_x;
                node->cursor_pos_y = ctx_wr->cursor_pos_y;
                node->left_col = ctx_wr->left_col;
                node->right_col = ctx_wr->right_col;
                node->top_row = ctx_wr->top_row;
                node->bottom_row = ctx_wr->bottom_row;
                node->parent = NULL;
                node->next = NULL;
                node->prev = NULL;
                node->alt_next = NULL;
                node->alt_prev = NULL;
                if (!undo_tree->root) {
                    node->branch_id = undo_tree->next_branch_id++;
                    undo_tree->root = node;
                    undo_tree->current = node;
                } else if (!undo_tree->current) {
                    node->branch_id = undo_tree->next_branch_id++;
                    node->parent = NULL;
                    undo_tree->root = node;
                    undo_tree->current = node;
                } else {
                    war_undo_node* cur = undo_tree->current;
                    if (cur->next) { cur->next = NULL; }
                    node->parent = cur;
                    node->branch_id = cur->branch_id;
                    cur->next = node;
                    node->prev = cur;
                    undo_tree->current = node;
                }
            }
            war_note_quad note_quad;
            note_quad.alive = note_quads->alive[i];
            note_quad.id = note_quads->id[i];
            note_quad.pos_x = note_quads->pos_x[i];
            note_quad.pos_y = note_quads->pos_y[i];
            note_quad.layer = note_quads->layer[i];
            note_quad.size_x = note_quads->size_x[i];
            note_quad.navigation_x = note_quads->navigation_x[i];
            note_quad.navigation_x_numerator =
                note_quads->navigation_x_numerator[i];
            note_quad.navigation_x_denominator =
                note_quads->navigation_x_denominator[i];
            note_quad.size_x_numerator = note_quads->size_x_numerator[i];
            note_quad.size_x_denominator = note_quads->size_x_denominator[i];
            note_quad.color = note_quads->color[i];
            note_quad.outline_color = note_quads->outline_color[i];
            note_quad.gain = note_quads->gain[i];
            note_quad.voice = note_quads->voice[i];
            note_quad.hidden = note_quads->hidden[i];
            note_quad.mute = note_quads->mute[i];
            double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
            double bpm = atomic_load(&ctx_lua->A_BPM);
            double frames_per_beat = sample_rate * 60.0 / bpm;
            double columns_per_beat =
                atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
            war_note note;
            double start_beats = note_quad.pos_x / columns_per_beat;
            note.note_start_frames =
                (uint64_t)(start_beats * frames_per_beat + 0.5);
            double duration_beats = note_quad.size_x / columns_per_beat;
            note.note_duration_frames =
                (uint64_t)(duration_beats * frames_per_beat + 0.5);
            note.note = note_quad.pos_y;
            note.layer = note_quad.layer;
            note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
            note.note_sustain = atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
            note.note_release = atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
            note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
            note.note_phase_increment = 0;
            note.id = note_quad.id;
            note.alive = note_quad.alive;
            node->payload.add_notes.note[delete_count] = note;
            node->payload.add_notes.note_quad[delete_count] = note_quad;
            note_quads->alive[i] = 0;
            delete_count++;
            if (delete_count >= undo_notes_batch_max) {
                // spillover
                ctx_wr->numeric_prefix = 0;
                return;
            }
            if (delete_count >= ctx_wr->numeric_prefix) { break; }
        }
        if (delete_count == 0) {
            ctx_wr->numeric_prefix = 0;
            return;
        }
        ctx_wr->numeric_prefix = 0;
        return;
    }
    int32_t delete_idx = -1; // overflow impossible
    for (uint32_t i = 0; i < note_quads->count; i++) {
        if (note_quads->alive[i] == 0 || note_quads->hidden[i] ||
            note_quads->layer[i] != layer) {
            continue;
        }
        double cursor_pos_x = ctx_wr->cursor_pos_x;
        double cursor_pos_y = ctx_wr->cursor_pos_y;
        double cursor_end_x = cursor_pos_x + ctx_wr->cursor_size_x;
        double note_pos_x = note_quads->pos_x[i];
        double note_pos_y = note_quads->pos_y[i];
        double note_end_x = note_pos_x + note_quads->size_x[i];
        if (cursor_pos_y != note_pos_y || cursor_pos_x >= note_end_x ||
            cursor_end_x <= note_pos_x) {
            continue;
        }
        delete_idx = i;
    }
    if (delete_idx == -1) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    war_note_quad note_quad;
    note_quad.alive = note_quads->alive[delete_idx];
    note_quad.id = note_quads->id[delete_idx];
    note_quad.pos_x = note_quads->pos_x[delete_idx];
    note_quad.pos_y = note_quads->pos_y[delete_idx];
    note_quad.layer = note_quads->layer[delete_idx];
    note_quad.size_x = note_quads->size_x[delete_idx];
    note_quad.navigation_x = note_quads->navigation_x[delete_idx];
    note_quad.navigation_x_numerator =
        note_quads->navigation_x_numerator[delete_idx];
    note_quad.navigation_x_denominator =
        note_quads->navigation_x_denominator[delete_idx];
    note_quad.size_x_numerator = note_quads->size_x_numerator[delete_idx];
    note_quad.size_x_denominator = note_quads->size_x_denominator[delete_idx];
    note_quad.color = note_quads->color[delete_idx];
    note_quad.outline_color = note_quads->outline_color[delete_idx];
    note_quad.gain = note_quads->gain[delete_idx];
    note_quad.voice = note_quads->voice[delete_idx];
    note_quad.hidden = note_quads->hidden[delete_idx];
    note_quad.mute = note_quads->mute[delete_idx];
    double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
    double bpm = atomic_load(&ctx_lua->A_BPM);
    double frames_per_beat = sample_rate * 60.0 / bpm;
    double columns_per_beat = atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
    war_note note;
    double start_beats = note_quad.pos_x / columns_per_beat;
    note.note_start_frames = (uint64_t)(start_beats * frames_per_beat + 0.5);
    double duration_beats = note_quad.size_x / columns_per_beat;
    note.note_duration_frames =
        (uint64_t)(duration_beats * frames_per_beat + 0.5);
    note.note = note_quad.pos_y;
    note.layer = note_quad.layer;
    note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
    note.note_sustain = atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
    note.note_release = atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
    note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
    note.note_phase_increment = 0;
    note.id = note_quad.id;
    note.alive = note_quad.alive;
    note_quads->alive[delete_idx] = 0;
    war_undo_node* node = war_pool_alloc(pool_wr, sizeof(war_undo_node));
    node->id = undo_tree->next_id++;
    node->seq_num = undo_tree->next_seq_num++;
    node->command = CMD_DELETE_NOTE;
    node->payload.add_note.note = note;
    node->payload.add_note.note_quad = note_quad;
    node->cursor_pos_x = ctx_wr->cursor_pos_x;
    node->cursor_pos_y = ctx_wr->cursor_pos_y;
    node->left_col = ctx_wr->left_col;
    node->right_col = ctx_wr->right_col;
    node->top_row = ctx_wr->top_row;
    node->bottom_row = ctx_wr->bottom_row;
    node->parent = NULL;
    node->next = NULL;
    node->prev = NULL;
    node->alt_next = NULL;
    node->alt_prev = NULL;
    if (!undo_tree->root) {
        node->branch_id = undo_tree->next_branch_id++;
        undo_tree->root = node;
        undo_tree->current = node;
    } else if (!undo_tree->current) {
        node->branch_id = undo_tree->next_branch_id++;
        node->parent = NULL;
        undo_tree->root = node;
        undo_tree->current = node;
    } else {
        war_undo_node* cur = undo_tree->current;
        if (cur->next) { cur->next = NULL; }
        node->parent = cur;
        node->branch_id = cur->branch_id;
        cur->next = node;
        node->prev = cur;
        undo_tree->current = node;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_delete_in_view(war_env* env) {
    call_carmack("war_roll_note_delete_in_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_delete_outside_view(war_env* env) {
    call_carmack("war_roll_note_delete_outside_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_delete_in_word(war_env* env) {
    call_carmack("war_roll_note_delete_in_word");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_delete_all(war_env* env) {
    call_carmack("war_roll_note_delete_all");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_note_quads* note_quads = env->note_quads;
    note_quads->count = 0;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_hide_outside_view(war_env* env) {
    call_carmack("war_roll_note_hide_outside_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_hide_in_view(war_env* env) {
    call_carmack("war_roll_note_hide_in_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_hide_in_word(war_env* env) {
    call_carmack("war_roll_note_hide_in_word");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_hide_all(war_env* env) {
    call_carmack("war_roll_note_hide_all");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_show_outside_view(war_env* env) {
    call_carmack("war_roll_note_show_outside_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_show_in_view(war_env* env) {
    call_carmack("war_roll_note_show_in_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_show_in_word(war_env* env) {
    call_carmack("war_roll_note_show_in_word");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_show_all(war_env* env) {
    call_carmack("war_roll_note_show_all");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_mute(war_env* env) {
    call_carmack("war_roll_note_mute");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_mute_outside_view(war_env* env) {
    call_carmack("war_roll_note_mute_outside_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_mute_in_view(war_env* env) {
    call_carmack("war_roll_note_mute_in_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_mute_all(war_env* env) {
    call_carmack("war_roll_note_mute_all");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_midi_mode(war_env* env) {
    call_carmack("war_roll_midi_mode");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_MIDI;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_unmute_outside_view(war_env* env) {
    call_carmack("war_roll_note_unmute_outside_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_unmute_in_view(war_env* env) {
    call_carmack("war_roll_note_unmute_in_view");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_unmute_in_word(war_env* env) {
    call_carmack("war_roll_note_unmute_in_word");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_note_unmute_all(war_env* env) {
    call_carmack("war_roll_note_unmute_all");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_save(war_env* env) {
    call_carmack("war_roll_views_save");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_lua_context* ctx_lua = env->ctx_lua;
    war_views* views = env->views;
    if (views->views_count < (uint32_t)atomic_load(&ctx_lua->WR_VIEWS_SAVED)) {
        views->col[views->views_count] = ctx_wr->cursor_pos_x;
        views->row[views->views_count] = ctx_wr->cursor_pos_y;
        views->left_col[views->views_count] = ctx_wr->left_col;
        views->bottom_row[views->views_count] = ctx_wr->bottom_row;
        views->right_col[views->views_count] = ctx_wr->right_col;
        views->top_row[views->views_count] = ctx_wr->top_row;
        views->views_count++;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacedspacea(war_env* env) {
    call_carmack("war_roll_spacedspacea");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_1(war_env* env) {
    call_carmack("war_roll_views_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 0) {
        uint32_t i_views = 0;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_2(war_env* env) {
    call_carmack("war_roll_views_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 1) {
        uint32_t i_views = 1;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_3(war_env* env) {
    call_carmack("war_roll_views_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 2) {
        uint32_t i_views = 2;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_4(war_env* env) {
    call_carmack("war_roll_views_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 3) {
        uint32_t i_views = 3;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_5(war_env* env) {
    call_carmack("war_roll_views_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 4) {
        uint32_t i_views = 4;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_6(war_env* env) {
    call_carmack("war_roll_views_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 5) {
        uint32_t i_views = 5;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_7(war_env* env) {
    call_carmack("war_roll_views_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 6) {
        uint32_t i_views = 6;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_8(war_env* env) {
    call_carmack("war_roll_views_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    if (views->views_count > 7) {
        uint32_t i_views = 7;
        ctx_wr->cursor_pos_x = views->col[i_views];
        ctx_wr->cursor_pos_y = views->row[i_views];
        ctx_wr->left_col = views->left_col[i_views];
        ctx_wr->bottom_row = views->bottom_row[i_views];
        ctx_wr->right_col = views->right_col[i_views];
        ctx_wr->top_row = views->top_row[i_views];
    }
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_views_mode(war_env* env) {
    call_carmack("war_roll_views_mode");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = (ctx_wr->mode != MODE_VIEWS) ? MODE_VIEWS : MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_play(war_env* env) {
    call_carmack("war_roll_play");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_play_context* ctx_play = env->ctx_play;
    ctx_play->play = !ctx_play->play;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_play_left_bound(war_env* env) {
    call_carmack("war_roll_play_left_bound");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_play_cursor(war_env* env) {
    call_carmack("war_roll_play_cursor");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_play_prefix(war_env* env) {
    call_carmack("war_roll_play_prefix");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_esc(war_env* env) {
    call_carmack("war_roll_alt_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_play_beginning(war_env* env) {
    call_carmack("war_roll_play_beginning");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space1(war_env* env) {
    call_carmack("war_roll_space1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space2(war_env* env) {
    call_carmack("war_roll_space2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space3(war_env* env) {
    call_carmack("war_roll_space3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space4(war_env* env) {
    call_carmack("war_roll_space4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space5(war_env* env) {
    call_carmack("war_roll_space5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space6(war_env* env) {
    call_carmack("war_roll_space6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space7(war_env* env) {
    call_carmack("war_roll_space7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space8(war_env* env) {
    call_carmack("war_roll_space8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space9(war_env* env) {
    call_carmack("war_roll_space9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space0(war_env* env) {
    call_carmack("war_roll_space0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_1(war_env* env) {
    call_carmack("war_roll_layer_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 0;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_2(war_env* env) {
    call_carmack("war_roll_layer_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 1;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_3(war_env* env) {
    call_carmack("war_roll_layer_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 2;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_4(war_env* env) {
    call_carmack("war_roll_layer_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 3;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_5(war_env* env) {
    call_carmack("war_roll_layer_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 4;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_6(war_env* env) {
    call_carmack("war_roll_layer_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 5;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_7(war_env* env) {
    call_carmack("war_roll_layer_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 6;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_8(war_env* env) {
    call_carmack("war_roll_layer_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 7;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_9(war_env* env) {
    call_carmack("war_roll_layer_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 8;
    ctx_wr->color_cursor = ctx_color->colors[idx];
    ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_1(war_env* env) {
    call_carmack("war_roll_layer_toggle_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 0;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_2(war_env* env) {
    call_carmack("war_roll_layer_toggle_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 1;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_3(war_env* env) {
    call_carmack("war_roll_layer_toggle_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 2;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_4(war_env* env) {
    call_carmack("war_roll_layer_toggle_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 3;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_5(war_env* env) {
    call_carmack("war_roll_layer_toggle_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 4;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_6(war_env* env) {
    call_carmack("war_roll_layer_toggle_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 5;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_7(war_env* env) {
    call_carmack("war_roll_layer_toggle_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 6;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_8(war_env* env) {
    call_carmack("war_roll_layer_toggle_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 7;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_layer_toggle_9(war_env* env) {
    call_carmack("war_roll_layer_toggle_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    int idx = 8;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_shift_0(war_env* env) {
    call_carmack("war_roll_alt_shift_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_next_note(war_env* env) {
    call_carmack("war_roll_cursor_next_note");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_next_note_end(war_env* env) {
    call_carmack("war_roll_cursor_next_note_end");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_cursor_current_note_end(war_env* env) {
    call_carmack("war_roll_cursor_current_note_end");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacediv(war_env* env) {
    call_carmack("war_roll_spacediv");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacedov(war_env* env) {
    call_carmack("war_roll_spacedov");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacediw(war_env* env) {
    call_carmack("war_roll_spacediw");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceda(war_env* env) {
    call_carmack("war_roll_spaceda");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_note_quads* note_quads = env->note_quads;
    note_quads->count = 0;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacehov(war_env* env) {
    call_carmack("war_roll_spacehov");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacehiv(war_env* env) {
    call_carmack("war_roll_spacehiv");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacehiw(war_env* env) {
    call_carmack("war_roll_spacehiw");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceha(war_env* env) {
    call_carmack("war_roll_spaceha");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacesov(war_env* env) {
    call_carmack("war_roll_spacesov");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacesiv(war_env* env) {
    call_carmack("war_roll_spacesiv");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacesiw(war_env* env) {
    call_carmack("war_roll_spacesiw");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacesa(war_env* env) {
    call_carmack("war_roll_spacesa");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacem(war_env* env) {
    call_carmack("war_roll_spacem");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacemov(war_env* env) {
    call_carmack("war_roll_spacemov");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacemiv(war_env* env) {
    call_carmack("war_roll_spacemiv");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spacema(war_env* env) {
    call_carmack("war_roll_spacema");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceumov(war_env* env) {
    call_carmack("war_roll_spaceumov");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceumiv(war_env* env) {
    call_carmack("war_roll_spaceumiv");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceumiw(war_env* env) {
    call_carmack("war_roll_spaceumiw");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_spaceuma(war_env* env) {
    call_carmack("war_roll_spaceuma");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_a(war_env* env) {
    call_carmack("war_roll_alt_a");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_A(war_env* env) {
    call_carmack("war_roll_alt_A");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_A(war_env* env) {
    call_carmack("war_roll_A");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_ctrl_a(war_env* env) {
    call_carmack("war_roll_ctrl_a");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_0(war_env* env) {
    call_carmack("war_roll_alt_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_lua_context* ctx_lua = env->ctx_lua;
    int layer_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
    ctx_wr->color_cursor = ctx_color->full_white_hex;
    ctx_wr->color_cursor_transparent = ctx_color->white_hex;
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << layer_count) - 1);
    ctx_wr->layers_active_count = 9;
    for (int i = 0; i < ctx_wr->layers_active_count; i++) {
        ctx_wr->layers_active[i] = (i + 1) + '0';
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_w(war_env* env) {
    call_carmack("war_roll_w");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_W(war_env* env) {
    call_carmack("war_roll_W");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_e(war_env* env) {
    call_carmack("war_roll_e");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_E(war_env* env) {
    call_carmack("war_roll_E");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_b(war_env* env) {
    call_carmack("war_roll_b");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_B(war_env* env) {
    call_carmack("war_roll_B");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_u(war_env* env) {
    call_carmack("war_roll_alt_u");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_alt_d(war_env* env) {
    call_carmack("war_roll_alt_d");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_tab(war_env* env) {
    call_carmack("war_roll_tab");
    war_window_render_context* ctx_wr = env->ctx_wr;
    switch (ctx_wr->cursor_blink_state) {
    case CURSOR_BLINK:
        ctx_wr->cursor_blink_state = CURSOR_BLINK_BPM;
        ctx_wr->cursor_blinking = false;
        ctx_wr->cursor_blink_previous_us = ctx_wr->now;
        break;
    case CURSOR_BLINK_BPM:
        ctx_wr->cursor_blink_state = 0;
        ctx_wr->cursor_blinking = false;
        ctx_wr->cursor_blink_previous_us = ctx_wr->now;
        break;
    case 0:
        ctx_wr->cursor_blink_state = CURSOR_BLINK;
        ctx_wr->cursor_blinking = false;
        ctx_wr->cursor_blink_previous_us = ctx_wr->now;
        ctx_wr->cursor_blink_duration_us = DEFAULT_CURSOR_BLINK_DURATION;
        break;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_shift_tab(war_env* env) {
    call_carmack("war_roll_shift_tab");
    war_window_render_context* ctx_wr = env->ctx_wr;
    switch (ctx_wr->hud_state) {
    case HUD_PIANO:
        ctx_wr->hud_state = HUD_PIANO_AND_LINE_NUMBERS;
        ctx_wr->num_cols_for_line_numbers = 6;
        ctx_wr->right_col -= 3;
        ctx_wr->cursor_pos_x =
            war_clamp_uint32(ctx_wr->cursor_pos_x, 0, ctx_wr->right_col);
        break;
    case HUD_PIANO_AND_LINE_NUMBERS:
        ctx_wr->hud_state = HUD_LINE_NUMBERS;
        ctx_wr->num_cols_for_line_numbers = 3;
        ctx_wr->right_col += 3;
        ctx_wr->cursor_pos_x =
            war_clamp_uint32(ctx_wr->cursor_pos_x, 0, ctx_wr->right_col);
        break;
    case HUD_LINE_NUMBERS:
        ctx_wr->hud_state = HUD_PIANO;
        ctx_wr->num_cols_for_line_numbers = 3;
        break;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_q(war_env* env) {
    call_carmack("war_roll_q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_Q(war_env* env) {
    call_carmack("war_roll_Q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_capture_context* ctx_capture = env->ctx_capture;
    ctx_capture->capture = 1;
    ctx_wr->mode = MODE_WAV;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_space(war_env* env) {
    call_carmack("war_roll_space");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_CAPTURE;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_colon(war_env* env) {
    call_carmack("war_roll_colon");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_command_context* ctx_command = env->ctx_command;
    war_status_context* ctx_status = env->ctx_status;
    ctx_command->command = 1;
    war_command_reset(ctx_command, ctx_status);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_u(war_env* env) {
    call_carmack("war_roll_u");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_undo_tree* undo_tree = env->undo_tree;
    assert(undo_tree != NULL);
    if (undo_tree->current) {
        war_undo_node* node = undo_tree->current;
        assert(node != NULL);
        switch (node->command) {
        case CMD_ADD_NOTE: {
            break;
        }
        case CMD_DELETE_NOTE: {
            break;
        }
        case CMD_ADD_NOTES: {
            break;
        }
        case CMD_DELETE_NOTES: {
            break;
        }
        case CMD_ADD_NOTES_SAME: {
            break;
        }
        case CMD_DELETE_NOTES_SAME: {
            break;
        }
        }
        undo_tree->current = node->prev ? node->prev : NULL;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_roll_ctrl_r(war_env* env) {
    call_carmack("war_roll_ctrl_r");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_undo_tree* undo_tree = env->undo_tree;
    assert(undo_tree != NULL);
    war_undo_node* next_node = NULL;
    if (!undo_tree->current) {
        next_node = undo_tree->root;
    } else if (undo_tree->current->next) {
        next_node = undo_tree->current->next;
    } else if (undo_tree->current->alt_next) {
        next_node = undo_tree->current->alt_next;
    }
    if (next_node) {
        assert(next_node != NULL);
        switch (next_node->command) {
        case CMD_ADD_NOTE: {
            break;
        }
        case CMD_DELETE_NOTE: {
            break;
        }
        case CMD_ADD_NOTES: {
            break;
        }
        case CMD_DELETE_NOTES: {
            break;
        }
        case CMD_ADD_NOTES_SAME: {
            break;
        }
        case CMD_DELETE_NOTES_SAME: {
            break;
        }
        }
        undo_tree->current = next_node;
    }
    ctx_wr->numeric_prefix = 0;
}

//-----------------------------------------------------------------------------
// Record mode commands
//-----------------------------------------------------------------------------

static inline void war_record_tab(war_env* env) {
    call_carmack("war_record_tab");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    atomic_fetch_xor(&atomics->capture_monitor, 1);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_K(war_env* env) {
    call_carmack("war_record_K");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
    gain = fminf(gain, 1.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_J(war_env* env) {
    call_carmack("war_record_J");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
    gain = fmaxf(gain, 0.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_k(war_env* env) {
    call_carmack("war_record_k");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->capture_gain) + ctx_wr->gain_increment;
    gain = fminf(gain, 1.0f);
    atomic_store(&atomics->capture_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_j(war_env* env) {
    call_carmack("war_record_j");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->capture_gain) - ctx_wr->gain_increment;
    gain = fmaxf(gain, 0.0f);
    atomic_store(&atomics->capture_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_Q(war_env* env) {
    call_carmack("war_record_Q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_space(war_env* env) {
    call_carmack("war_record_space");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_q(war_env* env) {
    call_carmack("war_record_q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_w(war_env* env) {
    call_carmack("war_record_w");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_e(war_env* env) {
    call_carmack("war_record_e");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_r(war_env* env) {
    call_carmack("war_record_r");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_t(war_env* env) {
    call_carmack("war_record_t");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_y(war_env* env) {
    call_carmack("war_record_y");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_u(war_env* env) {
    call_carmack("war_record_u");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_i(war_env* env) {
    call_carmack("war_record_i");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_o(war_env* env) {
    call_carmack("war_record_o");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_p(war_env* env) {
    call_carmack("war_record_p");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_record_leftbracket(war_env* env) {
    call_carmack("war_record_leftbracket");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    float note = 10 + 12 * (ctx_wr->record_octave + 1);
    if (note > 127) { return; }
    atomic_store(&atomics->map_note, note);
    ctx_wr->mode = MODE_NORMAL;
}

static inline void war_record_rightbracket(war_env* env) {
    call_carmack("war_record_rightbracket");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    float note = 11 + 12 * (ctx_wr->record_octave + 1);
    if (note > 127) { return; }
    atomic_store(&atomics->map_note, note);
    ctx_wr->mode = MODE_NORMAL;
}

static inline void war_record_minus(war_env* env) {
    call_carmack("war_record_minus");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = -1;
}

static inline void war_record_0(war_env* env) {
    call_carmack("war_record_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 0;
}

static inline void war_record_1(war_env* env) {
    call_carmack("war_record_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 1;
}

static inline void war_record_2(war_env* env) {
    call_carmack("war_record_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 2;
}

static inline void war_record_3(war_env* env) {
    call_carmack("war_record_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 3;
}

static inline void war_record_4(war_env* env) {
    call_carmack("war_record_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 4;
}

static inline void war_record_5(war_env* env) {
    call_carmack("war_record_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 5;
}

static inline void war_record_6(war_env* env) {
    call_carmack("war_record_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 6;
}

static inline void war_record_7(war_env* env) {
    call_carmack("war_record_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 7;
}

static inline void war_record_8(war_env* env) {
    call_carmack("war_record_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 8;
}

static inline void war_record_9(war_env* env) {
    call_carmack("war_record_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->numeric_prefix = 0;
    if (atomic_load(&atomics->capture)) { return; }
    ctx_wr->record_octave = 9;
}

static inline void war_record_esc(war_env* env) {
    call_carmack("war_record_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    ctx_wr->mode = MODE_NORMAL;
    atomic_store(&atomics->capture, 0);
    atomic_store(&atomics->map_note, -1);
    ctx_wr->numeric_prefix = 0;
}

//-----------------------------------------------------------------------------
// Views mode commands
//-----------------------------------------------------------------------------

static inline void war_views_k(war_env* env) {
    call_carmack("war_views_k");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_add_uint32(
        views->warpoon_row, increment, views->warpoon_max_row);
    if (views->warpoon_row >
        views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_add_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_max_row);
        views->warpoon_top_row = war_clamp_add_uint32(
            views->warpoon_top_row, increment, views->warpoon_max_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_bottom_row = war_clamp_subtract_uint32(
                views->warpoon_bottom_row, diff, views->warpoon_min_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_j(war_env* env) {
    call_carmack("war_views_j");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_subtract_uint32(
        views->warpoon_row, increment, views->warpoon_min_row);
    if (views->warpoon_row <
        views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_subtract_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_min_row);
        views->warpoon_top_row = war_clamp_subtract_uint32(
            views->warpoon_top_row, increment, views->warpoon_min_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_top_row = war_clamp_add_uint32(
                views->warpoon_top_row, diff, views->warpoon_max_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_h(war_env* env) {
    call_carmack("war_views_h");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    if (views->warpoon_mode == MODE_VISUAL_LINE) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    uint32_t increment = ctx_wr->col_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
    }
    views->warpoon_col = war_clamp_subtract_uint32(
        views->warpoon_col, increment, views->warpoon_min_col);
    if (views->warpoon_col <
        views->warpoon_left_col + ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        views->warpoon_left_col = war_clamp_subtract_uint32(
            views->warpoon_left_col, increment, views->warpoon_min_col);
        views->warpoon_right_col = war_clamp_subtract_uint32(
            views->warpoon_right_col, increment, views->warpoon_min_col);
        uint32_t new_viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            views->warpoon_right_col = war_clamp_add_uint32(
                views->warpoon_right_col, diff, views->warpoon_max_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_l(war_env* env) {
    call_carmack("war_views_l");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    if (views->warpoon_mode == MODE_VISUAL_LINE) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    uint32_t increment = ctx_wr->col_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
    }
    views->warpoon_col = war_clamp_add_uint32(
        views->warpoon_col, increment, views->warpoon_max_col);
    if (views->warpoon_col >
        views->warpoon_right_col - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        views->warpoon_left_col = war_clamp_add_uint32(
            views->warpoon_left_col, increment, views->warpoon_max_col);
        views->warpoon_right_col = war_clamp_add_uint32(
            views->warpoon_right_col, increment, views->warpoon_max_col);
        uint32_t new_viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            views->warpoon_left_col = war_clamp_subtract_uint32(
                views->warpoon_left_col, diff, views->warpoon_min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
    call_carmack("warpoon col: %u", views->warpoon_col);
}

static inline void war_views_alt_k(war_env* env) {
    call_carmack("war_views_alt_k");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    uint32_t increment = ctx_wr->row_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_add_uint32(
        views->warpoon_row, increment, views->warpoon_max_row);
    if (views->warpoon_row >
        views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_add_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_max_row);
        views->warpoon_top_row = war_clamp_add_uint32(
            views->warpoon_top_row, increment, views->warpoon_max_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_bottom_row = war_clamp_subtract_uint32(
                views->warpoon_bottom_row, diff, views->warpoon_min_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_alt_j(war_env* env) {
    call_carmack("war_views_alt_j");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    uint32_t increment = ctx_wr->row_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_subtract_uint32(
        views->warpoon_row, increment, views->warpoon_min_row);
    if (views->warpoon_row <
        views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_subtract_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_min_row);
        views->warpoon_top_row = war_clamp_subtract_uint32(
            views->warpoon_top_row, increment, views->warpoon_min_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_top_row = war_clamp_add_uint32(
                views->warpoon_top_row, diff, views->warpoon_max_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_alt_h(war_env* env) {
    call_carmack("war_views_alt_h");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    if (views->warpoon_mode == MODE_VISUAL_LINE) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    uint32_t increment = ctx_wr->col_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
    }
    views->warpoon_col = war_clamp_subtract_uint32(
        views->warpoon_col, increment, views->warpoon_min_col);
    if (views->warpoon_col <
        views->warpoon_left_col + ctx_wr->scroll_margin_cols) {
        uint32_t viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        views->warpoon_left_col = war_clamp_subtract_uint32(
            views->warpoon_left_col, increment, views->warpoon_min_col);
        views->warpoon_right_col = war_clamp_subtract_uint32(
            views->warpoon_right_col, increment, views->warpoon_min_col);
        uint32_t new_viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            views->warpoon_right_col = war_clamp_add_uint32(
                views->warpoon_right_col, diff, views->warpoon_max_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_alt_l(war_env* env) {
    call_carmack("war_views_alt_l");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    if (views->warpoon_mode == MODE_VISUAL_LINE) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    uint32_t increment = ctx_wr->col_leap_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
    }
    views->warpoon_col = war_clamp_add_uint32(
        views->warpoon_col, increment, views->warpoon_max_col);
    if (views->warpoon_col >
        views->warpoon_right_col - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        views->warpoon_left_col = war_clamp_add_uint32(
            views->warpoon_left_col, increment, views->warpoon_max_col);
        views->warpoon_right_col = war_clamp_add_uint32(
            views->warpoon_right_col, increment, views->warpoon_max_col);
        uint32_t new_viewport_width =
            views->warpoon_right_col - views->warpoon_left_col;
        if (new_viewport_width < viewport_width) {
            uint32_t diff = viewport_width - new_viewport_width;
            views->warpoon_left_col = war_clamp_subtract_uint32(
                views->warpoon_left_col, diff, views->warpoon_min_col);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_K(war_env* env) {
    call_carmack("war_views_K");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    war_warpoon_shift_up(views);
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_add_uint32(
        views->warpoon_row, increment, views->warpoon_max_row);
    if (views->warpoon_row >
        views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_add_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_max_row);
        views->warpoon_top_row = war_clamp_add_uint32(
            views->warpoon_top_row, increment, views->warpoon_max_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_bottom_row = war_clamp_subtract_uint32(
                views->warpoon_bottom_row, diff, views->warpoon_min_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_J(war_env* env) {
    call_carmack("war_views_J");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    war_warpoon_shift_down(views);
    uint32_t increment = ctx_wr->row_increment;
    if (ctx_wr->numeric_prefix) {
        increment = war_clamp_multiply_uint32(
            increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
    }
    views->warpoon_row = war_clamp_subtract_uint32(
        views->warpoon_row, increment, views->warpoon_min_row);
    if (views->warpoon_row <
        views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
        uint32_t viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        views->warpoon_bottom_row = war_clamp_subtract_uint32(
            views->warpoon_bottom_row, increment, views->warpoon_min_row);
        views->warpoon_top_row = war_clamp_subtract_uint32(
            views->warpoon_top_row, increment, views->warpoon_min_row);
        uint32_t new_viewport_height =
            views->warpoon_top_row - views->warpoon_bottom_row;
        if (new_viewport_height < viewport_height) {
            uint32_t diff = viewport_height - new_viewport_height;
            views->warpoon_top_row = war_clamp_add_uint32(
                views->warpoon_top_row, diff, views->warpoon_max_row);
        }
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_d(war_env* env) {
    call_carmack("war_views_d");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
    if (i_views >= views->views_count) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    war_warpoon_delete_at_i(views, i_views);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_V(war_env* env) {
    call_carmack("war_views_V");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    switch (views->warpoon_mode) {
    case MODE_NORMAL:
        views->warpoon_mode = MODE_VISUAL_LINE;
        views->warpoon_visual_line_row = views->warpoon_row;
        break;
    case MODE_VISUAL_LINE:
        views->warpoon_mode = MODE_NORMAL;
        break;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_esc(war_env* env) {
    call_carmack("war_views_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_views* views = env->views;
    if (views->warpoon_mode == MODE_VISUAL_LINE) {
        views->warpoon_mode = MODE_NORMAL;
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_z(war_env* env) {
    call_carmack("war_views_z");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->mode = MODE_NORMAL;
    uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
    if (i_views >= views->views_count) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_pos_x = views->col[i_views];
    ctx_wr->cursor_pos_y = views->row[i_views];
    ctx_wr->left_col = views->left_col[i_views];
    ctx_wr->bottom_row = views->bottom_row[i_views];
    ctx_wr->right_col = views->right_col[i_views];
    ctx_wr->top_row = views->top_row[i_views];
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_views_return(war_env* env) {
    call_carmack("war_views_return");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_views* views = env->views;
    war_play_context* ctx_play = env->ctx_play;
    ctx_wr->mode = MODE_NORMAL;
    uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
    if (i_views >= views->views_count) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->cursor_pos_x = views->col[i_views];
    ctx_wr->cursor_pos_y = views->row[i_views];
    ctx_wr->left_col = views->left_col[i_views];
    ctx_wr->bottom_row = views->bottom_row[i_views];
    ctx_wr->right_col = views->right_col[i_views];
    ctx_wr->top_row = views->top_row[i_views];
    if (ctx_wr->layer_flux) {
        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_1(war_env* env) {
    call_carmack("war_midi_alt_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 0;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_2(war_env* env) {
    call_carmack("war_midi_alt_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 1;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_3(war_env* env) {
    call_carmack("war_midi_alt_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 2;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_4(war_env* env) {
    call_carmack("war_midi_alt_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 3;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_5(war_env* env) {
    call_carmack("war_midi_alt_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 4;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_6(war_env* env) {
    call_carmack("war_midi_alt_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 5;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_7(war_env* env) {
    call_carmack("war_midi_alt_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 6;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_8(war_env* env) {
    call_carmack("war_midi_alt_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 7;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_9(war_env* env) {
    call_carmack("war_midi_alt_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 8;
    ctx_wr->color_cursor = colors[idx];
    ctx_wr->color_cursor_transparent = colors[idx];
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << idx));
    ctx_wr->layers_active_count = 1;
    ctx_wr->layers_active[0] = (idx + 1) + '0';
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_0(war_env* env) {
    call_carmack("war_midi_alt_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    war_lua_context* ctx_lua = env->ctx_lua;
    uint32_t layer_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
    ctx_wr->color_cursor = ctx_color->full_white_hex;
    ctx_wr->color_cursor_transparent = ctx_color->white_hex;
    ctx_wr->color_note_outline_default = ctx_color->white_hex;
    atomic_store(&atomics->layer, (1ULL << layer_count) - 1);
    ctx_wr->layers_active_count = 9;
    for (int i = 0; i < ctx_wr->layers_active_count; i++) {
        ctx_wr->layers_active[i] = (i + 1) + '0';
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_1(war_env* env) {
    call_carmack("war_midi_alt_shift_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 0;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->white_hex;
        ctx_wr->color_note_outline_default = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_2(war_env* env) {
    call_carmack("war_midi_alt_shift_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 1;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->white_hex;
        ctx_wr->color_note_outline_default = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_3(war_env* env) {
    call_carmack("war_midi_alt_shift_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 2;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->white_hex;
        ctx_wr->color_note_outline_default = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_4(war_env* env) {
    call_carmack("war_midi_alt_shift_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 3;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_5(war_env* env) {
    call_carmack("war_midi_alt_shift_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 4;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_6(war_env* env) {
    call_carmack("war_midi_alt_shift_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 5;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_7(war_env* env) {
    call_carmack("war_midi_alt_shift_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 6;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_8(war_env* env) {
    call_carmack("war_midi_alt_shift_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 7;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_9(war_env* env) {
    call_carmack("war_midi_alt_shift_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    war_color_context* ctx_color = env->ctx_color;
    uint32_t* colors = ctx_color->colors;
    int idx = 8;
    atomic_fetch_xor(&atomics->layer, (1ULL << idx));
    uint64_t layer = atomic_load(&atomics->layer);
    ctx_wr->layers_active_count = __builtin_popcountll(layer);
    switch (ctx_wr->layers_active_count) {
    case 0:
        ctx_wr->color_cursor = ctx_color->white_hex;
        ctx_wr->color_cursor_transparent = ctx_color->full_white_hex;
        break;
    case 1: {
        int active = __builtin_ctzll(layer);
        ctx_wr->color_cursor = colors[active];
        ctx_wr->color_cursor_transparent = colors[active];
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
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_alt_shift_0(war_env* env) {
    call_carmack("war_midi_alt_shift_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_m(war_env* env) {
    call_carmack("war_midi_m");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_T(war_env* env) {
    call_carmack("war_midi_T");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_toggle ^= 1;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_b(war_env* env) {
    call_carmack("war_midi_b");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_toggle ^= 1;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_x(war_env* env) {
    call_carmack("war_midi_x");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_c(war_env* env) {
    call_carmack("war_midi_c");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_K(war_env* env) {
    call_carmack("war_midi_K");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
    gain = fminf(gain, 1.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_J(war_env* env) {
    call_carmack("war_midi_J");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    float gain = atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
    gain = fmaxf(gain, 0.0f);
    atomic_store(&atomics->play_gain, gain);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_Q(war_env* env) {
    call_carmack("war_midi_Q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_space(war_env* env) {
    call_carmack("war_midi_space");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_q(war_env* env) {
    call_carmack("war_midi_q");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_w(war_env* env) {
    call_carmack("war_midi_w");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_e(war_env* env) {
    call_carmack("war_midi_e");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_r(war_env* env) {
    call_carmack("war_midi_r");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_t(war_env* env) {
    call_carmack("war_midi_t");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_y(war_env* env) {
    call_carmack("war_midi_y");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_u(war_env* env) {
    call_carmack("war_midi_u");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_i(war_env* env) {
    call_carmack("war_midi_i");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_o(war_env* env) {
    call_carmack("war_midi_o");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_p(war_env* env) {
    call_carmack("war_midi_p");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_leftbracket(war_env* env) {
    call_carmack("war_midi_leftbracket");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_rightbracket(war_env* env) {
    call_carmack("war_midi_rightbracket");
    war_window_render_context* ctx_wr = env->ctx_wr;
    int note = 11 + 12 * (ctx_wr->midi_octave + 1);
    if (note > 127) {
        ctx_wr->numeric_prefix = 0;
        return;
    }
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_l(war_env* env) {
    call_carmack("war_midi_l");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_atomics* atomics = env->atomics;
    atomic_fetch_xor(&atomics->loop, 1);
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_minus(war_env* env) {
    call_carmack("war_midi_minus");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = -1;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_esc(war_env* env) {
    call_carmack("war_midi_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->mode = MODE_NORMAL;
}

static inline void war_midi_0(war_env* env) {
    call_carmack("war_midi_0");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 0;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_1(war_env* env) {
    call_carmack("war_midi_1");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 1;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_2(war_env* env) {
    call_carmack("war_midi_2");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 2;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_3(war_env* env) {
    call_carmack("war_midi_3");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 3;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_4(war_env* env) {
    call_carmack("war_midi_4");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 4;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_5(war_env* env) {
    call_carmack("war_midi_5");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 5;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_6(war_env* env) {
    call_carmack("war_midi_6");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 6;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_7(war_env* env) {
    call_carmack("war_midi_7");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 7;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_8(war_env* env) {
    call_carmack("war_midi_8");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 8;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_midi_9(war_env* env) {
    call_carmack("war_midi_9");
    war_window_render_context* ctx_wr = env->ctx_wr;
    ctx_wr->midi_octave = 9;
    ctx_wr->numeric_prefix = 0;
}

static inline void war_wav_Q(war_env* env) {
    call_carmack("war_wav_Q");
    war_capture_context* ctx_capture = env->ctx_capture;
    war_wav* capture_wav = env->capture_wav;
    ctx_capture->capture = !ctx_capture->capture;
    if (ctx_capture->capture) { return; }
    if (ctx_capture->prompt) {
        ctx_capture->capture = 1;
        ctx_capture->prompt_step = 0;
        ctx_capture->state = CAPTURE_PROMPT;
        return;
    }
    if (ftruncate(capture_wav->fd, capture_wav->memfd_size) == -1) {
        call_carmack("save_file: fd ftruncate failed: %s", capture_wav->fname);
        return;
    }
    off_t offset = 0;
    call_carmack("saving file");
    ssize_t result = sendfile(
        capture_wav->fd, capture_wav->memfd, &offset, capture_wav->memfd_size);
    if (result == -1) {
        call_carmack("save_file: sendfile failed: %s", capture_wav->fname);
    }
    lseek(capture_wav->fd, 0, SEEK_SET);
    memset(capture_wav->wav + 44, 0, capture_wav->memfd_capacity - 44);
    capture_wav->memfd_size = 44;
}

static inline void war_wav_esc(war_env* env) {
    call_carmack("war_wav_esc");
    war_window_render_context* ctx_wr = env->ctx_wr;
    war_capture_context* ctx_capture = env->ctx_capture;
    ctx_wr->mode = MODE_NORMAL;
    ctx_capture->state = CAPTURE_WAITING;
    ctx_capture->capture = 0;
    ctx_wr->numeric_prefix = 0;
}

#endif // WAR_COMMAND_H
