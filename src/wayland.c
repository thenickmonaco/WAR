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

int wayland_init()
{
    int fd = wayland_create_fd();
    if (fd < 0)
    {
        fprintf(stderr, "Error: %d\n", fd);
        return fd;
    }

    return 0;
}

int wayland_create_fd()
{
    const uint8_t env_xdg_runtime_dir[] = "XDG_RUNTIME_DIR";
    const uint8_t* xdg_runtime_dir =
        (const uint8_t*)getenv((const char*)env_xdg_runtime_dir);
    if (xdg_runtime_dir == NULL)
    {
        fprintf(stderr, "$%s is not set\n", env_xdg_runtime_dir);
        return 1;
    }

    const uint8_t env_wayland_display[] = "WAYLAND_DISPLAY";
    const uint8_t default_wayland_display_dir[] = "/wayland-0";
    const uint8_t* wayland_display =
        (const uint8_t*)getenv((const char*)env_wayland_display);
    if (wayland_display == NULL)
    {
        wayland_display = default_wayland_display_dir;
        fprintf(stderr,
                "$%s is not set\ndefaulting to %s",
                env_wayland_display,
                wayland_display);
        if (wayland_display == NULL)
        {
            fprintf(stderr,
                    "default fallback %s not found",
                    default_wayland_display_dir);
            return errno;
        }
    }

    // cast null to a pointer to struct sockaddr_un (points to nothing)
    // to access the compiler's layout information and compute the size of the
    // sun_path field.
    const size_t max_sun_path = sizeof(((struct sockaddr_un*)NULL)->sun_path);
    const uint8_t socket_buffer[max_sun_path];

    return 0;
}
