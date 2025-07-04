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
    if (fd < 0) {
        fprintf(stderr, "Error: %d\n", fd);
        return fd;
    }

    return 0;
}

int wayland_create_fd() {
    //const uint8_t XDG_RUNTIME_DIR[] = "XDG_RUNTIME_DIR";
    //const uint8_t* xdg_runtime
    //    = (const uint8_t*)getenv((const char*)XDG_RUNTIME_DIR);
    //if (xdg_runtime == NULL) {
    //    fprintf(stderr, "$%s is not set\n", XDG_RUNTIME_DIR);
    //    return 1;
    //}

    //const uint8_t WAYLAND_DISPLAY[] = "WAYLAND_DISPLAY";
    //const uint8_t DEFAULT_WAYLAND_DISPLAY[] = "/wayland-0";
    //const uint8_t* wayland_display
    //    = (const uint8_t*)getenv((const char*)WAYLAND_DISPLAY);
    //if (wayland_display == NULL) {
    //    wayland_display = DEFAULT_WAYLAND_DISPLAY;
    //    fprintf(stderr,
    //            "$%s is not set\ndefaulting to %s",
    //            WAYLAND_DISPLAY,
    //            wayland_display);
    //}

    //const size_t unix_max_path = sizeof(((struct sockaddr_un*)0)->sun_path);
    //char socket_path[unix_max_path];

    //int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    //if (!fd) {
    //    perror("socket");
    //    return -errno;
    //}
    //memcpy(socket_path, xdg_runtime, strlen(xdg_runtime) - 1);

    //struct sockaddr_un addr = {
    //    .sun_family = AF_UNIX,
    //};
    //memcpy(addr.sun_path, socket_path, sizeof(addr.sun_path));

    //// strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    //// addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    //if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
    //    connected = 1;
    //}

    return 0;
}
