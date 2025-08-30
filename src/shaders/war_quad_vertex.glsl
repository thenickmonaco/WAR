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
// src/shaders/war_quad_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in uvec2 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in uvec2 in_corner;
layout(location = 3) in vec2 in_line_thickness;
layout(location = 4) in vec2 in_scale;

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
    uvec2 local_cell = (in_pos - pc.bottom_left) + pc.cell_offsets;
    vec2 pixel_pos = vec2(float(local_cell.x), float(local_cell.y)) * pc.cell_size;

    float offset_x = corner_sign.x * (in_line_thickness.x * pc.cell_size.x + abs(pc.cell_size.x - in_scale.x * pc.cell_size.x));
    float offset_y = corner_sign.y * (in_line_thickness.y * pc.cell_size.y + abs(pc.cell_size.y - in_scale.y * pc.cell_size.y));
    vec2 transformed = vec2(pixel_pos.x + offset_x, pixel_pos.y + offset_y);
    
    vec2 anchor_pixel = vec2(float(pc.anchor_cell.x - pc.bottom_left.x + pc.cell_offsets.x),
                             float(pc.anchor_cell.y - pc.bottom_left.y + pc.cell_offsets.y)) * pc.cell_size;
    vec2 delta = transformed - anchor_pixel;
    //delta *= in_scale;
    vec2 zoomed = delta * pc.zoom + anchor_pixel;
    vec2 ndc = vec2(
        (zoomed.x / pc.physical_size.x) * 2.0 - 1.0,
        1.0 - (zoomed.y / pc.physical_size.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    color = in_color;
}
