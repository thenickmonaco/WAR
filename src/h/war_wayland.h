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

war_key_trie_node* war_insert_trie_node(war_key_trie_pool* pool,
                                        war_key_event* key_seq,
                                        size_t key_seq_size,
                                        void* cmd);

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
