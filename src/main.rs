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
// src/main.rs
//=============================================================================

mod input;
mod lua;
mod render;

mod data;

use data::Vertex;
use std::ptr;
use wayland_sys::client::wayland_client_handle;

pub fn main() {
    let wl = wayland_client_handle();

    unsafe {
        let display = (wl.wl_display_connect)(ptr::null());
        if display.is_null() {
            eprintln!("Failed to connect to Wayland display");
            return;
        }
    }

    let (
        mut col,
        mut row,
        mut cursor_rects_vertices,
        mut cursor_rects_indices,
    ): (u32, u32, Vec<Vertex>, Vec<u16>) = (
        0,          // col
        0,          // row
        Vec::new(), // cursor_rects_vertices
        Vec::new(), // cursor_rects_indices
    );

    loop {
        input::tick(
            &mut col,
            &mut row,
            &mut cursor_rects_vertices,
            &mut cursor_rects_indices,
        );
        lua::tick();
        render::tick(&cursor_rects_vertices, &cursor_rects_indices);
    }
}
