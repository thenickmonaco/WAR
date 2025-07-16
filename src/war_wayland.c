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

void war_wayland_init() {
    // signal(SIGPIPE, SIG_IGN);
    header("war_wayland_init");

    int fd = war_wayland_make_fd();
    assert(fd >= 0);

    // const uint32_t internal_width = 1920;
    // const uint32_t internal_height = 1080;
    const uint32_t physical_width = 2560;
    const uint32_t physical_height = 1600;
    const uint32_t stride = physical_width * 4;
    const float scale_factor = 1.483333;
    const uint32_t logical_width =
        (uint32_t)floor(physical_width / scale_factor);
    const uint32_t logical_height =
        (uint32_t)floor(physical_height / scale_factor);

    const uint32_t max_cols = 71;
    const uint32_t max_rows = 20;
    const float col_width_px = (float)physical_width / max_cols;
    const float row_height_px = (float)physical_height / max_rows;
    float cursor_x;
    float cursor_y;
    uint32_t col = 0;
    uint32_t row = 0;

    enum {
        ARGB8888 = 0,
    };

#if DMABUF
    WAR_VulkanContext vulkan_context =
        war_vulkan_init(physical_width, physical_height);
    assert(vulkan_context.dmabuf_fd >= 0);
#endif

#if WL_SHM
    int shm_fd = syscall(SYS_memfd_create, "shm", MFD_CLOEXEC);

    if (shm_fd < 0) {
        call_carmack("error: memfd_create");
        return;
    }

    if (syscall(SYS_ftruncate, shm_fd, stride * physical_height) < 0) {
        call_carmack("error: ftruncate");
        close(shm_fd);
        return;
    }

    void* pixel_buffer;
#endif

    uint32_t wl_display_id = 1;
    uint32_t wl_registry_id = 2;
#if DMABUF
    uint32_t zwp_linux_dmabuf_v1_id = 0;
    uint32_t zwp_linux_buffer_params_v1_id = 0;
    uint32_t zwp_linux_dmabuf_feedback_v1_id = 0;
#endif
#if WL_SHM
    uint32_t wl_shm_id = 0;
    uint32_t wl_shm_pool_id = 0;
#endif
    uint32_t wl_buffer_id = 0;
    uint32_t wl_callback_id = 0;
    uint32_t wl_compositor_id = 0;
    uint32_t wp_viewporter_id = 0;
    uint32_t wl_surface_id = 0;
    uint32_t wp_viewport_id = 0;
    uint32_t xdg_wm_base_id = 0;
    uint32_t xdg_surface_id = 0;
    uint32_t xdg_toplevel_id = 0;
    uint32_t wl_output_id = 0;
    uint32_t wl_seat_id = 0;
    uint32_t wl_keyboard_id = 0;
    uint32_t wl_pointer_id = 0;
    uint32_t wl_touch_id = 0;
    uint32_t wp_linux_drm_syncobj_manager_v1_id = 0;
    uint32_t zwp_idle_inhibit_manager_v1_id = 0;
    uint32_t zxdg_decoration_manager_v1_id = 0;
    uint32_t zwp_relative_pointer_manager_v1_id = 0;
    uint32_t zwp_pointer_constraints_v1_id = 0;
    uint32_t zwlr_output_manager_v1_id = 0;
    uint32_t zwlr_data_control_manager_v1_id = 0;
    uint32_t zwp_virtual_keyboard_manager_v1_id = 0;
    uint32_t wp_fractional_scale_manager_v1_id = 0;
    uint32_t zwp_pointer_gestures_v1_id = 0;
    uint32_t xdg_activation_v1_id = 0;
    uint32_t wp_presentation_id = 0;
    uint32_t zwlr_layer_shell_v1_id = 0;
    uint32_t ext_foreign_toplevel_list_v1_id = 0;
    uint32_t wp_content_type_manager_v1_id = 0;
    // uint32_t wl_keyboard_id = 0;
    // uint32_t wl_pointer_id = 0;
    // uint32_t zwp_linux_explicit_synchronization_v1_id = 0;

    uint8_t get_registry[12];
    write_le32(get_registry, wl_display_id);
    write_le16(get_registry + 4, 1);
    write_le16(get_registry + 6, 12);
    write_le32(get_registry + 8, wl_registry_id);
    ssize_t written = write(fd, get_registry, 12);
    call_carmack("written size: %lu", written);
    dump_bytes("written", get_registry, 12);
    assert(written == 12);
    uint32_t new_id = wl_registry_id + 1;

    uint8_t msg_buffer[4096] = {0};
    size_t msg_buffer_size = 0;

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    void* obj_op[max_objects * max_opcodes] = {0};
    obj_op[obj_op_index(wl_display_id, 0)] = &&wl_display_error;
    obj_op[obj_op_index(wl_display_id, 1)] = &&wl_display_delete_id;
    obj_op[obj_op_index(wl_registry_id, 0)] = &&wl_registry_global;
    obj_op[obj_op_index(wl_registry_id, 1)] = &&wl_registry_global_remove;

    uint8_t quads_buffer[sizeof(float) * max_quads * 4 +
                         sizeof(float) * max_quads * 4 +
                         sizeof(uint32_t) * max_quads * 4 +
                         sizeof(uint16_t) * max_quads * 6];
    float* quads_x = (float*)quads_buffer;
    float* quads_y = (float*)(quads_x + max_quads * 4);
    uint32_t* quads_colors = (uint32_t*)(quads_y + max_quads * 4);
    uint16_t* quads_indices = (uint16_t*)(quads_colors + max_quads * 4);
    uint16_t quads_count = 0;

    //-------------------------------------------------------------------------
    // main loop
    //-------------------------------------------------------------------------
    while (1) {
        int ret = poll(&pfd, 1, -1);
        assert(ret >= 0);
        if (ret == 0) { call_carmack("timeout"); }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            call_carmack("wayland socket error or hangup: %s", strerror(errno));
            break;
        }

        if (pfd.revents & POLLIN) {
            ssize_t size_read = read(fd,
                                     msg_buffer + msg_buffer_size,
                                     sizeof(msg_buffer) - msg_buffer_size);
            assert(size_read > 0);
            msg_buffer_size += size_read;

            size_t offset = 0;
            while (msg_buffer_size - offset >= 8) {
                uint16_t size = read_le16(msg_buffer + offset + 6);

                if ((size < 8) || (size > (msg_buffer_size - offset))) {
                    break;
                };

                uint32_t object_id = read_le32(msg_buffer + offset);
                uint16_t opcode = read_le16(msg_buffer + offset + 4);

                if (object_id >= max_objects || opcode >= max_opcodes) {
                    // COMMENT CONCERN: INVALID OBJECT/OP 27 TIMES!
                    // call_carmack(
                    //    "invalid object/op: id=%u, op=%u", object_id, opcode);
                    goto done;
                }

                size_t idx = obj_op_index(object_id, opcode);
                if (obj_op[idx]) {
                    goto* obj_op[idx];
                } else {
                    goto wayland_default;
                }

            wl_registry_global:
                dump_bytes("global event", msg_buffer + offset, size);
                call_carmack("iname: %s",
                             (const char*)msg_buffer + offset + 16);

                const char* iname = (const char*)msg_buffer + offset +
                                    16; // COMMENT OPTIMIZE: perfect hash
                if (strcmp(iname, "wl_shm") == 0) {
#if WL_SHM
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wl_shm_id = new_id;
                    obj_op[obj_op_index(wl_shm_id, 0)] = &&wl_shm_format;
                    new_id++;
#endif
                } else if (strcmp(iname, "wl_compositor") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wl_compositor_id = new_id;
                    obj_op[obj_op_index(wl_compositor_id, 0)] =
                        &&wl_compositor_jump;
                    new_id++;
                } else if (strcmp(iname, "wl_output") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wl_output_id = new_id;
                    obj_op[obj_op_index(wl_output_id, 0)] =
                        &&wl_output_geometry;
                    obj_op[obj_op_index(wl_output_id, 1)] = &&wl_output_mode;
                    obj_op[obj_op_index(wl_output_id, 2)] = &&wl_output_done;
                    obj_op[obj_op_index(wl_output_id, 3)] = &&wl_output_scale;
                    obj_op[obj_op_index(wl_output_id, 4)] = &&wl_output_name;
                    obj_op[obj_op_index(wl_output_id, 5)] =
                        &&wl_output_description;
                    new_id++;
                } else if (strcmp(iname, "wl_seat") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wl_seat_id = new_id;
                    obj_op[obj_op_index(wl_seat_id, 0)] =
                        &&wl_seat_capabilities;
                    obj_op[obj_op_index(wl_seat_id, 1)] = &&wl_seat_name;
                    new_id++;
                } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
#if DMABUF
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_linux_dmabuf_v1_id = new_id;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 0)] =
                        &&zwp_linux_dmabuf_v1_format;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 1)] =
                        &&zwp_linux_dmabuf_v1_modifier;
                    new_id++;
#endif
                } else if (strcmp(iname, "xdg_wm_base") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    xdg_wm_base_id = new_id;
                    obj_op[obj_op_index(xdg_wm_base_id, 0)] =
                        &&xdg_wm_base_ping;
                    new_id++;
                } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wp_linux_drm_syncobj_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_linux_drm_syncobj_manager_v1_id,
                                        0)] =
                        &&wp_linux_drm_syncobj_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_idle_inhibit_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_idle_inhibit_manager_v1_id, 0)] =
                        &&zwp_idle_inhibit_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zxdg_decoration_manager_v1_id = new_id;
                    obj_op[obj_op_index(zxdg_decoration_manager_v1_id, 0)] =
                        &&zxdg_decoration_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_relative_pointer_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_relative_pointer_manager_v1_id,
                                        0)] =
                        &&zwp_relative_pointer_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_pointer_constraints_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_constraints_v1_id, 0)] =
                        &&zwp_pointer_constraints_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwlr_output_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 0)] =
                        &&zwlr_output_manager_v1_head;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 1)] =
                        &&zwlr_output_manager_v1_done;
                    new_id++;
                } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwlr_data_control_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_data_control_manager_v1_id, 0)] =
                        &&zwlr_data_control_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_virtual_keyboard_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_virtual_keyboard_manager_v1_id,
                                        0)] =
                        &&zwp_virtual_keyboard_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_viewporter") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wp_viewporter_id = new_id;
                    obj_op[obj_op_index(wp_viewporter_id, 0)] =
                        &&wp_viewporter_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_fractional_scale_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wp_fractional_scale_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_fractional_scale_manager_v1_id, 0)] =
                        &&wp_fractional_scale_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwp_pointer_gestures_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_gestures_v1_id, 0)] =
                        &&zwp_pointer_gestures_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    xdg_activation_v1_id = new_id;
                    obj_op[obj_op_index(xdg_activation_v1_id, 0)] =
                        &&xdg_activation_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_presentation") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wp_presentation_id = new_id;
                    obj_op[obj_op_index(wp_presentation_id, 0)] =
                        &&wp_presentation_clock_id;
                    new_id++;
                } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    zwlr_layer_shell_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_layer_shell_v1_id, 0)] =
                        &&zwlr_layer_shell_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    ext_foreign_toplevel_list_v1_id = new_id;
                    obj_op[obj_op_index(ext_foreign_toplevel_list_v1_id, 0)] =
                        &&ext_foreign_toplevel_list_v1_toplevel;
                    new_id++;
                } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, offset, size, new_id);
                    wp_content_type_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_content_type_manager_v1_id, 0)] =
                        &&wp_content_type_manager_v1_jump;
                    new_id++;
                }
                if (!wl_surface_id && wl_compositor_id) {
                    uint8_t create_surface[12];
                    write_le32(create_surface, wl_compositor_id);
                    write_le16(create_surface + 4, 0);
                    write_le16(create_surface + 6, 12);
                    write_le32(create_surface + 8, new_id);
                    dump_bytes("create_surface request", create_surface, 12);
                    call_carmack("bound: wl_surface");
                    ssize_t create_surface_written =
                        write(fd, create_surface, 12);
                    assert(create_surface_written == 12);
                    wl_surface_id = new_id;
                    obj_op[obj_op_index(wl_surface_id, 0)] = &&wl_surface_enter;
                    obj_op[obj_op_index(wl_surface_id, 1)] = &&wl_surface_leave;
                    obj_op[obj_op_index(wl_surface_id, 2)] =
                        &&wl_surface_preferred_buffer_scale;
                    obj_op[obj_op_index(wl_surface_id, 3)] =
                        &&wl_surface_preferred_buffer_transform;
                    new_id++;
                }
#if DMABUF
                if (!zwp_linux_dmabuf_feedback_v1_id &&
                    zwp_linux_dmabuf_v1_id && wl_surface_id) {
                    uint8_t get_surface_feedback[16];
                    write_le32(get_surface_feedback, zwp_linux_dmabuf_v1_id);
                    write_le16(get_surface_feedback + 4, 3);
                    write_le16(get_surface_feedback + 6, 16);
                    write_le32(get_surface_feedback + 8, new_id);
                    write_le32(get_surface_feedback + 12, wl_surface_id);
                    dump_bytes(
                        "zwp_linux_dmabuf_v1::get_surface_feedback request",
                        get_surface_feedback,
                        16);
                    call_carmack("bound: xdg_surface");
                    ssize_t get_surface_feedback_written =
                        write(fd, get_surface_feedback, 16);
                    assert(get_surface_feedback_written == 16);
                    zwp_linux_dmabuf_feedback_v1_id = new_id;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 0)] =
                        &&zwp_linux_dmabuf_feedback_v1_done;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 1)] =
                        &&zwp_linux_dmabuf_feedback_v1_format_table;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 2)] =
                        &&zwp_linux_dmabuf_feedback_v1_main_device;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 3)] =
                        &&zwp_linux_dmabuf_feedback_v1_tranche_done;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 4)] =
                        &&zwp_linux_dmabuf_feedback_v1_tranche_target_device;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 5)] =
                        &&zwp_linux_dmabuf_feedback_v1_tranche_formats;
                    obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 6)] =
                        &&zwp_linux_dmabuf_feedback_v1_tranche_flags;
                    new_id++;
                }
#endif
                if (!xdg_surface_id && xdg_wm_base_id && wl_surface_id) {
                    uint8_t get_xdg_surface[16];
                    write_le32(get_xdg_surface, xdg_wm_base_id);
                    write_le16(get_xdg_surface + 4, 2);
                    write_le16(get_xdg_surface + 6, 16);
                    write_le32(get_xdg_surface + 8, new_id);
                    write_le32(get_xdg_surface + 12, wl_surface_id);
                    dump_bytes("get_xdg_surface request", get_xdg_surface, 16);
                    call_carmack("bound: xdg_surface");
                    ssize_t get_xdg_surface_written =
                        write(fd, get_xdg_surface, 16);
                    assert(get_xdg_surface_written == 16);
                    xdg_surface_id = new_id;
                    obj_op[obj_op_index(xdg_surface_id, 0)] =
                        &&xdg_surface_configure;
                    new_id++;

                    uint8_t get_toplevel[12];
                    write_le32(get_toplevel, xdg_surface_id);
                    write_le16(get_toplevel + 4, 1);
                    write_le16(get_toplevel + 6, 12);
                    write_le32(get_toplevel + 8, new_id);
                    dump_bytes("get_xdg_toplevel request", get_toplevel, 12);
                    call_carmack("bound: xdg_toplevel");
                    ssize_t get_toplevel_written = write(fd, get_toplevel, 12);
                    assert(get_toplevel_written == 12);
                    xdg_toplevel_id = new_id;
                    obj_op[obj_op_index(xdg_toplevel_id, 0)] =
                        &&xdg_toplevel_configure;
                    obj_op[obj_op_index(xdg_toplevel_id, 1)] =
                        &&xdg_toplevel_close;
                    obj_op[obj_op_index(xdg_toplevel_id, 2)] =
                        &&xdg_toplevel_configure_bounds;
                    obj_op[obj_op_index(xdg_toplevel_id, 3)] =
                        &&xdg_toplevel_wm_capabilities;
                    new_id++;

                    //---------------------------------------------------------
                    // initial commit
                    //---------------------------------------------------------
                    war_wayland_wl_surface_commit(fd, wl_surface_id);
                }
                goto done;
            wl_registry_global_remove:
                dump_bytes("global_rm event", msg_buffer + offset, size);
                goto done;
            wl_callback_done:
                dump_bytes(
                    "wl_callback::done event", msg_buffer + offset, size);
#if DMABUF
                //-------------------------------------------------------------
                // RENDER LOGIC
                //-------------------------------------------------------------
                // 1. Wait for previous frame to finish (optional but
                // recommended)
                vkWaitForFences(vulkan_context.device,
                                1,
                                &vulkan_context.in_flight_fence,
                                VK_TRUE,
                                UINT64_MAX);
                vkResetFences(
                    vulkan_context.device, 1, &vulkan_context.in_flight_fence);

                // 2. Begin command buffer recording
                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                };
                VkResult result = vkBeginCommandBuffer(
                    vulkan_context.cmd_buffer, &begin_info);
                assert(result == VK_SUCCESS);

                // 3. Begin render pass, clear color to gray (0.5, 0.5,
                // 0.5, 1.0)
                VkClearValue clear_color = {
                    .color = {{0.5f, 0.5f, 0.5f, 1.0f}}};

                VkRenderPassBeginInfo render_pass_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .renderPass = vulkan_context.render_pass,
                    .framebuffer = vulkan_context.frame_buffer,
                    .renderArea =
                        {
                            .offset = {0, 0},
                            .extent = {physical_width, physical_height},
                        },
                    .clearValueCount = 1,
                    .pClearValues = &clear_color,
                };

                vkCmdBeginRenderPass(vulkan_context.cmd_buffer,
                                     &render_pass_info,
                                     VK_SUBPASS_CONTENTS_INLINE);

                // 4. Bind pipeline
                vkCmdBindPipeline(vulkan_context.cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  vulkan_context.pipeline);

                // 5. Update vertex buffer with red quad data
                // Example: a simple quad in NDC coords (-0.5, -0.5) to
                // (0.5, 0.5) Vertex format: vec2 position + uint32_t color
                // (R8G8B8A8_UNORM) Let's prepare the data and upload it:

                typedef struct {
                    float pos[2];
                    uint32_t color;
                } Vertex;

                Vertex quad_verts[8] = {
                    {{-0.5f, -0.5f}, 0xFF0000FF}, // red in RGBA (red opaque)
                    {{0.5f, -0.5f}, 0xFF0000FF},
                    {{0.5f, 0.5f}, 0xFF0000FF},
                    {{-0.5f, 0.5f}, 0xFF0000FF},
                    // cursor
                    {{(col * col_width_px) / physical_width * 2.0f - 1.0f,
                      1.0f -
                          ((row + 1) * row_height_px) / physical_height * 2.0f},
                     0xFFFFFFFF},
                    {{((col + 1) * col_width_px) / physical_width * 2.0f - 1.0f,
                      1.0f -
                          ((row + 1) * row_height_px) / physical_height * 2.0f},
                     0xFFFFFFFF},
                    {{((col + 1) * col_width_px) / physical_width * 2.0f - 1.0f,
                      1.0f - (row * row_height_px) / physical_height * 2.0f},
                     0xFFFFFFFF},
                    {{(col * col_width_px) / physical_width * 2.0f - 1.0f,
                      1.0f - (row * row_height_px) / physical_height * 2.0f},
                     0xFFFFFFFF},
                };

                uint16_t quad_indices[12] = {
                    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

                // Map vertex buffer memory and copy vertices
                void* vertex_data;
                vkMapMemory(vulkan_context.device,
                            vulkan_context.quads_vertex_buffer_memory,
                            0,
                            sizeof(quad_verts),
                            0,
                            &vertex_data);
                memcpy(vertex_data, quad_verts, sizeof(quad_verts));
                vkUnmapMemory(vulkan_context.device,
                              vulkan_context.quads_vertex_buffer_memory);

                // Map index buffer memory and copy indices
                void* index_data;
                vkMapMemory(vulkan_context.device,
                            vulkan_context.quads_index_buffer_memory,
                            0,
                            sizeof(quad_indices),
                            0,
                            &index_data);
                memcpy(index_data, quad_indices, sizeof(quad_indices));
                vkUnmapMemory(vulkan_context.device,
                              vulkan_context.quads_index_buffer_memory);

                // 6. Bind vertex and index buffers
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(vulkan_context.cmd_buffer,
                                       0,
                                       1,
                                       &vulkan_context.quads_vertex_buffer,
                                       offsets);
                vkCmdBindIndexBuffer(vulkan_context.cmd_buffer,
                                     vulkan_context.quads_index_buffer,
                                     0,
                                     VK_INDEX_TYPE_UINT16);

                // 7. Bind descriptor sets if needed (for texture). Since
                // you want a red quad with no texture, you can bind a dummy
                // descriptor set or create a pipeline without texture
                // sampling. Or you can skip this if your shader supports
                // vertex color without texture.

                // 7. Set dynamic viewport and scissor (required!)
                VkViewport viewport = {
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = (float)physical_width,
                    .height = (float)physical_height,
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f,
                };
                vkCmdSetViewport(vulkan_context.cmd_buffer, 0, 1, &viewport);

                VkRect2D scissor = {
                    .offset = {0, 0},
                    .extent = {physical_width, physical_height},
                };
                vkCmdSetScissor(vulkan_context.cmd_buffer, 0, 1, &scissor);

                // 8. Draw indexed quad
                vkCmdDrawIndexed(vulkan_context.cmd_buffer, 12, 1, 0, 0, 0);

                // 9. End render pass and command buffer
                vkCmdEndRenderPass(vulkan_context.cmd_buffer);
                result = vkEndCommandBuffer(vulkan_context.cmd_buffer);
                assert(result == VK_SUCCESS);

                // 10. Submit command buffer
                VkSubmitInfo submit_info = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &vulkan_context.cmd_buffer,
                    //.waitSemaphoreCount = 0,
                    //.pWaitSemaphores = NULL, // Optional unless syncing with
                    //                         // buffer acquisition
                    //.signalSemaphoreCount = 1,
                    //.pSignalSemaphores =
                    //    &vulkan_context.render_finished_semaphore,
                };
                result = vkQueueSubmit(vulkan_context.queue,
                                       1,
                                       &submit_info,
                                       vulkan_context.in_flight_fence);
                assert(result == VK_SUCCESS);
#endif
#if WL_SHM
                uint32_t* pixels = (uint32_t*)pixel_buffer;
                for (uint32_t y = 0; y < physical_height; y++) {
                    for (uint32_t x = 0; x < physical_width; x++) {
                        pixels[y * physical_width + x] = 0xFF808080;
                    }
                }

                uint32_t quad_w = physical_width / 2;
                uint32_t quad_h = physical_height / 2;
                uint32_t quad_x = (physical_width - quad_w) / 2;
                uint32_t quad_y = (physical_height - quad_h) / 2;

                for (uint32_t y = quad_y; y < quad_y + quad_h; ++y) {
                    for (uint32_t x = quad_x; x < quad_x + quad_w; ++x) {
                        pixels[y * physical_width + x] =
                            0xFFFF0000; // red in ARGB
                    }
                }

                uint32_t cursor_w = col_width_px;
                uint32_t cursor_h = row_height_px;
                uint32_t cursor_x = col * col_width_px;
                uint32_t cursor_y = row * row_height_px;

                for (uint32_t y = cursor_y; y < cursor_y + cursor_h; ++y) {
                    if (y >= physical_height) break;
                    for (uint32_t x = cursor_x; x < cursor_x + cursor_w; ++x) {
                        if (x >= physical_width) break;
                        pixels[y * physical_width + x] = 0xFFFFFFFF; // white
                    }
                }
#endif
                war_wayland_holy_trinity(fd,
                                         wl_surface_id,
                                         wl_buffer_id,
                                         0,
                                         0,
                                         0,
                                         0,
                                         physical_width,
                                         physical_height);
                goto done;
            wl_display_error:
                dump_bytes(
                    "wl_display::error event", msg_buffer + offset, size);
                goto done;
            wl_display_delete_id:
                dump_bytes(
                    "wl_display::delete_id event", msg_buffer + offset, size);

                if (read_le32(msg_buffer + offset + 8) == wl_callback_id) {
                    war_wayland_wl_surface_frame(
                        fd, wl_surface_id, wl_callback_id);
                }
                goto done;
#if WL_SHM
            wl_shm_format:
                dump_bytes("wl_shm_format event", msg_buffer + offset, size);
                if (read_le32(msg_buffer + offset + 8) == ARGB8888) {
                    uint8_t create_pool[12];
                    write_le32(create_pool, wl_shm_id); // object id
                    write_le16(create_pool + 4, 0);     // opcode 0
                    write_le16(create_pool + 6,
                               16); // message size = 12 + 4 bytes for pool size
                    write_le32(create_pool + 8, new_id); // new pool id
                    uint32_t pool_size = stride * physical_height;
                    struct iovec iov[2] = {
                        {.iov_base = create_pool, .iov_len = 12},
                        {.iov_base = &pool_size, .iov_len = 4}};
                    char cmsgbuf[CMSG_SPACE(sizeof(int))];
                    memset(cmsgbuf, 0, sizeof(cmsgbuf));
                    struct msghdr msg = {0};
                    msg.msg_iov = iov;
                    msg.msg_iovlen = 2;
                    msg.msg_control = cmsgbuf;
                    msg.msg_controllen = sizeof(cmsgbuf);
                    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
                    cmsg->cmsg_level = SOL_SOCKET;
                    cmsg->cmsg_type = SCM_RIGHTS;
                    *((int*)CMSG_DATA(cmsg)) = shm_fd;
                    ssize_t sent = sendmsg(fd, &msg, 0);
                    if (sent < 0) perror("sendmsg");
                    assert(sent == 16);
                    dump_bytes("wl_shm_create_pool request",
                               create_pool,
                               sizeof(create_pool));
                    call_carmack("bound wl_shm_pool");
                    wl_shm_pool_id = new_id;
                    new_id++;

                    uint8_t create_buffer[32];
                    write_le32(create_buffer, wl_shm_pool_id);
                    write_le16(create_buffer + 4, 0);
                    write_le16(create_buffer + 6, 32);
                    write_le32(create_buffer + 8, new_id);
                    write_le32(create_buffer + 12, 0); // offset
                    write_le32(create_buffer + 16, physical_width);
                    write_le32(create_buffer + 20, physical_height);
                    write_le32(create_buffer + 24, stride);
                    write_le32(create_buffer + 28, ARGB8888);
                    dump_bytes(
                        "wl_shm_pool_create_buffer request", create_buffer, 32);
                    call_carmack("bound wl_buffer");
                    ssize_t create_buffer_written =
                        write(fd, create_buffer, 32);
                    assert(create_buffer_written == 32);
                    wl_buffer_id = new_id;
                    obj_op[obj_op_index(wl_buffer_id, 0)] = &&wl_buffer_release;
                    new_id++;

                    pixel_buffer = mmap(NULL,
                                        pool_size,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        shm_fd,
                                        0);
                    assert(pixel_buffer != MAP_FAILED);
                }
                goto done;
#endif
            wl_buffer_release:
                dump_bytes(
                    "wl_buffer_release event", msg_buffer + offset, size);
                goto done;
            xdg_wm_base_ping:
                dump_bytes("xdg_wm_base_ping event", msg_buffer + offset, size);
                assert(size == 12);
                uint8_t pong[12];
                write_le32(pong, xdg_wm_base_id);
                write_le16(pong + 4, 3);
                write_le16(pong + 6, 12);
                write_le32(pong + 8, read_le32(msg_buffer + offset + 8));
                dump_bytes("xdg_wm_base_pong request", pong, 12);
                ssize_t pong_written = write(fd, pong, 12);
                assert(pong_written == 12);
                goto done;
            xdg_surface_configure:
                dump_bytes(
                    "xdg_surface_configure event", msg_buffer + offset, size);
                assert(size == 12);

                uint8_t ack_configure[12];
                write_le32(ack_configure, xdg_surface_id);
                write_le16(ack_configure + 4, 4);
                write_le16(ack_configure + 6, 12);
                write_le32(ack_configure + 8,
                           read_le32(msg_buffer + offset + 8));
                dump_bytes(
                    "xdg_surface_ack_configure request", ack_configure, 12);
                ssize_t ack_configure_written = write(fd, ack_configure, 12);
                assert(ack_configure_written == 12);

                if (!wp_viewport_id) {
                    uint8_t get_viewport[16];
                    write_le32(get_viewport, wp_viewporter_id);
                    write_le16(get_viewport + 4, 1);
                    write_le16(get_viewport + 6, 16);
                    write_le32(get_viewport + 8, new_id);
                    write_le32(get_viewport + 12, wl_surface_id);
                    dump_bytes("wp_viewporter::get_viewport request",
                               get_viewport,
                               16);
                    call_carmack("bound: wp_viewport");
                    ssize_t get_viewport_written = write(fd, get_viewport, 16);
                    assert(get_viewport_written == 16);
                    wp_viewport_id = new_id;
                    new_id++;

                    // COMMENT: unecessary
                    // uint8_t set_source[24];
                    // write_le32(set_source, wp_viewport_id);
                    // write_le16(set_source + 4, 1);
                    // write_le16(set_source + 6, 24);
                    // write_le32(set_source + 8, 0);
                    // write_le32(set_source + 12, 0);
                    // write_le32(set_source + 16, physical_width);
                    // write_le32(set_source + 20, physical_height);
                    // dump_bytes(
                    //     "wp_viewport::set_source request", set_source, 24);
                    // ssize_t set_source_written = write(fd, set_source, 24);
                    // assert(set_source_written == 24);

                    uint8_t set_destination[16];
                    write_le32(set_destination, wp_viewport_id);
                    write_le16(set_destination + 4, 2);
                    write_le16(set_destination + 6, 16);
                    write_le32(set_destination + 8, logical_width);
                    write_le32(set_destination + 12, logical_height);
                    dump_bytes("wp_viewport::set_destination request",
                               set_destination,
                               16);
                    ssize_t set_destination_written =
                        write(fd, set_destination, 16);
                    assert(set_destination_written == 16);
                }
                //-------------------------------------------------------------
                // initial attach, initial frame, commit
                //-------------------------------------------------------------
                war_wayland_wl_surface_attach(
                    fd, wl_surface_id, wl_buffer_id, 0, 0);
                if (!wl_callback_id) {
                    war_wayland_wl_surface_frame(fd, wl_surface_id, new_id);
                    wl_callback_id = new_id;
                    obj_op[obj_op_index(wl_callback_id, 0)] =
                        &&wl_callback_done;
                    new_id++;
                }
                war_wayland_wl_surface_commit(fd, wl_surface_id);

                goto done;
            xdg_toplevel_configure:
                dump_bytes(
                    "xdg_toplevel_configure event", msg_buffer + offset, size);
                goto done;
            xdg_toplevel_close:
                dump_bytes(
                    "xdg_toplevel_close event", msg_buffer + offset, size);

                uint8_t xdg_toplevel_destroy[8];
                write_le32(xdg_toplevel_destroy, xdg_toplevel_id);
                write_le16(xdg_toplevel_destroy + 4, 0);
                write_le16(xdg_toplevel_destroy + 6, 8);
                ssize_t xdg_toplevel_destroy_written =
                    write(fd, xdg_toplevel_destroy, 8);
                dump_bytes(
                    "xdg_toplevel::destroy request", xdg_toplevel_destroy, 8);
                assert(xdg_toplevel_destroy_written == 8);

                uint8_t xdg_surface_destroy[8];
                write_le32(xdg_surface_destroy, xdg_surface_id);
                write_le16(xdg_surface_destroy + 4, 0);
                write_le16(xdg_surface_destroy + 6, 8);
                ssize_t xdg_surface_destroy_written =
                    write(fd, xdg_surface_destroy, 8);
                dump_bytes(
                    "xdg_surface::destroy request", xdg_surface_destroy, 8);
                assert(xdg_surface_destroy_written == 8);

                uint8_t wl_buffer_destroy[8];
                write_le32(wl_buffer_destroy, wl_buffer_id);
                write_le16(wl_buffer_destroy + 4, 0);
                write_le16(wl_buffer_destroy + 6, 8);
                ssize_t wl_buffer_destroy_written =
                    write(fd, wl_buffer_destroy, 8);
                dump_bytes("wl_buffer::destroy request", wl_buffer_destroy, 8);
                assert(wl_buffer_destroy_written == 8);

                uint8_t wl_surface_destroy[8];
                write_le32(wl_surface_destroy, wl_surface_id);
                write_le16(wl_surface_destroy + 4, 0);
                write_le16(wl_surface_destroy + 6, 8);
                ssize_t wl_surface_destroy_written =
                    write(fd, wl_surface_destroy, 8);
                dump_bytes(
                    "wl_surface::destroy request", wl_surface_destroy, 8);
                assert(wl_surface_destroy_written == 8);

#if DMABUF
                close(vulkan_context.dmabuf_fd);
                vulkan_context.dmabuf_fd = -1;
#endif
                goto done;
            xdg_toplevel_configure_bounds:
                dump_bytes("xdg_toplevel_configure_bounds event",
                           msg_buffer + offset,
                           size);
                goto done;
            xdg_toplevel_wm_capabilities:
                dump_bytes("xdg_toplevel_wm_capabilities event",
                           msg_buffer + offset,
                           size);
                goto done;
#if DMABUF
            zwp_linux_dmabuf_v1_format:
                dump_bytes("zwp_linux_dmabuf_v1_format event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_v1_modifier:
                dump_bytes("zwp_linux_dmabuf_v1_modifier event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_buffer_params_v1_created:
                dump_bytes(
                    "zwp_linux_buffer_params_v1_created", // COMMENT REFACTOR:
                                                          // to ::
                    msg_buffer + offset,
                    size);
                goto done;
            zwp_linux_buffer_params_v1_failed:
                dump_bytes("zwp_linux_buffer_params_v1_failed event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_done event",
                           msg_buffer + offset,
                           size);
                uint8_t create_params[12]; // REFACTOR: zero initialize
                write_le32(create_params, zwp_linux_dmabuf_v1_id);
                write_le16(create_params + 4, 1);
                write_le16(create_params + 6, 12);
                write_le32(create_params + 8, new_id);
                dump_bytes("zwp_linux_dmabuf_v1_create_params request",
                           create_params,
                           12);
                call_carmack("bound: zwp_linux_buffer_params_v1");
                ssize_t create_params_written = write(fd, create_params, 12);
                assert(create_params_written == 12);
                zwp_linux_buffer_params_v1_id = new_id;
                obj_op[obj_op_index(zwp_linux_buffer_params_v1_id, 0)] =
                    &&zwp_linux_buffer_params_v1_created;
                obj_op[obj_op_index(zwp_linux_buffer_params_v1_id, 1)] =
                    &&zwp_linux_buffer_params_v1_failed;
                new_id++; // COMMENT REFACTOR: move increment to declaration
                          // (one line it)

                uint8_t header[8];
                write_le32(header, zwp_linux_buffer_params_v1_id);
                write_le16(header + 4, 1);
                write_le16(header + 6, 28);
                uint8_t tail[20];
                write_le32(tail, 0);
                write_le32(tail + 4, 0);
                write_le32(tail + 8, stride);
                write_le32(tail + 12, 0);
                write_le32(tail + 16, 0);
                struct iovec iov[2] = {
                    {.iov_base = header, .iov_len = 8},
                    {.iov_base = tail, .iov_len = 20},
                };
                char cmsgbuf[CMSG_SPACE(sizeof(int))] = {0};
                struct msghdr msg = {0};
                msg.msg_iov = iov;
                msg.msg_iovlen = 2;
                msg.msg_control = cmsgbuf;
                msg.msg_controllen = sizeof(cmsgbuf);
                struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
                cmsg->cmsg_len = CMSG_LEN(sizeof(int));
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                *((int*)CMSG_DATA(cmsg)) = vulkan_context.dmabuf_fd;
                ssize_t dmabuf_sent = sendmsg(fd, &msg, 0);
                if (dmabuf_sent < 0) perror("sendmsg");
                assert(dmabuf_sent == 28);
#if DEBUG
                uint8_t full_msg[32] = {0};
                memcpy(full_msg, header, 8);
                memcpy(full_msg + 8, tail, 20);
#endif
                dump_bytes(
                    "zwp_linux_buffer_params_v1::add request", full_msg, 28);

                uint8_t create_immed[28]; // REFACTOR: maybe 0 initialize
                write_le32(
                    create_immed,
                    zwp_linux_buffer_params_v1_id); // COMMENT REFACTOR: is it
                                                    // faster to copy the
                                                    // incoming message header
                                                    // and increment
                                                    // accordingly?
                write_le16(create_immed + 4,
                           3); // COMMENT REFACTOR CONCERN: check for duplicate
                               // variables names
                write_le16(create_immed + 6, 28);
                write_le32(create_immed + 8, new_id);
                write_le32(create_immed + 12, physical_width);
                write_le32(create_immed + 16, physical_height);
                write_le32(create_immed + 20, DRM_FORMAT_ARGB8888);
                write_le32(create_immed + 24, 0);
                dump_bytes("zwp_linux_buffer_params_v1::create_immed request",
                           create_immed,
                           28);
                call_carmack("bound: wl_buffer");
                ssize_t create_immed_written = write(fd, create_immed, 28);
                assert(create_immed_written == 28);
                wl_buffer_id = new_id;
                obj_op[obj_op_index(wl_buffer_id, 0)] = &&wl_buffer_release;
                new_id++;

                uint8_t destroy[8];
                write_le32(destroy, zwp_linux_buffer_params_v1_id);
                write_le16(destroy + 4, 0);
                write_le16(destroy + 6, 8);
                ssize_t destroy_written = write(fd, destroy, 8);
                assert(destroy_written == 8);
                dump_bytes("zwp_linux_buffer_params_v1_id::destroy request",
                           destroy,
                           8);
                goto done;
            zwp_linux_dmabuf_feedback_v1_format_table:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_format_table event",
                           msg_buffer + offset,
                           size); // REFACTOR: event
                goto done;
            zwp_linux_dmabuf_feedback_v1_main_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_main_device event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_done event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_target_device:
                dump_bytes(
                    "zwp_linux_dmabuf_feedback_v1_tranche_target_device event",
                    msg_buffer + offset,
                    size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_formats:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_formats event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_flags:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_flags event",
                           msg_buffer + offset,
                           size);
                goto done;
#endif
            wp_linux_drm_syncobj_manager_v1_jump:
                dump_bytes("wp_linux_drm_syncobj_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            wl_compositor_jump:
                dump_bytes(
                    "wl_compositor_jump event", msg_buffer + offset, size);
                goto done;
            wl_surface_enter:
                dump_bytes("wl_surface_enter event", msg_buffer + offset, size);
                goto done;
            wl_surface_leave:
                dump_bytes("wl_surface_leave event", msg_buffer + offset, size);
                goto done;
            wl_surface_preferred_buffer_scale:
                dump_bytes("wl_surface_preferred_buffer_scale event",
                           msg_buffer + offset,
                           size);
                assert(size == 12);

                uint8_t set_buffer_scale[12];
                write_le32(set_buffer_scale, wl_surface_id);
                write_le16(set_buffer_scale + 4, 8);
                write_le16(set_buffer_scale + 6, 12);
                write_le32(set_buffer_scale + 8,
                           read_le32(msg_buffer + offset + 8));
                dump_bytes("wl_surface::set_buffer_scale request",
                           set_buffer_scale,
                           12);
                ssize_t set_buffer_scale_written =
                    write(fd, set_buffer_scale, 12);
                assert(set_buffer_scale_written == 12);

                war_wayland_holy_trinity(fd,
                                         wl_surface_id,
                                         wl_buffer_id,
                                         0,
                                         0,
                                         0,
                                         0,
                                         physical_width,
                                         physical_height);
                goto done;
            wl_surface_preferred_buffer_transform:
                dump_bytes("wl_surface_preferred_buffer_transform event",
                           msg_buffer + offset,
                           size);
                assert(size == 12);

                uint8_t set_buffer_transform[12];
                write_le32(set_buffer_transform, wl_surface_id);
                write_le16(set_buffer_transform + 4, 7);
                write_le16(set_buffer_transform + 6, 12);
                write_le32(set_buffer_transform + 8,
                           read_le32(msg_buffer + offset + 8));
                dump_bytes("wl_surface::set_buffer_transform request",
                           set_buffer_transform,
                           12);
                ssize_t set_buffer_transform_written =
                    write(fd, set_buffer_transform, 12);
                assert(set_buffer_transform_written == 12);

                war_wayland_holy_trinity(fd,
                                         wl_surface_id,
                                         wl_buffer_id,
                                         0,
                                         0,
                                         0,
                                         0,
                                         physical_width,
                                         physical_height);
                goto done;
            zwp_idle_inhibit_manager_v1_jump:
                dump_bytes("zwp_idle_inhibit_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwlr_layer_shell_v1_jump:
                dump_bytes("zwlr_layer_shell_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            zxdg_decoration_manager_v1_jump:
                dump_bytes("zxdg_decoration_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_relative_pointer_manager_v1_jump:
                dump_bytes("zwp_relative_pointer_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_pointer_constraints_v1_jump:
                dump_bytes("zwp_pointer_constraints_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            wp_presentation_clock_id:
                dump_bytes("wp_presentation_clock_id event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwlr_output_manager_v1_head:
                dump_bytes("zwlr_output_manager_v1_head event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwlr_output_manager_v1_done:
                dump_bytes("zwlr_output_manager_v1_done event",
                           msg_buffer + offset,
                           size);
                goto done;
            ext_foreign_toplevel_list_v1_toplevel:
                dump_bytes("ext_foreign_toplevel_list_v1_toplevel event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwlr_data_control_manager_v1_jump:
                dump_bytes("zwlr_data_control_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            wp_viewporter_jump:
                dump_bytes(
                    "wp_viewporter_jump event", msg_buffer + offset, size);
                goto done;
            wp_content_type_manager_v1_jump:
                dump_bytes("wp_content_type_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            wp_fractional_scale_manager_v1_jump:
                dump_bytes("wp_fractional_scale_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            xdg_activation_v1_jump:
                dump_bytes(
                    "xdg_activation_v1_jump event", msg_buffer + offset, size);
                goto done;
            zwp_virtual_keyboard_manager_v1_jump:
                dump_bytes("zwp_virtual_keyboard_manager_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            zwp_pointer_gestures_v1_jump:
                dump_bytes("zwp_pointer_gestures_v1_jump event",
                           msg_buffer + offset,
                           size);
                goto done;
            wl_seat_capabilities:
                dump_bytes(
                    "wl_seat_capabilities event", msg_buffer + offset, size);
                enum {
                    wl_seat_pointer = 0x01,
                    wl_seat_keyboard = 0x02,
                    wl_seat_touch = 0x04,
                };
                uint32_t capabilities = read_le32(msg_buffer + offset + 8);
                if (capabilities & wl_seat_keyboard) {
                    call_carmack("keyboard detected");
                    assert(size == 12);
                    uint8_t get_keyboard[12];
                    write_le32(get_keyboard, wl_seat_id);
                    write_le16(get_keyboard + 4, 1);
                    write_le16(get_keyboard + 6, 12);
                    write_le32(get_keyboard + 8, new_id);
                    dump_bytes("get_keyboard request", get_keyboard, 12);
                    call_carmack("bound: wl_keyboard",
                                 (const char*)msg_buffer + offset + 12);
                    ssize_t get_keyboard_written = write(fd, get_keyboard, 12);
                    assert(get_keyboard_written == 12);
                    wl_keyboard_id = new_id;
                    obj_op[obj_op_index(wl_keyboard_id, 0)] =
                        &&wl_keyboard_keymap;
                    obj_op[obj_op_index(wl_keyboard_id, 1)] =
                        &&wl_keyboard_enter;
                    obj_op[obj_op_index(wl_keyboard_id, 2)] =
                        &&wl_keyboard_leave;
                    obj_op[obj_op_index(wl_keyboard_id, 3)] = &&wl_keyboard_key;
                    obj_op[obj_op_index(wl_keyboard_id, 4)] =
                        &&wl_keyboard_modifiers;
                    obj_op[obj_op_index(wl_keyboard_id, 5)] =
                        &&wl_keyboard_repeat_info;
                    new_id++;
                }
                if (capabilities & wl_seat_pointer) {
                    call_carmack("pointer detected");
                    assert(size == 12);
                    uint8_t get_pointer[12];
                    write_le32(get_pointer, wl_seat_id);
                    write_le16(get_pointer + 4, 0);
                    write_le16(get_pointer + 6, 12);
                    write_le32(get_pointer + 8, new_id);
                    dump_bytes("get_pointer request", get_pointer, 12);
                    call_carmack("bound: wl_pointer");
                    ssize_t get_pointer_written = write(fd, get_pointer, 12);
                    assert(get_pointer_written == 12);
                    wl_pointer_id = new_id;
                    obj_op[obj_op_index(wl_pointer_id, 0)] = &&wl_pointer_enter;
                    obj_op[obj_op_index(wl_pointer_id, 1)] = &&wl_pointer_leave;
                    obj_op[obj_op_index(wl_pointer_id, 2)] =
                        &&wl_pointer_motion;
                    obj_op[obj_op_index(wl_pointer_id, 3)] =
                        &&wl_pointer_button;
                    obj_op[obj_op_index(wl_pointer_id, 4)] = &&wl_pointer_axis;
                    obj_op[obj_op_index(wl_pointer_id, 5)] = &&wl_pointer_frame;
                    obj_op[obj_op_index(wl_pointer_id, 6)] =
                        &&wl_pointer_axis_source;
                    obj_op[obj_op_index(wl_pointer_id, 7)] =
                        &&wl_pointer_axis_stop;
                    obj_op[obj_op_index(wl_pointer_id, 8)] =
                        &&wl_pointer_axis_discrete;
                    obj_op[obj_op_index(wl_pointer_id, 9)] =
                        &&wl_pointer_axis_value120;
                    obj_op[obj_op_index(wl_pointer_id, 10)] =
                        &&wl_pointer_axis_relative_direction;
                    new_id++;
                }
                if (capabilities & wl_seat_touch) {
                    call_carmack("touch detected");
                    assert(size == 12);
                    uint8_t get_touch[12];
                    write_le32(get_touch, wl_seat_id);
                    write_le16(get_touch + 4, 2);
                    write_le16(get_touch + 6, 12);
                    write_le32(get_touch + 8, new_id);
                    dump_bytes("get_touch request", get_touch, 12);
                    call_carmack("bound: wl_touch");
                    ssize_t get_touch_written = write(fd, get_touch, 12);
                    assert(get_touch_written == 12);
                    wl_touch_id = new_id;
                    obj_op[obj_op_index(wl_touch_id, 0)] = &&wl_touch_down;
                    obj_op[obj_op_index(wl_touch_id, 1)] = &&wl_touch_up;
                    obj_op[obj_op_index(wl_touch_id, 2)] = &&wl_touch_motion;
                    obj_op[obj_op_index(wl_touch_id, 3)] = &&wl_touch_frame;
                    obj_op[obj_op_index(wl_touch_id, 4)] = &&wl_touch_cancel;
                    obj_op[obj_op_index(wl_touch_id, 5)] = &&wl_touch_shape;
                    obj_op[obj_op_index(wl_touch_id, 6)] =
                        &&wl_touch_orientation;
                    new_id++;
                }
                goto done;
            wl_seat_name:
                dump_bytes("wl_seat_name event", msg_buffer + offset, size);
                call_carmack(
                    "seat: %s", (const char*)msg_buffer + offset + 12, size);
                goto done;
            wl_keyboard_keymap:
                dump_bytes(
                    "wl_keyboard_keymap event", msg_buffer + offset, size);
                goto done;
            wl_keyboard_enter:
                dump_bytes(
                    "wl_keyboard_enter event", msg_buffer + offset, size);
                goto done;
            wl_keyboard_leave:
                dump_bytes(
                    "wl_keyboard_leave event", msg_buffer + offset, size);
                goto done;
            wl_keyboard_key:
                dump_bytes("wl_keyboard_key event", msg_buffer + offset, size);
                uint32_t wl_key_state = read_le32(msg_buffer + offset + 8 + 12);
                switch (read_le32(msg_buffer + offset + 8 + 8)) {
                case KEY_K:
                    switch (wl_key_state) {
                    case 0:
                        // released (not pressed)
                        break;
                    case 1:
                        // pressed
                        row--;
                        break;
                    case 2:
                        // repeated
                        row--;
                        break;
                    }
                    break;
                case KEY_J:
                    switch (wl_key_state) {
                    case 0:
                        break;
                    case 1:
                        row++;
                        break;
                    case 2:
                        row++;
                        break;
                    }
                    break;
                case KEY_H:
                    switch (wl_key_state) {
                    case 0:
                        break;
                    case 1:
                        col--;
                        break;
                    case 2:
                        col--;
                        break;
                    }
                    break;
                case KEY_L:
                    switch (wl_key_state) {
                    case 0:
                        break;
                    case 1:
                        col++;
                        break;
                    case 2:
                        col++;
                        break;
                    }
                    break;
                case KEY_0:
                    switch (wl_key_state) {
                    case 0:
                        break;
                    case 1:
                        col = 0;
                        break;
                    case 2:
                        col = 0;
                        break;
                    }
                    break;
                }
                war_wayland_holy_trinity(fd,
                                         wl_surface_id,
                                         wl_buffer_id,
                                         0,
                                         0,
                                         0,
                                         0,
                                         physical_width,
                                         physical_height);
                goto done;
            wl_keyboard_modifiers:
                dump_bytes(
                    "wl_keyboard_modifiers event", msg_buffer + offset, size);
                goto done;
            wl_keyboard_repeat_info:
                dump_bytes(
                    "wl_keyboard_repeat_info event", msg_buffer + offset, size);
                goto done;
            wl_pointer_enter:
                dump_bytes("wl_pointer_enter event", msg_buffer + offset, size);
                goto done;
            wl_pointer_leave:
                dump_bytes("wl_pointer_leave event", msg_buffer + offset, size);
                goto done;
            wl_pointer_motion:
                dump_bytes(
                    "wl_pointer_motion event", msg_buffer + offset, size);
                cursor_x = (float)(int32_t)read_le32(msg_buffer + offset + 12) /
                           256.0f * scale_factor;
                cursor_y = (float)(int32_t)read_le32(msg_buffer + offset + 16) /
                           256.0f * scale_factor;
                goto done;
            wl_pointer_button:
                dump_bytes(
                    "wl_pointer_button event", msg_buffer + offset, size);
                switch (read_le32(msg_buffer + offset + 8 + 12)) {
                case 1:
                    if (read_le32(msg_buffer + offset + 8 + 8) == BTN_LEFT) {
                        col = (uint32_t)(cursor_x / col_width_px);
                        row =
                            (uint32_t)((cursor_y) / row_height_px); // flipped Y

                        war_wayland_holy_trinity(fd,
                                                 wl_surface_id,
                                                 wl_buffer_id,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 physical_width,
                                                 physical_height);
                    }
                }
                goto done;
            wl_pointer_axis:
                dump_bytes("wl_pointer_axis event", msg_buffer + offset, size);
                goto done;
            wl_pointer_frame:
                dump_bytes("wl_pointer_frame event", msg_buffer + offset, size);
                goto done;
            wl_pointer_axis_source:
                dump_bytes(
                    "wl_pointer_axis_source event", msg_buffer + offset, size);
                goto done;
            wl_pointer_axis_stop:
                dump_bytes(
                    "wl_pointer_axis_stop event", msg_buffer + offset, size);
                goto done;
            wl_pointer_axis_discrete:
                dump_bytes("wl_pointer_axis_discrete event",
                           msg_buffer + offset,
                           size);
                goto done;
            wl_pointer_axis_value120:
                dump_bytes("wl_pointer_axis_value120 event",
                           msg_buffer + offset,
                           size);
                goto done;
            wl_pointer_axis_relative_direction:
                dump_bytes("wl_pointer_axis_relative_direction event",
                           msg_buffer + offset,
                           size);
                goto done;
            wl_touch_down:
                dump_bytes("wl_touch_down event", msg_buffer + offset, size);
                goto done;
            wl_touch_up:
                dump_bytes("wl_touch_up event", msg_buffer + offset, size);
                goto done;
            wl_touch_motion:
                dump_bytes("wl_touch_motion event", msg_buffer + offset, size);
                goto done;
            wl_touch_frame:
                dump_bytes("wl_touch_frame event", msg_buffer + offset, size);
                goto done;
            wl_touch_cancel:
                dump_bytes("wl_touch_cancel event", msg_buffer + offset, size);
                goto done;
            wl_touch_shape:
                dump_bytes("wl_touch_shape event", msg_buffer + offset, size);
                goto done;
            wl_touch_orientation:
                dump_bytes(
                    "wl_touch_orientation event", msg_buffer + offset, size);
                goto done;
            wl_output_geometry:
                dump_bytes(
                    "wl_output_geometry event", msg_buffer + offset, size);
                goto done;
            wl_output_mode:
                dump_bytes("wl_output_mode event", msg_buffer + offset, size);
                goto done;
            wl_output_done:
                dump_bytes("wl_output_done event", msg_buffer + offset, size);
                goto done;
            wl_output_scale:
                dump_bytes("wl_output_scale event", msg_buffer + offset, size);
                goto done;
            wl_output_name:
                dump_bytes("wl_output_name event", msg_buffer + offset, size);
                goto done;
            wl_output_description:
                dump_bytes(
                    "wl_output_description event", msg_buffer + offset, size);
                goto done;
            wayland_default:
                dump_bytes("default event", msg_buffer + offset, size);
                goto done;
            done:
                offset += size;
                continue;
            }

            if (offset > 0) {
                memmove(
                    msg_buffer, msg_buffer + offset, msg_buffer_size - offset);
                msg_buffer_size -= offset;
            }
        }
    }

#if DMABUF
    close(vulkan_context.dmabuf_fd);
    vulkan_context.dmabuf_fd = -1;
#endif

    end("war_wayland_init");
}

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
                               size_t offset,
                               uint16_t size,
                               uint16_t new_id) {
    header("war_wayland_registry_bind");

    uint8_t bind[128];
    assert(size + 4 <= 128);
    memcpy(bind, msg_buffer + offset, size);
    write_le32(bind, 2);
    write_le16(bind + 4, 0);
    uint16_t total_size = read_le16(bind + 6) + 4;
    write_le16(bind + 6, total_size);
    write_le32(bind + size, new_id);

    dump_bytes("bind request", bind, size + 4);
    call_carmack("bound: %s", (const char*)msg_buffer + offset + 16);
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
