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
use libc::{poll, pollfd, read, POLLIN}; // low-level syscalls
use std::{
    os::raw::{c_char, c_int, c_void},
    ptr,
};
use wayland_sys::client::wayland_client_handle;

#[repr(C)]
pub struct WaylandInterface {
    name: *const c_char,
    version: u32,
}

pub fn main() {
    let wl = wayland_client_handle();

    unsafe {
        // 1. Connect to the Wayland display
        let display = (wl.wl_display_connect)(ptr::null());
        if display.is_null() {
            eprintln!("Failed to connect to Wayland display");
            return;
        }

        // 2. Get the Wayland file descriptor
        let fd = (wl.wl_display_get_fd)(display);
        if fd < 0 {
            eprintln!("Invalid file descriptor from Wayland");
            (wl.wl_display_disconnect)(display);
            return;
        }

        // 3. Prepare to read events without dispatching them
        let ret = (wl.wl_display_prepare_read)(display);
        if ret < 0 {
            eprintln!("Prepare read failed");
            (wl.wl_display_cancel_read)(display); // must cancel on failure
            (wl.wl_display_disconnect)(display);
            return;
        }

        // 4. Poll the fd for readability
        let mut fds = [pollfd { fd, events: POLLIN, revents: 0 }];
        if poll(fds.as_mut_ptr(), 1, -1) < 0 {
            eprintln!("Poll failed");
            (wl.wl_display_cancel_read)(display);
            (wl.wl_display_disconnect)(display);
            return;
        }

        // 5. Read the event buffer into Wayland's internal queue
        if (wl.wl_display_read_events)(display) < 0 {
            eprintln!("Failed to read Wayland events");
            (wl.wl_display_disconnect)(display);
            return;
        }

        // 6. DO NOT call wl_display_dispatch_pending — no callbacks

        // 7. At this point, you'd normally parse the Wayland event buffer manually
        // NOTE: To read raw bytes from the socket directly yourself instead of using `read_events`,
        // you must skip wl_display_read_events and perform `read(fd, ...)` and decode the protocol.

        // Cleanup
        (wl.wl_display_disconnect)(display);
    }

    //let (
    //    mut col,
    //    mut row,
    //    mut cursor_rects_vertices,
    //    mut cursor_rects_indices,
    //): (u32, u32, Vec<Vertex>, Vec<u16>) = (
    //    0,          // col
    //    0,          // row
    //    Vec::new(), // cursor_rects_vertices
    //    Vec::new(), // cursor_rects_indices
    //);

    //loop {
    //    input::tick(
    //        &mut col,
    //        &mut row,
    //        &mut cursor_rects_vertices,
    //        &mut cursor_rects_indices,
    //    );
    //    lua::tick();
    //    render::tick(&cursor_rects_vertices, &cursor_rects_indices);
    //}
}
