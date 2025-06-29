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
use std::fs::File;
use std::io::{self, Read, Write};
use std::os::unix::io::FromRawFd;
use std::os::unix::io::RawFd;
use std::{
    os::raw::{c_char, c_int, c_void},
    ptr,
};
use wayland_sys::client::wayland_client_handle;

pub fn read_wayland_message(fd: RawFd) -> io::Result<()> {
    // First read the 8-byte header (object_id u32 + opcode u16 + size u16)
    let mut header = [0u8; 8];
    read_exact_bytes(fd, &mut header)?;

    // Parse header fields (assuming little endian)
    let object_id = u32::from_le_bytes(header[0..4].try_into().unwrap());
    let opcode = u16::from_le_bytes(header[4..6].try_into().unwrap());
    let size = u16::from_le_bytes(header[6..8].try_into().unwrap());

    println!("object_id={} opcode={} size={}", object_id, opcode, size);

    // size includes the header size (8 bytes), so subtract that for arg bytes
    let arg_size = size as usize - 8;
    let mut args_buf = vec![0u8; arg_size];
    read_exact_bytes(fd, &mut args_buf)?;

    // Now args_buf contains the raw arguments you need to decode

    Ok(())
}

pub fn read_exact_bytes(fd: RawFd, buf: &mut [u8]) -> io::Result<()> {
    let mut file = unsafe { File::from_raw_fd(fd) };
    let result = file.read_exact(buf);
    // Prevent closing fd when file goes out of scope:
    std::mem::forget(file);
    result
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

        loop {
            println!("start of loop");

            let mut fds = [pollfd { fd, events: POLLIN, revents: 0 }];
            let poll_ret = poll(fds.as_mut_ptr(), 1, -1);
            if poll_ret > 0 {
                println!("message: ");
                read_wayland_message(fd).unwrap();
            } else {
                break;
            }
        }

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
