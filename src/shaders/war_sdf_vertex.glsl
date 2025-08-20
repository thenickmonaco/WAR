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

#version 450

layout(location = 0) in uvec2 in_cell;          // cell coordinate
layout(location = 1) in vec2  in_uv;            // 0..1 UV
layout(location = 2) in vec2  in_glyph_bearing;
layout(location = 3) in vec2  in_glyph_size;
layout(location = 4) in float in_thickness;
layout(location = 5) in float in_feather;
layout(location = 6) in vec4  in_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out float frag_thickness;
layout(location = 3) out float frag_feather;

layout(push_constant) uniform PushConstants {
    uvec2 bottom_left;
    vec2  physical_size;
    vec2  cell_size;
    float zoom;
    uint   _pad1;
    uvec2 cell_offsets;
    uvec2 scroll_margin; 
    uvec2 anchor_cell;
    uvec2 top_right;
    vec2  scale;
} pc;

void main() {
    vec2 cell_origin = vec2(in_cell - pc.bottom_left + pc.cell_offsets) * pc.cell_size;

    // Corner derived from UV (0 or 1, not actual texcoords)
    vec2 corner = vec2(in_uv.x > 0.5 ? 1.0 : 0.0,
                       in_uv.y < 0.5 ? 1.0 : 0.0);

    vec2 glyph_offset = in_glyph_bearing + corner * in_glyph_size * pc.scale;
    vec2 quad_pos = cell_origin + glyph_offset;

    vec2 anchor_pixel = vec2(pc.anchor_cell - pc.bottom_left + pc.cell_offsets) * pc.cell_size;
    vec2 zoomed       = (quad_pos - anchor_pixel) * pc.zoom + anchor_pixel;

    vec2 ndc = vec2(
        (zoomed.x / pc.physical_size.x) * 2.0 - 1.0,
        1.0 - (zoomed.y / pc.physical_size.y) * 2.0
    );

    gl_Position    = vec4(ndc, 0.0, 1.0);
    frag_uv        = in_uv;
    frag_color     = in_color;
    frag_thickness = in_thickness;
    frag_feather   = in_feather;
}
