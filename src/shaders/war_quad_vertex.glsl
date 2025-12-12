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

layout (location = 0) in vec2 in_corner;
layout (location = 1) in vec3 in_pos;
layout (location = 2) in vec4 in_color;
layout (location = 3) in float in_outline_thickness;
layout (location = 4) in vec4 in_outline_color;
layout (location = 5) in vec2 in_line_thickness;
layout (location = 6) in uint in_flags;
layout (location = 7) in vec2 in_span;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out float frag_outline_thickness;
layout(location = 2) out vec4 frag_outline_color;
layout(location = 3) out vec2 frag_uv;
layout(location = 4) out vec2 frag_span;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec2 bottom_left;
    layout(offset = 8) vec2 physical_size;
    layout(offset = 16) vec2 cell_size;
    layout(offset = 24) float zoom;
    layout(offset = 32) vec2 cell_offsets;
    layout(offset = 40) vec2 scroll_margin; 
    layout(offset = 48) vec2 anchor_cell;
    layout(offset = 56) vec2 top_right;
} pc;

void main() {
    const uint QUAD_LINE = 1u << 0;
    const uint QUAD_OUTLINE = 1u << 1;
    const uint QUAD_GRID = 1u << 2;
    bool quad_grid = (in_flags & QUAD_GRID) != 0u;
    bool quad_line = (in_flags & QUAD_LINE) != 0u;

    vec2 corner_sign = vec2(
         (in_corner.x == 0u ? -1.0 : 1.0), // left -> -1, right -> +1
         (in_corner.y == 0u ? -1.0 : 1.0) // bottom -> -1, top -> +1
    );
    vec2 offsets = quad_grid ? pc.cell_offsets : vec2(0.0);
    offsets += (quad_line ? vec2(corner_sign * in_line_thickness * (1.0 / pc.zoom)) : vec2(0.0));
    vec2 ndc = vec2((in_pos.x + offsets.x - pc.bottom_left.x) * pc.cell_size.x * pc.zoom / pc.physical_size.x * 2.0 - 1.0, 
            1.0 - (in_pos.y + offsets.y - pc.bottom_left.y) * pc.cell_size.y * pc.zoom / pc.physical_size.y * 2.0);
    gl_Position = vec4(ndc, in_pos.z, 1.0);
    frag_color = in_color;
    frag_outline_thickness = in_outline_thickness * pc.cell_size.x;
    frag_outline_color = in_outline_color;
    frag_uv = in_corner;
    frag_span = in_span * pc.cell_size * pc.zoom;
}
