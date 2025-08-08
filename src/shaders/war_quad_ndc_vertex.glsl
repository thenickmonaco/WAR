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
// src/shaders/war_quad_ndc_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 color;

// Add push constants or uniform for zoom and pan
layout(push_constant) uniform PushConstants {
    layout(offset = 0) float zoom;          // 4 bytes
    layout(offset = 8) vec2 pan;            // 8-byte aligned, starts at 8
    layout(offset = 16) float padding;      // optional, but avoids spillover
} pc;

void main() {
    // Apply pan and zoom to the input position before passing to gl_Position
    vec2 zoomed = in_pos * pc.zoom;
    vec2 translated = zoomed + vec2(pc.pan.x, pc.pan.y);

    gl_Position = vec4(translated, 0.0, 1.0);
    color = in_color;
}
