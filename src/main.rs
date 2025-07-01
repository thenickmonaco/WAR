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
use std::io::{Error, ErrorKind, Read};
use std::{fs::File, mem::ManuallyDrop, os::unix::io::FromRawFd, ptr};
use wayland_sys::client::{wayland_client_handle, wl_display, WaylandClient};

pub unsafe fn write_all(fd: i32, mut buf: &[u8]) -> Result<(), Error> {
    while !buf.is_empty() {
        let written = libc::write(fd, buf.as_ptr() as *const _, buf.len());
        if written < 0 {
            let err = *libc::__errno_location();
            if err == libc::EPIPE {
                return Err(Error::new(
                    ErrorKind::BrokenPipe,
                    "connection closed",
                ));
            } else {
                return Err(Error::last_os_error());
            }
        } else {
            let written = written as usize;
            buf = &buf[written..];
        }
    }
    Ok(())
}

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
            //println!("- [src/main.rs]");
            //println!("    - written get_registry message size: {:?}", written);
            //println!("    - message contents: {:?}", msg);
        }

        if written < 0 || (wl.wl_display_flush)(display) < 0 {
            eprintln!("ERROR: write/flush failed");
            (wl.wl_display_disconnect)(display);
            return;
        }

        let mut file = ManuallyDrop::new(File::from_raw_fd(fd));
        let mut fds = [pollfd { fd, events: POLLIN, revents: 0 }];

        let mut buffer = Vec::<u8>::new();
        let mut temp_buf = [0u8; 4096];

        let (mut ids, mut interfaces, mut interfaces_start_len, mut versions) = (
            Vec::<u32>::new(),
            Vec::<u8>::new(),
            Vec::<[u16; 2]>::new(),
            Vec::<u32>::new(),
        );

        const WL_COMPOSITOR: &[u8] = b"wl_compositor\0";
        const WL_SEAT: &[u8] = b"wl_seat\0";
        const WL_SHM: &[u8] = b"wl_shm\0";
        const XDG_WM_BASE: &[u8] = b"xdg_wm_base\0";
        #[cfg(features = "zwp_linux_dmabuf_v1")]
        {
            const ZWP_LINUX_DMABUF_V1: &[u8] = b"zwp_linux_dmabuf_v1\0";
            let mut found = [false; 5]; // 5 total interfaces
        }
        let mut found = [false; 4]; // 4 total interfaces
        let mut message_pending = false;
        let mut bind_messages = Vec::<Vec<u8>>::new();
        let mut interface_index = 0;

        loop {
            #[cfg(debug_assertions)]
            {
                //println!("- [src/main.rs]");
                //println!("    - waiting for poll_result");
            }

            let poll_result = poll(fds.as_mut_ptr(), 1, -1);
            if poll_result < 0 {
                eprintln!("poll() failed");
                break;
            }

            if fds[0].revents & POLLIN != 0 {
                // handle partial reads
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

                    // keep polling till message is completed
                    if buffer.len() < size {
                        message_pending = true;
                        break;
                    }
                    message_pending = false;

                    let message_bytes: Vec<u8> =
                        buffer.drain(..size).collect();

                    let object_id = u32::from_le_bytes(
                        message_bytes[0..4].try_into().unwrap(),
                    );
                    let opcode = u16::from_le_bytes(
                        message_bytes[4..6].try_into().unwrap(),
                    );
                    let _msg_size = u16::from_le_bytes(
                        message_bytes[6..8].try_into().unwrap(),
                    );

                    let body = &message_bytes[8..];

                    let global_name =
                        u32::from_le_bytes(body[0..4].try_into().unwrap());
                    let interface_len =
                        u32::from_le_bytes(body[4..8].try_into().unwrap())
                            as usize;
                    let interface = &body[8..8 + interface_len];
                    let padded_len = (interface_len + 3) & !3;
                    let version = u32::from_le_bytes(
                        body[8 + padded_len..8 + padded_len + 4]
                            .try_into()
                            .unwrap(),
                    );

                    #[cfg(debug_assertions)]
                    {
                        //println!("- [src/main.rs]");
                        //println!("    - object_id: {:?}", object_id);
                        //println!("    - global_name: {:?}", global_name);
                        //println!(
                        //    "    - interface: {:?}",
                        //    std::str::from_utf8(interface)
                        //);
                        //println!("    - version: {:?}", version);
                    }

                    match interface {
                        WL_COMPOSITOR => {
                            found[interface_index] = true;
                            interface_index += 1;
                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!("    - found WL_COMPOSITOR");
                                println!("    - object_id: {:?}", object_id);
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!("    - interface: {:?}", interface);
                                println!("    - version: {:?}", version);
                            }

                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind

                            let mut interface_bytes = interface.to_vec();
                            if !interface_bytes.ends_with(&[0]) {
                                interface_bytes.push(0); // Ensure null-termination
                            }
                            let padded_len = (interface_bytes.len() + 3) & !3;

                            // Now safe to calculate message size
                            let size = 8 + 4 + padded_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Temporarily fill header with zeroes; we’ll patch later if needed
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes()); // Now correct

                            msg.extend_from_slice(&global_name.to_le_bytes());
                            msg.extend_from_slice(&interface_bytes);
                            msg.resize(
                                msg.len()
                                    + (padded_len - interface_bytes.len()),
                                0,
                            ); // pad with 0

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            bind_messages.push(msg);

                            // Try to write all bytes
                            //write_all(fd, &msg).unwrap();

                            //// Flush the display buffer
                            //if (wl.wl_display_flush)(display) < 0 {
                            //    break;
                            //}

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                //println!(
                                //    "    - bind request message: {:?}",
                                //    msg
                                //);
                            }
                        },
                        WL_SEAT => {
                            found[interface_index] = true;
                            interface_index += 1;
                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!("    - found WL_SEAT");
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!("    - interface: {:?}", interface);
                                println!("    - version: {:?}", version);
                            }

                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind

                            let mut interface_bytes = interface.to_vec();
                            if !interface_bytes.ends_with(&[0]) {
                                interface_bytes.push(0); // Ensure null-termination
                            }
                            let padded_len = (interface_bytes.len() + 3) & !3;

                            // Now safe to calculate message size
                            let size = 8 + 4 + padded_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Temporarily fill header with zeroes; we’ll patch later if needed
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes()); // Now correct

                            msg.extend_from_slice(&global_name.to_le_bytes());
                            msg.extend_from_slice(&interface_bytes);
                            msg.resize(
                                msg.len()
                                    + (padded_len - interface_bytes.len()),
                                0,
                            ); // pad with 0

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            bind_messages.push(msg);

                            // Try to write all bytes
                            //write_all(fd, &msg).unwrap();

                            //// Flush the display buffer
                            //if (wl.wl_display_flush)(display) < 0 {
                            //    break;
                            //}

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                //println!(
                                //    "    - bind request message: {:?}",
                                //    msg
                                //);
                            }
                        },
                        WL_SHM => {
                            found[interface_index] = true;
                            interface_index += 1;
                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!("    - found WL_SHM");
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!("    - interface: {:?}", interface);
                                println!("    - version: {:?}", version);
                            }

                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind

                            let mut interface_bytes = interface.to_vec();
                            if !interface_bytes.ends_with(&[0]) {
                                interface_bytes.push(0); // Ensure null-termination
                            }
                            let padded_len = (interface_bytes.len() + 3) & !3;

                            // Now safe to calculate message size
                            let size = 8 + 4 + padded_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Temporarily fill header with zeroes; we’ll patch later if needed
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes()); // Now correct

                            msg.extend_from_slice(&global_name.to_le_bytes());
                            msg.extend_from_slice(&interface_bytes);
                            msg.resize(
                                msg.len()
                                    + (padded_len - interface_bytes.len()),
                                0,
                            ); // pad with 0

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            bind_messages.push(msg);

                            // Try to write all bytes
                            //write_all(fd, &msg).unwrap();

                            //// Flush the display buffer
                            //if (wl.wl_display_flush)(display) < 0 {
                            //    break;
                            //}

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                //println!(
                                //    "    - bind request message: {:?}",
                                //    msg
                                //);
                            }
                        },
                        XDG_WM_BASE => {
                            found[interface_index] = true;
                            interface_index += 1;
                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!("    - found XDG_WM_BASE");
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!("    - interface: {:?}", interface);
                                println!("    - version: {:?}", version);
                            }

                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind

                            let mut interface_bytes = interface.to_vec();
                            if !interface_bytes.ends_with(&[0]) {
                                interface_bytes.push(0); // Ensure null-termination
                            }
                            let padded_len = (interface_bytes.len() + 3) & !3;

                            // Now safe to calculate message size
                            let size = 8 + 4 + padded_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Temporarily fill header with zeroes; we’ll patch later if needed
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes()); // Now correct

                            msg.extend_from_slice(&global_name.to_le_bytes());
                            msg.extend_from_slice(&interface_bytes);
                            msg.resize(
                                msg.len()
                                    + (padded_len - interface_bytes.len()),
                                0,
                            ); // pad with 0

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            bind_messages.push(msg);

                            // Try to write all bytes
                            //write_all(fd, &msg).unwrap();

                            //// Flush the display buffer
                            //if (wl.wl_display_flush)(display) < 0 {
                            //    break;
                            //}

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                //println!(
                                //    "    - bind request message: {:?}",
                                //    msg
                                //);
                            }
                        },
                        #[cfg(features = "zwp_linux_dmabuf_v1")]
                        ZWP_LINUX_DMABUF_V1 => {
                            found[interface_index] = true;
                            interface_index += 1;
                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!("    - found ZWP_LINUX_DMABUF_V1");
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!(
                                    "    - global_name: {:?}",
                                    global_name
                                );
                                println!("    - interface: {:?}", interface);
                                println!("    - version: {:?}", version);
                            }

                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind

                            let mut interface_bytes = interface.to_vec();
                            if !interface_bytes.ends_with(&[0]) {
                                interface_bytes.push(0); // Ensure null-termination
                            }
                            let padded_len = (interface_bytes.len() + 3) & !3;

                            // Now safe to calculate message size
                            let size = 8 + 4 + padded_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Temporarily fill header with zeroes; we’ll patch later if needed
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes()); // Now correct

                            msg.extend_from_slice(&global_name.to_le_bytes());
                            msg.extend_from_slice(&interface_bytes);
                            msg.resize(
                                msg.len()
                                    + (padded_len - interface_bytes.len()),
                                0,
                            ); // pad with 0

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            bind_messages.push(msg);

                            // Try to write all bytes
                            //write_all(fd, &msg).unwrap();

                            //// Flush the display buffer
                            //if (wl.wl_display_flush)(display) < 0 {
                            //    break;
                            //}

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                //println!(
                                //    "    - bind request message: {:?}",
                                //    msg
                                //);
                            }
                        },
                        _ => {},
                    }
                }
                // After the match, check if all are found:
                if found.iter().all(|&f| f) && !message_pending {
                    #[cfg(debug_assertions)]
                    {
                        println!("- [src/main.rs]");
                        println!("    - found all, will attempt binds...");
                    }

                    break; // exit the loop once all found
                }
            }
        }
        // finally free

        for msg in &bind_messages {
            #[cfg(debug_assertions)]
            {
                // Validate message size
                let msg_len = msg.len();
                let expected_size =
                    u16::from_le_bytes([msg[6], msg[7]]) as usize;

                let interface_bytes = &msg[12..msg_len - 8];
                let interface_str = std::str::from_utf8(interface_bytes)
                    .unwrap_or("<invalid utf8>")
                    .trim_end_matches('\0');

                println!("- [src/main.rs]");
                println!(
                    "    - {:?} bind: size={} (header says {}), send_id={}, opcode={}",
                    interface_str,
                    msg_len,
                    expected_size,
                    u32::from_le_bytes([msg[0], msg[1], msg[2], msg[3]]),
                    u16::from_le_bytes([msg[4], msg[5]])
                );

                if msg_len != expected_size {
                    println!("    - ❌ size mismatch!");
                }

                println!(
                    "    - global_name={}, new_id={}",
                    u32::from_le_bytes([msg[8], msg[9], msg[10], msg[11]]),
                    u32::from_le_bytes([
                        msg[msg.len() - 4],
                        msg[msg.len() - 3],
                        msg[msg.len() - 2],
                        msg[msg.len() - 1]
                    ])
                );
            }

            write_all(fd, &msg).unwrap();

            if (wl.wl_display_flush)(display) < 0 {
                break;
            }

        }

        (wl.wl_display_disconnect)(display);
    }
}
