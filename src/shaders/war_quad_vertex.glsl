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

layout(location = 0) in vec2 in_pos;   // in pixels or cell units
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 color;

// Push constants for zoom, pan, and viewport size in pixels
layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec2 pan;
    layout(offset = 8) vec2 viewport_size;
    layout(offset = 16) vec2 cell_size;
    layout(offset = 24) float zoom;
    layout(offset = 28) int _pad1;
    layout(offset = 32) ivec2 cell_offsets;
} pc;

void main() {
    // convert from cell coordinates to pixels
    vec2 pixel_pos = in_pos * pc.cell_size; 

    // apply cell-based offsets (convert offsets to pixels, then add)
    pixel_pos = pixel_pos + (vec2(pc.cell_offsets) * pc.cell_size);

    // Apply zoom and pan in pixel/cell space
    vec2 zoomed = pixel_pos * pc.zoom;
    vec2 translated = zoomed + pc.pan;

    // Convert from pixel coords to normalized device coords (NDC)
    // Vulkan NDC x: -1 (left) to 1 (right)
    // Vulkan NDC y: -1 (bottom) to 1 (top)
    vec2 ndc = vec2(
        (translated.x) / pc.viewport_size.x * 2.0 - 1.0,
        1.0 - (translated.y) * 2.0 / pc.viewport_size.y
    );

    gl_Position = vec4(ndc, 0.0, 1.0);
    color = in_color;
}
