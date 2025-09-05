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
// src/shaders/war_quad_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 1) in float frag_outline_thickness;
layout(location = 2) in vec4 frag_outline_color;
layout(location = 3) in vec2 frag_local_pos;
layout(location = 4) in vec2 frag_quad_size;
layout(location = 5) in vec2 frag_corner_sign;
layout(location = 6) in vec2 frag_pc_cell_size;

layout(location = 0) out vec4 out_color;

void main() {
    float min_dist_px = min(min(frag_local_pos.x, frag_quad_size.x - frag_local_pos.x),
                        min(frag_local_pos.y, frag_quad_size.y - frag_local_pos.y));
    if (min_dist_px < frag_outline_thickness) {
        out_color = frag_outline_color;
    } else {
        out_color = frag_color;
    }
}
