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
layout(location = 8) in vec2 in_line_thickness;
layout(location = 9) in vec2 in_float_offset;

layout(location = 0) out vec4 color;

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
    vec2 corner_sign = vec2((in_corner.x == 0u ? -1.0 : 1.0), // left -> -1, right -> +1
         (in_corner.y == 0u ? -1.0 : 1.0) // bottom -> -1, top -> +1
    );
    if (in_line_thickness.x == 0.0 && in_line_thickness.y == 0.0) {
        corner_sign = vec2((in_corner.x == 0u ? 0.0 : 1.0), // left -> 0, right -> +1
             (in_corner.y == 0u ? 0.0 : 1.0) // bottom -> 0, top -> +1
        );
    }
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
    color = in_color;
}
