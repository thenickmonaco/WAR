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
    max_opcodes = 128,
};

int wayland_init() {
    header("wayland init");

    int fd = wayland_make_fd();
    assert(fd >= 0);

    uint8_t get_registry[12];

    uint32_t wl_display_id = 1;
    uint32_t wl_registry_id = 2;
    uint32_t wl_shm_id = 0;
    uint32_t wl_compositor_id = 0;
    uint32_t xdg_wm_base_id = 0;
    uint32_t wl_output_id = 0;
    uint32_t wl_seat_id = 0;
    uint32_t zwp_linux_dmabuf_v1_id = 0;
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
    // uint32_t wl_surface_id = 0;
    // uint32_t wl_keyboard_id = 0;
    // uint32_t wl_pointer_id = 0;
    // uint32_t wl_callback_id = 0;
    // uint32_t wl_buffer_id = 0;
    // uint32_t zwp_linux_dmabuf_feedback_v1_id = 0;
    // uint32_t zwp_linux_explicit_synchronization_v1_id = 0;

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
    obj_op[obj_op_index(wl_registry_id, 0)] = &&wl_registry_global;
    obj_op[obj_op_index(wl_registry_id, 1)] = &&wl_registry_global_remove;
    obj_op[obj_op_index(wl_display_id, 0)] = &&wl_display_ping;

    uint32_t new_id = wl_registry_id + 1;
    while (1) {
        int ret = poll(&pfd, 1, 16);
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
                    // call_carmack(
                    //     "invalid object/op: id=%u, op=%u", object_id,
                    //     opcode);
                    goto done;
                }

                size_t idx = obj_op_index(object_id, opcode);
                if (obj_op[idx]) {
                    goto* obj_op[idx];
                } else {
                    goto wayland_default;
                }

            wl_registry_global:
                dump_bytes("global msg", buffer + offset, size);
                call_carmack("iname: %s", (const char*)buffer + offset + 16);

                const char* iname = (const char*)buffer + offset + 16;
                if (strcmp(iname, "wl_shm") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wl_shm_id = new_id;
                    obj_op[obj_op_index(wl_shm_id, 0)] = &&wl_shm_format;
                    new_id++;
                } else if (strcmp(iname, "wl_compositor") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wl_compositor_id = new_id;
                    obj_op[obj_op_index(wl_compositor_id, 0)] =
                        &&wl_compositor_jump;
                    new_id++;
                } else if (strcmp(iname, "wl_output") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wl_output_id = new_id;
                    obj_op[obj_op_index(wl_output_id, 0)] =
                        &&wl_output_geometry;
                    new_id++;
                } else if (strcmp(iname, "wl_seat") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wl_seat_id = new_id;
                    obj_op[obj_op_index(wl_seat_id, 0)] = &&wl_seat_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_linux_dmabuf_v1_id = new_id;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 0)] =
                        &&zwp_linux_dmabuf_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "xdg_wm_base") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    xdg_wm_base_id = new_id;
                    obj_op[obj_op_index(xdg_wm_base_id, 0)] =
                        &&xdg_wm_base_ping;
                    new_id++;
                } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") ==
                           0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wp_linux_drm_syncobj_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_linux_drm_syncobj_manager_v1_id,
                                        0)] =
                        &&wp_linux_drm_syncobj_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_idle_inhibit_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_idle_inhibit_manager_v1_id, 0)] =
                        &&zwp_idle_inhibit_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zxdg_decoration_manager_v1_id = new_id;
                    obj_op[obj_op_index(zxdg_decoration_manager_v1_id, 0)] =
                        &&zxdg_decoration_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") ==
                           0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_relative_pointer_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_relative_pointer_manager_v1_id,
                                        0)] =
                        &&zwp_relative_pointer_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_pointer_constraints_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_constraints_v1_id, 0)] =
                        &&zwp_pointer_constraints_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwlr_output_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 0)] =
                        &&zwlr_output_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwlr_data_control_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_data_control_manager_v1_id, 0)] =
                        &&zwlr_data_control_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") ==
                           0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_virtual_keyboard_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_virtual_keyboard_manager_v1_id,
                                        0)] =
                        &&zwp_virtual_keyboard_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_viewporter") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wp_viewporter_id = new_id;
                    obj_op[obj_op_index(wp_viewporter_id, 0)] =
                        &&wp_viewporter_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_fractional_scale_manager_v1") ==
                           0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wp_fractional_scale_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_fractional_scale_manager_v1_id, 0)] =
                        &&wp_fractional_scale_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwp_pointer_gestures_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_gestures_v1_id, 0)] =
                        &&zwp_pointer_gestures_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    xdg_activation_v1_id = new_id;
                    obj_op[obj_op_index(xdg_activation_v1_id, 0)] =
                        &&xdg_activation_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_presentation") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wp_presentation_id = new_id;
                    obj_op[obj_op_index(wp_presentation_id, 0)] =
                        &&wp_presentation_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    zwlr_layer_shell_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_layer_shell_v1_id, 0)] =
                        &&zwlr_layer_shell_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    ext_foreign_toplevel_list_v1_id = new_id;
                    obj_op[obj_op_index(ext_foreign_toplevel_list_v1_id, 0)] =
                        &&ext_foreign_toplevel_list_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                    wayland_registry_bind_request(
                        fd, buffer, offset, size, new_id);
                    wp_content_type_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_content_type_manager_v1_id, 0)] =
                        &&wp_content_type_manager_v1_jump;
                    new_id++;
                }
                goto done;
            wl_registry_global_remove:
                dump_bytes("global_rm msg", buffer + offset, size);
                goto done;
            wl_display_ping:
                dump_bytes("wl_display_ping msg", buffer + offset, size);
                goto done;
            wl_shm_format:
                dump_bytes("wl_shm_fmt msg", buffer + offset, size);
                goto done;
            xdg_wm_base_ping:
                dump_bytes("xdg_wm_base ping msg", buffer + offset, size);
                goto done;
            zwp_linux_dmabuf_v1_jump:
                dump_bytes(
                    "zwp_linux_dmabuf_v1_jump msg", buffer + offset, size);
                goto done;
            wp_linux_drm_syncobj_manager_v1_jump:
                dump_bytes("wp_linux_drm_syncobj_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            wl_compositor_jump:
                dump_bytes("wl_compositor_jump msg", buffer + offset, size);
                goto done;
            zwp_idle_inhibit_manager_v1_jump:
                dump_bytes("zwp_idle_inhibit_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            zwlr_layer_shell_v1_jump:
                dump_bytes(
                    "zwlr_layer_shell_v1_jump msg", buffer + offset, size);
                goto done;
            zxdg_decoration_manager_v1_jump:
                dump_bytes("zxdg_decoration_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            zwp_relative_pointer_manager_v1_jump:
                dump_bytes("zwp_relative_pointer_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            zwp_pointer_constraints_v1_jump:
                dump_bytes("zwp_pointer_constraints_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            wp_presentation_jump:
                dump_bytes("wp_presentation_jump msg", buffer + offset, size);
                goto done;
            zwlr_output_manager_v1_jump:
                dump_bytes(
                    "zwlr_output_manager_v1_jump msg", buffer + offset, size);
                goto done;
            ext_foreign_toplevel_list_v1_jump:
                dump_bytes("ext_foreign_toplevel_list_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            zwlr_data_control_manager_v1_jump:
                dump_bytes("zwlr_data_control_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            wp_viewporter_jump:
                dump_bytes("wp_viewporter_jump msg", buffer + offset, size);
                goto done;
            wp_content_type_manager_v1_jump:
                dump_bytes("wp_content_type_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            wp_fractional_scale_manager_v1_jump:
                dump_bytes("wp_fractional_scale_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            xdg_activation_v1_jump:
                dump_bytes("xdg_activation_v1_jump msg", buffer + offset, size);
                goto done;
            zwp_virtual_keyboard_manager_v1_jump:
                dump_bytes("zwp_virtual_keyboard_manager_v1_jump msg",
                           buffer + offset,
                           size);
                goto done;
            zwp_pointer_gestures_v1_jump:
                dump_bytes(
                    "zwp_pointer_gestures_v1_jump msg", buffer + offset, size);
                goto done;
            wl_seat_jump:
                dump_bytes("wl_seat_jump msg", buffer + offset, size);
                goto done;
            wl_output_geometry:
                dump_bytes("wl_output_geometry msg", buffer + offset, size);
                goto done;
            wayland_default:
                dump_bytes("default msg", buffer + offset, size);
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

void wayland_registry_bind_request(
    int fd, uint8_t* buffer, size_t offset, uint16_t size, uint16_t new_id) {
    header("reg bind request");

    uint8_t bind[128];
    assert(size + 4 <= 128); // add 4 for new_id
    memcpy(bind, buffer + offset, size);
    write_le32(bind, 2);
    write_le16(bind + 4, 0);
    uint16_t total_size = read_le16(bind + 6) + 4;
    write_le16(bind + 6, total_size);
    write_le32(bind + size, new_id);

    dump_bytes("bind msg", bind, size + 4);
    call_carmack("bound: %s", (const char*)buffer + offset + 16);
    call_carmack("to id: %u", new_id);

    ssize_t bind_written = write(fd, bind, size + 4);
    assert(bind_written == size + 4);

    end("reg bind request");
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
