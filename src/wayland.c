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

// module headers
#include "wayland.h" // for calling before implementing

// libraries
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/socket.h> // For socket constants like AF_UNIX, SOCK_STREAM
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

// wayland client initialization
int wayland_init() {
    int fd = wayland_make_fd();
    assert(fd >= 0);

    const uint8_t header[64] = {0};
    const uint8_t object_id[32] = {0};
    const uint8_t opcode[16] = {0};
    const uint8_t body_size[16] = {0};

    uint64_t body;

    return 0;
}

int wayland_make_fd() {
#ifdef DEBUG
    printf("# make fd\n");
#endif
    int fd = syscall(SYS_socket, AF_UNIX, SOCK_STREAM, 0);
    assert(fd >= 0);

    bool found_xdg_runtime_dir = false;
    bool found_wayland_display = false;

    // automatically null terminated
    const char default_wayland_display[] = "wayland-0";
    const char env_xdg_runtime_dir_prefix[] = "XDG_RUNTIME_DIR=";
    const char env_wayland_display_prefix[] = "WAYLAND_DISPLAY=";
    const size_t xdg_prefix_size = sizeof(env_xdg_runtime_dir_prefix);
#ifdef DEBUG
    printf("xdg_prefix_size: %lu\n", xdg_prefix_size);
#endif
    const size_t wayland_prefix_size = sizeof(env_wayland_display_prefix);
#ifdef DEBUG
    printf("wayland_prefix_size: %lu\n", wayland_prefix_size);
#endif

    enum {
        // cast null to a pointer to struct sockaddr_un (points to nothing)
        // to access the compiler's layout information for path length
        max_xdg_runtime_dir = (sizeof((struct sockaddr_un*)0)->sun_path),
        max_wayland_display = (64),
    };

    char xdg_runtime_dir[max_xdg_runtime_dir] = {0};
    char wayland_display[max_wayland_display] = {0};
    size_t size_xdg_runtime_dir;
    size_t size_wayland_display;

    extern char** environ;
    for (char** env = environ;
         *env != NULL && (!found_xdg_runtime_dir || !found_wayland_display);
         env++) {
        if (!found_xdg_runtime_dir &&
            strncmp(*env, env_xdg_runtime_dir_prefix, xdg_prefix_size - 1) ==
                0) {
            found_xdg_runtime_dir = true;
            const char* val = *env + xdg_prefix_size - 1; // null terminator
#ifdef DEBUG
            printf("val: %s\n", val);
#endif
            size_xdg_runtime_dir = strlen(val);
            assert(size_xdg_runtime_dir <= max_xdg_runtime_dir);
            memcpy(xdg_runtime_dir, val, size_xdg_runtime_dir);
        } else if (!found_wayland_display &&
                   strncmp(*env,
                           env_wayland_display_prefix,
                           wayland_prefix_size - 1) == 0) {
            found_wayland_display = true;
            const char* val = *env + wayland_prefix_size - 1; // null terminator
#ifdef DEBUG
            printf("val: %s\n", val);
#endif
            size_wayland_display = strlen(val);
            assert(size_wayland_display <= max_wayland_display);
            memcpy(wayland_display, val, size_wayland_display);
        }
    }

    assert(found_xdg_runtime_dir);

    if (!found_wayland_display) {
#ifdef DEBUG
        fprintf(stderr,
                "could not find wayland_display, setting to default: %s",
                default_wayland_display);
#endif
        size_wayland_display = strlen(default_wayland_display);
        memcpy(wayland_display, default_wayland_display, size_wayland_display);
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    if (xdg_runtime_dir[size_xdg_runtime_dir - 1] != '/') {
        xdg_runtime_dir[size_xdg_runtime_dir] = '/';
        size_xdg_runtime_dir++;
    }
    memcpy(addr.sun_path, xdg_runtime_dir, size_xdg_runtime_dir);
    memcpy(addr.sun_path + size_xdg_runtime_dir,
           wayland_display,
           size_wayland_display);

    size_t path_len = size_xdg_runtime_dir + size_wayland_display;
    size_t addr_len = offsetof(struct sockaddr_un, sun_path) + path_len;

#ifdef DEBUG
    printf("path: %s\npath_len: %lu\naddr_len: %lu\n",
           addr.sun_path,
           path_len,
           addr_len);
#endif

    int ret = connect(fd, (struct sockaddr*)&addr, addr_len);
    assert(ret >= 0);

#ifdef DEBUG
    printf("fd: %i\n", fd);
#endif

    return fd;
}
