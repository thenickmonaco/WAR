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
// src/shaders/war_quad_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in uvec2 in_col_row;
layout(location = 1) in uvec2 in_sub_col_row;
layout(location = 2) in uvec2 in_sub_cells;
layout(location = 3) in uvec2 in_cursor_size_sub_col_row;
layout(location = 4) in uvec2 in_cursor_size_whole_number;
layout(location = 5) in uvec2 in_cursor_size_sub_cells;
layout(location = 6) in vec4 in_color;
layout(location = 7) in uvec2 in_corner;
layout(location = 8) in float in_outline_thickness;
layout(location = 9) in vec4 in_outline_color;
layout(location = 10) in vec2 in_line_thickness;
layout(location = 11) in vec2 in_float_offset;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out float frag_outline_thickness;
layout(location = 2) out vec4 frag_outline_color;
layout(location = 3) out vec2 frag_local_pos;
layout(location = 4) out vec2 frag_quad_size;
layout(location = 5) out vec2 frag_corner_sign;
layout(location = 6) out vec2 frag_pc_cell_size;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) uvec2 bottom_left;
    layout(offset = 8) vec2 physical_size;
    layout(offset = 16) vec2 cell_size;
    layout(offset = 24) float zoom;
    layout(offset = 28) uint _pad1;
    layout(offset = 32) uvec2 cell_offsets;
    layout(offset = 40) uvec2 scroll_margin; 
    layout(offset = 48) uvec2 anchor_cell;
    layout(offset = 56) uvec2 top_right;
    // 64 bytes
} pc;

void main() {
    vec2 corner_sign = vec2(
        (in_corner.x == 0u ? (in_line_thickness.x == 0.0 ? 0.0 : -1.0) : 1.0),
        (in_corner.y == 0u ? (in_line_thickness.y == 0.0 ? 0.0 : -1.0) : 1.0)
    );
    uvec2 local_cell = (in_col_row - pc.bottom_left) + pc.cell_offsets;
    vec2 pixel_pos = vec2(float(local_cell.x) + float(in_sub_col_row.x) / float(in_sub_cells.x), float(local_cell.y) + float(in_sub_col_row.y) / float(in_sub_cells.y)) * pc.cell_size;

    float cursor_x = (in_cursor_size_sub_col_row.x * in_cursor_size_whole_number.x) / float(in_cursor_size_sub_cells.x) * pc.cell_size.x;
    float offset_x = corner_sign.x * (in_line_thickness.x * pc.cell_size.x + abs(pc.cell_size.x - pc.cell_size.x) + cursor_x);
    float offset_y = corner_sign.y * (in_line_thickness.y * pc.cell_size.y + abs(pc.cell_size.y - pc.cell_size.y));
    vec2 transformed = vec2(pixel_pos.x + offset_x, pixel_pos.y + offset_y);
    
    vec2 anchor_pixel = vec2(float(pc.anchor_cell.x - pc.bottom_left.x + pc.cell_offsets.x),
                             float(pc.anchor_cell.y - pc.bottom_left.y + pc.cell_offsets.y)) * pc.cell_size;
    vec2 delta = transformed - anchor_pixel;
    vec2 zoomed = delta * pc.zoom + anchor_pixel;
    vec2 ndc = vec2(
        (zoomed.x / pc.physical_size.x) * 2.0 - 1.0,
        1.0 - (zoomed.y / pc.physical_size.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);

    frag_color = in_color;
    frag_outline_color = in_outline_color;
    frag_outline_thickness = in_outline_thickness * pc.cell_size.x;
    frag_local_pos = vec2(in_corner) * vec2(cursor_x, pc.cell_size.y);
    frag_quad_size = vec2(cursor_x, pc.cell_size.y);
    frag_corner_sign = corner_sign;
    frag_pc_cell_size = pc.cell_size;
}
