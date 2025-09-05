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

layout(location = 0) out vec4 out_color;

void main() {
    if (frag_outline_thickness <= 0.0) {
        out_color = frag_color;
        return;
    }

    // Distance to nearest edge (0..1)
    float minDist = min(min(frag_local_pos.x, 1.0 - frag_local_pos.x),
                        min(frag_local_pos.y, 1.0 - frag_local_pos.y));

    // Normalize outline thickness to 0..1 quad coordinates
    // (Assume outline_thickness in pixels; scale to fraction of quad width/height)
    float thicknessX = frag_outline_thickness; // For now, assume quad width = 1
    float thicknessY = frag_outline_thickness; // For now, assume quad height = 1
    float thickness = min(thicknessX, thicknessY); // conservative

    if (minDist < thickness) {
        out_color = frag_outline_color;
    } else {
        out_color = frag_color;
    }
}
