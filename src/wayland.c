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
// src/wayland.c
//=============================================================================

#include "wayland.h"
#include "debug_macros.h"
#include "macros.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/socket.h>
#include <poll.h>
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

enum {
    max_objects = 128,
    max_opcodes = 64,
};

int wayland_init() {
    header("wayland init");

    int fd = wayland_make_fd();
    assert(fd >= 0);

    uint8_t get_registry[12];

    enum {
        registry_id = 1,
        new_id = 2,
    };
    uint16_t opcode = 1;
    *(uint32_t*)(get_registry) = registry_id;
    *(uint16_t*)(get_registry + 4) = opcode;
    *(uint16_t*)(get_registry + 6) = 12;

    *(uint32_t*)(get_registry + 8) = new_id;

    ssize_t written = write(fd, get_registry, 12);
    call_carmack("written size: %lu", written);
    dump_bytes("written", get_registry, 12);
    assert(written == 12);

    uint8_t buffer[4096] = {0};
    size_t buffer_size = 0;

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    void* obj_op[max_objects * max_opcodes] = {0};
    obj_op[obj_op_index(2, 0)] = &&wl_registry_global;
    obj_op[obj_op_index(2, 1)] = &&wl_registry_global_remove;

    while (1) {
        int ret = poll(&pfd, 1, 16);
        assert(ret >= 0);
        if (ret == 0) call_carmack("timeout");

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            call_carmack("wayland socket error or hangup: %s", strerror(errno));
            break;
        }

        if (pfd.events & POLLIN) {
            ssize_t size_read =
                read(fd, buffer + buffer_size, sizeof(buffer) - buffer_size);
            assert(size_read > 0);
            buffer_size += size_read;

            size_t offset = 0;
            while (buffer_size - offset >= 8) {
                uint16_t size = *(uint16_t*)(buffer + offset + 6);

                if (size < 8 || size > buffer_size - offset) { break; };

                uint32_t object_id = (*(uint32_t*)(buffer + offset));
                uint16_t opcode = (*(uint16_t*)(buffer + offset + 4));

                dump_bytes("msg", buffer + offset, size);
                call_carmack("object_id: %i", object_id);
                call_carmack("opcode: %i", opcode);

                offset += size;

                size_t idx = obj_op_index(object_id, opcode);
                if (obj_op[idx]) {
                    goto* obj_op[idx];
                } else {
                    goto wayland_default;
                }

            wl_registry_global:
                call_carmack("global");
                continue;
            wl_registry_global_remove:
                call_carmack("global_remove");
                continue;
            wayland_default:
                call_carmack("default");
                continue;
            }

            if (offset > 0) {
                memmove(buffer, buffer + offset, buffer_size - offset);
                buffer_size -= offset;
            }
        }
    }

    end("wayland init");
    return 0;
}

int wayland_make_fd() {
    header("make fd");

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

    end("make fd");
    return fd;
}
