// vimDAW - make music with vim motions
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

//=============================================================================
// src/wayland.zig
//=============================================================================

pub const wayland_display_object_id: u32 = 1;
pub const wayland_wl_registry_event_global: u16 = 0;
pub const wayland_shm_pool_event_format: u16 = 0;
pub const wayland_wl_buffer_event_release: u16 = 0;
pub const wayland_xdg_wm_base_event_ping: u16 = 0;
pub const wayland_xdg_toplevel_event_configure: u16 = 0;
pub const wayland_xdg_toplevel_event_close: u16 = 1;
pub const wayland_xdg_surface_event_configure: u16 = 0;
pub const wayland_wl_display_get_registry_opcode: u16 = 1;
pub const wayland_wl_registry_bind_opcode: u16 = 0;
pub const wayland_wl_compositor_create_surface_opcode: u16 = 0;
pub const wayland_xdg_wm_base_pong_opcode: u16 = 3;
pub const wayland_xdg_surface_ack_configure_opcode: u16 = 4;
pub const wayland_wl_shm_create_pool_opcode: u16 = 0;
pub const wayland_xdg_wm_base_get_xdg_surface_opcode: u16 = 2;
pub const wayland_wl_shm_pool_create_buffer_opcode: u16 = 0;
pub const wayland_wl_surface_attach_opcode: u16 = 1;
pub const wayland_xdg_surface_get_toplevel_opcode: u16 = 1;
pub const wayland_wl_surface_commit_opcode: u16 = 6;
pub const wayland_wl_display_error_event: u16 = 0;
pub const wayland_format_xrgb8888: u32 = 1;
pub const wayland_header_size: u32 = 8;
pub const color_channels: u32 = 4;

// modules

// libraries
const stdio = @cImport({
    @cInclude("stdio.h");
});

pub fn init() !void {
    _ = stdio.printf("hello world");
}
