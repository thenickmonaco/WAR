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
// src/h/war_wayland.h
//-----------------------------------------------------------------------------

#ifndef WAR_WAYLAND_H
#define WAR_WAYLAND_H

#include "h/war_data.h"

void war_wayland_init();

void war_wayland_registry_bind(
    int fd, uint8_t* buffer, size_t offset, uint16_t size, uint16_t new_id);

int war_wayland_make_fd(void);

void war_wayland_wl_surface_attach(int fd,
                                   uint32_t wl_surface_id,
                                   uint32_t wl_buffer_id,
                                   uint32_t x,
                                   uint32_t y);

void war_wayland_wl_surface_damage(int fd,
                                   uint32_t wl_surface_id,
                                   uint32_t x,
                                   uint32_t y,
                                   uint32_t width,
                                   uint32_t height);

void war_wayland_wl_surface_commit(int fd, uint32_t wl_surface_id);

void war_wayland_wl_surface_frame(int fd,
                                  uint32_t wl_surface_id,
                                  uint32_t new_id);

void war_wayland_holy_trinity(int fd,
                              uint32_t wl_surface_id,
                              uint32_t wl_buffer_id,
                              uint32_t attach_x,
                              uint32_t attach_y,
                              uint32_t damage_x,
                              uint32_t damage_y,
                              uint32_t width,
                              uint32_t height);

#endif // WAR_WAYLAND_H
