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
// src/shaders/war_text_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(location = 2) in float frag_thickness;
layout(location = 3) in float frag_feather;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D sdf_sampler;

void main() {
    float distance = texture(sdf_sampler, frag_uv).r;

    float alpha = smoothstep(0.5 - frag_thickness - frag_feather,
                             0.5 - frag_thickness + frag_feather,
                             distance);

    out_color = vec4(frag_color.rgb, frag_color.a * alpha);

    if (alpha <= 0.01) {
        discard;
    }
}
