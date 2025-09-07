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
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

#include "war_alsa.c"
#include "war_drm.c"
#include "war_vulkan.c"
#include "war_wayland.c"

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_main.h"

#include <alsa/asoundlib.h>
#include <alsa/timer.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

int main() {
    CALL_CARMACK("main");
    uint8_t war_window_render_to_audio_ring_buffer[ring_buffer_size];
    uint8_t war_audio_to_window_render_ring_buffer[ring_buffer_size];
    uint8_t write_to_audio_index = 0;
    uint8_t read_from_audio_index = 0;
    uint8_t write_to_window_render_index = 0;
    uint8_t read_from_window_render_index = 0;
    war_thread_args war_ring_buffer_thread_args = {
        .window_render_to_audio_ring_buffer =
            war_window_render_to_audio_ring_buffer,
        .audio_to_window_render_ring_buffer =
            war_audio_to_window_render_ring_buffer,
        .write_to_audio_index = &write_to_audio_index,
        .read_from_audio_index = &read_from_audio_index,
        .write_to_window_render_index = &write_to_window_render_index,
        .read_from_window_render_index = &read_from_window_render_index,
    };
    pthread_t war_window_render_thread;
    pthread_t war_audio_thread;

    pthread_create(&war_window_render_thread,
                   NULL,
                   war_window_render,
                   &war_ring_buffer_thread_args);
    pthread_create(
        &war_audio_thread, NULL, war_audio, &war_ring_buffer_thread_args);

    pthread_join(war_window_render_thread, NULL);
    pthread_join(war_audio_thread, NULL);
    END("main");
    return 0;
}

void* war_window_render(void* args) {
    header("war_window_render");
    war_thread_args* a = (war_thread_args*)args;
    uint8_t* window_render_to_audio_ring_buffer =
        a->window_render_to_audio_ring_buffer;
    uint8_t* write_to_audio_index = a->write_to_audio_index;
    uint8_t* audio_to_window_render_ring_buffer =
        a->audio_to_window_render_ring_buffer;
    uint8_t* read_from_audio_index = a->read_from_audio_index;
    uint8_t* write_to_window_render_index = a->write_to_window_render_index;

    // const uint32_t internal_width = 1920;
    // const uint32_t internal_height = 1080;

    uint32_t physical_width = 2560;
    uint32_t physical_height = 1600;
    const uint32_t stride = physical_width * 4;

#if DMABUF
    war_vulkan_context vk_ctx =
        war_vulkan_init(physical_width, physical_height);
    assert(vk_ctx.dmabuf_fd >= 0);
    uint32_t zwp_linux_dmabuf_v1_id = 0;
    uint32_t zwp_linux_buffer_params_v1_id = 0;
    uint32_t zwp_linux_dmabuf_feedback_v1_id = 0;
#endif

    float scale_factor = 1.483333;
    const uint32_t logical_width =
        (uint32_t)floor(physical_width / scale_factor);
    const uint32_t logical_height =
        (uint32_t)floor(physical_height / scale_factor);

    uint32_t num_rows_for_status_bars = 3;
    uint32_t num_cols_for_line_numbers = 3;
    uint32_t viewport_cols = (uint32_t)(physical_width / vk_ctx.cell_width);
    uint32_t viewport_rows = (uint32_t)(physical_height / vk_ctx.cell_height);
    uint32_t visible_rows =
        (uint32_t)((physical_height -
                    ((float)num_rows_for_status_bars * vk_ctx.cell_height)) /
                   vk_ctx.cell_height);
    war_input_cmd_context ctx = {
        .now = 0,
        .mode = MODE_NORMAL,
        .hud_state = SHOW_PIANO,
        .col = 0,
        .row = 60,
        .sub_col = 0,
        .sub_row = 0,
        .navigation_whole_number_col = 1,
        .navigation_whole_number_row = 1,
        .navigation_sub_cells_col = 1,
        .navigation_sub_cells_row = 1,
        .previous_navigation_whole_number_col = 1,
        .previous_navigation_whole_number_row = 1,
        .previous_navigation_sub_cells_col = 1,
        .previous_navigation_sub_cells_row = 1,
        .f_navigation_whole_number = 1,
        .t_navigation_sub_cells = 1,
        .t_navigation_whole_number = 1,
        .f_navigation_sub_cells = 1,
        .cursor_width_whole_number = 1,
        .cursor_width_sub_col = 1,
        .cursor_width_sub_cells = 1,
        .f_cursor_width_whole_number = 1,
        .f_cursor_width_sub_cells = 1,
        .t_cursor_width_whole_number = 1,
        .t_cursor_width_sub_cells = 1,
        .gridline_splits = {4, 1, 0, 0},
        .bottom_row = 60 - visible_rows / 2 + 1,
        .top_row = 60 + visible_rows / 2,
        .left_col = 0,
        .right_col =
            (uint32_t)((physical_width - ((float)num_cols_for_line_numbers *
                                          vk_ctx.cell_width)) /
                       vk_ctx.cell_width) -
            1,
        .col_increment = 1,
        .row_increment = 1,
        .col_leap_increment = 13,
        .row_leap_increment = 7,
        .cursor_x = 0,
        .cursor_y = 0,
        .numeric_prefix = 0,
        .zoom_scale = 1.0f, // 1.0 = normal, <1 = zoom out, >1 = zoom in
        .max_zoom_scale = 5.0f,
        .min_zoom_scale = 0.1f,
        .num_rows_for_status_bars = num_rows_for_status_bars,
        .num_cols_for_line_numbers = num_cols_for_line_numbers,
        .panning_x = 0.0f,
        .panning_y = 0.0f,
        .zoom_increment = 0.1f,
        .zoom_leap_increment = 0.5f,
        .anchor_x = 0.0f,
        .anchor_y = 0.0f,
        .anchor_ndc_x = 0.0f,
        .anchor_ndc_y = 0.0f,
        .viewport_cols = viewport_cols,
        .viewport_rows = viewport_rows,
        .scroll_margin_cols = 0, // cols from visible min/max col
        .scroll_margin_rows = 0, // rows from visible min/max row
        .cell_width = vk_ctx.cell_width,
        .cell_height = vk_ctx.cell_height,
        .physical_width = physical_width,
        .physical_height = physical_height,
        .logical_width = logical_width,
        .logical_height = logical_height,
        .max_col = 144635, // shaking start happening after 289290,
                           // line numbers thinning happens after 144635
        .max_row = MAX_MIDI_NOTES - 1,
        .min_col = 0,
        .min_row = 0,
        .input_sequence = {0},
        .num_chars_in_sequence = 0,
        .layer_count = (float)LAYER_COUNT,
    };
    for (int i = 0; i < LAYER_COUNT; i++) {
        ctx.layers[i] = i / ctx.layer_count;
    }

    const uint64_t repeat_delay_us = 150000;
    const uint64_t repeat_rate_us = 40000;
    uint32_t repeat_keysym = 0;
    uint8_t repeat_mod = 0;
    bool repeating = false;
    bool goto_cmd_repeat_done = false;
    const uint64_t timeout_duration_us = 500000;
    uint16_t timeout_state_index = 0;
    uint64_t timeout_start_us = 0;
    bool timeout = false;
    bool goto_cmd_timeout_done = false;
    war_fsm_state fsm[MAX_STATES];
    uint16_t current_state_index = 0;
    for (int i = 0; i < MAX_STATES; i++) {
        fsm[i].is_terminal = false;
        memset(fsm[i].command, 0, sizeof(fsm[i].command));
        memset(fsm[i].next_state, 0, sizeof(fsm[i].next_state));
    }
    bool key_down[MAX_KEYSYM][MAX_MOD];
    uint64_t key_last_event_us[MAX_KEYSYM][MAX_MOD];
    uint64_t fsm_state_last_event_us = 0;

    uint32_t mod_shift;
    uint32_t mod_ctrl;
    uint32_t mod_alt;
    uint32_t mod_logo;
    uint32_t mod_caps;
    uint32_t mod_num;
    uint32_t mod_fn;
    struct xkb_context* xkb_context;
    struct xkb_state* xkb_state;

    enum war_pixel_format {
        ARGB8888 = 0,
    };

#if WL_SHM
    int shm_fd = syscall(SYS_memfd_create, "shm", MFD_CLOEXEC);
    if (shm_fd < 0) {
        call_carmack("error: memfd_create");
        return NULL;
    }
    if (syscall(SYS_ftruncate, shm_fd, stride * physical_height) < 0) {
        call_carmack("error: ftruncate");
        close(shm_fd);
        return NULL;
    }
    uint32_t wl_shm_id = 0;
    uint32_t wl_shm_pool_id = 0;
    void* pixel_buffer;
#endif

    uint32_t wl_display_id = 1;
    uint32_t wl_registry_id = 2;
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

    int fd = war_wayland_make_fd();
    assert(fd >= 0);

    uint8_t get_registry[12];
    war_write_le32(get_registry, wl_display_id);
    war_write_le16(get_registry + 4, 1);
    war_write_le16(get_registry + 6, 12);
    war_write_le32(get_registry + 8, wl_registry_id);
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

    uint32_t max_viewport_cols =
        (uint32_t)(physical_width / (vk_ctx.cell_width * ctx.min_zoom_scale));
    uint32_t max_viewport_rows =
        (uint32_t)(physical_height / (vk_ctx.cell_height * ctx.min_zoom_scale));
    uint32_t max_gridlines_per_split = max_viewport_cols + max_viewport_rows;

    // --- WAR_NOTE_QUADS ALLOCATION WITH 32-BYTE PER-ARRAY ALIGNMENT ---
    size_t num_uint32_arrays = 12;
    size_t num_uint64_arrays = 1;
    size_t num_float_arrays = 2;
    size_t padding_per_array = 31;
    size_t note_quads_total_size =
        num_uint32_arrays *
            (sizeof(uint32_t) * max_note_quads + padding_per_array) +
        num_float_arrays *
            (sizeof(float) * max_note_quads + padding_per_array) +
        num_uint64_arrays *
            (sizeof(uint64_t) * max_note_quads + padding_per_array);
    void* note_quads_block = NULL;
    int note_quads_res =
        posix_memalign(&note_quads_block, 32, note_quads_total_size);
    assert(note_quads_res == 0 && note_quads_block != NULL);
    uint8_t* note_quads_p = (uint8_t*)note_quads_block;
    war_note_quads note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.timestamp = (uint64_t*)note_quads_p;
    note_quads_p += sizeof(uint64_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.col = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.row = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.sub_col = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.sub_row = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.sub_cells_col = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.cursor_width_whole_number = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.cursor_width_sub_col = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.cursor_width_sub_cells = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.color = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.outline_color = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.strength = (float*)note_quads_p;
    note_quads_p += sizeof(float) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.voice = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.hidden = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    note_quads_p = ALIGN32(note_quads_p);
    note_quads.mute = (uint32_t*)note_quads_p;
    note_quads_p += sizeof(uint32_t) * max_note_quads;
    assert(note_quads_p <= (uint8_t*)note_quads_block + note_quads_total_size);
    uint32_t note_quads_count = 0;
    uint32_t* note_quads_in_x = malloc(sizeof(uint32_t) * max_note_quads);
    uint32_t note_quads_in_x_count = 0;
    war_quad_vertex* quad_vertices =
        malloc(sizeof(war_quad_vertex) * max_quads);
    uint32_t quad_vertices_count = 0;
    uint16_t* quad_indices = malloc(sizeof(uint16_t) * max_quads);
    uint32_t quad_indices_count = 0;
    war_text_vertex* text_vertices =
        malloc(sizeof(war_text_vertex) * max_text_quads);
    uint32_t text_vertices_count = 0;
    uint16_t* text_indices = malloc(sizeof(uint16_t) * max_text_quads);
    uint32_t text_indices_count = 0;

    const uint64_t frame_duration_us = 16666; // 60 fps
    uint64_t last_frame_time = war_get_monotonic_time_us();
    int end_window_render = 0;
    //-------------------------------------------------------------------------
    // window_render loop
    //-------------------------------------------------------------------------
    while (!end_window_render) {
        ctx.now = war_get_monotonic_time_us();
        if (ctx.now - last_frame_time >= frame_duration_us) {
            last_frame_time += frame_duration_us;
        }
        // COMMENT ADD: SYNC REPEAT TO AUDIO THREAD (AUDIO_GET_TIMESTAMP)
        // if (*read_from_audio_index != *write_to_window_render_index) {
        //     switch (audio_to_window_render_ring_buffer
        //                 [*read_from_audio_index]) {
        //     case AUDIO_GET_TIMESTAMP:
        //         *read_from_audio_index =
        //             (*read_from_audio_index + 1) & 0xFF;
        //        size_t space_till_end =
        //            ring_buffer_size - *read_from_audio_index;
        //        if (space_till_end < 16) { *read_from_audio_index = 0; }
        //        current_repeat_us =
        //            read_le64(audio_to_window_render_ring_buffer +
        //                      *read_from_audio_index) *
        //                1000000 +
        //            read_le64(audio_to_window_render_ring_buffer +
        //                      *read_from_audio_index + 8);
        //        call_carmack("current_repeat_us: %lu", current_repeat_us);
        //        *read_from_audio_index =
        //            (*read_from_audio_index + 16) & 0xFF;
        //        break;
        //    }
        //}
        // COMMENT TODO ISSUE: fix repeat_key switching between alt+j and
        // alt+k
        //---------------------------------------------------------------------
        // KEY REPEATS
        //---------------------------------------------------------------------
        if (repeat_keysym) {
            uint32_t k = repeat_keysym;
            uint8_t m = repeat_mod;
            if (key_down[k][m]) {
                uint64_t elapsed = ctx.now - key_last_event_us[k][m];
                if (!repeating) {
                    // still waiting for initial delay
                    if (elapsed >= repeat_delay_us) {
                        repeating = true;
                        key_last_event_us[k][m] = ctx.now; // reset timer
                    }
                } else {
                    // normal repeating
                    if (elapsed >= repeat_rate_us) {
                        key_last_event_us[k][m] = ctx.now;
                        uint16_t next_state_index =
                            fsm[current_state_index].next_state[k][m];
                        if (next_state_index != 0) {
                            current_state_index = next_state_index;
                            fsm_state_last_event_us = ctx.now;
                            if (fsm[current_state_index].is_terminal &&
                                !war_state_is_prefix(current_state_index,
                                                     fsm)) {
                                uint16_t temp = current_state_index;
                                current_state_index = 0;
                                goto_cmd_repeat_done = true;
                                goto* fsm[temp].command[ctx.mode];
                            }
                        }
                    }
                }
            }
        } else {
            repeat_keysym = 0;
            repeat_mod = 0;
            repeating = false;
        }
    cmd_repeat_done:
        //--------------------------------------------------------------------
        // KEY TIMEOUTS
        //--------------------------------------------------------------------
        if (timeout && ctx.now >= timeout_start_us + timeout_duration_us) {
            uint16_t temp = timeout_state_index;
            timeout = false;
            timeout_state_index = 0;
            timeout_start_us = 0;
            goto_cmd_timeout_done = true;
            // clear current
            current_state_index = 0;
            fsm_state_last_event_us = ctx.now;
            goto* fsm[temp].command[ctx.mode];
        }
    cmd_timeout_done:
        //---------------------------------------------------------------------
        // WAYLAND MESSAGE PARSING
        //---------------------------------------------------------------------
        int ret = poll(&pfd, 1, 0);
        assert(ret >= 0);
        // if (ret == 0) { call_carmack("timeout"); }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            call_carmack("wayland socket error or hangup: %s", strerror(errno));
            break;
        }
        if (pfd.revents & POLLIN) {
            struct msghdr poll_msg_hdr = {0};
            struct iovec poll_iov;
            poll_iov.iov_base = msg_buffer + msg_buffer_size;
            poll_iov.iov_len = sizeof(msg_buffer) - msg_buffer_size;
            poll_msg_hdr.msg_iov = &poll_iov;
            poll_msg_hdr.msg_iovlen = 1;
            char poll_ctrl_buf[CMSG_SPACE(sizeof(int) * 4)];
            poll_msg_hdr.msg_control = poll_ctrl_buf;
            poll_msg_hdr.msg_controllen = sizeof(poll_ctrl_buf);
            ssize_t size_read = recvmsg(fd, &poll_msg_hdr, 0);
            assert(size_read > 0);
            msg_buffer_size += size_read;
            size_t msg_buffer_offset = 0;
            while (msg_buffer_size - msg_buffer_offset >= 8) {
                uint16_t size =
                    war_read_le16(msg_buffer + msg_buffer_offset + 6);
                if ((size < 8) ||
                    (size > (msg_buffer_size - msg_buffer_offset))) {
                    break;
                };
                uint32_t object_id =
                    war_read_le32(msg_buffer + msg_buffer_offset);
                uint16_t opcode =
                    war_read_le16(msg_buffer + msg_buffer_offset + 4);
                if (object_id >= max_objects || opcode >= max_opcodes) {
                    // COMMENT CONCERN: INVALID OBJECT/OP 27 TIMES!
                    // call_carmack(
                    //    "invalid object/op: id=%u, op=%u", object_id,
                    //    opcode);
                    goto done;
                }
                size_t idx = obj_op_index(object_id, opcode);
                if (obj_op[idx]) {
                    goto* obj_op[idx];
                } else {
                    goto wayland_default;
                }
            wl_registry_global:
                dump_bytes(
                    "global event", msg_buffer + msg_buffer_offset, size);
                call_carmack("iname: %s",
                             (const char*)msg_buffer + msg_buffer_offset + 16);

                const char* iname = (const char*)msg_buffer +
                                    msg_buffer_offset +
                                    16; // COMMENT OPTIMIZE: perfect hash
                if (strcmp(iname, "wl_shm") == 0) {
#if WL_SHM
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wl_shm_id = new_id;
                    obj_op[obj_op_index(wl_shm_id, 0)] = &&wl_shm_format;
                    new_id++;
#endif
                } else if (strcmp(iname, "wl_compositor") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wl_compositor_id = new_id;
                    obj_op[obj_op_index(wl_compositor_id, 0)] =
                        &&wl_compositor_jump;
                    new_id++;
                } else if (strcmp(iname, "wl_output") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
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
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wl_seat_id = new_id;
                    obj_op[obj_op_index(wl_seat_id, 0)] =
                        &&wl_seat_capabilities;
                    obj_op[obj_op_index(wl_seat_id, 1)] = &&wl_seat_name;
                    new_id++;
                } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
#if DMABUF
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_linux_dmabuf_v1_id = new_id;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 0)] =
                        &&zwp_linux_dmabuf_v1_format;
                    obj_op[obj_op_index(zwp_linux_dmabuf_v1_id, 1)] =
                        &&zwp_linux_dmabuf_v1_modifier;
                    new_id++;
#endif
                } else if (strcmp(iname, "xdg_wm_base") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    xdg_wm_base_id = new_id;
                    obj_op[obj_op_index(xdg_wm_base_id, 0)] =
                        &&xdg_wm_base_ping;
                    new_id++;
                } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wp_linux_drm_syncobj_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_linux_drm_syncobj_manager_v1_id,
                                        0)] =
                        &&wp_linux_drm_syncobj_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_idle_inhibit_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_idle_inhibit_manager_v1_id, 0)] =
                        &&zwp_idle_inhibit_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zxdg_decoration_manager_v1_id = new_id;
                    obj_op[obj_op_index(zxdg_decoration_manager_v1_id, 0)] =
                        &&zxdg_decoration_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_relative_pointer_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_relative_pointer_manager_v1_id,
                                        0)] =
                        &&zwp_relative_pointer_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_pointer_constraints_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_constraints_v1_id, 0)] =
                        &&zwp_pointer_constraints_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwlr_output_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 0)] =
                        &&zwlr_output_manager_v1_head;
                    obj_op[obj_op_index(zwlr_output_manager_v1_id, 1)] =
                        &&zwlr_output_manager_v1_done;
                    new_id++;
                } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwlr_data_control_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_data_control_manager_v1_id, 0)] =
                        &&zwlr_data_control_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_virtual_keyboard_manager_v1_id = new_id;
                    obj_op[obj_op_index(zwp_virtual_keyboard_manager_v1_id,
                                        0)] =
                        &&zwp_virtual_keyboard_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_viewporter") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wp_viewporter_id = new_id;
                    obj_op[obj_op_index(wp_viewporter_id, 0)] =
                        &&wp_viewporter_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_fractional_scale_manager_v1") ==
                           0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wp_fractional_scale_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_fractional_scale_manager_v1_id, 0)] =
                        &&wp_fractional_scale_manager_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwp_pointer_gestures_v1_id = new_id;
                    obj_op[obj_op_index(zwp_pointer_gestures_v1_id, 0)] =
                        &&zwp_pointer_gestures_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    xdg_activation_v1_id = new_id;
                    obj_op[obj_op_index(xdg_activation_v1_id, 0)] =
                        &&xdg_activation_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "wp_presentation") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wp_presentation_id = new_id;
                    obj_op[obj_op_index(wp_presentation_id, 0)] =
                        &&wp_presentation_clock_id;
                    new_id++;
                } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    zwlr_layer_shell_v1_id = new_id;
                    obj_op[obj_op_index(zwlr_layer_shell_v1_id, 0)] =
                        &&zwlr_layer_shell_v1_jump;
                    new_id++;
                } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    ext_foreign_toplevel_list_v1_id = new_id;
                    obj_op[obj_op_index(ext_foreign_toplevel_list_v1_id, 0)] =
                        &&ext_foreign_toplevel_list_v1_toplevel;
                    new_id++;
                } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                    war_wayland_registry_bind(
                        fd, msg_buffer, msg_buffer_offset, size, new_id);
                    wp_content_type_manager_v1_id = new_id;
                    obj_op[obj_op_index(wp_content_type_manager_v1_id, 0)] =
                        &&wp_content_type_manager_v1_jump;
                    new_id++;
                }
                if (!wl_surface_id && wl_compositor_id) {
                    uint8_t create_surface[12];
                    war_write_le32(create_surface, wl_compositor_id);
                    war_write_le16(create_surface + 4, 0);
                    war_write_le16(create_surface + 6, 12);
                    war_write_le32(create_surface + 8, new_id);
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
                    war_write_le32(get_surface_feedback,
                                   zwp_linux_dmabuf_v1_id);
                    war_write_le16(get_surface_feedback + 4, 3);
                    war_write_le16(get_surface_feedback + 6, 16);
                    war_write_le32(get_surface_feedback + 8, new_id);
                    war_write_le32(get_surface_feedback + 12, wl_surface_id);
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
                    war_write_le32(get_xdg_surface, xdg_wm_base_id);
                    war_write_le16(get_xdg_surface + 4, 2);
                    war_write_le16(get_xdg_surface + 6, 16);
                    war_write_le32(get_xdg_surface + 8, new_id);
                    war_write_le32(get_xdg_surface + 12, wl_surface_id);
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
                    war_write_le32(get_toplevel, xdg_surface_id);
                    war_write_le16(get_toplevel + 4, 1);
                    war_write_le16(get_toplevel + 6, 12);
                    war_write_le32(get_toplevel + 8, new_id);
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
                dump_bytes(
                    "global_rm event", msg_buffer + msg_buffer_offset, size);
                goto done;
            wl_callback_done:
                //-------------------------------------------------------------
                // RENDERING WITH VULKAN
                //-------------------------------------------------------------
                dump_bytes("wl_callback::done event",
                           msg_buffer + msg_buffer_offset,
                           size);
#if DMABUF
                const uint32_t light_gray_hex = 0xFF454950;
                const uint32_t darker_light_gray_hex =
                    0xFF36383C; // gutter / line numbers
                const uint32_t dark_gray_hex = 0xFF282828;
                const uint32_t red_hex = 0xFF011FDD;
                const uint32_t white_hex = 0xFFB1D9E9; // nvim status text
                const uint32_t bright_white_hex =
                    0xFFEEEEEE; // tmux status text
                const uint32_t black_hex = 0xFF000000;
                const uint32_t full_white_hex = 0xFFFFFFFF;
                const float default_horizontal_line_thickness = 0.018f;
                const float default_vertical_line_thickness =
                    0.018f; // default: 0.018
                const float default_outline_thickness =
                    0.027f; // 0.027 is minimum for preventing 1/4, 1/7, 1/9,
                            // sub_cursor right outline from disappearing
                // single buffer
                assert(vk_ctx.current_frame == 0);
                vkWaitForFences(vk_ctx.device,
                                1,
                                &vk_ctx.in_flight_fences[vk_ctx.current_frame],
                                VK_TRUE,
                                UINT64_MAX);
                vkResetFences(vk_ctx.device,
                              1,
                              &vk_ctx.in_flight_fences[vk_ctx.current_frame]);
                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                };
                VkResult result =
                    vkBeginCommandBuffer(vk_ctx.cmd_buffer, &begin_info);
                assert(result == VK_SUCCESS);
                VkClearValue clear_color = {
                    .color = {{0.1569f, 0.1569f, 0.1569f, 1.0f}},
                };
                VkRenderPassBeginInfo render_pass_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .renderPass = vk_ctx.render_pass,
                    .framebuffer = vk_ctx.frame_buffer,
                    .renderArea =
                        {
                            .offset = {0, 0},
                            .extent = {physical_width, physical_height},
                        },
                    .clearValueCount = 1,
                    .pClearValues = &clear_color,
                };
                vkCmdBeginRenderPass(vk_ctx.cmd_buffer,
                                     &render_pass_info,
                                     VK_SUBPASS_CONTENTS_INLINE);
                //---------------------------------------------------------
                // QUAD PIPELINE
                //---------------------------------------------------------
                vkCmdBindPipeline(vk_ctx.cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  vk_ctx.quad_pipeline);
                quad_vertices_count = 0;
                quad_indices_count = 0;
                war_make_quad(
                    quad_vertices,
                    quad_indices,
                    &quad_vertices_count,
                    &quad_indices_count,
                    (float[3]){ctx.col, ctx.row, ctx.layers[LAYER_CURSOR]},
                    (float[2]){ctx.cell_width, ctx.cell_height},
                    white_hex,
                    0,
                    0,
                    (float[2]){0.0f, 0.0f},
                    QUAD_GRID);
                memcpy(vk_ctx.quads_vertex_buffer_mapped,
                       quad_vertices,
                       sizeof(war_quad_vertex) * quad_vertices_count);
                memcpy(vk_ctx.quads_index_buffer_mapped,
                       quad_indices,
                       sizeof(uint16_t) * quad_indices_count);
                VkMappedMemoryRange quad_flush_ranges[2] = {
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = vk_ctx.quads_vertex_buffer_memory,
                     .offset = 0,
                     .size = war_align64(sizeof(war_quad_vertex) *
                                         quad_vertices_count)},
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = vk_ctx.quads_index_buffer_memory,
                     .offset = 0,
                     .size =
                         war_align64(sizeof(uint16_t) * quad_indices_count)}};
                vkFlushMappedMemoryRanges(vk_ctx.device, 2, quad_flush_ranges);
                VkDeviceSize quad_vertices_offsets[2] = {0};
                vkCmdBindVertexBuffers(vk_ctx.cmd_buffer,
                                       0,
                                       1,
                                       &vk_ctx.quads_vertex_buffer,
                                       quad_vertices_offsets);
                VkDeviceSize quad_instances_offsets[2] = {0};
                vkCmdBindVertexBuffers(vk_ctx.cmd_buffer,
                                       1,
                                       1,
                                       &vk_ctx.quads_instance_buffer,
                                       quad_instances_offsets);
                VkDeviceSize quad_indices_offset = 0;
                vkCmdBindIndexBuffer(vk_ctx.cmd_buffer,
                                     vk_ctx.quads_index_buffer,
                                     quad_indices_offset,
                                     VK_INDEX_TYPE_UINT16);
                war_quad_push_constants quad_push_constants = {
                    .bottom_left = {ctx.left_col, ctx.bottom_row},
                    .physical_size = {physical_width, physical_height},
                    .cell_size = {ctx.cell_width, ctx.cell_height},
                    .zoom = ctx.zoom_scale,
                    .cell_offsets = {ctx.num_cols_for_line_numbers,
                                     ctx.num_rows_for_status_bars},
                    .scroll_margin = {ctx.scroll_margin_cols,
                                      ctx.scroll_margin_rows},
                    .anchor_cell = {ctx.col, ctx.row},
                    .top_right = {ctx.right_col, ctx.top_row},
                };
                vkCmdPushConstants(vk_ctx.cmd_buffer,
                                   vk_ctx.pipeline_layout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(war_quad_push_constants),
                                   &quad_push_constants);
                vkCmdDrawIndexed(
                    vk_ctx.cmd_buffer, quad_indices_count, 1, 0, 0, 0);
                //---------------------------------------------------------
                // SDF TEXT PIPELINE
                //---------------------------------------------------------
                // vkCmdBindPipeline(vk_ctx.cmd_buffer,
                //                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                //                  vk_ctx.text_pipeline);
                //---------------------------------------------------------
                //  END RENDER PASS
                //---------------------------------------------------------
                vkCmdEndRenderPass(vk_ctx.cmd_buffer);
                result = vkEndCommandBuffer(vk_ctx.cmd_buffer);
                assert(result == VK_SUCCESS);
                VkSubmitInfo submit_info = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &vk_ctx.cmd_buffer,
                    .waitSemaphoreCount = 0,
                    .pWaitSemaphores = NULL,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores = NULL,
                };
                result = vkQueueSubmit(
                    vk_ctx.queue,
                    1,
                    &submit_info,
                    vk_ctx.in_flight_fences[vk_ctx.current_frame]);
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

                uint32_t cursor_w = vk_ctx.cell_width;
                uint32_t cursor_h = vk_ctx.cell_height;
                ctx.cursor_x = ctx.col * vk_ctx.cell_width;
                ctx.cursor_y =
                    (physical_height - 1) - (ctx.row * vk_ctx.cell_height);

                for (uint32_t y = ctx.cursor_y; y < ctx.cursor_y + cursor_h;
                     ++y) {
                    if (y >= physical_height) break;
                    for (uint32_t x = ctx.cursor_x; x < ctx.cursor_x + cursor_w;
                         ++x) {
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
                dump_bytes("wl_display::error event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_display_delete_id:
                dump_bytes("wl_display::delete_id event",
                           msg_buffer + msg_buffer_offset,
                           size);

                if (war_read_le32(msg_buffer + msg_buffer_offset + 8) ==
                    wl_callback_id) {
                    war_wayland_wl_surface_frame(
                        fd, wl_surface_id, wl_callback_id);
                }
                goto done;
#if WL_SHM
            wl_shm_format:
                dump_bytes("wl_shm_format event",
                           msg_buffer + msg_buffer_offset,
                           size);
                if (war_read_le32(msg_buffer + msg_buffer_offset + 8) ==
                    ARGB8888) {
                    uint8_t create_pool[12];
                    war_write_le32(create_pool, wl_shm_id); // object id
                    war_write_le16(create_pool + 4, 0);     // opcode 0
                    war_write_le16(
                        create_pool + 6,
                        16); // message size = 12 + 4 bytes for pool size
                    war_write_le32(create_pool + 8, new_id); // new pool id
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
                    war_write_le32(create_buffer, wl_shm_pool_id);
                    war_write_le16(create_buffer + 4, 0);
                    war_write_le16(create_buffer + 6, 32);
                    war_write_le32(create_buffer + 8, new_id);
                    war_write_le32(create_buffer + 12, 0); // msg_buffer_offset
                    war_write_le32(create_buffer + 16, physical_width);
                    war_write_le32(create_buffer + 20, physical_height);
                    war_write_le32(create_buffer + 24, stride);
                    war_write_le32(create_buffer + 28, ARGB8888);
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
                dump_bytes("wl_buffer_release event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            xdg_wm_base_ping:
                dump_bytes("xdg_wm_base_ping event",
                           msg_buffer + msg_buffer_offset,
                           size);
                assert(size == 12);
                uint8_t pong[12];
                war_write_le32(pong, xdg_wm_base_id);
                war_write_le16(pong + 4, 3);
                war_write_le16(pong + 6, 12);
                war_write_le32(
                    pong + 8,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8));
                dump_bytes("xdg_wm_base_pong request", pong, 12);
                ssize_t pong_written = write(fd, pong, 12);
                assert(pong_written == 12);
                goto done;
            xdg_surface_configure:
                dump_bytes("xdg_surface_configure event",
                           msg_buffer + msg_buffer_offset,
                           size);
                assert(size == 12);

                uint8_t ack_configure[12];
                war_write_le32(ack_configure, xdg_surface_id);
                war_write_le16(ack_configure + 4, 4);
                war_write_le16(ack_configure + 6, 12);
                war_write_le32(
                    ack_configure + 8,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8));
                dump_bytes(
                    "xdg_surface_ack_configure request", ack_configure, 12);
                ssize_t ack_configure_written = write(fd, ack_configure, 12);
                assert(ack_configure_written == 12);

                if (!wp_viewport_id) {
                    uint8_t get_viewport[16];
                    war_write_le32(get_viewport, wp_viewporter_id);
                    war_write_le16(get_viewport + 4, 1);
                    war_write_le16(get_viewport + 6, 16);
                    war_write_le32(get_viewport + 8, new_id);
                    war_write_le32(get_viewport + 12, wl_surface_id);
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
                    // war_write_le32(set_source, wp_viewport_id);
                    // war_write_le16(set_source + 4, 1);
                    // war_write_le16(set_source + 6, 24);
                    // war_write_le32(set_source + 8, 0);
                    // war_write_le32(set_source + 12, 0);
                    // war_write_le32(set_source + 16, physical_width);
                    // war_write_le32(set_source + 20, physical_height);
                    // dump_bytes(
                    //     "wp_viewport::set_source request", set_source,
                    //     24);
                    // ssize_t set_source_written = write(fd, set_source,
                    // 24); assert(set_source_written == 24);

                    uint8_t set_destination[16];
                    war_write_le32(set_destination, wp_viewport_id);
                    war_write_le16(set_destination + 4, 2);
                    war_write_le16(set_destination + 6, 16);
                    war_write_le32(set_destination + 8, logical_width);
                    war_write_le32(set_destination + 12, logical_height);
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
                dump_bytes("xdg_toplevel_configure event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            xdg_toplevel_close:
                dump_bytes("xdg_toplevel_close event",
                           msg_buffer + msg_buffer_offset,
                           size);

                uint8_t xdg_toplevel_destroy[8];
                war_write_le32(xdg_toplevel_destroy, xdg_toplevel_id);
                war_write_le16(xdg_toplevel_destroy + 4, 0);
                war_write_le16(xdg_toplevel_destroy + 6, 8);
                ssize_t xdg_toplevel_destroy_written =
                    write(fd, xdg_toplevel_destroy, 8);
                dump_bytes(
                    "xdg_toplevel::destroy request", xdg_toplevel_destroy, 8);
                assert(xdg_toplevel_destroy_written == 8);

                uint8_t xdg_surface_destroy[8];
                war_write_le32(xdg_surface_destroy, xdg_surface_id);
                war_write_le16(xdg_surface_destroy + 4, 0);
                war_write_le16(xdg_surface_destroy + 6, 8);
                ssize_t xdg_surface_destroy_written =
                    write(fd, xdg_surface_destroy, 8);
                dump_bytes(
                    "xdg_surface::destroy request", xdg_surface_destroy, 8);
                assert(xdg_surface_destroy_written == 8);

                uint8_t wl_buffer_destroy[8];
                war_write_le32(wl_buffer_destroy, wl_buffer_id);
                war_write_le16(wl_buffer_destroy + 4, 0);
                war_write_le16(wl_buffer_destroy + 6, 8);
                ssize_t wl_buffer_destroy_written =
                    write(fd, wl_buffer_destroy, 8);
                dump_bytes("wl_buffer::destroy request", wl_buffer_destroy, 8);
                assert(wl_buffer_destroy_written == 8);

                uint8_t wl_surface_destroy[8];
                war_write_le32(wl_surface_destroy, wl_surface_id);
                war_write_le16(wl_surface_destroy + 4, 0);
                war_write_le16(wl_surface_destroy + 6, 8);
                ssize_t wl_surface_destroy_written =
                    write(fd, wl_surface_destroy, 8);
                dump_bytes(
                    "wl_surface::destroy request", wl_surface_destroy, 8);
                assert(wl_surface_destroy_written == 8);

                window_render_to_audio_ring_buffer[*write_to_audio_index] =
                    AUDIO_END_WAR;
                *write_to_audio_index = (*write_to_audio_index + 1) & 0xFF;

                end_window_render = 1;
                goto done;
            xdg_toplevel_configure_bounds:
                dump_bytes("xdg_toplevel_configure_bounds event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            xdg_toplevel_wm_capabilities:
                dump_bytes("xdg_toplevel_wm_capabilities event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
#if DMABUF
            zwp_linux_dmabuf_v1_format:
                dump_bytes("zwp_linux_dmabuf_v1_format event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_v1_modifier:
                dump_bytes("zwp_linux_dmabuf_v1_modifier event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_buffer_params_v1_created:
                dump_bytes(
                    "zwp_linux_buffer_params_v1_created", // COMMENT
                                                          // REFACTOR: to ::
                    msg_buffer + msg_buffer_offset,
                    size);
                goto done;
            zwp_linux_buffer_params_v1_failed:
                dump_bytes("zwp_linux_buffer_params_v1_failed event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                uint8_t create_params[12]; // REFACTOR: zero initialize
                war_write_le32(create_params, zwp_linux_dmabuf_v1_id);
                war_write_le16(create_params + 4, 1);
                war_write_le16(create_params + 6, 12);
                war_write_le32(create_params + 8, new_id);
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
                war_write_le32(header, zwp_linux_buffer_params_v1_id);
                war_write_le16(header + 4, 1);
                war_write_le16(header + 6, 28);
                uint8_t tail[20];
                war_write_le32(tail, 0);
                war_write_le32(tail + 4, 0);
                war_write_le32(tail + 8, stride);
                war_write_le32(tail + 12, 0);
                war_write_le32(tail + 16, 0);
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
                *((int*)CMSG_DATA(cmsg)) = vk_ctx.dmabuf_fd;
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
                war_write_le32(
                    create_immed,
                    zwp_linux_buffer_params_v1_id); // COMMENT REFACTOR: is
                                                    // it faster to copy the
                                                    // incoming message
                                                    // header and increment
                                                    // accordingly?
                war_write_le16(create_immed + 4,
                               3); // COMMENT REFACTOR CONCERN: check for
                                   // duplicate variables names
                war_write_le16(create_immed + 6, 28);
                war_write_le32(create_immed + 8, new_id);
                war_write_le32(create_immed + 12, physical_width);
                war_write_le32(create_immed + 16, physical_height);
                war_write_le32(create_immed + 20, DRM_FORMAT_ARGB8888);
                war_write_le32(create_immed + 24, 0);
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
                war_write_le32(destroy, zwp_linux_buffer_params_v1_id);
                war_write_le16(destroy + 4, 0);
                war_write_le16(destroy + 6, 8);
                ssize_t destroy_written = write(fd, destroy, 8);
                assert(destroy_written == 8);
                dump_bytes("zwp_linux_buffer_params_v1_id::destroy request",
                           destroy,
                           8);
                goto done;
            zwp_linux_dmabuf_feedback_v1_format_table:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_format_table event",
                           msg_buffer + msg_buffer_offset,
                           size); // REFACTOR: event
                goto done;
            zwp_linux_dmabuf_feedback_v1_main_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_main_device event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_target_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_target_"
                           "device event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_formats:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_formats event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_linux_dmabuf_feedback_v1_tranche_flags:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_flags event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
#endif
            wp_linux_drm_syncobj_manager_v1_jump:
                dump_bytes("wp_linux_drm_syncobj_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_compositor_jump:
                dump_bytes("wl_compositor_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_surface_enter:
                dump_bytes("wl_surface_enter event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_surface_leave:
                dump_bytes("wl_surface_leave event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_surface_preferred_buffer_scale:
                dump_bytes("wl_surface_preferred_buffer_scale event",
                           msg_buffer + msg_buffer_offset,
                           size);
                assert(size == 12);

                uint8_t set_buffer_scale[12];
                war_write_le32(set_buffer_scale, wl_surface_id);
                war_write_le16(set_buffer_scale + 4, 8);
                war_write_le16(set_buffer_scale + 6, 12);
                war_write_le32(
                    set_buffer_scale + 8,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8));
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
                           msg_buffer + msg_buffer_offset,
                           size);
                assert(size == 12);

                uint8_t set_buffer_transform[12];
                war_write_le32(set_buffer_transform, wl_surface_id);
                war_write_le16(set_buffer_transform + 4, 7);
                war_write_le16(set_buffer_transform + 6, 12);
                war_write_le32(
                    set_buffer_transform + 8,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8));
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
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwlr_layer_shell_v1_jump:
                dump_bytes("zwlr_layer_shell_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zxdg_decoration_manager_v1_jump:
                dump_bytes("zxdg_decoration_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_relative_pointer_manager_v1_jump:
                dump_bytes("zwp_relative_pointer_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_pointer_constraints_v1_jump:
                dump_bytes("zwp_pointer_constraints_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wp_presentation_clock_id:
                dump_bytes("wp_presentation_clock_id event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwlr_output_manager_v1_head:
                dump_bytes("zwlr_output_manager_v1_head event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwlr_output_manager_v1_done:
                dump_bytes("zwlr_output_manager_v1_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            ext_foreign_toplevel_list_v1_toplevel:
                dump_bytes("ext_foreign_toplevel_list_v1_toplevel event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwlr_data_control_manager_v1_jump:
                dump_bytes("zwlr_data_control_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wp_viewporter_jump:
                dump_bytes("wp_viewporter_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wp_content_type_manager_v1_jump:
                dump_bytes("wp_content_type_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wp_fractional_scale_manager_v1_jump:
                dump_bytes("wp_fractional_scale_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            xdg_activation_v1_jump:
                dump_bytes("xdg_activation_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_virtual_keyboard_manager_v1_jump:
                dump_bytes("zwp_virtual_keyboard_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            zwp_pointer_gestures_v1_jump:
                dump_bytes("zwp_pointer_gestures_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_seat_capabilities:
                dump_bytes("wl_seat_capabilities event",
                           msg_buffer + msg_buffer_offset,
                           size);
                enum {
                    wl_seat_pointer = 0x01,
                    wl_seat_keyboard = 0x02,
                    wl_seat_touch = 0x04,
                };
                uint32_t capabilities =
                    war_read_le32(msg_buffer + msg_buffer_offset + 8);
                if (capabilities & wl_seat_keyboard) {
                    call_carmack("keyboard detected");
                    assert(size == 12);
                    uint8_t get_keyboard[12];
                    war_write_le32(get_keyboard, wl_seat_id);
                    war_write_le16(get_keyboard + 4, 1);
                    war_write_le16(get_keyboard + 6, 12);
                    war_write_le32(get_keyboard + 8, new_id);
                    dump_bytes("get_keyboard request", get_keyboard, 12);
                    call_carmack("bound: wl_keyboard",
                                 (const char*)msg_buffer + msg_buffer_offset +
                                     12);
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
                    war_write_le32(get_pointer, wl_seat_id);
                    war_write_le16(get_pointer + 4, 0);
                    war_write_le16(get_pointer + 6, 12);
                    war_write_le32(get_pointer + 8, new_id);
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
                    war_write_le32(get_touch, wl_seat_id);
                    war_write_le16(get_touch + 4, 2);
                    war_write_le16(get_touch + 6, 12);
                    war_write_le32(get_touch + 8, new_id);
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
                dump_bytes(
                    "wl_seat_name event", msg_buffer + msg_buffer_offset, size);
                call_carmack("seat: %s",
                             (const char*)msg_buffer + msg_buffer_offset + 12);
                goto done;
            wl_keyboard_keymap:
                dump_bytes("wl_keyboard_keymap event", msg_buffer, size);
                assert(size == 16);
                int keymap_fd = -1;
                for (struct cmsghdr* poll_cmsg = CMSG_FIRSTHDR(&poll_msg_hdr);
                     poll_cmsg != NULL;
                     poll_cmsg = CMSG_NXTHDR(&poll_msg_hdr, poll_cmsg)) {
                    if (poll_cmsg->cmsg_level == SOL_SOCKET &&
                        poll_cmsg->cmsg_type == SCM_RIGHTS) {
                        keymap_fd = *(int*)CMSG_DATA(poll_cmsg);
                        break;
                    }
                }
                assert(keymap_fd >= 0);
                uint32_t keymap_format =
                    war_read_le32(msg_buffer + msg_buffer_offset + 8);
                assert(keymap_format == XKB_KEYMAP_FORMAT_TEXT_V1);
                uint32_t keymap_size =
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4);
                assert(keymap_size > 0);
                xkb_context = xkb_context_new(0);
                assert(xkb_context);
                char* keymap_map = mmap(
                    NULL, keymap_size, PROT_READ, MAP_PRIVATE, keymap_fd, 0);
                assert(keymap_map != MAP_FAILED);
                struct xkb_keymap* xkb_keymap = xkb_keymap_new_from_string(
                    xkb_context, keymap_map, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
                assert(xkb_keymap);
                xkb_state = xkb_state_new(xkb_keymap);
                assert(xkb_state);
                mod_shift =
                    xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_SHIFT);
                mod_ctrl =
                    xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_CTRL);
                mod_alt =
                    xkb_keymap_mod_get_index(xkb_keymap, XKB_MOD_NAME_ALT);
                mod_logo = xkb_keymap_mod_get_index(
                    xkb_keymap, XKB_MOD_NAME_LOGO); // Super/Meta
                mod_caps = xkb_keymap_mod_get_index(
                    xkb_keymap, XKB_MOD_NAME_CAPS); // Super/Meta
                mod_num = xkb_keymap_mod_get_index(
                    xkb_keymap, XKB_MOD_NAME_NUM); // Super/Meta
                war_key_event
                    key_sequences[SEQUENCE_COUNT][MAX_SEQUENCE_LENGTH] = {
                        {
                            {.keysym = XKB_KEY_k, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_j, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_h, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_l, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_k, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_j, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_h, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_l, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_0, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_4, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_1, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_2, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_3, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_4, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_5, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_6, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_7, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_8, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_9, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_equal, .mod = MOD_CTRL},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_minus, .mod = MOD_CTRL},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_equal,
                             .mod = MOD_CTRL | MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_minus,
                             .mod = MOD_CTRL | MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_0, .mod = MOD_CTRL},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_ESCAPE, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_f, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_t, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_x, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {.keysym = XKB_KEY_t, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {.keysym = XKB_KEY_f, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {.keysym = XKB_KEY_b, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {.keysym = XKB_KEY_t, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_s, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_z, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_RETURN, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_h, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_h, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_h, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_s, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_s, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_s, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_u, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_u, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_u, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_t, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_n, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_s, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_m, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_y, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_z, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_q, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_e, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_1, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_2, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_3, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_4, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_5, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_6, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_7, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_8, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_9, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_0, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_1, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_2, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_3, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_4, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_5, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_6, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_7, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_8, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_9, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_0, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_k, .mod = MOD_ALT | MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_j, .mod = MOD_ALT | MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_h, .mod = MOD_ALT | MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_l, .mod = MOD_ALT | MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_c, .mod = 0},
                            {0},
                        },
                    };
                void* key_labels[SEQUENCE_COUNT][MODE_COUNT] = {
                    // normal, visual, visual_line, visual_block, insert,
                    // command, mode_m, mode_o
                    {&&cmd_normal_k},
                    {&&cmd_normal_j},
                    {&&cmd_normal_h},
                    {&&cmd_normal_l},
                    {&&cmd_normal_alt_k},
                    {&&cmd_normal_alt_j},
                    {&&cmd_normal_alt_h},
                    {&&cmd_normal_alt_l},
                    {&&cmd_normal_0},
                    {&&cmd_normal_$},
                    {&&cmd_normal_G},
                    {&&cmd_normal_gg},
                    {&&cmd_normal_1},
                    {&&cmd_normal_2},
                    {&&cmd_normal_3},
                    {&&cmd_normal_4},
                    {&&cmd_normal_5},
                    {&&cmd_normal_6},
                    {&&cmd_normal_7},
                    {&&cmd_normal_8},
                    {&&cmd_normal_9},
                    {&&cmd_normal_ctrl_equal},
                    {&&cmd_normal_ctrl_minus},
                    {&&cmd_normal_ctrl_alt_equal},
                    {&&cmd_normal_ctrl_alt_minus},
                    {&&cmd_normal_ctrl_0},
                    {&&cmd_normal_esc},
                    {&&cmd_normal_f},
                    {&&cmd_normal_t},
                    {&&cmd_normal_x},
                    {&&cmd_normal_mt},
                    {&&cmd_normal_mf},
                    {&&cmd_normal_gb},
                    {&&cmd_normal_gt},
                    {&&cmd_normal_gm},
                    {&&cmd_normal_s},
                    {&&cmd_normal_z},
                    {&&cmd_normal_return},
                    {&&cmd_normal_div},
                    {&&cmd_normal_dov},
                    {&&cmd_normal_diw},
                    {&&cmd_normal_spacehov},
                    {&&cmd_normal_spacehiv},
                    {&&cmd_normal_spaceha},
                    {&&cmd_normal_spacesov},
                    {&&cmd_normal_spacesiv},
                    {&&cmd_normal_spacesa},
                    {&&cmd_normal_spacemov},
                    {&&cmd_normal_spacemiv},
                    {&&cmd_normal_spacema},
                    {&&cmd_normal_spaceuov},
                    {&&cmd_normal_spaceuiv},
                    {&&cmd_normal_spaceua},
                    {&&cmd_normal_spacea},
                    {&&cmd_normal_alt_g},
                    {&&cmd_normal_alt_t},
                    {&&cmd_normal_alt_n},
                    {&&cmd_normal_alt_s},
                    {&&cmd_normal_alt_m},
                    {&&cmd_normal_alt_y},
                    {&&cmd_normal_alt_z},
                    {&&cmd_normal_alt_q},
                    {&&cmd_normal_alt_e},
                    {&&cmd_normal_a},
                    {&&cmd_normal_space1},
                    {&&cmd_normal_space2},
                    {&&cmd_normal_space3},
                    {&&cmd_normal_space4},
                    {&&cmd_normal_space5},
                    {&&cmd_normal_space6},
                    {&&cmd_normal_space7},
                    {&&cmd_normal_space8},
                    {&&cmd_normal_space9},
                    {&&cmd_normal_space0},
                    {&&cmd_normal_alt_1},
                    {&&cmd_normal_alt_2},
                    {&&cmd_normal_alt_3},
                    {&&cmd_normal_alt_4},
                    {&&cmd_normal_alt_5},
                    {&&cmd_normal_alt_6},
                    {&&cmd_normal_alt_7},
                    {&&cmd_normal_alt_8},
                    {&&cmd_normal_alt_9},
                    {&&cmd_normal_alt_0},
                    {&&cmd_normal_dspacea},
                    {&&cmd_normal_alt_shift_k},
                    {&&cmd_normal_alt_shift_j},
                    {&&cmd_normal_alt_shift_h},
                    {&&cmd_normal_alt_shift_l},
                    {&&cmd_normal_spacedc},
                };
                // default to normal mode command if unset
                for (size_t s = 0; s < SEQUENCE_COUNT; s++) {
                    for (size_t m = 0; m < MODE_COUNT; m++) {
                        if (key_labels[s][m] == NULL) {
                            key_labels[s][m] = key_labels[s][MODE_NORMAL];
                        }
                    }
                }
                size_t state_counter = 1; // 0 = root
                for (size_t seq_idx = 0; seq_idx < SEQUENCE_COUNT; seq_idx++) {
                    size_t parent = 0; // start at root
                    size_t len = 0;
                    while (len < MAX_SEQUENCE_LENGTH &&
                           key_sequences[seq_idx][len].keysym != 0)
                        len++;
                    for (size_t key_idx = 0; key_idx < len; key_idx++) {
                        war_key_event* ev = &key_sequences[seq_idx][key_idx];
                        uint16_t next =
                            fsm[parent].next_state[ev->keysym][ev->mod];
                        if (next == 0) {
                            next = state_counter++;
                            fsm[parent].next_state[ev->keysym][ev->mod] = next;
                            fsm[next].is_terminal = false;
                            memset(fsm[next].command,
                                   0,
                                   sizeof(fsm[next].command));
                            memset(fsm[next].next_state,
                                   0,
                                   sizeof(fsm[next].next_state));
                        }
                        parent = next;
                    }
                    fsm[parent].is_terminal = true;
                    memcpy(fsm[parent].command,
                           key_labels[seq_idx],
                           sizeof(void*) * MODE_COUNT);
                }
                assert(state_counter < MAX_STATES);
                munmap(keymap_map, keymap_size);
                close(keymap_fd);
                xkb_keymap_unref(xkb_keymap);
                xkb_keymap = NULL;
                goto done;
            wl_keyboard_enter:
                dump_bytes("wl_keyboard_enter event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_keyboard_leave:
                dump_bytes("wl_keyboard_leave event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_keyboard_key:
                dump_bytes("wl_keyboard_key event",
                           msg_buffer + msg_buffer_offset,
                           size);
                if (end_window_render) { goto done; }
                uint32_t wl_key_state = war_read_le32(
                    msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4);
                uint32_t keycode =
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4) +
                    8; // + 8 cuz wayland
                xkb_keysym_t keysym =
                    xkb_state_key_get_one_sym(xkb_state, keycode);
                // TODO: Normalize the other things like '<' (regarding
                // MOD_SHIFT and XKB)
                keysym = war_normalize_keysym(
                    keysym); // normalize to lowercase and prevent out of bounds
                xkb_mod_mask_t mods = xkb_state_serialize_mods(
                    xkb_state, XKB_STATE_MODS_DEPRESSED);
                uint8_t mod = 0;
                if (mods & (1 << mod_shift)) mod |= MOD_SHIFT;
                if (mods & (1 << mod_ctrl)) mod |= MOD_CTRL;
                if (mods & (1 << mod_alt)) mod |= MOD_ALT;
                if (mods & (1 << mod_logo)) mod |= MOD_LOGO;
                if (mods & (1 << mod_caps)) mod |= MOD_CAPS;
                if (mods & (1 << mod_num)) mod |= MOD_NUM;
                // if (mods & (1 << mod_fn)) mod |= MOD_FN;
                bool pressed = (wl_key_state == 1);
                if (!pressed) {
                    key_down[keysym][mod] = false;
                    key_last_event_us[keysym][mod] = 0;
                    // repeats
                    if (repeat_keysym == keysym) {
                        repeat_keysym = 0;
                        repeat_mod = 0;
                        repeating = false;
                        // timeouts
                        timeout = false;
                        timeout_state_index = 0;
                        timeout_start_us = 0;
                    }
                    goto cmd_done;
                }
                if (ctx.num_chars_in_sequence < MAX_SEQUENCE_LENGTH) {
                    ctx.input_sequence[ctx.num_chars_in_sequence] =
                        war_keysym_to_key(keysym, mod);
                }
                if (!key_down[keysym][mod]) {
                    key_down[keysym][mod] = true;
                    key_last_event_us[keysym][mod] = ctx.now;
                }
                uint16_t next_state_index =
                    fsm[current_state_index].next_state[keysym][mod];
                if (timeout &&
                    fsm[timeout_state_index].next_state[keysym][mod]) {
                    next_state_index =
                        fsm[timeout_state_index].next_state[keysym][mod];
                }
                if (next_state_index == 0) {
                    current_state_index = 0;
                    // timeouts
                    timeout = false;
                    timeout_state_index = 0;
                    timeout_start_us = 0;
                    goto cmd_done;
                }
                current_state_index = next_state_index;
                fsm_state_last_event_us = ctx.now;
                if (fsm[current_state_index].is_terminal &&
                    !war_state_is_prefix(current_state_index, fsm)) {
                    uint16_t temp = current_state_index;
                    current_state_index = 0;
                    // repeats
                    repeat_keysym = keysym;
                    repeat_mod = mod;
                    repeating = false;
                    // timeouts
                    timeout_state_index = 0;
                    timeout_start_us = 0;
                    timeout = false;
                    goto* fsm[temp].command[ctx.mode];
                } else if (fsm[current_state_index].is_terminal &&
                           war_state_is_prefix(current_state_index, fsm)) {
                    // repeats
                    repeat_keysym = 0;
                    repeat_mod = 0;
                    repeating = false;
                    // timeouts
                    timeout_state_index = current_state_index;
                    timeout_start_us = ctx.now;
                    timeout = true;
                    current_state_index = 0;
                }
                goto cmd_done;
            cmd_normal_k:
                uint32_t increment = ctx.row_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                uint32_t scaled_whole =
                    (increment * ctx.navigation_whole_number_row) /
                    ctx.navigation_sub_cells_row;
                uint32_t scaled_frac =
                    (increment * ctx.navigation_whole_number_row) %
                    ctx.navigation_sub_cells_row;
                ctx.row =
                    war_clamp_add_uint32(ctx.row, scaled_whole, ctx.max_row);
                ctx.sub_row =
                    war_clamp_add_uint32(ctx.sub_row, scaled_frac, ctx.max_row);
                ctx.row = war_clamp_add_uint32(ctx.row,
                                               ctx.sub_row /
                                                   ctx.navigation_sub_cells_row,
                                               ctx.max_row);
                ctx.sub_row =
                    war_clamp_uint32(ctx.sub_row % ctx.navigation_sub_cells_row,
                                     ctx.min_row,
                                     ctx.max_row);
                if (ctx.row > ctx.top_row - ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_add_uint32(
                        ctx.bottom_row, increment, ctx.max_row);
                    ctx.top_row = war_clamp_add_uint32(
                        ctx.top_row, increment, ctx.max_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_j:
                call_carmack("cmd_normal_j");
                increment = ctx.row_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                scaled_whole = (increment * ctx.navigation_whole_number_row) /
                               ctx.navigation_sub_cells_row;
                scaled_frac = (increment * ctx.navigation_whole_number_row) %
                              ctx.navigation_sub_cells_row;
                ctx.row = war_clamp_subtract_uint32(
                    ctx.row, scaled_whole, ctx.min_row);
                if (ctx.sub_row < scaled_frac) {
                    ctx.row =
                        war_clamp_subtract_uint32(ctx.row, 1, ctx.min_row);
                    ctx.sub_row += ctx.navigation_sub_cells_row;
                }
                ctx.sub_row = war_clamp_subtract_uint32(
                    ctx.sub_row, scaled_frac, ctx.min_row);
                ctx.row = war_clamp_subtract_uint32(
                    ctx.row,
                    ctx.sub_row / ctx.navigation_sub_cells_row,
                    ctx.min_row);
                ctx.sub_row =
                    war_clamp_uint32(ctx.sub_row % ctx.navigation_sub_cells_row,
                                     ctx.min_row,
                                     ctx.max_row);
                if (ctx.row < ctx.bottom_row + ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_subtract_uint32(
                        ctx.bottom_row, increment, ctx.min_row);
                    ctx.top_row = war_clamp_subtract_uint32(
                        ctx.top_row, increment, ctx.min_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.top_row = war_clamp_add_uint32(
                            ctx.top_row, diff, ctx.max_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_l:
                call_carmack("cmd_normal_l");
                uint32_t initial = ctx.col;
                increment = ctx.col_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                scaled_whole = (increment * ctx.navigation_whole_number_col) /
                               ctx.navigation_sub_cells_col;
                scaled_frac = (increment * ctx.navigation_whole_number_col) %
                              ctx.navigation_sub_cells_col;
                ctx.col =
                    war_clamp_add_uint32(ctx.col, scaled_whole, ctx.max_col);
                ctx.sub_col =
                    war_clamp_add_uint32(ctx.sub_col, scaled_frac, ctx.max_col);
                if (ctx.sub_col >= ctx.navigation_sub_cells_col) {
                    uint32_t carry = ctx.sub_col / ctx.navigation_sub_cells_col;
                    ctx.col = war_clamp_add_uint32(ctx.col, carry, ctx.max_col);
                    ctx.sub_col = ctx.sub_col % ctx.navigation_sub_cells_col;
                }
                uint32_t pan = ctx.col - initial;
                if (ctx.col > ctx.right_col - ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col =
                        war_clamp_add_uint32(ctx.left_col, pan, ctx.max_col);
                    ctx.right_col =
                        war_clamp_add_uint32(ctx.right_col, pan, ctx.max_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.left_col = war_clamp_subtract_uint32(
                            ctx.left_col, diff, ctx.min_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_h:
                call_carmack("cmd_normal_h");
                initial = ctx.col;
                increment = ctx.col_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                scaled_whole = (increment * ctx.navigation_whole_number_col) /
                               ctx.navigation_sub_cells_col;
                scaled_frac = (increment * ctx.navigation_whole_number_col) %
                              ctx.navigation_sub_cells_col;
                ctx.col = war_clamp_subtract_uint32(
                    ctx.col, scaled_whole, ctx.min_col);
                if (ctx.sub_col < scaled_frac) {
                    if (ctx.col > ctx.min_col) {
                        ctx.col =
                            war_clamp_subtract_uint32(ctx.col, 1, ctx.min_col);
                        ctx.sub_col += ctx.navigation_sub_cells_col;
                    } else {
                        ctx.sub_col = 0;
                    }
                }
                ctx.sub_col = war_clamp_subtract_uint32(
                    ctx.sub_col, scaled_frac, ctx.min_col);
                ctx.col = war_clamp_subtract_uint32(
                    ctx.col,
                    ctx.sub_col / ctx.navigation_sub_cells_col,
                    ctx.min_col);
                ctx.sub_col =
                    war_clamp_uint32(ctx.sub_col % ctx.navigation_sub_cells_col,
                                     ctx.min_col,
                                     ctx.max_col);
                pan = initial - ctx.col;
                if (ctx.col < ctx.left_col + ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col = war_clamp_subtract_uint32(
                        ctx.left_col, pan, ctx.min_col);
                    ctx.right_col = war_clamp_subtract_uint32(
                        ctx.right_col, pan, ctx.min_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.right_col = war_clamp_add_uint32(
                            ctx.right_col, diff, ctx.max_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_k:
                call_carmack("cmd_normal_alt_k");
                increment = ctx.row_leap_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                ctx.row = war_clamp_add_uint32(ctx.row, increment, ctx.max_row);
                if (ctx.row > ctx.top_row - ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_add_uint32(
                        ctx.bottom_row, increment, ctx.max_row);
                    ctx.top_row = war_clamp_add_uint32(
                        ctx.top_row, increment, ctx.max_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_j:
                call_carmack("cmd_normal_alt_j");
                increment = ctx.row_leap_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                ctx.row =
                    war_clamp_subtract_uint32(ctx.row, increment, ctx.min_row);
                if (ctx.row < ctx.bottom_row + ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_subtract_uint32(
                        ctx.bottom_row, increment, ctx.min_row);
                    ctx.top_row = war_clamp_subtract_uint32(
                        ctx.top_row, increment, ctx.min_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.top_row = war_clamp_add_uint32(
                            ctx.top_row, diff, ctx.max_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_l:
                call_carmack("cmd_normal_alt_l");
                increment = ctx.col_leap_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                ctx.col = war_clamp_add_uint32(ctx.col, increment, ctx.max_col);
                if (ctx.col > ctx.right_col - ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col = war_clamp_add_uint32(
                        ctx.left_col, increment, ctx.max_col);
                    ctx.right_col = war_clamp_add_uint32(
                        ctx.right_col, increment, ctx.max_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.left_col = war_clamp_subtract_uint32(
                            ctx.left_col, diff, ctx.min_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_h:
                call_carmack("cmd_normal_alt_h");
                increment = ctx.col_leap_increment;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                ctx.col =
                    war_clamp_subtract_uint32(ctx.col, increment, ctx.min_col);
                if (ctx.col < ctx.left_col + ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col = war_clamp_subtract_uint32(
                        ctx.left_col, increment, ctx.min_col);
                    ctx.right_col = war_clamp_subtract_uint32(
                        ctx.right_col, increment, ctx.min_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.right_col = war_clamp_add_uint32(
                            ctx.right_col, diff, ctx.max_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_shift_k:
                call_carmack("cmd_normal_alt_shift_k");
                increment = ctx.viewport_rows - ctx.num_rows_for_status_bars;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                ctx.row = war_clamp_add_uint32(ctx.row, increment, ctx.max_row);
                if (ctx.row > ctx.top_row - ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_add_uint32(
                        ctx.bottom_row, increment, ctx.max_row);
                    ctx.top_row = war_clamp_add_uint32(
                        ctx.top_row, increment, ctx.max_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_shift_j:
                call_carmack("cmd_normal_alt_shift_j");
                increment = ctx.viewport_rows - ctx.num_rows_for_status_bars;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_row);
                }
                ctx.row =
                    war_clamp_subtract_uint32(ctx.row, increment, ctx.min_row);
                if (ctx.row < ctx.bottom_row + ctx.scroll_margin_rows) {
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    ctx.bottom_row = war_clamp_subtract_uint32(
                        ctx.bottom_row, increment, ctx.min_row);
                    ctx.top_row = war_clamp_subtract_uint32(
                        ctx.top_row, increment, ctx.min_row);
                    uint32_t new_viewport_height = ctx.top_row - ctx.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx.top_row = war_clamp_add_uint32(
                            ctx.top_row, diff, ctx.max_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_shift_l:
                call_carmack("cmd_normal_alt_shift_l");
                increment = ctx.viewport_cols - ctx.num_cols_for_line_numbers;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                ctx.col = war_clamp_add_uint32(ctx.col, increment, ctx.max_col);
                if (ctx.col > ctx.right_col - ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col = war_clamp_add_uint32(
                        ctx.left_col, increment, ctx.max_col);
                    ctx.right_col = war_clamp_add_uint32(
                        ctx.right_col, increment, ctx.max_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.left_col = war_clamp_subtract_uint32(
                            ctx.left_col, diff, ctx.min_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_shift_h:
                call_carmack("cmd_normal_alt_shift_h");
                increment = ctx.viewport_cols - ctx.num_cols_for_line_numbers;
                if (ctx.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx.numeric_prefix, ctx.max_col);
                }
                ctx.col =
                    war_clamp_subtract_uint32(ctx.col, increment, ctx.min_col);
                if (ctx.col < ctx.left_col + ctx.scroll_margin_cols) {
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    ctx.left_col = war_clamp_subtract_uint32(
                        ctx.left_col, increment, ctx.min_col);
                    ctx.right_col = war_clamp_subtract_uint32(
                        ctx.right_col, increment, ctx.min_col);
                    uint32_t new_viewport_width = ctx.right_col - ctx.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx.right_col = war_clamp_add_uint32(
                            ctx.right_col, diff, ctx.max_col);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_0:
                call_carmack("cmd_normal_0");
                if (ctx.numeric_prefix) {
                    ctx.numeric_prefix = ctx.numeric_prefix * 10;
                    goto cmd_done;
                }
                ctx.col = ctx.left_col;
                ctx.sub_col = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_$:
                call_carmack("cmd_normal_$");
                if (ctx.numeric_prefix) {
                    ctx.col = war_clamp_uint32(
                        ctx.numeric_prefix, ctx.min_col, ctx.max_col);
                    ctx.sub_col = 0;
                    uint32_t viewport_width = ctx.right_col - ctx.left_col;
                    uint32_t distance = viewport_width / 2;
                    ctx.left_col = war_clamp_subtract_uint32(
                        ctx.col, distance, ctx.min_col);
                    ctx.right_col =
                        war_clamp_add_uint32(ctx.col, distance, ctx.max_col);
                    uint32_t new_viewport_width = war_clamp_subtract_uint32(
                        ctx.right_col, ctx.left_col, ctx.min_col);
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = war_clamp_subtract_uint32(
                            viewport_width, new_viewport_width, ctx.min_col);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx.right_col, diff, ctx.max_col);
                        if (sum < ctx.max_col) {
                            ctx.right_col = sum;
                        } else {
                            ctx.left_col = war_clamp_subtract_uint32(
                                ctx.left_col, diff, ctx.min_col);
                        }
                    }
                    ctx.numeric_prefix = 0;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx.col = ctx.right_col;
                ctx.sub_col = 0;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_G:
                call_carmack("cmd_normal_G");
                if (ctx.numeric_prefix) {
                    ctx.row = war_clamp_uint32(
                        ctx.numeric_prefix, ctx.min_row, ctx.max_row);
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    uint32_t distance = viewport_height / 2;
                    ctx.bottom_row = war_clamp_subtract_uint32(
                        ctx.row, distance, ctx.min_row);
                    ctx.top_row =
                        war_clamp_add_uint32(ctx.row, distance, ctx.max_row);
                    uint32_t new_viewport_height = war_clamp_subtract_uint32(
                        ctx.top_row, ctx.bottom_row, 0);
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = war_clamp_subtract_uint32(
                            viewport_height, new_viewport_height, 0);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx.top_row, diff, ctx.max_row);
                        if (sum < ctx.max_row) {
                            ctx.top_row = sum;
                        } else {
                            ctx.bottom_row = war_clamp_subtract_uint32(
                                ctx.bottom_row, diff, ctx.min_row);
                        }
                    }
                    ctx.numeric_prefix = 0;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx.row = ctx.bottom_row;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gg:
                call_carmack("cmd_normal_gg");
                if (ctx.numeric_prefix) {
                    ctx.row = war_clamp_uint32(
                        ctx.numeric_prefix, ctx.min_row, ctx.max_row);
                    uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                    uint32_t distance = viewport_height / 2;
                    ctx.bottom_row = war_clamp_subtract_uint32(
                        ctx.row, distance, ctx.min_row);
                    ctx.top_row =
                        war_clamp_add_uint32(ctx.row, distance, ctx.max_row);
                    uint32_t new_viewport_height = war_clamp_subtract_uint32(
                        ctx.top_row, ctx.bottom_row, ctx.min_row);
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = war_clamp_subtract_uint32(
                            viewport_height, new_viewport_height, ctx.min_row);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx.top_row, diff, ctx.max_row);
                        if (sum < ctx.max_row) {
                            ctx.top_row = sum;
                        } else {
                            ctx.bottom_row = war_clamp_subtract_uint32(
                                ctx.bottom_row, diff, ctx.min_row);
                        }
                    }
                    ctx.numeric_prefix = 0;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx.row = ctx.top_row;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_1:
                call_carmack("cmd_normal_1");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 1, UINT32_MAX);
                goto cmd_done;
            cmd_normal_2:
                call_carmack("cmd_normal_2");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 2, UINT32_MAX);
                goto cmd_done;
            cmd_normal_3:
                call_carmack("cmd_normal_3");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 3, UINT32_MAX);
                goto cmd_done;
            cmd_normal_4:
                call_carmack("cmd_normal_4");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 4, UINT32_MAX);
                goto cmd_done;
            cmd_normal_5:
                call_carmack("cmd_normal_5");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 5, UINT32_MAX);
                goto cmd_done;
            cmd_normal_6:
                call_carmack("cmd_normal_6");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 6, UINT32_MAX);
                goto cmd_done;
            cmd_normal_7:
                call_carmack("cmd_normal_7");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 7, UINT32_MAX);
                goto cmd_done;
            cmd_normal_8:
                call_carmack("cmd_normal_8");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 8, UINT32_MAX);
                goto cmd_done;
            cmd_normal_9:
                call_carmack("cmd_normal_9");
                ctx.numeric_prefix = war_clamp_multiply_uint32(
                    ctx.numeric_prefix, 10, UINT32_MAX);
                ctx.numeric_prefix =
                    war_clamp_add_uint32(ctx.numeric_prefix, 9, UINT32_MAX);
                goto cmd_done;
            cmd_normal_ctrl_equal:
                call_carmack("cmd_normal_ctrl_equal");
                // ctx.zoom_scale +=
                // ctx.zoom_increment; if
                // (ctx.zoom_scale > 5.0f) {
                // ctx.zoom_scale = 5.0f; }
                // ctx.panning_x = ctx.anchor_ndc_x
                // - ctx.anchor_x * ctx.zoom_scale;
                // ctx.panning_y = ctx.anchor_ndc_y
                // - ctx.anchor_y * ctx.zoom_scale;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_minus:
                call_carmack("cmd_normal_ctrl_minus");
                // ctx.zoom_scale -=
                // ctx.zoom_increment; if
                // (ctx.zoom_scale < 0.1f) {
                // ctx.zoom_scale = 0.1f; }
                // ctx.panning_x = ctx.anchor_ndc_x
                // - ctx.anchor_x * ctx.zoom_scale;
                // ctx.panning_y = ctx.anchor_ndc_y
                // - ctx.anchor_y * ctx.zoom_scale;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_alt_equal:
                call_carmack("cmd_normal_ctrl_alt_equal");
                // ctx.zoom_scale +=
                // ctx.zoom_leap_increment; if
                // (ctx.zoom_scale > 5.0f) {
                // ctx.zoom_scale = 5.0f; }
                // ctx.panning_x = ctx.anchor_ndc_x
                // - ctx.anchor_x * ctx.zoom_scale;
                // ctx.panning_y = ctx.anchor_ndc_y
                // - ctx.anchor_y * ctx.zoom_scale;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_alt_minus:
                call_carmack("cmd_normal_ctrl_alt_minus");
                // ctx.zoom_scale -=
                // ctx.zoom_leap_increment; if
                // (ctx.zoom_scale < 0.1f) {
                // ctx.zoom_scale = 0.1f; }
                // ctx.panning_x = ctx.anchor_ndc_x
                // - ctx.anchor_x * ctx.zoom_scale;
                // ctx.panning_y = ctx.anchor_ndc_y
                // - ctx.anchor_y * ctx.zoom_scale;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_0:
                call_carmack("cmd_normal_ctrl_0");
                ctx.zoom_scale = 1.0f;
                ctx.left_col = 0;
                ctx.bottom_row = 0;
                ctx.right_col =
                    (uint32_t)((ctx.physical_width -
                                ((float)ctx.num_cols_for_line_numbers *
                                 ctx.cell_width)) /
                               ctx.cell_width) -
                    1;
                ctx.top_row = (uint32_t)((ctx.physical_height -
                                          ((float)ctx.num_rows_for_status_bars *
                                           ctx.cell_height)) /
                                         ctx.cell_height) -
                              1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_esc:
                call_carmack("cmd_normal_esc");
                ctx.mode = MODE_NORMAL;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_s:
                call_carmack("cmd_normal_s");
                if (ctx.numeric_prefix) {
                    ctx.cursor_width_sub_col = ctx.numeric_prefix;
                    ctx.t_cursor_width_sub_col = ctx.numeric_prefix;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    ctx.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx.cursor_width_sub_col = 1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_f:
                call_carmack("cmd_normal_f");
                if (ctx.numeric_prefix) {
                    ctx.cursor_width_whole_number = ctx.numeric_prefix;
                    ctx.f_cursor_width_whole_number = ctx.numeric_prefix;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    ctx.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx.cursor_width_whole_number = 1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_t:
                call_carmack("cmd_normal_t");
                if (ctx.numeric_prefix) {
                    ctx.cursor_width_sub_cells = ctx.numeric_prefix;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    ctx.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx.cursor_width_sub_cells = 1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
            cmd_normal_mt:
                call_carmack("cmd_normal_mt");
                if (ctx.numeric_prefix) {
                    ctx.previous_navigation_sub_cells_col =
                        ctx.navigation_sub_cells_col;
                    ctx.navigation_sub_cells_col = ctx.numeric_prefix;
                    ctx.t_navigation_sub_cells = ctx.numeric_prefix;
                    if (ctx.navigation_sub_cells_col !=
                        ctx.previous_navigation_sub_cells_col) {
                        ctx.sub_col =
                            (ctx.sub_col * ctx.navigation_sub_cells_col) /
                            ctx.previous_navigation_sub_cells_col;
                        ctx.sub_col = war_clamp_uint32(
                            ctx.sub_col, 0, ctx.navigation_sub_cells_col - 1);

                        ctx.previous_navigation_sub_cells_col =
                            ctx.navigation_sub_cells_col;
                    }
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    ctx.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx.navigation_sub_cells_col = 1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_mf:
                call_carmack("cmd_normal_mf");
                if (ctx.numeric_prefix) {
                    ctx.navigation_whole_number_col = ctx.numeric_prefix;
                    ctx.f_navigation_whole_number = ctx.numeric_prefix;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    ctx.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx.navigation_whole_number_col = 1;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                ctx.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_gb:
                call_carmack("cmd_normal_gb");
                ctx.row = ctx.min_row;
                uint32_t viewport_height = ctx.top_row - ctx.bottom_row;
                uint32_t distance = viewport_height / 2;
                ctx.bottom_row =
                    war_clamp_subtract_uint32(ctx.row, distance, ctx.min_row);
                ctx.top_row =
                    war_clamp_add_uint32(ctx.row, distance, ctx.max_row);
                uint32_t new_viewport_height = war_clamp_subtract_uint32(
                    ctx.top_row, ctx.bottom_row, ctx.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx.min_row);
                    uint32_t sum =
                        war_clamp_add_uint32(ctx.top_row, diff, ctx.max_row);
                    if (sum < ctx.max_row) {
                        ctx.top_row = sum;
                    } else {
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gt:
                call_carmack("cmd_normal_gt");
                ctx.row = ctx.max_row;
                viewport_height = ctx.top_row - ctx.bottom_row;
                distance = viewport_height / 2;
                ctx.bottom_row =
                    war_clamp_subtract_uint32(ctx.row, distance, ctx.min_row);
                ctx.top_row =
                    war_clamp_add_uint32(ctx.row, distance, ctx.max_row);
                new_viewport_height = war_clamp_subtract_uint32(
                    ctx.top_row, ctx.bottom_row, ctx.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx.min_row);
                    uint32_t sum =
                        war_clamp_add_uint32(ctx.top_row, diff, ctx.max_row);
                    if (sum < ctx.max_row) {
                        ctx.top_row = sum;
                    } else {
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gm:
                call_carmack("cmd_normal_gm");
                ctx.row = 60;
                viewport_height = ctx.top_row - ctx.bottom_row;
                distance = viewport_height / 2;
                ctx.bottom_row =
                    war_clamp_subtract_uint32(ctx.row, distance, ctx.min_row);
                ctx.top_row =
                    war_clamp_add_uint32(ctx.row, distance, ctx.max_row);
                new_viewport_height = war_clamp_subtract_uint32(
                    ctx.top_row, ctx.bottom_row, ctx.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx.min_row);
                    uint32_t sum =
                        war_clamp_add_uint32(ctx.top_row, diff, ctx.max_row);
                    if (sum < ctx.max_row) {
                        ctx.top_row = sum;
                    } else {
                        ctx.bottom_row = war_clamp_subtract_uint32(
                            ctx.bottom_row, diff, ctx.min_row);
                    }
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                goto cmd_done;
            cmd_normal_z:
                call_carmack("cmd_normal_z");
                if (ctx.numeric_prefix) {
                    for (uint32_t i = 0; i < ctx.numeric_prefix; i++) {
                        war_note_quads_add(&note_quads,
                                           &note_quads_count,
                                           window_render_to_audio_ring_buffer,
                                           write_to_audio_index,
                                           &ctx,
                                           red_hex,
                                           full_white_hex,
                                           100.0f,
                                           VOICE_GRAND_PIANO,
                                           false,
                                           false);
                    }
                    ctx.numeric_prefix = 0;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_note_quads_add(&note_quads,
                                   &note_quads_count,
                                   window_render_to_audio_ring_buffer,
                                   write_to_audio_index,
                                   &ctx,
                                   red_hex,
                                   full_white_hex,
                                   100.0f,
                                   VOICE_GRAND_PIANO,
                                   false,
                                   false);
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_return:
                call_carmack("cmd_normal_return");
                // war_note_quads_add(&note_quads,
                //                    &note_quads_count,
                //                    window_render_to_audio_ring_buffer,
                //                    write_to_audio_index,
                //                    &ctx,
                //                    red_hex,
                //                    full_white_hex,
                //                    100.0f,
                //                    VOICE_GRAND_PIANO,
                //                    false,
                //                    false);
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_x:
                call_carmack("cmd_normal_x");
                if (ctx.numeric_prefix) {
                    for (uint32_t i = 0; i < ctx.numeric_prefix; i++) {
                        war_note_quads_delete(
                            &note_quads,
                            &note_quads_count,
                            window_render_to_audio_ring_buffer,
                            write_to_audio_index,
                            &ctx);
                    }
                    ctx.numeric_prefix = 0;
                    memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                    ctx.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_note_quads_delete(&note_quads,
                                      &note_quads_count,
                                      window_render_to_audio_ring_buffer,
                                      write_to_audio_index,
                                      &ctx);
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_div:
                call_carmack("cmd_normal_div");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (int32_t i = (int32_t)note_quads_in_x_count - 1; i >= 0;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    war_note_quads_delete_at_i(
                        &note_quads,
                        &note_quads_count,
                        window_render_to_audio_ring_buffer,
                        write_to_audio_index,
                        i_delete);
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_dov:
                call_carmack("cmd_normal_dov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (int32_t i = (int32_t)note_quads_in_x_count - 1; i >= 0;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    war_note_quads_delete_at_i(
                        &note_quads,
                        &note_quads_count,
                        window_render_to_audio_ring_buffer,
                        write_to_audio_index,
                        i_delete);
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacehov:
                call_carmack("cmd_normal_spacehov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacehiv:
                call_carmack("cmd_normal_spacehiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceha:
                call_carmack("cmd_normal_spaceha");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.hidden[i] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesov:
                call_carmack("cmd_normal_spacesov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesiv:
                call_carmack("cmd_normal_spacesiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesa:
                call_carmack("cmd_normal_spacesa");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.hidden[i] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacemov:
                call_carmack("cmd_normal_spacemov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacemiv:
                call_carmack("cmd_normal_spacemiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacema:
                call_carmack("cmd_normal_spacema");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.mute[i] = true;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceuov:
                call_carmack("cmd_normal_spaceuov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceuiv:
                call_carmack("cmd_normal_spaceuiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceua:
                call_carmack("cmd_normal_spaceua");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = false;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_diw:
                call_carmack("cmd_normal_diw");
                // note_quads_in_x_count = 0;
                // war_note_quads_in_view(&note_quads,
                //                        note_quads_count,
                //                        &ctx,
                //                        note_quads_in_x,
                //                        &note_quads_in_x_count);
                // for (int32_t i = (int32_t)note_quads_in_x_count - 1; i >= 0;
                //      i--) {
                //     uint32_t i_delete = note_quads_in_x[i];
                //     war_note_quads_delete_at_i(
                //         &note_quads,
                //         &note_quads_count,
                //         window_render_to_audio_ring_buffer,
                //         write_to_audio_index,
                //         i_delete);
                // }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacea:
                call_carmack("cmd_normal_spacea");
                if (ctx.views_saved_count < MAX_VIEWS_SAVED) {
                    ctx.views_saved.col[ctx.views_saved_count] = ctx.col;
                    ctx.views_saved.row[ctx.views_saved_count] = ctx.row;
                    ctx.views_saved.left_col[ctx.views_saved_count] =
                        ctx.left_col;
                    ctx.views_saved.right_col[ctx.views_saved_count] =
                        ctx.right_col;
                    ctx.views_saved.bottom_row[ctx.views_saved_count] =
                        ctx.bottom_row;
                    ctx.views_saved.top_row[ctx.views_saved_count] =
                        ctx.top_row;
                    ctx.views_saved_count++;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_dspacea:
                call_carmack("cmd_normal_dspacea");
                ctx.views_saved_count = 0;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_g:
                call_carmack("cmd_normal_alt_g");
                if (ctx.views_saved_count > 0) {
                    ctx.col = ctx.views_saved.col[0];
                    ctx.row = ctx.views_saved.row[0];
                    ctx.left_col = ctx.views_saved.left_col[0];
                    ctx.right_col = ctx.views_saved.right_col[0];
                    ctx.bottom_row = ctx.views_saved.bottom_row[0];
                    ctx.top_row = ctx.views_saved.top_row[0];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_t:
                call_carmack("cmd_normal_alt_t");
                if (ctx.views_saved_count > 1) {
                    ctx.col = ctx.views_saved.col[1];
                    ctx.row = ctx.views_saved.row[1];
                    ctx.left_col = ctx.views_saved.left_col[1];
                    ctx.right_col = ctx.views_saved.right_col[1];
                    ctx.bottom_row = ctx.views_saved.bottom_row[1];
                    ctx.top_row = ctx.views_saved.top_row[1];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_n:
                call_carmack("cmd_normal_alt_n");
                if (ctx.views_saved_count > 2) {
                    ctx.col = ctx.views_saved.col[2];
                    ctx.row = ctx.views_saved.row[2];
                    ctx.left_col = ctx.views_saved.left_col[2];
                    ctx.right_col = ctx.views_saved.right_col[2];
                    ctx.bottom_row = ctx.views_saved.bottom_row[2];
                    ctx.top_row = ctx.views_saved.top_row[2];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_s:
                call_carmack("cmd_normal_alt_s");
                if (ctx.views_saved_count > 3) {
                    ctx.col = ctx.views_saved.col[3];
                    ctx.row = ctx.views_saved.row[3];
                    ctx.left_col = ctx.views_saved.left_col[3];
                    ctx.right_col = ctx.views_saved.right_col[3];
                    ctx.bottom_row = ctx.views_saved.bottom_row[3];
                    ctx.top_row = ctx.views_saved.top_row[3];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_m:
                call_carmack("cmd_normal_alt_m");
                if (ctx.views_saved_count > 4) {
                    ctx.col = ctx.views_saved.col[4];
                    ctx.row = ctx.views_saved.row[4];
                    ctx.left_col = ctx.views_saved.left_col[4];
                    ctx.right_col = ctx.views_saved.right_col[4];
                    ctx.bottom_row = ctx.views_saved.bottom_row[4];
                    ctx.top_row = ctx.views_saved.top_row[4];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_y:
                call_carmack("cmd_normal_alt_y");
                if (ctx.views_saved_count > 5) {
                    ctx.col = ctx.views_saved.col[5];
                    ctx.row = ctx.views_saved.row[5];
                    ctx.left_col = ctx.views_saved.left_col[5];
                    ctx.right_col = ctx.views_saved.right_col[5];
                    ctx.bottom_row = ctx.views_saved.bottom_row[5];
                    ctx.top_row = ctx.views_saved.top_row[5];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_z:
                call_carmack("cmd_normal_alt_z");
                if (ctx.views_saved_count > 6) {
                    ctx.col = ctx.views_saved.col[6];
                    ctx.row = ctx.views_saved.row[6];
                    ctx.left_col = ctx.views_saved.left_col[6];
                    ctx.right_col = ctx.views_saved.right_col[6];
                    ctx.bottom_row = ctx.views_saved.bottom_row[6];
                    ctx.top_row = ctx.views_saved.top_row[6];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_q:
                call_carmack("cmd_normal_alt_q");
                if (ctx.views_saved_count > 7) {
                    ctx.col = ctx.views_saved.col[7];
                    ctx.row = ctx.views_saved.row[7];
                    ctx.left_col = ctx.views_saved.left_col[7];
                    ctx.right_col = ctx.views_saved.right_col[7];
                    ctx.bottom_row = ctx.views_saved.bottom_row[7];
                    ctx.top_row = ctx.views_saved.top_row[7];
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_e:
                call_carmack("cmd_normal_alt_e");
                ctx.mode = (ctx.mode != MODE_VIEWS_SAVED) ? MODE_VIEWS_SAVED :
                                                            MODE_NORMAL;
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                call_carmack("mode: %u", ctx.mode);
                goto cmd_done;
            cmd_normal_a:
                call_carmack("cmd_normal_a");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space1:
                call_carmack("cmd_normal_space1");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space2:
                call_carmack("cmd_normal_space2");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space3:
                call_carmack("cmd_normal_space3");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space4:
                call_carmack("cmd_normal_space4");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space5:
                call_carmack("cmd_normal_space5");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space6:
                call_carmack("cmd_normal_space6");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space7:
                call_carmack("cmd_normal_space7");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space8:
                call_carmack("cmd_normal_space8");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space9:
                call_carmack("cmd_normal_space9");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space0:
                call_carmack("cmd_normal_space0");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_1:
                call_carmack("cmd_normal_alt_1");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_2:
                call_carmack("cmd_normal_alt_2");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_3:
                call_carmack("cmd_normal_alt_3");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_4:
                call_carmack("cmd_normal_alt_4");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_5:
                call_carmack("cmd_normal_alt_5");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_6:
                call_carmack("cmd_normal_alt_6");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_7:
                call_carmack("cmd_normal_alt_7");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_8:
                call_carmack("cmd_normal_alt_8");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_9:
                call_carmack("cmd_normal_alt_9");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_0:
                call_carmack("cmd_normal_alt_0");
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacedc:
                call_carmack("cmd_normal_spacedc");
                ctx.cursor_width_sub_cells = 1;
                ctx.cursor_width_whole_number = 1;
                ctx.cursor_width_sub_col = 1;
                ctx.navigation_whole_number_col = 1;
                ctx.navigation_sub_cells_col = 1;
                if (ctx.navigation_sub_cells_col !=
                    ctx.previous_navigation_sub_cells_col) {
                    ctx.sub_col = (ctx.sub_col * ctx.navigation_sub_cells_col) /
                                  ctx.previous_navigation_sub_cells_col;
                    ctx.sub_col = war_clamp_uint32(
                        ctx.sub_col, 0, ctx.navigation_sub_cells_col - 1);

                    ctx.previous_navigation_sub_cells_col =
                        ctx.navigation_sub_cells_col;
                }
                ctx.numeric_prefix = 0;
                memset(ctx.input_sequence, 0, sizeof(ctx.input_sequence));
                ctx.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_done:
                war_wayland_holy_trinity(fd,
                                         wl_surface_id,
                                         wl_buffer_id,
                                         0,
                                         0,
                                         0,
                                         0,
                                         physical_width,
                                         physical_height);
                if (goto_cmd_repeat_done) {
                    goto_cmd_repeat_done = false;
                    goto cmd_repeat_done;
                }
                if (goto_cmd_timeout_done) {
                    goto_cmd_timeout_done = false;
                    goto cmd_timeout_done;
                }
                goto done;
            wl_keyboard_modifiers:
                dump_bytes("wl_keyboard_modifiers event",
                           msg_buffer + msg_buffer_offset,
                           size);
                xkb_state_update_mask(
                    xkb_state,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4),
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4),
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 +
                                  4),
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 +
                                  4 + 4),
                    0,
                    0);
                goto done;
            wl_keyboard_repeat_info:
                dump_bytes("wl_keyboard_repeat_info event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_enter:
                dump_bytes("wl_pointer_enter event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_leave:
                dump_bytes("wl_pointer_leave event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_motion:
                dump_bytes("wl_pointer_motion event",
                           msg_buffer + msg_buffer_offset,
                           size);
                ctx.cursor_x = (float)(int32_t)war_read_le32(
                                   msg_buffer + msg_buffer_offset + 12) /
                               256.0f * scale_factor;
                ctx.cursor_y = (float)(int32_t)war_read_le32(
                                   msg_buffer + msg_buffer_offset + 16) /
                               256.0f * scale_factor;
                call_carmack("CURSOR_X: %f", ctx.cursor_x);
                call_carmack("CURSOR_Y: %f", ctx.cursor_y);
                goto done;
            wl_pointer_button:
                dump_bytes("wl_pointer_button event",
                           msg_buffer + msg_buffer_offset,
                           size);
                switch (
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 12)) {
                case 1:
                    if (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 8) ==
                        BTN_LEFT) {
                        if (((int)(ctx.cursor_x / vk_ctx.cell_width) -
                             (int)ctx.num_cols_for_line_numbers) < 0) {
                            ctx.col = ctx.left_col;
                            break;
                        }
                        if ((((physical_height - ctx.cursor_y) /
                              vk_ctx.cell_height) -
                             ctx.num_rows_for_status_bars) < 0) {
                            ctx.row = ctx.bottom_row;
                            break;
                        }
                        ctx.col = (uint32_t)(ctx.cursor_x / vk_ctx.cell_width) -
                                  ctx.num_cols_for_line_numbers + ctx.left_col;
                        ctx.row = (uint32_t)((physical_height - ctx.cursor_y) /
                                             vk_ctx.cell_height) -
                                  ctx.num_rows_for_status_bars + ctx.bottom_row;
                        if (ctx.row > ctx.max_row) { ctx.row = ctx.max_row; }
                        if (ctx.row > ctx.top_row) { ctx.row = ctx.top_row; }
                        if (ctx.row < ctx.bottom_row) {
                            ctx.row = ctx.bottom_row;
                        }
                        if (ctx.col > ctx.max_col) { ctx.col = ctx.max_col; }
                        if (ctx.col > ctx.right_col) {
                            ctx.col = ctx.right_col;
                        }
                        if (ctx.col < ctx.left_col) { ctx.col = ctx.left_col; }
                    }
                }
                goto done;
            wl_pointer_axis:
                dump_bytes("wl_pointer_axis event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_frame:
                dump_bytes("wl_pointer_frame event",
                           msg_buffer + msg_buffer_offset,
                           size);
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
            wl_pointer_axis_source:
                dump_bytes("wl_pointer_axis_source event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_axis_stop:
                dump_bytes("wl_pointer_axis_stop event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_axis_discrete:
                dump_bytes("wl_pointer_axis_discrete event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_axis_value120:
                dump_bytes("wl_pointer_axis_value120 event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_pointer_axis_relative_direction:
                dump_bytes("wl_pointer_axis_relative_direction event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_down:
                dump_bytes("wl_touch_down event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_up:
                dump_bytes(
                    "wl_touch_up event", msg_buffer + msg_buffer_offset, size);
                goto done;
            wl_touch_motion:
                dump_bytes("wl_touch_motion event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_frame:
                dump_bytes("wl_touch_frame event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_cancel:
                dump_bytes("wl_touch_cancel event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_shape:
                dump_bytes("wl_touch_shape event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_touch_orientation:
                dump_bytes("wl_touch_orientation event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_geometry:
                dump_bytes("wl_output_geometry event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_mode:
                dump_bytes("wl_output_mode event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_done:
                dump_bytes("wl_output_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_scale:
                dump_bytes("wl_output_scale event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_name:
                dump_bytes("wl_output_name event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wl_output_description:
                dump_bytes("wl_output_description event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto done;
            wayland_default:
                dump_bytes(
                    "default event", msg_buffer + msg_buffer_offset, size);
                goto done;
            done:
                msg_buffer_offset += size;
                continue;
            }

            if (msg_buffer_offset > 0) {
                memmove(msg_buffer,
                        msg_buffer + msg_buffer_offset,
                        msg_buffer_size - msg_buffer_offset);
                msg_buffer_size -= msg_buffer_offset;
            }
        }
    }
    //-------------------------------------------------------------------------
    // CLEANUP
    //-------------------------------------------------------------------------
#if DMABUF
    close(vk_ctx.dmabuf_fd);
    vk_ctx.dmabuf_fd = -1;
#endif
    xkb_state_unref(xkb_state);
    xkb_context_unref(xkb_context);

    end("war_window_render");
    return 0;
}

void* war_audio(void* args) {
    header("war_audio");
    war_thread_args* a = (war_thread_args*)args;
    uint8_t* audio_to_window_render_ring_buffer =
        a->audio_to_window_render_ring_buffer;
    uint8_t* write_to_window_render_index = a->write_to_window_render_index;
    uint8_t* window_render_to_audio_ring_buffer =
        a->window_render_to_audio_ring_buffer;
    uint8_t* read_from_window_render_index = a->read_from_window_render_index;
    uint8_t* write_to_audio_index = a->write_to_audio_index;
    uint8_t from_window_render = 0;

    // Open the default PCM device for playback (blocking mode)
    snd_pcm_t* pcm_handle;
    int result =
        snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    assert(result >= 0);

    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(
        pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(
        pcm_handle, hw_params, SND_PCM_FORMAT_FLOAT_LE);

    unsigned int sample_rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0);

    snd_pcm_hw_params_set_channels(
        pcm_handle, hw_params, NUM_CHANNELS); // stereo

    snd_pcm_uframes_t period_size = PERIOD_SIZE;
    snd_pcm_hw_params_set_period_size_near(
        pcm_handle, hw_params, &period_size, 0);

    snd_pcm_uframes_t buffer_size = period_size * 4;
    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);

    result = snd_pcm_hw_params(pcm_handle, hw_params);
    assert(result >= 0);

    // prepare the device
    result = snd_pcm_prepare(pcm_handle);
    assert(result >= 0);

    // timestamp
    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);
    result = snd_pcm_status(pcm_handle, status);
    assert(result >= 0);
    snd_timestamp_t time_stamp;

    size_t sec_size = sizeof(time_stamp.tv_sec);
    assert(sec_size == 8);
    size_t usec_size = sizeof(time_stamp.tv_usec);
    assert(usec_size == 8);

    float sample_buffer[PERIOD_SIZE * NUM_CHANNELS];

    uint8_t end_audio = false;
    while (!end_audio) {
        if (read_from_window_render_index != write_to_audio_index) {
            from_window_render = window_render_to_audio_ring_buffer
                [*read_from_window_render_index];
            (*read_from_window_render_index) =
                ((*read_from_window_render_index) + 1) & 0xFF;

            switch (from_window_render) {
            case AUDIO_GET_TIMESTAMP:
                snd_pcm_status(pcm_handle, status);
                snd_pcm_status_get_tstamp(status, &time_stamp);
                audio_to_window_render_ring_buffer
                    [*write_to_window_render_index] = AUDIO_GET_TIMESTAMP;
                *write_to_window_render_index =
                    (*write_to_window_render_index + 1) & 0xFF;
                size_t space_till_end =
                    ring_buffer_size - *write_to_window_render_index;
                if (space_till_end < 16) { *write_to_window_render_index = 0; }
                war_write_le64(audio_to_window_render_ring_buffer +
                                   *write_to_window_render_index,
                               time_stamp.tv_sec);
                war_write_le64(audio_to_window_render_ring_buffer +
                                   *write_to_window_render_index + 8,
                               time_stamp.tv_usec);
                *write_to_window_render_index =
                    (*write_to_window_render_index + 16) & 0xFF;

                from_window_render = 0;
                break;
            case AUDIO_END_WAR:
                call_carmack("AUDIO_END_WAR");

                end_audio = true;
                continue;
                break;
            }
        }

        for (int i = 0; i < PERIOD_SIZE * NUM_CHANNELS; i++) {
            sample_buffer[i] = 0;
        }

        snd_pcm_sframes_t frames =
            snd_pcm_writei(pcm_handle, sample_buffer, PERIOD_SIZE);
        if (frames < 0) {
            frames = snd_pcm_recover(pcm_handle, frames, 1);
            if (frames < 0) {
                call_carmack("snd_pcm_writei failed: %s", snd_strerror(frames));
            }
        }
    }
    end("war_audio");
    return 0;
}
