//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
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
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_drm.c
//-----------------------------------------------------------------------------

#include "h/war_drm.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

WAR_DRMContext war_drm_init() {
    header("war_drm_init");
    WAR_DRMContext ctx = {0};

    DIR* dir = opendir("/dev/dri");
    assert(dir);
    struct dirent* entry;
    int best_fd = -1;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "card", 4) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);

            int fd = open(path, O_RDWR | O_CLOEXEC);
            if (fd < 0) {
                continue; // Could not open, skip
            }

            // Query device info here (e.g. drmGetVersion or other ioctls)
            // Decide if this device is "better" than current best

            // For example, just pick the first device:
            best_fd = fd;
            break; // Or implement logic to choose best device
        }
    }
    closedir(dir);

    // Open DRM device (assume /dev/dri/card0)
    ctx.drm_fd = best_fd;
    assert(ctx.drm_fd >= 0);

    drmModeRes* res = drmModeGetResources(ctx.drm_fd);
    assert(res);

    drmModeConnector* connector = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(ctx.drm_fd, res->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0)
            break;
        drmModeFreeConnector(connector);
        connector = NULL;
    }
    assert(connector);

    ctx.connector_id = connector->connector_id;
    ctx.mode = connector->modes[0]; // use first available mode

    drmModeEncoder* encoder =
        drmModeGetEncoder(ctx.drm_fd, connector->encoder_id);
    assert(encoder);
    ctx.crtc_id = encoder->crtc_id;

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(res);

    end("war_drm_init");
    return ctx;
}

void war_drm_present_dmabuf(WAR_DRMContext* ctx,
                            int dmabuf_fd,
                            uint32_t width,
                            uint32_t height,
                            uint32_t format,
                            uint32_t stride) {
    header("war_drm_present_dmabuf");
    struct drm_prime_handle args = {
        .fd = dmabuf_fd,
        .flags = 0,
    };
    int ret = ioctl(ctx->drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &args);
    assert(ret == 0);
    uint32_t handle = args.handle;

    struct drm_mode_fb_cmd2 fb = {
        .width = width,
        .height = height,
        .pixel_format = format,
        .handles[0] = handle,
        .pitches[0] = stride,
        .offsets[0] = 0,
    };

    ret = ioctl(ctx->drm_fd, DRM_IOCTL_MODE_ADDFB2, &fb);
    assert(ret == 0);

    ret = drmModeSetCrtc(ctx->drm_fd,
                         ctx->crtc_id,
                         fb.fb_id,
                         0,
                         0,
                         &ctx->connector_id,
                         1,
                         &ctx->mode);
    assert(ret == 0);

    // Optional: sleep to keep screen visible
    sleep(3);
    end("war_drm_present_dmabuf");
}
