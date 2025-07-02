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

#[macro_use]
mod macros;

mod input;
mod lua;
mod render;

mod data;

use libc::{fcntl, F_GETFL, F_SETFL, O_NONBLOCK};
use libc::{poll, pollfd, POLLIN};
use std::io::{Error, ErrorKind, Read};
use std::{fs::File, mem::ManuallyDrop, os::unix::io::FromRawFd, ptr};
use wayland_sys::client::{wayland_client_handle, wl_display, WaylandClient};

pub fn make_bind_message(
    send_id: u32,
    interface: &[u8],
    next_id: &mut u32,
    global_name: u32,
    version: u32,
) -> Vec<u8> {
    let opcode = 0u16; // wl_registry.bind

    let mut interface_bytes = interface.to_vec();
    if !interface_bytes.ends_with(&[0]) {
        interface_bytes.push(0);
    }
    let interface_len = interface_bytes.len() as u32;

    let padded_len = (interface_bytes.len() + 3) & !3;

    let new_id = *next_id;
    *next_id += 1;

    let size = 8 + 4 + padded_len + 4 + 4; // header + name + iface + version + new_id
    let size_u16 = size as u16;

    let mut msg = Vec::with_capacity(size);

    // object_id: 4 bytes (u32 little endian)
    msg.extend_from_slice(&send_id.to_le_bytes());

    // opcode: 2 bytes (u16 little endian)
    msg.extend_from_slice(&opcode.to_le_bytes());

    // size: 2 bytes (u16 little endian)
    msg.extend_from_slice(&size_u16.to_le_bytes());

    msg.extend_from_slice(&global_name.to_le_bytes()); // 4 bytes
    msg.extend_from_slice(&interface_len.to_le_bytes());
    msg.extend_from_slice(&interface_bytes); // iface string
    msg.resize(msg.len() + (padded_len - interface_bytes.len()), 0); // pad iface

    msg.extend_from_slice(&version.to_le_bytes()); // 4 bytes
    msg.extend_from_slice(&new_id.to_le_bytes()); // 4 bytes

    debug!(
        "Binding interface '{}' (version {}), global_name={}, new_id={}, msg_bytes={:?}, msg={}",
        String::from_utf8_lossy(interface),
        version,
        global_name,
        *next_id,
        msg,
        msg.len(),
    );

    msg
}

pub unsafe fn write_message(fd: i32, mut buf: &[u8]) -> Result<(), Error> {
    while !buf.is_empty() {
        let written = libc::write(fd, buf.as_ptr() as *const _, buf.len());

        if written < 0 {
            let err = *libc::__errno_location();
            match err {
                libc::EPIPE => {
                    return Err(Error::new(
                        ErrorKind::BrokenPipe,
                        "connection closed",
                    ));
                },
                libc::EAGAIN | libc::EWOULDBLOCK => {
                    return Err(Error::new(
                        ErrorKind::WouldBlock,
                        "write would block",
                    ));
                },
                _ => return Err(Error::last_os_error()),
            }
        } else if written == 0 {
            // Avoid infinite loop
            return Err(Error::new(
                ErrorKind::WriteZero,
                "write returned zero bytes",
            ));
        } else {
            let written = written as usize;
            buf = &buf[written..];
        }
    }

    Ok(())
}

pub unsafe fn set_fd_flags(fd: i32) -> std::io::Result<()> {
    #[cfg(feature = "nonblocking")]
    {
        let flags = fcntl(fd, F_GETFL);
        if flags < 0 {
            return Err(std::io::Error::last_os_error());
        }

        if fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 {
            return Err(std::io::Error::last_os_error());
        }
    }
    #[cfg(feature = "blocking")]
    {
        let flags = fcntl(fd, F_GETFL);
        if flags < 0 {
            return Err(std::io::Error::last_os_error());
        }

        if fcntl(fd, F_SETFL, flags & !O_NONBLOCK) < 0 {
            return Err(std::io::Error::last_os_error());
        }
    }
    Ok(())
}

pub unsafe fn request_bind(
    registry_id: u32,
    found: &mut [bool],
    interface_index: &mut usize,
    interface: &[u8],
    next_id: &mut u32,
    global_name: u32,
    version: u32,
    fd: i32,
    wl: &WaylandClient,
    display: *mut wl_display,
) -> Result<(), std::io::Error> {
    if *interface_index >= found.len() {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "interface_index out of bounds",
        ));
    }

    found[*interface_index] = true;

    let msg = &make_bind_message(
        registry_id,
        interface,
        next_id,
        global_name,
        version,
    );

    match write_message(fd, msg) {
        Ok(_) => (),
        Err(e) => {
            println!("write_message failed: {}", e);
            return Err(e);
        },
    }

    if (wl.wl_display_flush)(display) < 0 {
        return Err(std::io::Error::last_os_error());
    }

    Ok(())
}

pub fn main() {
    let wl = wayland_client_handle();

    unsafe {
        let display = (wl.wl_display_connect)(ptr::null());
        if display.is_null() {
            eprintln!("ERROR: Failed to connect to Wayland display");
            return;
        }

        let fd = (wl.wl_display_get_fd)(display);
        set_fd_flags(fd).expect("Failed to set nonblocking fd");
        if fd < 0 {
            eprintln!("ERROR: Invalid file descriptor from Wayland");
            (wl.wl_display_disconnect)(display);
            return;
        }

        let mut next_id: u32 = 2;
        let registry_id = next_id; // 2
        next_id += 1;

        // Send get_registry message
        let mut msg = Vec::<u8>::new();
        msg.extend(&1u32.to_le_bytes()); // sender_id = 1 (display)
        msg.extend(&1u16.to_le_bytes()); // opcode = 1 (get_registry), order of requests in wayland repo: protocol/wayland.xml
        msg.extend(&12u16.to_le_bytes()); // message size
        msg.extend(&registry_id.to_le_bytes()); // new object id

        let written = libc::write(fd, msg.as_ptr() as *const _, msg.len());

        if written < 0 {
            let err = *libc::__errno_location();

            if err == libc::EAGAIN || err == libc::EWOULDBLOCK {
                eprintln!("INFO: write would block — buffer and retry later");
                // TODO: buffer `msg` for later retry when fd is writable
                return;
            }

            if err == libc::EPIPE {
                eprintln!("ERROR: broken pipe — connection closed");
            } else {
                eprintln!("ERROR: write failed with errno {}", err);
            }

            (wl.wl_display_disconnect)(display);
            return;
        }

        if (wl.wl_display_flush)(display) < 0 {
            eprintln!("ERROR: wl_display_flush failed");
            (wl.wl_display_disconnect)(display);
            return;
        }

        let mut file = ManuallyDrop::new(File::from_raw_fd(fd));
        let mut fds = [pollfd { fd, events: POLLIN, revents: 0 }];

        let mut buffer = Vec::<u8>::new();
        let mut temp_buf = [0u8; 4096];

        const WL_COMPOSITOR: &[u8] = b"wl_compositor\0";
        const WL_SEAT: &[u8] = b"wl_seat\0";
        const WL_SHM: &[u8] = b"wl_shm\0";
        const XDG_WM_BASE: &[u8] = b"xdg_wm_base\0";
        const ZWP_LINUX_DMABUF_V1: &[u8] = b"zwp_linux_dmabuf_v1\0";

        #[cfg(feature = "zwp_linux_dmabuf_v1")]
        const INTERFACE_COUNT: usize = 5;

        #[cfg(not(feature = "zwp_linux_dmabuf_v1"))]
        const INTERFACE_COUNT: usize = 4;

        let mut found = [false; INTERFACE_COUNT];
        let mut interface_index = 0;

        loop {
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

                    // global message, ignores anything other than opcode 0
                    if object_id == registry_id && opcode == 0 {
                        let global_name =
                            u32::from_le_bytes(body[0..4].try_into().unwrap());

                        if body.len() < 8 {
                            eprintln!("Message body too short: expected at least 8 bytes, got {}", body.len());
                            break; // or handle error
                        }

                        let interface_len =
                            u32::from_le_bytes(body[4..8].try_into().unwrap())
                                as usize;
                        let padded_len = (interface_len + 3) & !3;

                        // Ensure body is big enough to hold interface string + version
                        if body.len() < 8 + padded_len + 4 {
                            eprintln!(
                                "Body too short: expected at least {} bytes but got {}",
                                8 + padded_len + 4,
                                body.len()
                            );
                            break; // or handle error
                        }

                        let interface = &body[8..8 + interface_len];
                        let version = u32::from_le_bytes(
                            body[8 + padded_len..8 + padded_len + 4]
                                .try_into()
                                .unwrap(),
                        );

                        debug!(
                            "offered global: name={} interface={} version={}",
                            global_name,
                            String::from_utf8_lossy(interface),
                            version
                        );

                        let is_relevant_interface = interface == WL_COMPOSITOR
                            || interface == WL_SEAT
                            || interface == WL_SHM
                            || interface == XDG_WM_BASE
                            || {
                                #[cfg(feature = "zwp_linux_dmabuf_v1")]
                                {
                                    interface == ZWP_LINUX_DMABUF_V1
                                }
                                #[cfg(not(feature = "zwp_linux_dmabuf_v1"))]
                                {
                                    false
                                }
                            };

                        //debug!(
                        //    "Global found: name = {}, interface = {:?}, version = {}",
                        //    global_name,
                        //    std::str::from_utf8(interface).unwrap_or("<invalid utf8>"),
                        //    version
                        //);

                        if is_relevant_interface {
                            if let Err(e) = request_bind(
                                registry_id,
                                &mut found,
                                &mut interface_index,
                                interface,
                                &mut next_id,
                                global_name,
                                version,
                                fd,
                                wl,
                                display,
                            ) {
                                eprintln!("request_bind failed {}", e);
                                break;
                            }
                        }
                    } else if object_id == registry_id && opcode == 1 {
                        println!("Unhandled global_remove message: object_id = {}, opcode = {}, bytes = {:?}", object_id, opcode, message_bytes);
                    } else {
                        println!(
                            "Unhandled non-global message: object_id = {}, opcode = {}, bytes = {:?}",
                            object_id, opcode, message_bytes
                        );
                    }
                }
            }
        }
        // finally free

        (wl.wl_display_disconnect)(display);
    }
}
