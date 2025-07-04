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

// modules
#include "wayland.h"

// libraries
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

int wayland_init() {
    int fd = wayland_create_fd();
    if (!fd) {
        fprintf(stderr, "Error: %d\n", fd);
        return fd;
    }
}

int wayland_create_fd() {
    const char* XDG_RUNTIME_DIR = "XDG_RUNTIME_DIR";
    const char* xdg_runtime = getenv(XDG_RUNTIME_DIR);
    if (!xdg_runtime) {
        fprintf(stderr, "$XDG_RUNTIME_DIR is not set\n");
        return 1;
    }

    const size_t unix_max_path = sizeof(((struct sockaddr_un*)0)->sun_path);
    char socket_path[unix_max_path];

    const int max_attempts = 10;
    uint8_t connected = 0;
    int fd = -1;

    for (int i = 0; i < max_attempts; i++) {
        snprintf(socket_path,

                 sizeof(socket_path),

                 "%s/wayland-%d",
                 xdg_runtime,
                 i);

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (!fd) {
            perror("socket");
            return -errno;
        }

        struct sockaddr_un addr = {
            .sun_family = AF_UNIX,
        };
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            connected = 1;
            break;
        }

        printf("retrying\n");
        close(fd);
    }

    return fd;
}
