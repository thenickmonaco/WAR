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

use libc::{poll, pollfd, POLLIN};
use std::io::Read;
use std::{fs::File, os::unix::io::FromRawFd, ptr};
use wayland_sys::client::wayland_client_handle;

pub fn main() {
    #[cfg(debug_assertions)]
    println!("\n# [src/main.rs] Wayland Initialization\n");

    let wl = wayland_client_handle();

    unsafe {
        let display = (wl.wl_display_connect)(ptr::null());
        if display.is_null() {
            eprintln!("ERROR: Failed to connect to Wayland display");
            return;
        }

        let fd = (wl.wl_display_get_fd)(display);
        if fd < 0 {
            eprintln!("ERROR: Invalid file descriptor from Wayland");
            (wl.wl_display_disconnect)(display);
            return;
        }

        let mut next_id: u32 = 2;
        let registry_id = next_id;
        next_id += 1;

        // Send get_registry message
        let mut msg = Vec::<u8>::new();
        msg.extend(&1u32.to_le_bytes()); // sender_id = 1 (display)
        msg.extend(&1u16.to_le_bytes()); // opcode = 1 (get_registry), order of requests in wayland repo: protocol/wayland.xml
        msg.extend(&12u16.to_le_bytes()); // message size
        msg.extend(&registry_id.to_le_bytes()); // new object id

        let written = libc::write(fd, msg.as_ptr() as *const _, msg.len());

        #[cfg(debug_assertions)]
        {
            println!("- [src/main.rs]");
            println!("    - written get_registry message size: {:?}", written);
            println!("    - message contents: {:?}", msg);
        }

        if written < 0 || (wl.wl_display_flush)(display) < 0 {
            eprintln!("ERROR: write/flush failed");
            (wl.wl_display_disconnect)(display);
            return;
        }

        let mut file = File::from_raw_fd(fd);
        let mut fds = [pollfd { fd, events: POLLIN, revents: 0 }];

        let mut buffer = Vec::<u8>::new();
        let mut temp_buf = [0u8; 4096];

        loop {
            #[cfg(debug_assertions)]
            {
                println!("- [src/main.rs]");
                println!("    - start loop");
            }

            let poll_result = poll(fds.as_mut_ptr(), 1, -1);
            if poll_result < 0 {
                eprintln!("poll() failed");
                break;
            }

            if fds[0].revents & POLLIN != 0 {
                let bytes_read = match file.read(&mut temp_buf) {
                    Ok(0) => {
                        // EOF
                        eprintln!("EOF reached");
                        break;
                    },
                    Ok(n) => n,
                    Err(e) => {
                        eprintln!("Read error: {}", e);
                        break;
                    },
                };

                buffer.extend_from_slice(&temp_buf[..bytes_read]);

                while buffer.len() >= 8 {
                    let size =
                        u16::from_le_bytes(buffer[6..8].try_into().unwrap())
                            as usize;

                    if size < 8 {
                        eprintln!("Invalid message size: {}", size);
                        // Drop invalid data to avoid infinite loop
                        buffer.drain(..8);
                        continue;
                    }

                    if buffer.len() < size {
                        break;
                    }

                    let message_bytes: Vec<u8> =
                        buffer.drain(..size).collect();

                    let obj_id = u32::from_le_bytes(
                        message_bytes[0..4].try_into().unwrap(),
                    );
                    let opcode = u16::from_le_bytes(
                        message_bytes[4..6].try_into().unwrap(),
                    );
                    let _msg_size = u16::from_le_bytes(
                        message_bytes[6..8].try_into().unwrap(),
                    );

                    let body = &message_bytes[8..];

                    #[cfg(debug_assertions)]
                    {
                        println!("- [src/main.rs]");
                        println!("    - obj_id: {}", obj_id);
                        println!("    - opcode: {}", opcode);
                        println!("    - size: {}", _msg_size);
                        println!("    - body bytes: {:?}", body);
                        println!(
                            "Raw body (hex): [{}]",
                            body.iter()
                                .map(|b| format!("{:02X}", b))
                                .collect::<Vec<_>>()
                                .join(" ")
                        );
                    }
                }
            }
        }

        (wl.wl_display_disconnect)(display);
    }
}
