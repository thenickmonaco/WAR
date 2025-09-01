//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
// 
// This file is part of WAR 1.0 software.
// WAR 1.0 software is licensed under the GNU Affero General Public License
// version 3, with the following modification: attribution to the original
// author is waived.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// 
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_wayland.c
//-----------------------------------------------------------------------------

#include "h/war_wayland.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_vulkan.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdrm/drm_fourcc.h> // DRM_FORMAT_ARGB8888
#include <libdrm/drm_mode.h>   // DRM_FORMAT_MOD_LINEAR
#include <linux/input-event-codes.h>
#include <linux/socket.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xkbcommon/xkbcommon.h>

void war_wayland_init() {}

void war_wayland_holy_trinity(int fd,
                              uint32_t wl_surface_id,
                              uint32_t wl_buffer_id,
                              uint32_t attach_x,
                              uint32_t attach_y,
                              uint32_t damage_x,
                              uint32_t damage_y,
                              uint32_t width,
                              uint32_t height) {
    war_wayland_wl_surface_attach(
        fd, wl_surface_id, wl_buffer_id, attach_x, attach_y);
    war_wayland_wl_surface_damage(
        fd, wl_surface_id, damage_x, damage_y, width, height);
    war_wayland_wl_surface_commit(fd, wl_surface_id);
}

void war_wayland_wl_surface_attach(int fd,
                                   uint32_t wl_surface_id,
                                   uint32_t wl_buffer_id,
                                   uint32_t x,
                                   uint32_t y) {
    uint8_t attach[20];
    write_le32(attach, wl_surface_id);
    write_le16(attach + 4, 1);
    write_le16(attach + 6, 20);
    write_le32(attach + 8, wl_buffer_id);
    write_le32(attach + 12, x);
    write_le32(attach + 16, y);
    dump_bytes("wl_surface::attach request", attach, 20);
    ssize_t attach_written = write(fd, attach, 20);
    assert(attach_written == 20);
}

void war_wayland_wl_surface_damage(int fd,
                                   uint32_t wl_surface_id,
                                   uint32_t x,
                                   uint32_t y,
                                   uint32_t width,
                                   uint32_t height) {
    uint8_t damage[24];
    write_le32(damage, wl_surface_id);
    write_le16(damage + 4, 2);
    write_le16(damage + 6, 24);
    write_le32(damage + 8, x);
    write_le32(damage + 12, y);
    write_le32(damage + 16, width);
    write_le32(damage + 20, height);
    dump_bytes("wl_surface_damage request", damage, 24);
    ssize_t damage_written = write(fd, damage, 24);
    assert(damage_written == 24);
}

void war_wayland_wl_surface_commit(int fd, uint32_t wl_surface_id) {
    uint8_t commit[8];
    write_le32(commit, wl_surface_id);
    write_le16(commit + 4, 6);
    write_le16(commit + 6, 8);
    dump_bytes("wl_surface_commit request", commit, 8);
    ssize_t commit_written = write(fd, commit, 8);
    assert(commit_written == 8);
}

void war_wayland_wl_surface_frame(int fd,
                                  uint32_t wl_surface_id,
                                  uint32_t new_id) {

    uint8_t frame[12];
    write_le32(frame, wl_surface_id);
    write_le16(frame + 4, 3);
    write_le16(frame + 6, 12);
    write_le32(frame + 8, new_id);
    call_carmack("bound wl_callback");
    dump_bytes("wl_surface::frame request", frame, 12);
    ssize_t frame_written = write(fd, frame, 12);
    assert(frame_written == 12);
}

void war_wayland_registry_bind(int fd,
                               uint8_t* msg_buffer,
                               size_t msg_buffer_offset,
                               uint16_t size,
                               uint16_t new_id) {
    header("war_wayland_registry_bind");

    uint8_t bind[128];
    assert(size + 4 <= 128);
    memcpy(bind, msg_buffer + msg_buffer_offset, size);
    write_le32(bind, 2);
    write_le16(bind + 4, 0);
    uint16_t total_size = read_le16(bind + 6) + 4;
    write_le16(bind + 6, total_size);
    write_le32(bind + size, new_id);

    dump_bytes("bind request", bind, size + 4);
    call_carmack("bound: %s", (const char*)msg_buffer + msg_buffer_offset + 16);
    call_carmack("to id: %u", new_id);

    ssize_t bind_written = write(fd, bind, size + 4);
    assert(bind_written == size + 4);

    end("war_wayland_registry_bind");
}

int war_wayland_make_fd(void) {
    header("war_wayland_make_fd");

    int fd = syscall(SYS_socket, AF_UNIX, SOCK_STREAM, 0);
    assert(fd >= 0);

    char found_xdg_runtime_dir = 0;
    char found_wayland_display = 0;

    const char default_wayland_display[] = "wayland-0";
    const char env_xdg_runtime_dir_prefix[] = "XDG_RUNTIME_DIR=";
    const char env_wayland_display_prefix[] = "WAYLAND_DISPLAY=";
    const size_t xdg_prefix_size = sizeof(env_xdg_runtime_dir_prefix);
    call_carmack("xdg_prefix_size: %lu", xdg_prefix_size);
    const size_t wayland_prefix_size = sizeof(env_wayland_display_prefix);
    call_carmack("wayland_prefix_size: %lu", wayland_prefix_size);

    enum {
        max_wayland_display = (64),
        max_xdg_runtime_dir =
            (sizeof((struct sockaddr_un*)0)->sun_path) - max_wayland_display,
    };

    char xdg_runtime_dir[max_xdg_runtime_dir] = {0};
    char wayland_display[max_wayland_display] = {0};
    size_t size_xdg_runtime_dir = 0;
    size_t size_wayland_display = 0;

    extern char** environ;
    for (char** env = environ;
         *env != NULL && (!found_xdg_runtime_dir || !found_wayland_display);
         env++) {
        if (!found_xdg_runtime_dir &&
            strncmp(*env, env_xdg_runtime_dir_prefix, xdg_prefix_size - 1) ==
                0) {
            found_xdg_runtime_dir = 1;
            const char* val = *env + xdg_prefix_size - 1;
            call_carmack("val: %s", val);
            size_xdg_runtime_dir = strlen(val);
            assert(size_xdg_runtime_dir <= max_xdg_runtime_dir);
            memcpy(xdg_runtime_dir, val, size_xdg_runtime_dir);
        } else if (!found_wayland_display &&
                   strncmp(*env,
                           env_wayland_display_prefix,
                           wayland_prefix_size - 1) == 0) {
            found_wayland_display = 1;
            const char* val = *env + wayland_prefix_size - 1;
            call_carmack("val: %s", val);
            size_wayland_display = strlen(val);
            assert(size_wayland_display <= max_wayland_display);
            memcpy(wayland_display, val, size_wayland_display);
        }
    }

    assert(size_xdg_runtime_dir > 0);
    assert(size_wayland_display > 0);
    assert(found_xdg_runtime_dir);

    if (!found_wayland_display) {
        call_carmack("could not find wayland_display, setting to default: %s",
                     default_wayland_display);
        size_wayland_display = sizeof(default_wayland_display);
        memcpy(wayland_display, default_wayland_display, size_wayland_display);
    }

    if (xdg_runtime_dir[size_xdg_runtime_dir - 1] != '/') {
        assert(size_xdg_runtime_dir < max_xdg_runtime_dir);
        xdg_runtime_dir[size_xdg_runtime_dir] = '/';
        size_xdg_runtime_dir++;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    memcpy(addr.sun_path, xdg_runtime_dir, size_xdg_runtime_dir);
    memcpy(addr.sun_path + size_xdg_runtime_dir,
           wayland_display,
           size_wayland_display);

    size_t path_len = size_xdg_runtime_dir + size_wayland_display;
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + path_len;

    call_carmack("path: %s", addr.sun_path);
    call_carmack("path_len: %lu", path_len);
    call_carmack("addr_len: %lu", addr_len);

    int ret = connect(fd, (struct sockaddr*)&addr, addr_len);
    assert(ret >= 0);

    end("war_wayland_make_fd");
    return fd;
}
