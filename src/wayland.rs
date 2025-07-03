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
// src/wayland.rs
//=============================================================================

static WAYLAND_DISPLAY_OBJECT_ID: u32 = 1;
static WAYLAND_WL_REGISTRY_EVENT_GLOBAL: u16 = 0;
static WAYLAND_SHM_POOL_EVENT_FORMAT: u16 = 0;
static WAYLAND_WL_BUFFER_EVENT_RELEASE: u16 = 0;
static WAYLAND_XDG_WM_BASE_EVENT_PING: u16 = 0;
static WAYLAND_XDG_TOPLEVEL_EVENT_CONFIGURE: u16 = 0;
static WAYLAND_XDG_TOPLEVEL_EVENT_CLOSE: u16 = 1;
static WAYLAND_XDG_SURFACE_EVENT_CONFIGURE: u16 = 0;
static WAYLAND_WL_DISPLAY_GET_REGISTRY_OPCODE: u16 = 1;
static WAYLAND_WL_REGISTRY_BIND_OPCODE: u16 = 0;
static WAYLAND_WL_COMPOSITOR_CREATE_SURFACE_OPCODE: u16 = 0;
static WAYLAND_XDG_WM_BASE_PONG_OPCODE: u16 = 3;
static WAYLAND_XDG_SURFACE_ACK_CONFIGURE_OPCODE: u16 = 4;
static WAYLAND_WL_SHM_CREATE_POOL_OPCODE: u16 = 0;
static WAYLAND_XDG_WM_BASE_GET_XDG_SURFACE_OPCODE: u16 = 2;
static WAYLAND_WL_SHM_POOL_CREATE_BUFFER_OPCODE: u16 = 0;
static WAYLAND_WL_SURFACE_ATTACH_OPCODE: u16 = 1;
static WAYLAND_XDG_SURFACE_GET_TOPLEVEL_OPCODE: u16 = 1;
static WAYLAND_WL_SURFACE_COMMIT_OPCODE: u16 = 6;
static WAYLAND_WL_DISPLAY_ERROR_EVENT: u16 = 0;
static WAYLAND_FORMAT_XRGB8888: u32 = 1;
static WAYLAND_HEADER_SIZE: u32 = 8;
static COLOR_CHANNELS: u32 = 4;

use crate::error::CallCarmack;
use libc::{fcntl, poll, pollfd, F_GETFL, F_SETFL, O_NONBLOCK, POLLIN, getenv};
use std::ffi::CString;
use std::{
    fs::File, io::Error, io::ErrorKind, io::Read, mem::ManuallyDrop,
    os::unix::io::FromRawFd, ptr, env
};

pub fn init() -> Result<(), CallCarmack> {
    const xdg_runtime_dir: *i8 = unsafe {getenv("XDG_RUNTIME_DIR"); };

    let xdg_runtime_dir_len: u64 = xdg_runtime_dir.len() as u64;

    let wayland_display = env::var("WAYLAND_DISPLAY")?;

    let mut socket_path_bytes: Vec<u8> = Vec::with_capacity(xdg_runtime_dir.len() + 1 + wayland_display.len());


    Ok(())
}
