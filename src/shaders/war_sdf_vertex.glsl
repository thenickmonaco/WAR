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

layout(location = 0) in uvec2 in_cell_pos;   // glyph quad position in cell units (like quad shader)
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_thickness;
layout(location = 3) in float in_feather;
layout(location = 4) in vec4 in_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out float frag_thickness;
layout(location = 3) out float frag_feather;

// Push constants matching quad shader (or adapted for SDF needs)
layout(push_constant) uniform PushConstants {
    layout(offset = 0) uvec2 bottom_left;       // cell coord of bottom-left visible cell
    layout(offset = 8) vec2 physical_size;      // window or framebuffer size in pixels
    layout(offset = 16) vec2 cell_size;         // size of one cell in pixels
    layout(offset = 24) float zoom;              // zoom scale
    layout(offset = 28) uint _pad1;              // padding for alignment
    layout(offset = 32) uvec2 cell_offsets;     // scroll offsets in cells
    layout(offset = 40) uvec2 scroll_margin;     // maybe unused, keep for compatibility
    layout(offset = 48) uvec2 anchor_cell;       // anchor cell for zoom/pan
    layout(offset = 56) uvec2 top_right;         // top-right cell coordinate (optional)
    // 64 bytes
} pc;

void main() {
    // Calculate local cell position with offsets
    uvec2 local_cell = (in_cell_pos - pc.bottom_left) + pc.cell_offsets;

    // Pixel coordinates of this glyph quad
    vec2 pixel_pos = vec2(local_cell) * pc.cell_size;

    // Anchor pixel for zoom/pan center
    vec2 anchor_pixel = vec2(pc.anchor_cell - pc.bottom_left + pc.cell_offsets) * pc.cell_size;

    // Apply zoom around anchor pixel and pan if needed (adjust if you have a pan vector instead)
    vec2 zoomed = ((pixel_pos - anchor_pixel) * pc.zoom) + anchor_pixel;

    // Convert to NDC (-1 to 1 range)
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
