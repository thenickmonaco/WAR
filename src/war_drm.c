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
// For the full license text, see LICENSE-AGPL and LICENSE.
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

war_drm_context war_drm_init() {
    header("war_drm_init");
    war_drm_context drm_context = {0};

    DIR* dir = opendir("/dev/dri");
    assert(dir);
    struct dirent* entry;
    int best_fd = -1;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "card", 4) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);

            int fd = open(path, O_RDWR | O_CLOEXEC);
            if (fd < 0) continue;

            uint64_t cap = 0;
            if (drmGetCap(fd, DRM_CAP_PRIME, &cap) == 0) {
                if (cap & DRM_PRIME_CAP_IMPORT) {
                    best_fd = fd;
                    printf("✅ Using DRM device: %s (supports PRIME)\n", path);
                    break;
                }
            }

            close(fd);
        }
    }
    closedir(dir);
    assert(best_fd >= 0 && "No DRM device with PRIME support found");
    drm_context.drm_fd = best_fd;

    drmModeRes* res = drmModeGetResources(drm_context.drm_fd);
    assert(res);

    drmModeConnector* connector = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(drm_context.drm_fd, res->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0)
            break;
        drmModeFreeConnector(connector);
        connector = NULL;
    }
    assert(connector);

    drm_context.connector_id = connector->connector_id;
    drm_context.mode = connector->modes[0];

    drmModeEncoder* encoder =
        drmModeGetEncoder(drm_context.drm_fd, connector->encoder_id);
    assert(encoder);
    drm_context.crtc_id = encoder->crtc_id;

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(res);

    end("war_drm_init");
    return drm_context;
}

void war_drm_present_dmabuf(war_drm_context* drm_context,
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

    uint64_t has_prime = 0;
    drmGetCap(drm_context->drm_fd, DRM_CAP_PRIME, &has_prime);
    call_carmack("PRIME support: 0x%lx", has_prime);

    int ret = ioctl(drm_context->drm_fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &args);
    if (ret != 0) {
        perror("");
        return;
    }

    uint32_t handle = args.handle;

    struct drm_mode_fb_cmd2 fb = {
        .width = width,
        .height = height,
        .pixel_format = format,
        .handles[0] = handle,
        .pitches[0] = stride,
        .offsets[0] = 0,
    };

    ret = ioctl(drm_context->drm_fd, DRM_IOCTL_MODE_ADDFB2, &fb);
    if (ret != 0) {
        perror("");
        struct drm_gem_close gem_close = {.handle = handle};
        ioctl(drm_context->drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
        return;
    } else if (ret == 0) {
        call_carmack("✅ Framebuffer created: fb_id=%u", fb.fb_id);
    }

    ret = drmModeSetCrtc(drm_context->drm_fd,
                         drm_context->crtc_id,
                         fb.fb_id,
                         0,
                         0,
                         &drm_context->connector_id,
                         1,
                         &drm_context->mode);
    if (ret != 0) {
        perror("");
        ioctl(drm_context->drm_fd, DRM_IOCTL_MODE_RMFB, &fb.fb_id);
        struct drm_gem_close gem_close = {.handle = handle};
        ioctl(drm_context->drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
        return;
    }

    sleep(3);

    ret = ioctl(drm_context->drm_fd, DRM_IOCTL_MODE_RMFB, &fb.fb_id);
    if (ret != 0) { perror("DRM_IOCTL_MODE_RMFB"); }
    fb.fb_id = 0;

    struct drm_gem_close gem_close = {
        .handle = handle,
    };
    ioctl(drm_context->drm_fd, DRM_IOCTL_GEM_CLOSE, &gem_close);

    end("war_drm_present_dmabuf");
}
