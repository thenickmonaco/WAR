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
// src/shaders/war_sdf_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in uvec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_glyph_bearing;
layout(location = 3) in vec2 in_glyph_size;
layout(location = 4) in float in_thickness;
layout(location = 5) in float in_feather;
layout(location = 6) in vec4 in_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out float frag_thickness;
layout(location = 3) out float frag_feather;

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
    layout(offset = 64) float ascent;
    layout(offset = 68) float descent;
    layout(offset = 72) float line_gap;
    layout(offset = 76) float baseline;
    layout(offset = 80) float font_height;
    layout(offset = 84) uint _pad2;
} pc;

void main() {
    uvec2 local_cell = (in_pos - pc.bottom_left) + pc.cell_offsets;
    vec2 cell_pixel = vec2(float(local_cell.x), float(local_cell.y)) * pc.cell_size;
    vec2 glyph_pixel = in_pos;
    vec2 pixel_pos = cell_pixel + glyph_pixel;
    vec2 anchor_pixel = vec2(
        float(pc.anchor_cell.x - pc.bottom_left.x + pc.cell_offsets.x),
        float(pc.anchor_cell.y - pc.bottom_left.y + pc.cell_offsets.y)
    ) * pc.cell_size;
    vec2 delta = pixel_pos - anchor_pixel;
    vec2 zoomed = delta * pc.zoom + anchor_pixel;
    vec2 ndc = vec2(
        (zoomed.x / pc.physical_size.x) * 2.0 - 1.0,
        1.0 - (zoomed.y / pc.physical_size.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);

    frag_uv = in_uv;
    frag_color = in_color;
    frag_thickness = in_thickness;
    frag_feather = in_feather;
}
