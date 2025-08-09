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

layout(location = 0) in uvec2 in_pos;   // in pixels or cell units
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 color;

// Push constants for zoom, pan, and viewport size in pixels
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
    uvec2 local_cell = (in_pos - pc.bottom_left) + pc.cell_offsets;

    vec2 pixel_pos = vec2(float(local_cell.x), float(local_cell.y)) * pc.cell_size;

    vec2 anchor_pixel = vec2(float(pc.anchor_cell.x - pc.bottom_left.x), float(pc.anchor_cell.y - pc.bottom_left.y)) * pc.cell_size;

    vec2 zoomed = ((pixel_pos - anchor_pixel) * pc.zoom) + anchor_pixel;

    vec2 ndc = vec2(
        (zoomed.x / pc.physical_size.x) * 2.0 - 1.0,
        1.0 - (zoomed.y / pc.physical_size.y) * 2.0
    );

    gl_Position = vec4(ndc, 0.0, 1.0);
    color = in_color;
}
