//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Monaco
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
// src/h/war_drm.h
//-----------------------------------------------------------------------------

#ifndef WAR_DRM_H
#define WAR_DRM_H

#include "war_data.h"

war_drm_context war_drm_init();

void war_drm_present_dmabuf(war_drm_context* ctx,
                            int dmabuf_fd,
                            uint32_t width,
                            uint32_t height,
                            uint32_t format,
                            uint32_t stride);

#endif // WAR_DRM_H
