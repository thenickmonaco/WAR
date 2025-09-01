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
// src/shaders/war_sdf_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in uvec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_glyph_bearing;
layout(location = 3) in vec2 in_glyph_size;
layout(location = 4) in float in_glyph_ascent;
layout(location = 5) in float in_glyph_descent;
layout(location = 6) in float in_thickness;
layout(location = 7) in float in_feather;
layout(location = 8) in uvec2 in_corner;
layout(location = 9) in vec4 in_color;

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
    vec2 corner_sign = vec2((in_corner.x == 0u ? 1.0 : -1.0), // left -> +1, right -> -1
         (in_corner.y == 0u ? 1.0 : -1.0) // bottom -> +1, top -> -1
    );
    uvec2 local_cell = (in_pos - pc.bottom_left) + pc.cell_offsets;
    vec2 cell_pixel = vec2(float(local_cell.x), float(local_cell.y)) * pc.cell_size;

    float glyph_offset_x = corner_sign.x * (((pc.cell_size.x - in_glyph_size.x) * 0.5 + in_glyph_bearing.x) * 0.5);
    float glyph_offset_y_bottom = corner_sign.y * pc.cell_size.y - pc.baseline - in_glyph_descent;
    float glyph_offset_y_top = corner_sign.y * (pc.baseline - in_glyph_bearing.y);
    float glyph_offset_y = mix(glyph_offset_y_bottom, glyph_offset_y_top, float(in_corner.y));

    vec2 glyph_transform = vec2(glyph_offset_x, glyph_offset_y);
    vec2 pixel_pos = cell_pixel + glyph_transform;
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
