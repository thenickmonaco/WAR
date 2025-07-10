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

//=============================================================================
// src/wayland.c
//=============================================================================

#include "wayland.h"
#include "debug_macros.h"
#include "macros.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdrm/drm_fourcc.h> // DRM_FORMAT_ARGB8888
#include <libdrm/drm_mode.h>   // DRM_FORMAT_MOD_LINEAR
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
    max_opcodes = 128,
};

int wayland_init() {
    header("wayland init");

    int fd = wayland_make_fd();
    assert(fd >= 0);

    enum {
        width = 1920,
        height = 1080,
        stride = width * 4,
#if WL_SHM
        ARGB8888 = 0,
#endif
    };
#if DMABUF
    int dmabuf_fd = wayland_make_dma_fd();
    uint8_t has_ARGB8888 = 0; // COMMENT REMOVE: not needed?
#endif
#if WL_SHM
    int shm_fd = syscall(SYS_memfd_create, "shm", MFD_CLOEXEC);

    if (shm_fd < 0) {
        call_carmack("error: memfd_create");
        return -1;
    }

    if (syscall(SYS_ftruncate, shm_fd, stride * height) < 0) {
        call_carmack("error: ftruncate");
        close(shm_fd);
        return -1;
    }
#endif

#if DMABUF
    uint32_t serial = 0; // COMMENT REMOVE: not needed?
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
    uint32_t wl_compositor_id = 0;
    uint32_t wl_surface_id = 0;
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
    uint32_t wp_viewporter_id = 0;
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

    uint8_t buffer[4096] = {0};
    size_t buffer_size = 0;

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    void* obj_op[max_objects * max_opcodes] = {0};
    obj_op[obj_op_index(wl_display_id, 0)] = &&wl_display_error;
    obj_op[obj_op_index(wl_display_id, 1)] = &&wl_display_delete_id;
    obj_op[obj_op_index(wl_registry_id, 0)] = &&wl_registry_global;
    obj_op[obj_op_index(wl_registry_id, 1)] = &&wl_registry_global_remove;

    uint32_t new_id = wl_registry_id + 1;
    while (1) {
        int ret = poll(&pfd, 1, -1); // COMMENT OPTIMIZE: 16 ms vs. -1
        assert(ret >= 0);
        if (ret == 0) call_carmack("timeout");

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            call_carmack("wayland socket error or hangup: %s", strerror(errno));
            break;
        }

        if (pfd.revents & POLLIN) {
            ssize_t size_read =
                read(fd, buffer + buffer_size, sizeof(buffer) - buffer_size);
            assert(size_read > 0);
            buffer_size += size_read;

            size_t offset = 0;
            while (buffer_size - offset >= 8) {
                uint16_t size = read_le16(buffer + offset + 6);

                if ((size < 8) || (size > (buffer_size - offset))) { break; };

                uint32_t object_id = read_le32(buffer + offset);
                uint16_t opcode = read_le16(buffer + offset + 4);

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
                dump_bytes("global event", buffer + offset, size);
                call_carmack("iname: %s", (const char*)buffer + offset + 16);

                const char* iname = (const char*)buffer + offset +
                                    16; // COMMENT OPTIMIZE: perfect hash
                if (strcmp(iname, "wl_shm") == 0) {
#if WL_SHM
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wl_shm_id = new_id;
                    obj_op[obj_op_index(wl_shm_id, 0)] = &&wl_shm_format;
                    new_id++;
#endif
                } else if (strcmp(iname, "wl_compositor") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wl_compositor_id = new_id;
                    obj_op[obj_op_index(wl_compositor_id, 0)] =
                        &&wl_compositor_jump;
                    new_id++;
                } else if (strcmp(iname, "wl_output") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
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
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wl_seat_id = new_id;
                    obj_op[obj_op_index(wl_seat_id, 0)] =
                        &&wl_seat_capabilities;
                    obj_op[obj_op_index(wl_seat_id, 1)] = &&wl_seat_name;
                    new_id++;
                } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
#if DMABUF
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_linux_dmabuf_v1_id = new_id;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 0)] =
                        &&zwp_linux_dmabuf_v1_format;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 1)] =
                        &&zwp_linux_dmabuf_v1_modifier;
                    new_id++;

                    // COMMENT REMOVE: prob not needed since i have surface
                    // feedback uint8_t get_default_feedback[12];
                    // write_le32(get_default_feedback, zwp_linux_dmabuf_v1_id);
                    // write_le16(get_default_feedback + 4, 2);
                    // write_le16(get_default_feedback + 6, 12);
                    // write_le32(get_default_feedback + 8, new_id);
                    // dump_bytes("get_default_feedback request",
                    //            get_default_feedback,
                    //            12);
                    // call_carmack(
                    //     "bound: zwp_linux_dmabuf_feedback_v1"); // REFACTOR:
                    //     ":"
                    //                                             // after
                    //                                             bound
                    // ssize_t get_default_feedback_written =
                    //     write(fd, get_default_feedback, 12);
                    // assert(get_default_feedback_written == 12);
                    // zwp_linux_dmabuf_feedback_v1_id = new_id;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 0)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_done;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 1)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_format_table;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 2)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_main_device;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 3)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_tranche_done;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 4)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_tranche_target_device;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 5)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_tranche_formats;
                    // obj_op[obj_op_index(zwp_linux_dmabuf_feedback_v1_id, 6)]
                    // =
                    //     &&zwp_linux_dmabuf_feedback_v1_tranche_flags;
                    // new_id++;
#endif
                } else if (strcmp(iname, "xdg_wm_base") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    xdg_wm_base_id = new_id;
                    obj_op[obj_op_index(xdg_wm_base_id, 0)] =
                        &&xdg_wm_base_ping;
                    new_id++;
                } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") ==
                           0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wp_linux_drm_syncobj_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_linux_drm_syncobj_manager_v1_id,
                                        0)] =
                        &&wp_linux_drm_syncobj_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_idle_inhibit_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_idle_inhibit_manager_v1_id, 0)] =
                        &&zwp_idle_inhibit_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zxdg_decoration_manager_v1_id = new_id;
                    obj_op[obj_op_index(zxdg_decoration_manager_v1_id, 0)] =
                        &&zxdg_decoration_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") ==
                           0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_relative_pointer_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_relative_pointer_manager_v1_id,
                                        0)] =
                        &&zwp_relative_pointer_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_pointer_constraints_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_constraints_v1_id, 0)] =
                        &&zwp_pointer_constraints_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwlr_output_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 0)] =
                        &&zwlr_output_manager_v1_head;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 1)] =
                        &&zwlr_output_manager_v1_done;
                    new_id++;
                } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwlr_data_control_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_data_control_manager_v1_id, 0)] =
                        &&zwlr_data_control_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") ==
                           0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_virtual_keyboard_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_virtual_keyboard_manager_v1_id,
                                        0)] =
                        &&zwp_virtual_keyboard_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_viewporter") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wp_viewporter_id = new_id;
                    obj_op[obj_op_index(wp_viewporter_id, 0)] =
                        &&wp_viewporter_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_fractional_scale_manager_v1") ==
                           0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wp_fractional_scale_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_fractional_scale_manager_v1_id, 0)] =
                        &&wp_fractional_scale_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwp_pointer_gestures_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_gestures_v1_id, 0)] =
                        &&zwp_pointer_gestures_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    xdg_activation_v1_id = new_id;
                    obj_op[obj_op_index(xdg_activation_v1_id, 0)] =
                        &&xdg_activation_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_presentation") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    wp_presentation_id = new_id;
                    obj_op[obj_op_index(wp_presentation_id, 0)] =
                        &&wp_presentation_clock_id;
                    new_id++;
                } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    zwlr_layer_shell_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_layer_shell_v1_id, 0)] =
                        &&zwlr_layer_shell_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
                    ext_foreign_toplevel_list_v1_id = new_id;
                    obj_op[obj_op_index(ext_foreign_toplevel_list_v1_id, 0)] =
                        &&ext_foreign_toplevel_list_v1_toplevel;
                    new_id++;
                } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                    wayland_registry_bind(fd, buffer, offset, size, new_id);
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
#if WL_SHM
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

                    uint8_t commit[8];
                    write_le32(commit, wl_surface_id);
                    write_le16(commit + 4, 6);
                    write_le16(commit + 6, 8);
                    dump_bytes("wl_surface_commit request", commit, 8);
                    ssize_t commit_written = write(fd, commit, 8);
                    assert(commit_written == 8);
                }
#endif
                goto done;
            wl_registry_global_remove:
                dump_bytes("global_rm event", buffer + offset, size);
                goto done;
            wl_callback_done:
                dump_bytes("wl_callback::done event", buffer + offset, size);
                goto done;
            wl_display_error:
                dump_bytes("wl_display::error event", buffer + offset, size);
                goto done;
            wl_display_delete_id:
                dump_bytes(
                    "wl_display::delete_id event", buffer + offset, size);
                goto done;
#if WL_SHM
            wl_shm_format:
                dump_bytes("wl_shm_format event", buffer + offset, size);
                if (read_le32(buffer + offset + 8) == ARGB8888) {
                    uint8_t create_pool[12];
                    write_le32(create_pool, wl_shm_id); // object id
                    write_le16(create_pool + 4, 0);     // opcode 0
                    write_le16(create_pool + 6,
                               16); // message size = 12 + 4 bytes for pool size
                    write_le32(create_pool + 8, new_id); // new pool id
                    uint32_t pool_size = stride * height;
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
                    write_le32(create_buffer + 16, width);
                    write_le32(create_buffer + 20, height);
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
                }
                goto done;
#endif
            wl_buffer_release:
                dump_bytes("wl_buffer_release event", buffer + offset, size);
                goto done;
            xdg_wm_base_ping:
                dump_bytes("xdg_wm_base_ping event", buffer + offset, size);
                assert(size == 12);
                uint8_t pong[12];
                write_le32(pong, xdg_wm_base_id);
                write_le16(pong + 4, 3);
                write_le16(pong + 6, 12);
                write_le32(pong + 8, read_le32(buffer + offset + 8));
                dump_bytes("xdg_wm_base_pong request", pong, 12);
                ssize_t pong_written = write(fd, pong, 12);
                assert(pong_written == 12);
                goto done;
            xdg_surface_configure:
                dump_bytes(
                    "xdg_surface_configure event", buffer + offset, size);
                assert(size == 12);
#if DMABUF
                // CONCERN COMMENT: unsure
                if (!serial) {
                    serial = read_le32(buffer + offset + 8);
                    goto done;
                }
                uint8_t attach[20];
                write_le32(attach, wl_surface_id);
                write_le16(attach + 4, 1);
                write_le16(attach + 6, 20);
                write_le32(attach + 8, wl_buffer_id);
                write_le32(attach + 12, 0);
                write_le32(attach + 16, 0);
                dump_bytes("wl_surface_attach request", attach, 20);
                ssize_t attach_written = write(fd, attach, 20);
                assert(attach_written == 20);

                uint8_t damage[24];
                write_le32(damage, wl_surface_id);
                write_le16(damage + 4, 2);
                write_le16(damage + 6, 24);
                write_le32(damage + 8, 0);
                write_le32(damage + 12, 0);
                write_le32(damage + 16, width);
                write_le32(damage + 20, height);
                dump_bytes("wl_surface_damage request", damage, 24);
                ssize_t damage_written = write(fd, damage, 24);
                assert(damage_written == 24);

                uint8_t dmabuf_ack_configure[12];
                write_le32(dmabuf_ack_configure, xdg_surface_id);
                write_le16(dmabuf_ack_configure + 4, 4);
                write_le16(dmabuf_ack_configure + 6, 12);
                write_le32(dmabuf_ack_configure + 8,
                           read_le32(buffer + offset + 8));
                dump_bytes("xdg_surface_ack_configure request",
                           dmabuf_ack_configure,
                           12);
                ssize_t dmabuf_ack_configure_written =
                    write(fd, dmabuf_ack_configure, 12);
                assert(dmabuf_ack_configure_written == 12);

                uint8_t commit[8];
                write_le32(commit, wl_surface_id);
                write_le16(commit + 4, 6);
                write_le16(commit + 6, 8);
                dump_bytes("wl_surface_commit request", commit, 8);
                ssize_t commit_written = write(fd, commit, 8);
                assert(commit_written == 8);
#endif
#if WL_SHM
                uint8_t ack_configure[12];
                write_le32(ack_configure, xdg_surface_id);
                write_le16(ack_configure + 4, 4);
                write_le16(ack_configure + 6, 12);
                write_le32(ack_configure + 8, read_le32(buffer + offset + 8));
                dump_bytes(
                    "xdg_surface_ack_configure request", ack_configure, 12);
                ssize_t ack_configure_written = write(fd, ack_configure, 12);
                assert(ack_configure_written == 12);

                uint8_t shm_attach[20];
                write_le32(shm_attach, wl_surface_id);
                write_le16(shm_attach + 4, 1);
                write_le16(shm_attach + 6, 20);
                write_le32(shm_attach + 8, wl_buffer_id);
                write_le32(shm_attach + 12, 0);
                write_le32(shm_attach + 16, 0);
                dump_bytes("wl_surface_attach request", shm_attach, 20);
                ssize_t shm_attach_written = write(fd, shm_attach, 20);
                assert(shm_attach_written == 20);

                uint8_t shm_damage[24];
                write_le32(shm_damage, wl_surface_id);
                write_le16(shm_damage + 4, 2);
                write_le16(shm_damage + 6, 24);
                write_le32(shm_damage + 8, 0);
                write_le32(shm_damage + 12, 0);
                write_le32(shm_damage + 16, width);
                write_le32(shm_damage + 20, height);
                dump_bytes("wl_surface_damage request", shm_damage, 24);
                ssize_t shm_damage_written = write(fd, shm_damage, 24);
                assert(shm_damage_written == 24);

                uint8_t shm_commit[8];
                write_le32(shm_commit, wl_surface_id);
                write_le16(shm_commit + 4, 6);
                write_le16(shm_commit + 6, 8);
                dump_bytes("wl_surface_commit request", shm_commit, 8);
                ssize_t shm_commit_written = write(fd, shm_commit, 8);
                assert(shm_commit_written == 8);
#endif
                goto done;
            xdg_toplevel_configure:
                dump_bytes(
                    "xdg_toplevel_configure event", buffer + offset, size);
                goto done;
            xdg_toplevel_close:
                dump_bytes("xdg_toplevel_close event", buffer + offset, size);
                goto done;
            xdg_toplevel_configure_bounds:
                dump_bytes("xdg_toplevel_configure_bounds event",
                           buffer + offset,
                           size);
                goto done;
            xdg_toplevel_wm_capabilities:
                dump_bytes("xdg_toplevel_wm_capabilities event",
                           buffer + offset,
                           size);
                goto done;
#if DMABUF
            zwp_linux_dmabuf_v1_format:
                dump_bytes(
                    "zwp_linux_dmabuf_v1_format event", buffer + offset, size);
                goto done;
            zwp_linux_dmabuf_v1_modifier:
                dump_bytes("zwp_linux_dmabuf_v1_modifier event",
                           buffer + offset,
                           size);
                goto done;
            zwp_linux_buffer_params_v1_created:
                dump_bytes(
                    "zwp_linux_buffer_params_v1_created", // COMMENT REFACTOR:
                                                          // to ::
                    buffer + offset,
                    size);
                // uint8_t destroy[8];
                // write_le32(destroy, zwp_linux_buffer_params_v1_id);
                // write_le16(destroy + 4, 0);
                // write_le16(destroy + 6, 8);
                // ssize_t destroy_written = write(fd, destroy, 8);
                // assert(destroy_written == 8);
                // dump_bytes("zwp_linux_buffer_params_v1_id::destroy request",
                //            destroy,
                //            8);

                // wl_buffer_id = read_le32(buffer + offset + 8);
                // call_carmack("bound wl_buffer");
                // obj_op[obj_op_index(wl_buffer_id, 0)] = &&wl_buffer_release;

                // uint8_t first_attach[20];
                // write_le32(first_attach, wl_surface_id);
                // write_le16(first_attach + 4, 1);
                // write_le16(first_attach + 6, 20);
                // write_le32(first_attach + 8, wl_buffer_id);
                // write_le32(first_attach + 12, 0);
                // write_le32(first_attach + 16, 0);
                // dump_bytes("wl_surface_attach request", first_attach, 20);
                // ssize_t first_attach_written = write(fd, first_attach, 20);
                // assert(first_attach_written == 20);

                // uint8_t first_damage[24];
                // write_le32(first_damage, wl_surface_id);
                // write_le16(first_damage + 4, 2);
                // write_le16(first_damage + 6, 24);
                // write_le32(first_damage + 8, 0);
                // write_le32(first_damage + 12, 0);
                // write_le32(first_damage + 16, width);
                // write_le32(first_damage + 20, height);
                // dump_bytes("wl_surface_damage request", first_damage, 24);
                // ssize_t first_damage_written = write(fd, first_damage, 24);
                // assert(first_damage_written == 24);

                // assert(serial);
                // uint8_t first_dmabuf_ack_configure[12];
                // write_le32(first_dmabuf_ack_configure, xdg_surface_id);
                // write_le16(first_dmabuf_ack_configure + 4, 4);
                // write_le16(first_dmabuf_ack_configure + 6, 12);
                // write_le32(first_dmabuf_ack_configure + 8, serial);
                // dump_bytes("xdg_surface_ack_configure request",
                //            first_dmabuf_ack_configure,
                //            12);
                // ssize_t first_dmabuf_ack_configure_written =
                //     write(fd, first_dmabuf_ack_configure, 12);
                // assert(first_dmabuf_ack_configure_written == 12);

                // uint8_t first_commit[8];
                // write_le32(first_commit, wl_surface_id);
                // write_le16(first_commit + 4, 6);
                // write_le16(first_commit + 6, 8);
                // dump_bytes("wl_surface_commit request", first_commit, 8);
                // ssize_t first_commit_written = write(fd, first_commit, 8);
                // assert(first_commit_written == 8);
                goto done;
            zwp_linux_buffer_params_v1_failed:
                dump_bytes("zwp_linux_buffer_params_v1_failed event",
                           buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_done event",
                           buffer + offset,
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
                write_le16(header + 4, 0);
                write_le16(header + 6, 28);
                uint8_t tail[20];
                write_le32(tail + 0, 0);
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
                *((int*)CMSG_DATA(cmsg)) = dmabuf_fd;
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

                // COMMENT: create_immed vs. create
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
                write_le32(create_immed + 12, width);
                write_le32(create_immed + 16, height);
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

                // COMMENT REMOVE: ?
                // uint8_t destroy[8];
                // write_le32(destroy, zwp_linux_buffer_params_v1_id);
                // write_le16(destroy + 4, 0);
                // write_le16(destroy + 6, 8);
                // ssize_t destroy_written = write(fd, destroy, 8);
                // assert(destroy_written == 8);
                // dump_bytes("zwp_linux_buffer_params_v1_id::destroy request",
                //            destroy,
                //            8);

                // uint8_t attach_dmabuf[20];
                // write_le32(attach_dmabuf, wl_surface_id);
                // write_le16(attach_dmabuf + 4, 1);
                // write_le16(attach_dmabuf + 6, 20);
                // write_le32(attach_dmabuf + 8, wl_buffer_id);
                // write_le32(attach_dmabuf + 12, 0);
                // write_le32(attach_dmabuf + 16, 0);
                // dump_bytes("wl_surface_attach request", attach_dmabuf, 20);
                // ssize_t attach_dmabuf_written = write(fd, attach_dmabuf, 20);
                // assert(attach_dmabuf_written == 20);

                // uint8_t damage_dmabuf[24];
                // write_le32(damage_dmabuf, wl_surface_id);
                // write_le16(damage_dmabuf + 4, 2);
                // write_le16(damage_dmabuf + 6, 24);
                // write_le32(damage_dmabuf + 8, 0);
                // write_le32(damage_dmabuf + 12, 0);
                // write_le32(damage_dmabuf + 16, width);
                // write_le32(damage_dmabuf + 20, height);
                // dump_bytes("wl_surface_damage request", damage_dmabuf, 24);
                // ssize_t damage_dmabuf_written = write(fd, damage_dmabuf, 24);
                // assert(damage_dmabuf == 24);

                // uint8_t commit_dmabuf[8];
                // write_le32(commit_dmabuf, wl_surface_id);
                // write_le16(commit_dmabuf + 4, 6);
                // write_le16(commit_dmabuf + 6, 8);
                // dump_bytes("wl_surface_commit request", commit, 8);
                // ssize_t commit_dmabuf_written = write(fd, commit, 8);
                // assert(commit_dmabuf_written == 8);

                goto done;
            zwp_linux_dmabuf_feedback_v1_format_table:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_format_table event",
                           buffer + offset,
                           size); // REFACTOR: event
                goto done;
            zwp_linux_dmabuf_feedback_v1_main_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_main_device event",
                           buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_done event",
                           buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_target_device:
                dump_bytes(
                    "zwp_linux_dmabuf_feedback_v1_tranche_target_device event",
                    buffer + offset,
                    size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_formats:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_formats event",
                           buffer + offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_flags:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_flags event",
                           buffer + offset,
                           size);
                goto done;
#endif
            wp_linux_drm_syncobj_manager_v1_jump:
                dump_bytes("wp_linux_drm_syncobj_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            wl_compositor_jump:
                dump_bytes("wl_compositor_jump event", buffer + offset, size);
                goto done;
            wl_surface_enter:
                dump_bytes("wl_surface_enter event", buffer + offset, size);
                goto done;
            wl_surface_leave:
                dump_bytes("wl_surface_leave event", buffer + offset, size);
                goto done;
            wl_surface_preferred_buffer_scale:
                dump_bytes("wl_surface_preferred_buffer_scale event",
                           buffer + offset,
                           size);
                goto done;
            wl_surface_preferred_buffer_transform:
                dump_bytes("wl_surface_preferred_buffer_transform event",
                           buffer + offset,
                           size);
                goto done;
            zwp_idle_inhibit_manager_v1_jump:
                dump_bytes("zwp_idle_inhibit_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            zwlr_layer_shell_v1_jump:
                dump_bytes(
                    "zwlr_layer_shell_v1_jump event", buffer + offset, size);
                goto done;
            zxdg_decoration_manager_v1_jump:
                dump_bytes("zxdg_decoration_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            zwp_relative_pointer_manager_v1_jump:
                dump_bytes("zwp_relative_pointer_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            zwp_pointer_constraints_v1_jump:
                dump_bytes("zwp_pointer_constraints_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            wp_presentation_clock_id:
                dump_bytes(
                    "wp_presentation_clock_id event", buffer + offset, size);
                goto done;
            zwlr_output_manager_v1_head:
                dump_bytes(
                    "zwlr_output_manager_v1_head event", buffer + offset, size);
                goto done;
            zwlr_output_manager_v1_done:
                dump_bytes(
                    "zwlr_output_manager_v1_done event", buffer + offset, size);
                goto done;
            ext_foreign_toplevel_list_v1_toplevel:
                dump_bytes("ext_foreign_toplevel_list_v1_toplevel event",
                           buffer + offset,
                           size);
                goto done;
            zwlr_data_control_manager_v1_jump:
                dump_bytes("zwlr_data_control_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            wp_viewporter_jump:
                dump_bytes("wp_viewporter_jump event", buffer + offset, size);
                goto done;
            wp_content_type_manager_v1_jump:
                dump_bytes("wp_content_type_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            wp_fractional_scale_manager_v1_jump:
                dump_bytes("wp_fractional_scale_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            xdg_activation_v1_jump:
                dump_bytes(
                    "xdg_activation_v1_jump event", buffer + offset, size);
                goto done;
            zwp_virtual_keyboard_manager_v1_jump:
                dump_bytes("zwp_virtual_keyboard_manager_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            zwp_pointer_gestures_v1_jump:
                dump_bytes("zwp_pointer_gestures_v1_jump event",
                           buffer + offset,
                           size);
                goto done;
            wl_seat_capabilities:
                dump_bytes("wl_seat_capabilities event", buffer + offset, size);
                enum {
                    wl_seat_pointer = 0x01,
                    wl_seat_keyboard = 0x02,
                    wl_seat_touch = 0x04,
                };
                uint32_t capabilities = read_le32(buffer + offset + 8);
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
                                 (const char*)buffer + offset + 12);
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
                dump_bytes("wl_seat_name event", buffer + offset, size);
                call_carmack(
                    "seat: %s", (const char*)buffer + offset + 12, size);
                goto done;
            wl_keyboard_keymap:
                dump_bytes("wl_keyboard_keymap event", buffer + offset, size);
                goto done;
            wl_keyboard_enter:
                dump_bytes("wl_keyboard_enter event", buffer + offset, size);
                goto done;
            wl_keyboard_leave:
                dump_bytes("wl_keyboard_leave event", buffer + offset, size);
                goto done;
            wl_keyboard_key:
                dump_bytes("wl_keyboard_key event", buffer + offset, size);
                goto done;
            wl_keyboard_modifiers:
                dump_bytes(
                    "wl_keyboard_modifiers event", buffer + offset, size);
                goto done;
            wl_keyboard_repeat_info:
                dump_bytes(
                    "wl_keyboard_repeat_info event", buffer + offset, size);
                goto done;
            wl_pointer_enter:
                dump_bytes("wl_pointer_enter event", buffer + offset, size);
                goto done;
            wl_pointer_leave:
                dump_bytes("wl_pointer_leave event", buffer + offset, size);
                goto done;
            wl_pointer_motion:
                dump_bytes("wl_pointer_motion event", buffer + offset, size);
                goto done;
            wl_pointer_button:
                dump_bytes("wl_pointer_button event", buffer + offset, size);
                goto done;
            wl_pointer_axis:
                dump_bytes("wl_pointer_axis event", buffer + offset, size);
                goto done;
            wl_pointer_frame:
                dump_bytes("wl_pointer_frame event", buffer + offset, size);
                goto done;
            wl_pointer_axis_source:
                dump_bytes(
                    "wl_pointer_axis_source event", buffer + offset, size);
                goto done;
            wl_pointer_axis_stop:
                dump_bytes("wl_pointer_axis_stop event", buffer + offset, size);
                goto done;
            wl_pointer_axis_discrete:
                dump_bytes(
                    "wl_pointer_axis_discrete event", buffer + offset, size);
                goto done;
            wl_pointer_axis_value120:
                dump_bytes(
                    "wl_pointer_axis_value120 event", buffer + offset, size);
                goto done;
            wl_pointer_axis_relative_direction:
                dump_bytes("wl_pointer_axis_relative_direction event",
                           buffer + offset,
                           size);
                goto done;
            wl_touch_down:
                dump_bytes("wl_touch_down event", buffer + offset, size);
                goto done;
            wl_touch_up:
                dump_bytes("wl_touch_up event", buffer + offset, size);
                goto done;
            wl_touch_motion:
                dump_bytes("wl_touch_motion event", buffer + offset, size);
                goto done;
            wl_touch_frame:
                dump_bytes("wl_touch_frame event", buffer + offset, size);
                goto done;
            wl_touch_cancel:
                dump_bytes("wl_touch_cancel event", buffer + offset, size);
                goto done;
            wl_touch_shape:
                dump_bytes("wl_touch_shape event", buffer + offset, size);
                goto done;
            wl_touch_orientation:
                dump_bytes("wl_touch_orientation event", buffer + offset, size);
                goto done;
            wl_output_geometry:
                dump_bytes("wl_output_geometry event", buffer + offset, size);
                goto done;
            wl_output_mode:
                dump_bytes("wl_output_mode event", buffer + offset, size);
                goto done;
            wl_output_done:
                dump_bytes("wl_output_done event", buffer + offset, size);
                goto done;
            wl_output_scale:
                dump_bytes("wl_output_scale event", buffer + offset, size);
                goto done;
            wl_output_name:
                dump_bytes("wl_output_name event", buffer + offset, size);
                goto done;
            wl_output_description:
                dump_bytes(
                    "wl_output_description event", buffer + offset, size);
                goto done;
            wayland_default:
                dump_bytes("default event", buffer + offset, size);
                goto done;
            done:
                offset += size;
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

int wayland_make_dma_fd() {
    const char* dri_path = "/dev/dri";
    DIR* dir = opendir(dri_path);
    if (!dir) {
        perror("opendir /dev/dri");
        return -1;
    }

    struct dirent* entry;
    int fd = -1;

    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "renderD", 7) == 0) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dri_path, entry->d_name);

            fd = open(path, O_RDWR | O_CLOEXEC);
            if (fd >= 0) {
                call_carmack("node: %s", path);
                break;
            }
        }
    }

    closedir(dir);
    return fd;
}

void wayland_registry_bind(
    int fd, uint8_t* buffer, size_t offset, uint16_t size, uint16_t new_id) {
    header("reg_bind request");

    uint8_t bind[128];
    assert(size + 4 <= 128);
    memcpy(bind, buffer + offset, size);
    write_le32(bind, 2);
    write_le16(bind + 4, 0);
    uint16_t total_size = read_le16(bind + 6) + 4;
    write_le16(bind + 6, total_size);
    write_le32(bind + size, new_id);

    dump_bytes("bind request", bind, size + 4);
    call_carmack("bound: %s", (const char*)buffer + offset + 16);
    call_carmack("to id: %u", new_id);

    ssize_t bind_written = write(fd, bind, size + 4);
    assert(bind_written == size + 4);

    end("reg_bind request");
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
