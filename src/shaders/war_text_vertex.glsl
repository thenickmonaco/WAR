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
// src/shaders/war_text_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_corner;
layout(location = 1) in vec3 in_pos;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec2 in_glyph_bearing;
layout(location = 5) in vec2 in_glyph_size;
layout(location = 6) in vec2 in_ascent;
layout(location = 7) in vec2 in_descent;
layout(location = 8) in float in_thickness;
layout(location = 9) in float in_feather;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_uv;
layout(location = 2) out float frag_thickness;
layout(location = 3) out float frag_feather;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec2 bottom_left;
    layout(offset = 8) vec2 physical_size;
    layout(offset = 16) vec2 cell_size;
    layout(offset = 24) float zoom;
    layout(offset = 32) vec2 cell_offsets;
    layout(offset = 40) vec2 scroll_margin; 
    layout(offset = 48) vec2 anchor_cell;
    layout(offset = 56) vec2 top_right;
    layout(offset = 64) float ascent;
    layout(offset = 68) float descent;
    layout(offset = 72) float line_gap;
    layout(offset = 76) float baseline;
    layout(offset = 80) float font_height;
} pc;

void main() {
    gl_Position = vec4(in_pos.xy, in_pos.z, 1.0);
    frag_uv = in_uv;
    frag_color = in_color;
    frag_thickness = in_thickness;
    frag_feather = in_feather;
}
