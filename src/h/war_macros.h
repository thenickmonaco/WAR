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
#include <xkbcommon/xkbcommon.h>

#define obj_op_index(obj, op) ((obj) * max_opcodes + (op))

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

static inline uint32_t
clamp_add_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t sum = (uint64_t)a + b;
    uint64_t mask = -(sum > max_value);
    return (uint32_t)((sum & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
clamp_subtract_uint32(uint32_t a, uint32_t b, uint32_t min_value) {
    uint32_t diff = a - b;
    uint32_t underflow_mask = -(a < b);
    uint32_t below_min_mask = -(diff < min_value);
    uint32_t clamped_diff =
        (diff & ~below_min_mask) | (min_value & below_min_mask);
    return (clamped_diff & ~underflow_mask) | (min_value & underflow_mask);
}

static inline uint32_t
clamp_multiply_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t prod = (uint64_t)a * (uint64_t)b;
    uint64_t mask = -(prod > max_value);
    return (uint32_t)((prod & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
clamp_uint32(uint32_t a, uint32_t min_value, uint32_t max_value) {
    a = a < min_value ? min_value : a;
    a = a > max_value ? max_value : a;
    return a;
}

static inline bool state_is_prefix(uint16_t state_index, war_fsm_state* fsm) {
    for (int k = 0; k < 256; k++) {
        for (int m = 0; m < 16; m++) {
            if (fsm[state_index].next_state[k][m] != 0) { return true; }
        }
    }
    return false;
}

static inline uint64_t align64(uint64_t value) { return (value + 63) & ~63ULL; }

static inline uint16_t normalize_keysym(xkb_keysym_t ks) {
    if ((ks >= XKB_KEY_a && ks <= XKB_KEY_z) ||
        (ks >= XKB_KEY_0 && ks <= XKB_KEY_9)) {
        return (uint16_t)ks;
    }
    switch (ks) {
    case XKB_KEY_Escape:
        return 256;
    case XKB_KEY_Left:
        return 257;
    case XKB_KEY_Up:
        return 258;
    case XKB_KEY_Right:
        return 259;
    case XKB_KEY_Down:
        return 260;
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
        return 511; // fallback / unknown
    }
}

static inline int war_num_digits(uint32_t n) {
    int digits = 0;
    do {
        digits++;
        n /= 10;
    } while (n != 0);
    return digits;
}

static inline void war_make_sdf_quad(sdf_vertex* sdf_verts,
                                     uint16_t* sdf_indices,
                                     size_t i_verts,
                                     size_t i_indices,
                                     uint32_t bottom_left_corner[2],
                                     war_glyph_info glyph_info,
                                     float thickness,
                                     float feather,
                                     uint32_t color) {
    sdf_verts[i_verts] = (sdf_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_corner[0], bottom_left_corner[1]},
        .uv = {glyph_info.uv_x0, glyph_info.uv_y1},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    sdf_verts[i_verts + 1] = (sdf_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_corner[0] + 1, bottom_left_corner[1]},
        .uv = {glyph_info.uv_x1, glyph_info.uv_y1},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    sdf_verts[i_verts + 2] = (sdf_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_corner[0] + 1, bottom_left_corner[1] + 1},
        .uv = {glyph_info.uv_x1, glyph_info.uv_y0},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    sdf_verts[i_verts + 3] = (sdf_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_corner[0], bottom_left_corner[1] + 1},
        .uv = {glyph_info.uv_x0, glyph_info.uv_y0},
        .glyph_size = {glyph_info.width, glyph_info.height},
        .glyph_bearing = {glyph_info.bearing_x, glyph_info.bearing_y},
        .ascent = glyph_info.ascent,
        .descent = glyph_info.descent,
        .thickness = thickness,
        .feather = feather,
        .color = color,
    };
    sdf_indices[i_indices] = i_verts;
    sdf_indices[i_indices + 1] = i_verts + 1;
    sdf_indices[i_indices + 2] = i_verts + 2;
    sdf_indices[i_indices + 3] = i_verts + 2;
    sdf_indices[i_indices + 4] = i_verts + 3;
    sdf_indices[i_indices + 5] = i_verts;
}

static inline void war_make_blank_sdf_quad(sdf_vertex* sdf_verts,
                                           uint16_t* sdf_indices,
                                           size_t i_verts,
                                           size_t i_indices) {
    sdf_verts[i_verts] = (sdf_vertex){
        .corner = {0, 0},
        .pos = {0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
    };
    sdf_verts[i_verts + 1] = (sdf_vertex){
        .corner = {0, 0},
        .pos = {0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
    };
    sdf_verts[i_verts + 2] = (sdf_vertex){
        .corner = {0, 0},
        .pos = {0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
    };
    sdf_verts[i_verts + 3] = (sdf_vertex){
        .corner = {0, 0},
        .pos = {0, 0},
        .uv = {0, 0},
        .glyph_size = {0, 0},
        .glyph_bearing = {0, 0},
        .ascent = 0,
        .descent = 0,
        .thickness = 0,
        .feather = 0,
        .color = 0,
    };
    sdf_indices[i_indices] = i_verts;
    sdf_indices[i_indices + 1] = i_verts + 1;
    sdf_indices[i_indices + 2] = i_verts + 2;
    sdf_indices[i_indices + 3] = i_verts + 2;
    sdf_indices[i_indices + 4] = i_verts + 3;
    sdf_indices[i_indices + 5] = i_verts;
}

static inline void war_make_quad(quad_vertex* quad_verts,
                                 uint16_t* quad_indices,
                                 size_t i_verts,
                                 size_t i_indices,
                                 uint32_t bottom_left_corner[2],
                                 uint32_t span[2],
                                 float cell_size[2],
                                 float line_thickness[2],
                                 uint32_t color) {
    quad_verts[i_verts] = (quad_vertex){
        .corner = {0, 0},
        .pos = {bottom_left_corner[0], bottom_left_corner[1]},
        .color = color,
        .cell_size = {cell_size[0], cell_size[1]},
        .line_thickness = {line_thickness[0], line_thickness[1]},
    };
    quad_verts[i_verts + 1] = (quad_vertex){
        .corner = {1, 0},
        .pos = {bottom_left_corner[0] + span[0], bottom_left_corner[1]},
        .color = color,
        .cell_size = {cell_size[0], cell_size[1]},
        .line_thickness = {line_thickness[0], line_thickness[1]},
    };
    quad_verts[i_verts + 2] = (quad_vertex){
        .corner = {1, 1},
        .pos = {bottom_left_corner[0] + span[0],
                bottom_left_corner[1] + span[1]},
        .color = color,
        .cell_size = {cell_size[0], cell_size[1]},
        .line_thickness = {line_thickness[0], line_thickness[1]},
    };
    quad_verts[i_verts + 3] = (quad_vertex){
        .corner = {0, 1},
        .pos = {bottom_left_corner[0], bottom_left_corner[1] + span[1]},
        .color = color,
        .cell_size = {cell_size[0], cell_size[1]},
        .line_thickness = {line_thickness[0], line_thickness[1]},
    };
    quad_indices[i_indices] = i_verts;
    quad_indices[i_indices + 1] = i_verts + 1;
    quad_indices[i_indices + 2] = i_verts + 2;
    quad_indices[i_indices + 3] = i_verts + 2;
    quad_indices[i_indices + 4] = i_verts + 3;
    quad_indices[i_indices + 5] = i_verts;
}

static inline void war_make_blank_quad(quad_vertex* quad_verts,
                                       uint16_t* quad_indices,
                                       size_t i_verts,
                                       size_t i_indices) {
    quad_verts[i_verts] = (quad_vertex){
        .pos = {0, 0},
        .color = 0,
        .corner = {0, 0},
        .line_thickness = {0, 0},
        .cell_size = {0, 0},
    };
    quad_verts[i_verts + 1] = (quad_vertex){
        .pos = {0, 0},
        .color = 0,
        .corner = {0, 0},
        .line_thickness = {0, 0},
        .cell_size = {0, 0},
    };
    quad_verts[i_verts + 2] = (quad_vertex){
        .pos = {0, 0},
        .color = 0,
        .corner = {0, 0},
        .line_thickness = {0, 0},
        .cell_size = {0, 0},
    };
    quad_verts[i_verts + 3] = (quad_vertex){
        .pos = {0, 0},
        .color = 0,
        .corner = {0, 0},
        .line_thickness = {0, 0},
        .cell_size = {0, 0},
    };
    quad_indices[i_indices] = i_verts;
    quad_indices[i_indices + 1] = i_verts + 1;
    quad_indices[i_indices + 2] = i_verts + 2;
    quad_indices[i_indices + 3] = i_verts + 2;
    quad_indices[i_indices + 4] = i_verts + 3;
    quad_indices[i_indices + 5] = i_verts;
}

#endif // WAR_MACROS_H
