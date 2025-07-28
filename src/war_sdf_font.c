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
// src/h/war_sdf_font.c
//-----------------------------------------------------------------------------

#include "h/war_data.h"
#include "h/war_debug_macros.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

war_sdf_font_context war_sdf_font_init(war_vulkan_context* vulkan_context) {
    war_sdf_font_context sdf_font_context = {};

    assert(FT_Init_FreeType(&sdf_font_context.ft_library));
    assert(FT_New_Face(sdf_font_context.ft_library,
                       "assets/fonts/FreeMono.otf",
                       0,
                       &sdf_font_context.ft_regular));
    assert(FT_New_Face(sdf_font_context.ft_library,
                       "assets/fonts/FreeMonoBold.otf",
                       0,
                       &sdf_font_context.ft_bold));

    // FT_Set_Pixel_Sizes(ft_face, 0, 24);
    // FT_Set_Pixel_Sizes(ft_bold_face, 0, 24);
    //  assert(FT_Load_Char(ft_face, 'A', FT_LOAD_RENDER));
    //  assert(FT_Load_Char(ft_bold_face, 'A', FT_LOAD_RENDER));

    sdf_font_context.pixel_height = 24;
    FT_Set_Pixel_Sizes(
        sdf_font_context.ft_regular, 0, sdf_font_context.pixel_height);
    FT_Set_Pixel_Sizes(
        sdf_font_context.ft_bold, 0, sdf_font_context.pixel_height);

    // Create a bitmap atlas buffer big enough for all glyphs
    uint8_t atlas[512 * 512] = {0};

    // For each glyph (e.g. 32 to 127):
    for (int c = 32; c < 128; ++c) {
        FT_Load_Char(sdf_font_context.ft_regular, c, FT_LOAD_RENDER);
        // Access bitmap: ctx.ft_regular->glyph->bitmap
        // Generate SDF from bitmap (CPU)
        // Copy SDF glyph into atlas at allocated position
        // Store glyph metrics and atlas UV coords into ctx.glyphs[c]
    }

    // Create Vulkan VkImage for atlas and upload pixels

    // Create VkImageView and VkSampler

    // Allocate and update descriptor set for this texture

    // Store Vulkan handles in ctx (sdf_image, sdf_image_view, sdf_sampler,
    // descriptor_set)

    return sdf_font_context;
}
