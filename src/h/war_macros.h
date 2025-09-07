//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
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
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <xkbcommon/xkbcommon.h>

// COMMENT OPTIMIZE: Duff's Device + SIMD (intrinsics)

#define ALIGN32(p) (uint8_t*)(((uintptr_t)(p) + 31) & ~((uintptr_t)31))

#define obj_op_index(obj, op) ((obj) * max_opcodes + (op))

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

static inline bool war_state_is_prefix(uint16_t state_index,
                                       war_fsm_state* fsm) {
    for (int k = 0; k < 256; k++) {
        for (int m = 0; m < 16; m++) {
            if (fsm[state_index].next_state[k][m] != 0) { return true; }
        }
    }
    return false;
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

static inline char war_keysym_to_key(xkb_keysym_t ks, uint8_t mod) {
    char lowercase = ks;
    int mod_shift_difference = (mod == MOD_SHIFT ? 32 : 0);
    // Letters a-z or A-Z -> always lowercase
    if (ks >= XKB_KEY_a && ks <= XKB_KEY_z)
        return (char)ks - mod_shift_difference;
    if (ks >= XKB_KEY_A && ks <= XKB_KEY_Z) return (char)(ks - XKB_KEY_A + 'a');

    // Numbers 0-9
    if (ks >= XKB_KEY_0 && ks <= XKB_KEY_9) return (char)ks;

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

static inline void war_make_text_quad(war_text_vertex* text_vertices,
                                      uint16_t* text_indices,
                                      uint32_t* text_vertices_count,
                                      uint32_t* text_indices_count,
                                      float bottom_left_pos[3],
                                      war_glyph_info glyph_info,
                                      float thickness,
                                      float feather,
                                      uint32_t color) {
    text_vertices[*text_vertices_count] = (war_text_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_pos[0], bottom_left_pos[1], bottom_left_pos[2]},
        .uv = {glyph_info.uv_x0, glyph_info.uv_y1},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    text_vertices[*text_vertices_count + 1] = (war_text_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_pos[0] + 1, bottom_left_pos[1], bottom_left_pos[2]},
        .uv = {glyph_info.uv_x1, glyph_info.uv_y1},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    text_vertices[*text_vertices_count + 2] = (war_text_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_pos[0] + 1,
                bottom_left_pos[1] + 1,
                bottom_left_pos[2]},
        .uv = {glyph_info.uv_x1, glyph_info.uv_y0},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    text_vertices[*text_vertices_count + 3] = (war_text_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_pos[0], bottom_left_pos[1] + 1, bottom_left_pos[2]},
        .uv = {glyph_info.uv_x0, glyph_info.uv_y0},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    text_indices[*text_indices_count] = *text_vertices_count;
    text_indices[*text_indices_count + 1] = *text_vertices_count + 1;
    text_indices[*text_indices_count + 2] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 3] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 4] = *text_vertices_count + 3;
    text_indices[*text_indices_count + 5] = *text_vertices_count;
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
    };
    text_indices[*text_indices_count] = *text_vertices_count;
    text_indices[*text_indices_count + 1] = *text_vertices_count + 1;
    text_indices[*text_indices_count + 2] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 3] = *text_vertices_count + 2;
    text_indices[*text_indices_count + 4] = *text_vertices_count + 3;
    text_indices[*text_indices_count + 5] = *text_vertices_count;
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

static inline void war_make_blank_quad(war_quad_vertex* quad_vertices,
                                       uint16_t* quad_indices,
                                       uint32_t* vertices_count,
                                       uint32_t* indices_count) {
    quad_vertices[*vertices_count] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 1] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 2] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
        .color = 0,
        .outline_thickness = 0,
        .outline_color = 0,
        .line_thickness = {0, 0},
        .flags = 0,
    };
    quad_vertices[*vertices_count + 3] = (war_quad_vertex){
        .corner = {0, 0},
        .pos = {0, 0, 0},
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

static inline void
war_note_quads_add(war_note_quads* note_quads,
                   uint32_t* note_quads_count,
                   uint8_t* window_render_to_audio_ring_buffer,
                   uint8_t* write_to_audio_index,
                   war_input_cmd_context* ctx,
                   uint32_t color,
                   uint32_t outline_color,
                   float strength,
                   uint32_t voice,
                   uint32_t hidden,
                   uint32_t mute) {
    assert(*note_quads_count < max_note_quads);
    note_quads->timestamp[*note_quads_count] = ctx->now;
    note_quads->col[*note_quads_count] = ctx->col;
    note_quads->row[*note_quads_count] = ctx->row;
    note_quads->sub_col[*note_quads_count] = ctx->sub_col;
    note_quads->sub_cells_col[*note_quads_count] =
        ctx->navigation_sub_cells_col;
    note_quads->cursor_width_whole_number[*note_quads_count] =
        ctx->cursor_width_whole_number;
    note_quads->cursor_width_sub_col[*note_quads_count] =
        ctx->cursor_width_sub_col;
    note_quads->cursor_width_sub_cells[*note_quads_count] =
        ctx->cursor_width_sub_cells;
    note_quads->color[*note_quads_count] = color;
    note_quads->outline_color[*note_quads_count] = outline_color;
    note_quads->strength[*note_quads_count] = strength;
    note_quads->voice[*note_quads_count] = voice;
    note_quads->hidden[*note_quads_count] = hidden;
    note_quads->mute[*note_quads_count] = mute;
    (*note_quads_count)++;

    // TODO: send add command to audio thread
}

static inline void
war_note_quads_delete(war_note_quads* note_quads,
                      uint32_t* note_quads_count,
                      uint8_t* window_render_to_audio_ring_buffer,
                      uint8_t* write_to_audio_index,
                      war_input_cmd_context* ctx) {
    if (*note_quads_count == 0) return;

    // Find the index of the freshest note at the given col/row
    uint64_t freshest_time = 0;
    int32_t freshest_index = -1;

    for (uint32_t i = 0; i < *note_quads_count; i++) {
        if (note_quads->col[i] == ctx->col && note_quads->row[i] == ctx->row &&
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
        note_quads->col[freshest_index] = note_quads->col[last];
        note_quads->row[freshest_index] = note_quads->row[last];
        note_quads->sub_col[freshest_index] = note_quads->sub_col[last];
        note_quads->sub_cells_col[freshest_index] =
            note_quads->sub_cells_col[last];
        note_quads->cursor_width_whole_number[freshest_index] =
            note_quads->cursor_width_whole_number[last];
        note_quads->cursor_width_sub_col[freshest_index] =
            note_quads->cursor_width_sub_col[last];
        note_quads->cursor_width_sub_cells[freshest_index] =
            note_quads->cursor_width_sub_cells[last];
        note_quads->color[freshest_index] = note_quads->color[last];
        note_quads->outline_color[freshest_index] =
            note_quads->outline_color[last];
        note_quads->strength[freshest_index] = note_quads->strength[last];
        note_quads->voice[freshest_index] = note_quads->voice[last];
        note_quads->hidden[freshest_index] = note_quads->hidden[last];
        note_quads->mute[freshest_index] = note_quads->mute[last];
    }

    (*note_quads_count)--;

    // TODO: send delete command to audio thread
}

static inline void
war_note_quads_delete_at_i(war_note_quads* note_quads,
                           uint32_t* note_quads_count,
                           uint8_t* window_render_to_audio_ring_buffer,
                           uint8_t* write_to_audio_index,
                           uint32_t i_delete) {
    if (*note_quads_count == 0 || i_delete >= *note_quads_count) return;

    // Swap-back delete
    uint32_t last = *note_quads_count - 1;

    // Swap the element to delete with the last element
    note_quads->timestamp[i_delete] = note_quads->timestamp[last];
    note_quads->col[i_delete] = note_quads->col[last];
    note_quads->row[i_delete] = note_quads->row[last];
    note_quads->sub_col[i_delete] = note_quads->sub_col[last];
    note_quads->sub_cells_col[i_delete] = note_quads->sub_cells_col[last];
    note_quads->cursor_width_whole_number[i_delete] =
        note_quads->cursor_width_whole_number[last];
    note_quads->cursor_width_sub_col[i_delete] =
        note_quads->cursor_width_sub_col[last];
    note_quads->cursor_width_sub_cells[i_delete] =
        note_quads->cursor_width_sub_cells[last];
    note_quads->color[i_delete] = note_quads->color[last];
    note_quads->outline_color[i_delete] = note_quads->outline_color[last];
    note_quads->strength[i_delete] = note_quads->strength[last];
    note_quads->voice[i_delete] = note_quads->voice[last];
    note_quads->hidden[i_delete] = note_quads->hidden[last];
    note_quads->mute[i_delete] = note_quads->mute[last];

    (*note_quads_count)--;

    // TODO: send delete command to audio thread if needed
}

static inline void war_note_quads_in_view(war_note_quads* note_quads,
                                          uint32_t note_quads_count,
                                          war_input_cmd_context* ctx,
                                          uint32_t* out_indices,
                                          uint32_t* out_indices_count) {
    for (uint32_t i = 0; i < note_quads_count; i++) {
        uint32_t note_col = note_quads->col[i];
        uint32_t note_row = note_quads->row[i];
        uint32_t note_col_end =
            (note_quads->sub_cells_col[i] == 1) ?
                note_col + note_quads->cursor_width_whole_number[i] :
                note_col + note_quads->cursor_width_sub_col[i] *
                               note_quads->cursor_width_whole_number[i] /
                               note_quads->cursor_width_sub_cells[i];
        call_carmack("note_col_end: %u", note_col_end);
        if (((note_col >= ctx->left_col && note_col <= ctx->right_col) ||
             (note_col_end >= ctx->left_col)) &&
            (note_row >= ctx->bottom_row && note_row <= ctx->top_row)) {
            out_indices[*out_indices_count] = i;
            (*out_indices_count)++;
        }
    }
}

static inline void war_note_quads_outside_view(war_note_quads* note_quads,
                                               uint32_t note_quads_count,
                                               war_input_cmd_context* ctx,
                                               uint32_t* out_indices,
                                               uint32_t* out_indices_count) {
    for (uint32_t i = 0; i < note_quads_count; i++) {
        uint32_t note_col = note_quads->col[i];
        uint32_t note_row = note_quads->row[i];
        if ((note_col < ctx->left_col || note_col > ctx->right_col) ||
            (note_row < ctx->bottom_row || note_row > ctx->top_row)) {
            out_indices[*out_indices_count] = i;
            (*out_indices_count)++;
        }
    }
}

static inline void war_note_quads_in_cursor(war_note_quads* note_quads,
                                            uint32_t note_quads_count,
                                            war_input_cmd_context* ctx,
                                            uint32_t* out_indices,
                                            uint32_t* out_indices_count) {
    uint32_t cursor_start_col = ctx->col;
    uint32_t cursor_start_row = ctx->row;
    uint32_t cursor_start_sub_col = ctx->sub_col;
    uint32_t cursor_start_whole_number = ctx->navigation_whole_number_col;
    uint32_t cursor_start_sub_cells = ctx->navigation_sub_cells_col;
    for (uint32_t i = 0; i < note_quads_count; i++) {}
}

#endif // WAR_MACROS_H
