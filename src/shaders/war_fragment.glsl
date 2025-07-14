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
// src/shaders/war_fragment.glsl
//-----------------------------------------------------------------------------

#version 450
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;

layout(binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 tex_color = texture(tex_sampler, uv);
    // Combine texture color with vertex color (modulate)
    out_color = tex_color * color;
}

