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

layout(location = 0) in vec2 in_pos;   // quad corner position
layout(location = 1) in vec2 in_uv;    // SDF texture UV coords
layout(location = 2) in vec4 in_color; // per-vertex color
layout(location = 3) in float in_thickness; // per-vertex thickness
layout(location = 4) in float in_feather;   // per-vertex feather

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out float frag_thickness;
layout(location = 3) out float frag_feather;

// Push constants for zoom/pan/padding with explicit offsets
layout(push_constant) uniform PushConstants {
    layout(offset = 0) float zoom;      // 4 bytes
    layout(offset = 8) vec2 pan;        // 8-byte aligned, starts at 8
    layout(offset = 16) float padding;  // optional padding to 24 bytes total
} pc;

void main() {
    // Apply zoom and pan
    vec2 zoomed = in_pos * pc.zoom;
    vec2 translated = zoomed + pc.pan;

    gl_Position = vec4(translated, 0.0, 1.0);

    frag_uv = in_uv;
    frag_color = in_color;
    frag_thickness = in_thickness;
    frag_feather = in_feather;
}
