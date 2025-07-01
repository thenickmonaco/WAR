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

pub unsafe fn request_bind(
    fd: i32,
    wl: &WaylandClient,
    display: *mut wl_display,
    global_name: u32,
    interface_len: usize,
    interface: &[u8],
    version: u32,
    new_id: u32,
) -> Result<(), std::io::Error> {
    let send_id = 2u32; // registry id
    let opcode = 0u16; // bind opcode

    let padded_interface_len = (interface_len + 3) & !3;
    let size = 8 + 4 + padded_interface_len + 4 + 4;
    let size_u16 = size as u16;

    let mut msg = Vec::<u8>::with_capacity(size);

    // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
    msg.extend_from_slice(&send_id.to_le_bytes());
    msg.extend_from_slice(&opcode.to_le_bytes());
    msg.extend_from_slice(&size_u16.to_le_bytes());

    // Body:
    // global_name (4 bytes LE)
    msg.extend_from_slice(&global_name.to_le_bytes());

    let mut interface_bytes = interface.to_vec();
    interface_bytes.push(0); // Null-terminate
    let interface_len = interface_bytes.len();
    let padded_len = (interface_len + 3) & !3;
    msg.extend_from_slice(&interface_bytes);
    // Pad with zeroes to 4-byte alignment
    for _ in interface_len..padded_len {
        msg.push(0);
    }

    msg.extend_from_slice(&version.to_le_bytes());
    msg.extend_from_slice(&new_id.to_le_bytes());

    // Try to write all bytes
    unsafe { write_all(fd, &msg)? };

    // Flush the display buffer
    if (wl.wl_display_flush)(display) < 0 {
        return Err(std::io::Error::new(
            std::io::ErrorKind::BrokenPipe,
            "wl_display_flush failed",
        ));
    }

    #[cfg(debug_assertions)]
    {
        println!("- [src/main.rs]");
        println!("    - bind request message: {:?}", msg);
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
                        break;
                    }

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
                            found[0] = true;
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
                            // global name, interface, version
                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind opcode

                            let padded_interface_len =
                                (interface_len + 3) & !3;
                            let size = 8 + 4 + padded_interface_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes());

                            // Body:
                            // global_name (4 bytes LE)
                            msg.extend_from_slice(&global_name.to_le_bytes());

                            let mut interface_bytes = interface.to_vec();
                            interface_bytes.push(0); // Null-terminate
                            let interface_len = interface_bytes.len();
                            let padded_len = (interface_len + 3) & !3;
                            msg.extend_from_slice(&interface_bytes);
                            // Pad with zeroes to 4-byte alignment
                            for _ in interface_len..padded_len {
                                msg.push(0);
                            }

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            // Try to write all bytes
                            write_all(fd, &msg).unwrap();

                            // Flush the display buffer
                            if (wl.wl_display_flush)(display) < 0 {
                                break;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                println!(
                                    "    - bind request message: {:?}",
                                    msg
                                );
                            }
                        },
                        WL_SEAT => {
                            found[1] = true;
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
                            // global name, interface, version
                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind opcode

                            let padded_interface_len =
                                (interface_len + 3) & !3;
                            let size = 8 + 4 + padded_interface_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes());

                            // Body:
                            // global_name (4 bytes LE)
                            msg.extend_from_slice(&global_name.to_le_bytes());

                            let mut interface_bytes = interface.to_vec();
                            interface_bytes.push(0); // Null-terminate
                            let interface_len = interface_bytes.len();
                            let padded_len = (interface_len + 3) & !3;
                            msg.extend_from_slice(&interface_bytes);
                            // Pad with zeroes to 4-byte alignment
                            for _ in interface_len..padded_len {
                                msg.push(0);
                            }

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            // Try to write all bytes
                            write_all(fd, &msg).unwrap();

                            // Flush the display buffer
                            if (wl.wl_display_flush)(display) < 0 {
                                break;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                println!(
                                    "    - bind request message: {:?}",
                                    msg
                                );
                            }
                        },
                        WL_SHM => {
                            found[2] = true;
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

                            // global name, interface, version
                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind opcode

                            let padded_interface_len =
                                (interface_len + 3) & !3;
                            let size = 8 + 4 + padded_interface_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes());

                            // Body:
                            // global_name (4 bytes LE)
                            msg.extend_from_slice(&global_name.to_le_bytes());

                            let mut interface_bytes = interface.to_vec();
                            interface_bytes.push(0); // Null-terminate
                            let interface_len = interface_bytes.len();
                            let padded_len = (interface_len + 3) & !3;
                            msg.extend_from_slice(&interface_bytes);
                            // Pad with zeroes to 4-byte alignment
                            for _ in interface_len..padded_len {
                                msg.push(0);
                            }

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            // Try to write all bytes
                            write_all(fd, &msg).unwrap();

                            // Flush the display buffer
                            if (wl.wl_display_flush)(display) < 0 {
                                break;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                println!(
                                    "    - bind request message: {:?}",
                                    msg
                                );
                            }
                        },
                        XDG_WM_BASE => {
                            found[3] = true;
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

                            // global name, interface, version
                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind opcode

                            let padded_interface_len =
                                (interface_len + 3) & !3;
                            let size = 8 + 4 + padded_interface_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes());

                            // Body:
                            // global_name (4 bytes LE)
                            msg.extend_from_slice(&global_name.to_le_bytes());

                            let mut interface_bytes = interface.to_vec();
                            interface_bytes.push(0); // Null-terminate
                            let interface_len = interface_bytes.len();
                            let padded_len = (interface_len + 3) & !3;
                            msg.extend_from_slice(&interface_bytes);
                            // Pad with zeroes to 4-byte alignment
                            for _ in interface_len..padded_len {
                                msg.push(0);
                            }

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            // Try to write all bytes
                            write_all(fd, &msg).unwrap();

                            // Flush the display buffer
                            if (wl.wl_display_flush)(display) < 0 {
                                break;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                println!(
                                    "    - bind request message: {:?}",
                                    msg
                                );
                            }
                        },
                        #[cfg(features = "zwp_linux_dmabuf_v1")]
                        ZWP_LINUX_DMABUF_V1 => {
                            found[4] = true;
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
                            // global name, interface, version
                            let send_id = 2u32; // registry id
                            let opcode = 0u16; // bind opcode

                            let padded_interface_len =
                                (interface_len + 3) & !3;
                            let size = 8 + 4 + padded_interface_len + 4 + 4;
                            let size_u16 = size as u16;

                            let new_id = next_id;
                            next_id += 1;

                            let mut msg = Vec::<u8>::with_capacity(size);

                            // Header: send_id (4 bytes LE), opcode (2 bytes LE), size (2 bytes LE)
                            msg.extend_from_slice(&send_id.to_le_bytes());
                            msg.extend_from_slice(&opcode.to_le_bytes());
                            msg.extend_from_slice(&size_u16.to_le_bytes());

                            // Body:
                            // global_name (4 bytes LE)
                            msg.extend_from_slice(&global_name.to_le_bytes());

                            let mut interface_bytes = interface.to_vec();
                            interface_bytes.push(0); // Null-terminate
                            let interface_len = interface_bytes.len();
                            let padded_len = (interface_len + 3) & !3;
                            msg.extend_from_slice(&interface_bytes);
                            // Pad with zeroes to 4-byte alignment
                            for _ in interface_len..padded_len {
                                msg.push(0);
                            }

                            msg.extend_from_slice(&version.to_le_bytes());
                            msg.extend_from_slice(&new_id.to_le_bytes());

                            // Try to write all bytes
                            write_all(fd, &msg).unwrap();

                            // Flush the display buffer
                            if (wl.wl_display_flush)(display) < 0 {
                                break;
                            }

                            #[cfg(debug_assertions)]
                            {
                                println!("- [src/main.rs]");
                                println!(
                                    "    - sending to {:?}...",
                                    std::str::from_utf8(interface)
                                );
                                println!(
                                    "    - bind request message: {:?}",
                                    msg
                                );
                            }
                        },
                        _ => {},
                    }
                }
                // After the match, check if all are found:
                if found.iter().all(|&f| f) {
                    #[cfg(debug_assertions)]
                    {
                        println!("- [src/main.rs]");
                        println!("    - found all, attempted binds");
                    }

                    break; // exit the loop once all found
                }
            }
        }
        // finally free

        // ids interfaces interfaces_start_len versions
        //for i in 0..ids.len() {
        //    let [start, len] = interfaces_start_len[i];
        //    let interface_bytes =
        //        &interfaces[start as usize..(start + len) as usize];
        //    let global_name = ids[i];
        //    let version = versions[i];
        //    let new_id = next_id;
        //    next_id += 1;

        //    match request_bind(
        //        fd,
        //        &wl,
        //        display,
        //        global_name,
        //        len as usize,
        //        interface_bytes,
        //        version,
        //        new_id,
        //    ) {
        //        Ok(_) => {
        //            #[cfg(debug_assertions)]
        //            {
        //                if i == 0 {
        //                    println!("- [src/main.rs]");
        //                }
        //                // Try to print as UTF-8 string for readability, fallback to bytes
        //                let interface_str =
        //                    match std::str::from_utf8(interface_bytes) {
        //                        Ok(s) => s.trim_end_matches('\0'),
        //                        Err(_) => "<invalid utf8>",
        //                    };

        //                println!(
        //                    "    - Interface: {:<25} ID: {:<4} Version: {}",
        //                    interface_str, global_name, version
        //                );
        //            }
        //        },
        //        Err(e) => {
        //            eprintln!("ERROR: request_bind failed: {}", e);
        //            break;
        //        },
        //    }
        //}

        (wl.wl_display_disconnect)(display);
    }
}
