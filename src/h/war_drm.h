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
// src/h/war_drm.h
//-----------------------------------------------------------------------------

#ifndef WAR_DRM_H
#define WAR_DRM_H

#include "war_data.h"

WAR_DRMContext war_drm_init();

void war_drm_present_dmabuf(WAR_DRMContext* ctx,
                            int dmabuf_fd,
                            uint32_t width,
                            uint32_t height,
                            uint32_t format,
                            uint32_t stride);

#endif // WAR_DRM_H
