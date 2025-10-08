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

#include "war_vulkan.c"
#include "war_wayland.c"

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_main.h"

#include <errno.h>
#include <fcntl.h>
#include <pipewire-0.3/pipewire/context.h>
#include <pipewire-0.3/pipewire/core.h>
#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/stream.h>
#include <pthread.h>
#include <sched.h>
#include <spa-0.2/spa/param/audio/format-utils.h>
#include <spa-0.2/spa/param/audio/format.h>
#include <spa-0.2/spa/param/audio/raw.h>
#include <spa-0.2/spa/param/latency-utils.h>
#include <spa-0.2/spa/pod/builder.h>
#include <spa-0.2/spa/pod/pod.h>
#include <spa-0.2/spa/utils/hook.h>
#include <spa-0.2/spa/utils/list.h>
#include <spa-0.2/spa/utils/result.h>
#include <spa-0.2/spa/utils/string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

int main() {
    CALL_CARMACK("main");
    war_producer_consumer pc;
    {
        pc.to_a = malloc(sizeof(uint8_t) * PC_BUFFER_SIZE);
        assert(pc.to_a);
        memset(pc.to_a, 0, PC_BUFFER_SIZE);
        pc.to_wr = malloc(sizeof(uint8_t) * PC_BUFFER_SIZE);
        assert(pc.to_wr);
        memset(pc.to_wr, 0, PC_BUFFER_SIZE);
        pc.i_to_a = 0;
        pc.i_to_wr = 0;
        pc.i_from_a = 0;
        pc.i_from_wr = 0;
    }
    war_atomics atomics = {
        .state = AUDIO_CMD_STOP,
        .play_clock = 0,
        .play_frames = 0,
        .record_frames = 0,
        .record_monitor = false,
        .record_threshold = 0.01f,
        .play_gain = 1.0f,
        .record_gain = 1.0f,
        .record = 0,
        .play = 0,
        .map = 0,
        .map_note = -1,
        .loop = 0,
        .start_war = 0,
        .resample = 1,
        .midi_record_frames = 0,
    };
    pthread_t war_window_render_thread;
    pthread_create(&war_window_render_thread,
                   NULL,
                   war_window_render,
                   (void* [2]){&pc, &atomics});
    pthread_t war_audio_thread;
    pthread_create(
        &war_audio_thread, NULL, war_audio, (void* [2]){&pc, &atomics});
    pthread_join(war_window_render_thread, NULL);
    pthread_join(war_audio_thread, NULL);
    END("main");
    return 0;
}
//-----------------------------------------------------------------------------
// THREAD WINDOW RENDER
//-----------------------------------------------------------------------------
void* war_window_render(void* args) {
    header("war_window_render");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];

    // const uint32_t internal_width = 1920;
    // const uint32_t internal_height = 1080;

    uint32_t physical_width = 2560;
    uint32_t physical_height = 1600;
    const uint32_t stride = physical_width * 4;

    const uint32_t light_gray_hex = 0xFF454950;
    const uint32_t darker_light_gray_hex = 0xFF36383C; // gutter / line numbers
    const uint32_t dark_gray_hex = 0xFF282828;
    const uint32_t red_hex = 0xFF0000DE;
    const uint32_t white_hex = 0xFFB1D9E9; // nvim status text
    const uint32_t black_hex = 0xFF000000;
    const uint32_t full_white_hex = 0xFFFFFFFF;
    const float default_horizontal_line_thickness = 0.018f;
    const float default_vertical_line_thickness = 0.018f; // default: 0.018
    const float default_outline_thickness =
        0.04f; // 0.027 is minimum for preventing 1/4, 1/7, 1/9, max = 0.075f
               // sub_cursor right outline from disappearing
    const float default_alpha_scale = 0.2f;
    const float default_cursor_alpha_scale = 0.6f;
    const float default_playback_bar_thickness = 0.05f;
    const float default_text_feather = 0.5f;
    const float default_text_thickness = 0.0f;
    const float windowed_text_feather = 0.0f;
    const float windowed_text_thickness = 0.0f;

#if DMABUF
    war_vulkan_context ctx_vk =
        war_vulkan_init(physical_width, physical_height);
    assert(ctx_vk.dmabuf_fd >= 0);
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
    uint32_t viewport_cols = (uint32_t)(physical_width / ctx_vk.cell_width);
    uint32_t viewport_rows = (uint32_t)(physical_height / ctx_vk.cell_height);
    uint32_t visible_rows =
        (uint32_t)((physical_height -
                    ((float)num_rows_for_status_bars * ctx_vk.cell_height)) /
                   ctx_vk.cell_height);
    war_audio_context ctx_a = {
        .sample_rate = AUDIO_DEFAULT_SAMPLE_RATE,
        .BPM = AUDIO_DEFAULT_BPM,
        .channel_count = AUDIO_DEFAULT_CHANNEL_COUNT,
        .period_size = AUDIO_DEFAULT_PERIOD_SIZE,
    };
    war_window_render_context ctx_wr = {
        .skip_release = 0,
        .trigger = 0,
        .midi_octave = 4,
        .record_octave = 4,
        .gain_increment = 0.05f,
        .trinity = false,
        .fullscreen = false,
        .end_window_render = false,
        .FPS = 240,
        .now = 0,
        .mode = MODE_NORMAL,
        .hud_state = HUD_PIANO,
        .cursor_blink_state = 0,
        .cursor_blink_duration_us = DEFAULT_CURSOR_BLINK_DURATION,
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
                                          ctx_vk.cell_width)) /
                       ctx_vk.cell_width) -
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
        .alpha_scale_cursor = default_cursor_alpha_scale,
        .anchor_ndc_x = 0.0f,
        .anchor_ndc_y = 0.0f,
        .viewport_cols = viewport_cols,
        .viewport_rows = viewport_rows,
        .scroll_margin_cols = 0, // cols from visible min/max col
        .scroll_margin_rows = 0, // rows from visible min/max row
        .cell_width = ctx_vk.cell_width,
        .cell_height = ctx_vk.cell_height,
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
        .sleep = false,
        .playback_bar_pos_x = 0.0f,
        .light_gray_hex = light_gray_hex,
        .darker_light_gray_hex = darker_light_gray_hex,
        .dark_gray_hex = dark_gray_hex,
        .red_hex = red_hex,
        .white_hex = white_hex,
        .black_hex = black_hex,
        .full_white_hex = full_white_hex,
        .horizontal_line_thickness = default_horizontal_line_thickness,
        .vertical_line_thickness = default_vertical_line_thickness,
        .outline_thickness = default_outline_thickness,
        .alpha_scale = default_alpha_scale,
        .playback_bar_thickness = default_playback_bar_thickness,
        .text_feather = default_text_feather,
        .text_thickness = default_text_thickness,
        .text_feather_bold = 0.20f,
        .text_thickness_bold = 0.30f,
        .text_top_status_bar_count = 0,
        .text_middle_status_bar_count = 0,
        .text_bottom_status_bar_count = 0,
        .color_note_default = red_hex,
        .color_note_outline_default = white_hex,
        .color_cursor = red_hex,
        .color_cursor_transparent = white_hex,
    };
    for (int i = 0; i < LAYER_COUNT; i++) {
        ctx_wr.layers[i] = i / ctx_wr.layer_count;
    }
    war_views views;
    {
        uint32_t* views_col = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        uint32_t* views_row = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        uint32_t* views_left_col = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        uint32_t* views_right_col = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        uint32_t* views_bottom_row = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        uint32_t* views_top_row = malloc(sizeof(uint32_t) * MAX_VIEWS_SAVED);
        char** warpoon_text = malloc(sizeof(char*) * MAX_VIEWS_SAVED);
        for (uint32_t i = 0; i < MAX_VIEWS_SAVED; i++) {
            warpoon_text[i] = malloc(sizeof(char) * MAX_WARPOON_TEXT_COLS);
            memset(warpoon_text[i], 0, MAX_WARPOON_TEXT_COLS);
        }
        uint32_t warpoon_viewport_cols = 25;
        uint32_t warpoon_viewport_rows = 8;
        uint32_t warpoon_hud_cols = 2;
        uint32_t warpoon_hud_rows = 0;
        uint32_t warpoon_max_col = MAX_WARPOON_TEXT_COLS - 1 - warpoon_hud_cols;
        uint32_t warpoon_max_row = MAX_VIEWS_SAVED - 1 - warpoon_hud_rows;
        views = (war_views){
            .col = views_col,
            .row = views_row,
            .left_col = views_left_col,
            .right_col = views_right_col,
            .bottom_row = views_bottom_row,
            .top_row = views_top_row,
            .views_count = 0,
            .warpoon_text = warpoon_text,
            .warpoon_mode = MODE_NORMAL,
            .warpoon_max_col = warpoon_max_col,
            .warpoon_max_row = warpoon_max_row,
            .warpoon_viewport_cols = warpoon_viewport_cols,
            .warpoon_viewport_rows = warpoon_viewport_rows,
            .warpoon_hud_cols = warpoon_hud_cols,
            .warpoon_hud_rows = warpoon_hud_rows,
            .warpoon_left_col = 0,
            .warpoon_right_col = warpoon_viewport_cols - warpoon_hud_cols - 1,
            .warpoon_bottom_row = warpoon_max_row - warpoon_viewport_rows + 1,
            .warpoon_top_row = warpoon_max_row,
            .warpoon_min_col = 0,
            .warpoon_min_row = 0,
            .warpoon_col = 0,
            .warpoon_row = warpoon_max_row,
            .warpoon_color_bg = ctx_wr.darker_light_gray_hex,
            .warpoon_color_outline = ctx_wr.white_hex,
            .warpoon_color_hud = ctx_wr.red_hex,
            .warpoon_color_hud_text = ctx_wr.full_white_hex,
            .warpoon_color_text = ctx_wr.white_hex,
            .warpoon_color_cursor = ctx_wr.white_hex,
        };
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
        memset(fsm[i].is_terminal, 0, sizeof(fsm[i].is_terminal));
        memset(fsm[i].handle_release, 0, sizeof(fsm[i].handle_release));
        memset(fsm[i].handle_repeat, 0, sizeof(fsm[i].handle_repeat));
        memset(fsm[i].handle_timeout, 0, sizeof(fsm[i].handle_timeout));
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
    uint32_t wl_region_id = 0;
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
    uint32_t zxdg_toplevel_decoration_v1_id = 0;
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
        (uint32_t)(physical_width /
                   (ctx_vk.cell_width * ctx_wr.min_zoom_scale));
    uint32_t max_viewport_rows =
        (uint32_t)(physical_height /
                   (ctx_vk.cell_height * ctx_wr.min_zoom_scale));
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
    note_quads.gain = (float*)note_quads_p;
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
    war_quad_vertex* transparent_quad_vertices =
        malloc(sizeof(war_quad_vertex) * max_quads);
    uint32_t transparent_quad_vertices_count = 0;
    uint16_t* transparent_quad_indices = malloc(sizeof(uint16_t) * max_quads);
    uint32_t transparent_quad_indices_count = 0;
    war_text_vertex* text_vertices =
        malloc(sizeof(war_text_vertex) * max_text_quads);
    uint32_t text_vertices_count = 0;
    uint16_t* text_indices = malloc(sizeof(uint16_t) * max_text_quads);
    uint32_t text_indices_count = 0;

    ctx_wr.text_bottom_status_bar = malloc(sizeof(char) * MAX_STATUS_BAR_COLS);
    ctx_wr.text_middle_status_bar = malloc(sizeof(char) * MAX_STATUS_BAR_COLS);
    ctx_wr.text_top_status_bar = malloc(sizeof(char) * MAX_STATUS_BAR_COLS);

    const double microsecond_conversion = 1000000.0;
    ctx_wr.sleep_duration_us = 50000;
    ctx_wr.frame_duration_us =
        (uint64_t)round((1.0 / (double)ctx_wr.FPS) * microsecond_conversion);
    uint64_t last_frame_time = war_get_monotonic_time_us();
    ctx_wr.cursor_blink_previous_us = last_frame_time;
    ctx_wr.cursor_blinking = false;

    void* pc_window_render[AUDIO_CMD_COUNT];
    pc_window_render[AUDIO_CMD_STOP] = &&pc_stop;
    pc_window_render[AUDIO_CMD_PLAY] = &&pc_play;
    pc_window_render[AUDIO_CMD_PAUSE] = &&pc_pause;
    pc_window_render[AUDIO_CMD_GET_FRAMES] = &&pc_get_frames;
    pc_window_render[AUDIO_CMD_ADD_NOTE] = &&pc_add_note;
    pc_window_render[AUDIO_CMD_END_WAR] = &&pc_end_war;
    pc_window_render[AUDIO_CMD_SEEK] = &&pc_seek;
    pc_window_render[AUDIO_CMD_RECORD_WAIT] = &&pc_record_wait;
    pc_window_render[AUDIO_CMD_RECORD] = &&pc_record;
    pc_window_render[AUDIO_CMD_RECORD_MAP] = &&pc_record_map;
    pc_window_render[AUDIO_CMD_SET_THRESHOLD] = &&pc_set_threshold;
    pc_window_render[AUDIO_CMD_NOTE_ON] = &&pc_note_on;
    pc_window_render[AUDIO_CMD_NOTE_OFF] = &&pc_note_off;
    pc_window_render[AUDIO_CMD_RESET_MAPPINGS] = &&pc_reset_mappings;
    pc_window_render[AUDIO_CMD_MIDI_RECORD_WAIT] = &&pc_midi_record_wait;
    pc_window_render[AUDIO_CMD_MIDI_RECORD] = &&pc_midi_record;
    pc_window_render[AUDIO_CMD_MIDI_RECORD_MAP] = &&pc_midi_record_map;
    while (!atomic_load(&atomics->start_war)) { usleep(1000); }
    while (atomics->state != AUDIO_CMD_END_WAR) {
    pc_window_render:
        uint32_t header;
        uint32_t size;
        uint8_t payload[PC_BUFFER_SIZE];
        if (war_pc_from_a(pc, &header, &size, payload)) {
            goto* pc_window_render[header];
        }
        goto pc_window_render_done;
    pc_stop:
        call_carmack("from a: STOP");
        atomic_store(&atomics->state, AUDIO_CMD_STOP);
        goto pc_window_render;
    pc_play:
        call_carmack("from a: PLAY");
        ctx_wr.cursor_blink_previous_us = ctx_wr.now;
        ctx_wr.cursor_blinking = false;
        ctx_wr.cursor_blink_duration_us = (uint64_t)round(
            (60.0 / ((double)ctx_a.BPM)) * microsecond_conversion);
        goto pc_window_render;
    pc_pause:
        call_carmack("from a: PAUSE");
        goto pc_window_render;
    pc_get_frames:
        goto pc_window_render;
    pc_add_note:
        goto pc_window_render;
    pc_end_war:
        call_carmack("from a: END_WAR");
        goto pc_window_render;
    pc_seek:
        goto pc_window_render;
    pc_record_wait:
        call_carmack("from a: RECORD_WAIT");
        goto pc_window_render;
    pc_record:
        call_carmack("from a: RECORD");
        atomic_store(&atomics->state, AUDIO_CMD_RECORD);
        goto pc_window_render;
    pc_record_map:
        call_carmack("from a: RECORD_MAP");
        atomic_store(&atomics->state, AUDIO_CMD_STOP);
        goto pc_window_render;
    pc_set_threshold:
        goto pc_window_render;
    pc_note_on:
        goto pc_window_render;
    pc_note_off: {
        call_carmack("from a: NOTE_OFF");
        int note;
        memcpy(&note, payload, size);
        ctx_wr.skip_release = 1;
        goto pc_window_render;
    }
    pc_reset_mappings:
        call_carmack("from a: RESET_MAPPINGS");
        goto pc_window_render;
    pc_midi_record:
        call_carmack("from a: MIDI_RECORD");
        atomic_store(&atomics->state, AUDIO_CMD_MIDI_RECORD);
        goto pc_window_render;
    pc_midi_record_wait:
        call_carmack("from a: MIDI_RECORD_WAIT");
        goto pc_window_render;
    pc_midi_record_map:
        call_carmack("from a: MIDI_RECORD_MAP");
        goto pc_window_render;
    pc_window_render_done:
        ctx_wr.now = war_get_monotonic_time_us();
        if (ctx_wr.now - last_frame_time >= ctx_wr.frame_duration_us) {
            war_get_frame_duration_us(&ctx_wr);
            last_frame_time += ctx_wr.frame_duration_us;
            if (ctx_wr.trinity) {
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
        // cursor blink
        if (ctx_wr.cursor_blink_state &&
            ctx_wr.now - ctx_wr.cursor_blink_previous_us >=
                ctx_wr.cursor_blink_duration_us &&
            (ctx_wr.mode == MODE_NORMAL ||
             (ctx_wr.mode == MODE_VIEWS &&
              views.warpoon_mode != MODE_VISUAL_LINE))) {
            ctx_wr.cursor_blink_duration_us =
                (ctx_wr.cursor_blink_state == CURSOR_BLINK) ?
                    DEFAULT_CURSOR_BLINK_DURATION :
                    (uint64_t)round((60.0 / ((double)ctx_a.BPM)) *
                                    microsecond_conversion);
            ;
            ctx_wr.cursor_blink_previous_us += ctx_wr.cursor_blink_duration_us;
            ctx_wr.cursor_blinking = !ctx_wr.cursor_blinking;
        }
        //---------------------------------------------------------------------
        // KEY REPEATS
        //---------------------------------------------------------------------
        if (repeat_keysym) {
            uint32_t k = repeat_keysym;
            uint8_t m = repeat_mod;
            if (key_down[k][m]) {
                uint64_t elapsed = ctx_wr.now - key_last_event_us[k][m];
                if (!repeating) {
                    // still waiting for initial delay
                    if (elapsed >= repeat_delay_us) {
                        repeating = true;
                        key_last_event_us[k][m] = ctx_wr.now; // reset timer
                    }
                } else {
                    // normal repeating
                    if (elapsed >= repeat_rate_us) {
                        key_last_event_us[k][m] = ctx_wr.now;
                        uint16_t next_state_index =
                            fsm[current_state_index].next_state[k][m];
                        if (next_state_index != 0) {
                            current_state_index = next_state_index;
                            fsm_state_last_event_us = ctx_wr.now;
                            if (fsm[current_state_index]
                                    .is_terminal[ctx_wr.mode] &&
                                !war_state_is_prefix(
                                    &ctx_wr, current_state_index, fsm) &&
                                fsm[current_state_index]
                                    .handle_repeat[ctx_wr.mode]) {
                                uint16_t temp = current_state_index;
                                current_state_index = 0;
                                goto_cmd_repeat_done = true;
                                if (fsm[temp].command[ctx_wr.mode]) {
                                    goto* fsm[temp].command[ctx_wr.mode];
                                }
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
        if (timeout && ctx_wr.now >= timeout_start_us + timeout_duration_us) {
            uint16_t temp = timeout_state_index;
            timeout = false;
            timeout_state_index = 0;
            timeout_start_us = 0;
            goto_cmd_timeout_done = true;
            // clear current
            current_state_index = 0;
            fsm_state_last_event_us = ctx_wr.now;
            if (fsm[temp].command[ctx_wr.mode]) {
                goto* fsm[temp].command[ctx_wr.mode];
            }
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
                    goto wayland_done;
                }
                size_t idx = obj_op_index(object_id, opcode);
                if (obj_op[idx]) { goto* obj_op[idx]; }
                goto wayland_default;
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
                if (!wl_region_id && wl_surface_id && wl_compositor_id) {
                    uint8_t create_region[12];
                    war_write_le32(create_region, wl_compositor_id);
                    war_write_le16(create_region + 4, 1);
                    war_write_le16(create_region + 6, 12);
                    war_write_le32(create_region + 8, new_id);
                    dump_bytes("create_region request", create_region, 12);
                    call_carmack("bound: wl_region");
                    ssize_t create_region_written =
                        write(fd, create_region, 12);
                    assert(create_region_written == 12);
                    wl_region_id = new_id;
                    new_id++;

                    uint8_t region_add[24];
                    war_write_le32(region_add, wl_region_id);
                    war_write_le16(region_add + 4, 1);
                    war_write_le16(region_add + 6, 24);
                    war_write_le32(region_add + 8, 0);
                    war_write_le32(region_add + 12, 0);
                    war_write_le32(region_add + 16, physical_width);
                    war_write_le32(region_add + 20, physical_height);
                    dump_bytes("wl_region::add request", region_add, 24);
                    ssize_t region_add_written = write(fd, region_add, 24);
                    assert(region_add_written == 24);

                    war_wl_surface_set_opaque_region(
                        fd, wl_surface_id, wl_region_id);
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
                }
                if (!zxdg_toplevel_decoration_v1_id && xdg_toplevel_id &&
                    zxdg_decoration_manager_v1_id) {
                    uint8_t get_toplevel_decoration[16];
                    war_write_le32(get_toplevel_decoration,
                                   zxdg_decoration_manager_v1_id);
                    war_write_le16(get_toplevel_decoration + 4, 1);
                    war_write_le16(get_toplevel_decoration + 6, 16);
                    war_write_le32(get_toplevel_decoration + 8, new_id);
                    war_write_le32(get_toplevel_decoration + 12,
                                   xdg_toplevel_id);
                    dump_bytes("get_toplevel_decoration request",
                               get_toplevel_decoration,
                               16);
                    call_carmack("bound: zxdg_toplevel_decoration_v1");
                    ssize_t get_toplevel_decoration_written =
                        write(fd, get_toplevel_decoration, 16);
                    assert(get_toplevel_decoration_written == 16);
                    zxdg_toplevel_decoration_v1_id = new_id;
                    obj_op[obj_op_index(zxdg_toplevel_decoration_v1_id, 0)] =
                        &&zxdg_toplevel_decoration_v1_configure;
                    new_id++;

                    //---------------------------------------------------------
                    // initial commit
                    //---------------------------------------------------------
                    war_wayland_wl_surface_commit(fd, wl_surface_id);
                }
                goto wayland_done;
            wl_registry_global_remove:
                dump_bytes(
                    "global_rm event", msg_buffer + msg_buffer_offset, size);
                goto wayland_done;
            wl_callback_done:
                //-------------------------------------------------------------
                // RENDERING WITH VULKAN
                //-------------------------------------------------------------
                // dump_bytes("wl_callback::wayland_done event",
                //           msg_buffer + msg_buffer_offset,
                //           size);
#if DMABUF
                assert(ctx_vk.current_frame == 0);
                vkWaitForFences(ctx_vk.device,
                                1,
                                &ctx_vk.in_flight_fences[ctx_vk.current_frame],
                                VK_TRUE,
                                UINT64_MAX);
                vkResetFences(ctx_vk.device,
                              1,
                              &ctx_vk.in_flight_fences[ctx_vk.current_frame]);
                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                };
                VkResult result =
                    vkBeginCommandBuffer(ctx_vk.cmd_buffer, &begin_info);
                assert(result == VK_SUCCESS);
                VkClearValue clear_values[2];
                clear_values[0].color =
                    (VkClearColorValue){{0.1569f, 0.1569f, 0.1569f, 1.0f}};
                clear_values[1].depthStencil = (VkClearDepthStencilValue){
                    ctx_wr.layers[LAYER_OPAQUE_REGION], 0.0f};
                VkRenderPassBeginInfo render_pass_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                    .renderPass = ctx_vk.render_pass,
                    .framebuffer = ctx_vk.frame_buffer,
                    .renderArea =
                        {
                            .offset = {0, 0},
                            .extent = {physical_width, physical_height},
                        },
                    .clearValueCount = 2,
                    .pClearValues = clear_values,
                };
                vkCmdBeginRenderPass(ctx_vk.cmd_buffer,
                                     &render_pass_info,
                                     VK_SUBPASS_CONTENTS_INLINE);
                quad_vertices_count = 0;
                quad_indices_count = 0;
                transparent_quad_vertices_count = 0;
                transparent_quad_indices_count = 0;
                text_vertices_count = 0;
                text_indices_count = 0;
                //---------------------------------------------------------
                // QUAD PIPELINE
                //---------------------------------------------------------
                vkCmdBindPipeline(ctx_vk.cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  ctx_vk.quad_pipeline);
                // draw cursor quads
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                uint32_t cursor_color = ctx_wr.color_cursor;
                float alpha_factor = ctx_wr.alpha_scale_cursor;
                uint8_t color_alpha =
                    (ctx_wr.color_cursor_transparent >> 24) & 0xFF;
                uint32_t cursor_color_transparent =
                    ((uint8_t)(color_alpha * alpha_factor) << 24) |
                    (ctx_wr.color_cursor_transparent & 0x00FFFFFF);
                cursor_color = (note_quads_in_x_count) ?
                                   cursor_color_transparent :
                                   cursor_color;
                if (ctx_wr.mode == MODE_NORMAL && !ctx_wr.cursor_blinking) {
                    war_make_transparent_quad(
                        transparent_quad_vertices,
                        transparent_quad_indices,
                        &transparent_quad_vertices_count,
                        &transparent_quad_indices_count,
                        (float[3]){ctx_wr.col +
                                       (float)ctx_wr.sub_col /
                                           ctx_wr.navigation_sub_cells_col,
                                   ctx_wr.row,
                                   ctx_wr.layers[LAYER_CURSOR]},
                        (float[2]){(float)ctx_wr.cursor_width_whole_number *
                                       ctx_wr.cursor_width_sub_col /
                                       ctx_wr.cursor_width_sub_cells,
                                   1},
                        cursor_color,
                        0,
                        0,
                        (float[2]){0.0f, 0.0f},
                        QUAD_GRID);
                }
                if (ctx_wr.mode == MODE_VIEWS) {
                    // draw views
                    uint32_t offset_col =
                        ctx_wr.left_col +
                        ((ctx_wr.viewport_cols +
                          ctx_wr.num_cols_for_line_numbers - 1) /
                             2 -
                         views.warpoon_viewport_cols / 2);
                    uint32_t offset_row =
                        ctx_wr.bottom_row +
                        ((ctx_wr.viewport_rows +
                          ctx_wr.num_rows_for_status_bars - 1) /
                             2 -
                         views.warpoon_viewport_rows / 2);
                    // draw views background
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){offset_col,
                                   offset_row,
                                   ctx_wr.layers[LAYER_POPUP_BACKGROUND]},
                        (float[2]){views.warpoon_viewport_cols,
                                   views.warpoon_viewport_rows},
                        views.warpoon_color_bg,
                        ctx_wr.outline_thickness,
                        views.warpoon_color_outline,
                        (float[2]){0.0f, 0.0f},
                        QUAD_OUTLINE);
                    // draw views gutter
                    war_make_quad(quad_vertices,
                                  quad_indices,
                                  &quad_vertices_count,
                                  &quad_indices_count,
                                  (float[3]){offset_col,
                                             offset_row,
                                             ctx_wr.layers[LAYER_POPUP_HUD]},
                                  (float[2]){views.warpoon_hud_cols,
                                             views.warpoon_viewport_rows},
                                  views.warpoon_color_hud,
                                  ctx_wr.outline_thickness,
                                  views.warpoon_color_outline,
                                  (float[2]){0.0f, 0.0f},
                                  QUAD_OUTLINE);
                    // draw views cursor
                    if (!ctx_wr.cursor_blinking) {
                        uint32_t cursor_span_x = 1;
                        uint32_t cursor_pos_x = views.warpoon_col;
                        if (views.warpoon_mode == MODE_VISUAL_LINE) {
                            cursor_span_x = views.warpoon_viewport_cols -
                                            views.warpoon_hud_cols;
                            cursor_pos_x = 0;
                        }
                        float alpha_factor = ctx_wr.alpha_scale;
                        uint8_t color_alpha = (cursor_color >> 24) & 0xFF;
                        cursor_color =
                            ((uint8_t)(color_alpha * alpha_factor) << 24) |
                            (cursor_color & 0x00FFFFFF);
                        war_make_quad(
                            quad_vertices,
                            quad_indices,
                            &quad_vertices_count,
                            &quad_indices_count,
                            (float[3]){offset_col + views.warpoon_hud_cols +
                                           cursor_pos_x -
                                           views.warpoon_left_col,
                                       offset_row + views.warpoon_hud_rows +
                                           views.warpoon_row -
                                           views.warpoon_bottom_row,
                                       ctx_wr.layers[LAYER_POPUP_CURSOR]},
                            (float[2]){cursor_span_x, 1},
                            cursor_color_transparent,
                            0,
                            0,
                            (float[2]){0.0f, 0.0f},
                            0);
                    }
                    // draw views line numbers text
                    int number = views.warpoon_max_row - views.warpoon_top_row +
                                 views.warpoon_viewport_rows;
                    for (uint32_t row = views.warpoon_bottom_row;
                         row <= views.warpoon_top_row;
                         row++, number--) {
                        uint32_t digits[2];
                        digits[0] = (number / 10) % 10;
                        digits[1] = number % 10;
                        int digit_count = 2;
                        if (digits[0] == 0) { digit_count--; }
                        for (int col = 2; col > 2 - digit_count; col--) {
                            war_make_text_quad(
                                text_vertices,
                                text_indices,
                                &text_vertices_count,
                                &text_indices_count,
                                (float[3]){offset_col + col - 1,
                                           offset_row + row -
                                               views.warpoon_bottom_row +
                                               views.warpoon_hud_rows,
                                           ctx_wr.layers[LAYER_POPUP_HUD_TEXT]},
                                (float[2]){1, 1},
                                views.warpoon_color_hud_text,
                                &ctx_vk.glyphs['0' + digits[col - 1]],
                                ctx_wr.text_thickness,
                                ctx_wr.text_feather,
                                0);
                        }
                    }
                    // draw views text
                    war_get_warpoon_text(&views);
                    uint32_t row = views.warpoon_max_row;
                    for (uint32_t i_views = 0; i_views < views.views_count;
                         i_views++, row--) {
                        if (row > views.warpoon_top_row ||
                            row < views.warpoon_bottom_row) {
                            continue;
                        }
                        for (uint32_t col = 0;
                             col <= views.warpoon_right_col &&
                             views.warpoon_text[i_views][col] != '\0';
                             col++) {
                            war_make_text_quad(
                                text_vertices,
                                text_indices,
                                &text_vertices_count,
                                &text_indices_count,
                                (float[3]){offset_col + views.warpoon_hud_cols +
                                               col,
                                           offset_row + views.warpoon_hud_rows +
                                               row - views.warpoon_bottom_row,
                                           ctx_wr.layers[LAYER_POPUP_CURSOR]},
                                (float[2]){1, 1},
                                views.warpoon_color_text,
                                &ctx_vk
                                     .glyphs[views.warpoon_text[i_views][col]],
                                ctx_wr.text_thickness,
                                ctx_wr.text_feather,
                                0);
                        }
                    }
                }
                // draw note quads
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    uint32_t i_in_view = note_quads_in_x[i];
                    float pos_x = note_quads.col[i_in_view] +
                                  (float)note_quads.sub_col[i_in_view] /
                                      note_quads.sub_cells_col[i_in_view];
                    float span_x =
                        (float)note_quads.cursor_width_whole_number[i_in_view] *
                        note_quads.cursor_width_sub_col[i_in_view] /
                        note_quads.cursor_width_sub_cells[i_in_view];
                    uint32_t color = note_quads.color[i_in_view];
                    uint32_t outline_color =
                        note_quads.outline_color[i_in_view];
                    bool hidden = note_quads.hidden[i_in_view];
                    bool mute = note_quads.mute[i_in_view];
                    if (!hidden && !mute) {
                        war_make_quad(quad_vertices,
                                      quad_indices,
                                      &quad_vertices_count,
                                      &quad_indices_count,
                                      (float[3]){pos_x,
                                                 note_quads.row[i_in_view],
                                                 ctx_wr.layers[LAYER_NOTES]},
                                      (float[2]){span_x, 1},
                                      color,
                                      default_outline_thickness,
                                      outline_color,
                                      (float[2]){0.0f, 0.0f},
                                      QUAD_GRID);
                    } else if (!hidden && mute) {
                        float alpha_factor = ctx_wr.alpha_scale;
                        uint8_t color_alpha = (color >> 24) & 0xFF;
                        uint8_t outline_color_alpha =
                            (outline_color >> 24) & 0xFF;
                        color = ((uint8_t)(color_alpha * alpha_factor) << 24) |
                                (color & 0x00FFFFFF);
                        outline_color =
                            ((uint8_t)(outline_color_alpha * alpha_factor)
                             << 24) |
                            (outline_color & 0x00FFFFFF);
                        war_make_quad(quad_vertices,
                                      quad_indices,
                                      &quad_vertices_count,
                                      &quad_indices_count,
                                      (float[3]){pos_x,
                                                 note_quads.row[i_in_view],
                                                 ctx_wr.layers[LAYER_NOTES]},
                                      (float[2]){span_x, 1},
                                      color,
                                      default_outline_thickness,
                                      outline_color,
                                      (float[2]){0.0f, 0.0f},
                                      QUAD_GRID);
                    }
                }
                // draw playback bar
                uint32_t playback_bar_color = ctx_wr.red_hex;
                float span_y = ctx_wr.viewport_rows;
                if (ctx_wr.top_row == MAX_MIDI_NOTES - 1) {
                    span_y -= ctx_wr.num_rows_for_status_bars;
                }
                war_make_quad(
                    quad_vertices,
                    quad_indices,
                    &quad_vertices_count,
                    &quad_indices_count,
                    (float[3]){((float)atomic_load(&atomics->play_frames) /
                                ctx_a.sample_rate) /
                                   ((60.0f / ctx_a.BPM) / 4.0f),
                               ctx_wr.bottom_row,
                               ctx_wr.layers[LAYER_PLAYBACK_BAR]},
                    (float[2]){0, span_y},
                    playback_bar_color,
                    0,
                    0,
                    (float[2]){default_playback_bar_thickness, 0.0f},
                    QUAD_LINE | QUAD_GRID);
                // draw status bar quads
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){ctx_wr.left_col,
                                         ctx_wr.bottom_row,
                                         ctx_wr.layers[LAYER_HUD]},
                              (float[2]){ctx_wr.viewport_cols + 1, 1},
                              ctx_wr.red_hex,
                              0,
                              0,
                              (float[2]){0.0f, 0.0f},
                              0);
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){ctx_wr.left_col,
                                         ctx_wr.bottom_row + 1,
                                         ctx_wr.layers[LAYER_HUD]},
                              (float[2]){ctx_wr.viewport_cols + 1, 1},
                              ctx_wr.dark_gray_hex,
                              0,
                              0,
                              (float[2]){0.0f, 0.0f},
                              0);
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){ctx_wr.left_col,
                                         ctx_wr.bottom_row + 2,
                                         ctx_wr.layers[LAYER_HUD]},
                              (float[2]){ctx_wr.viewport_cols + 1, 1},
                              ctx_wr.darker_light_gray_hex,
                              0,
                              0,
                              (float[2]){0.0f, 0.0f},
                              0);
                // draw piano quads
                float gutter_end_span_inset =
                    5 * default_vertical_line_thickness;
                switch (ctx_wr.hud_state) {
                case HUD_PIANO_AND_LINE_NUMBERS:
                case HUD_PIANO:
                    float span_y = ctx_wr.viewport_rows;
                    if (ctx_wr.top_row == MAX_MIDI_NOTES - 1) {
                        span_y -= ctx_wr.num_rows_for_status_bars;
                    }
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){ctx_wr.left_col,
                                   ctx_wr.bottom_row +
                                       ctx_wr.num_rows_for_status_bars,
                                   ctx_wr.layers[LAYER_HUD]},
                        (float[2]){3 - gutter_end_span_inset, span_y},
                        ctx_wr.full_white_hex,
                        0,
                        0,
                        (float[2]){0.0f, 0.0f},
                        0);
                    for (uint32_t row = ctx_wr.bottom_row;
                         row <= ctx_wr.top_row;
                         row++) {
                        uint32_t note = row % 12;
                        if (note == 1 || note == 3 || note == 6 || note == 8 ||
                            note == 10) {
                            war_make_quad(
                                quad_vertices,
                                quad_indices,
                                &quad_vertices_count,
                                &quad_indices_count,
                                (float[3]){ctx_wr.left_col,
                                           row +
                                               ctx_wr.num_rows_for_status_bars,
                                           ctx_wr.layers[LAYER_HUD]},
                                (float[2]){2 - gutter_end_span_inset, 1},
                                ctx_wr.black_hex,
                                0,
                                0,
                                (float[2]){0.0f, 0.0f},
                                0);
                        }
                    }
                    break;
                }
                // draw line number quad
                int ln_offset = (ctx_wr.hud_state == HUD_LINE_NUMBERS) ? 0 : 3;
                if (ctx_wr.hud_state != HUD_PIANO) {
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){ctx_wr.left_col + ln_offset -
                                       default_vertical_line_thickness,
                                   ctx_wr.bottom_row +
                                       ctx_wr.num_rows_for_status_bars,
                                   ctx_wr.layers[LAYER_HUD]},
                        (float[2]){3 - gutter_end_span_inset,
                                   ctx_wr.viewport_rows},
                        ctx_wr.red_hex,
                        0,
                        0,
                        (float[2]){0.0f, 0.0f},
                        0);
                }
                // draw gridline quads
                for (uint32_t row = ctx_wr.bottom_row + 1;
                     row <= ctx_wr.top_row + 1;
                     row++) {
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){ctx_wr.left_col,
                                   row,
                                   ctx_wr.layers[LAYER_GRIDLINES]},
                        (float[2]){ctx_wr.viewport_cols, 0},
                        ctx_wr.darker_light_gray_hex,
                        0,
                        0,
                        (float[2]){0.0f, default_horizontal_line_thickness},
                        QUAD_LINE | QUAD_GRID);
                }
                qsort(ctx_wr.gridline_splits,
                      MAX_GRIDLINE_SPLITS,
                      sizeof(uint32_t),
                      war_compare_desc_uint32);
                for (uint32_t col = ctx_wr.left_col + 1;
                     col <= ctx_wr.right_col + 1;
                     col++) {
                    bool draw_vertical_line = false;
                    uint32_t color;
                    for (uint32_t i = 0; i < MAX_GRIDLINE_SPLITS; i++) {
                        draw_vertical_line =
                            (col % ctx_wr.gridline_splits[i]) == 0;
                        if (draw_vertical_line) {
                            switch (i) {
                            case 0:
                                color = ctx_wr.white_hex;
                                break;
                            case 1:
                                color = ctx_wr.darker_light_gray_hex;
                                break;
                            case 2:
                                color = ctx_wr.red_hex;
                                break;
                            case 3:
                                color = ctx_wr.black_hex;
                                break;
                            }
                            break;
                        }
                    }
                    if (!draw_vertical_line) { continue; }
                    uint32_t span_y = ctx_wr.viewport_rows;
                    if (ctx_wr.top_row == MAX_MIDI_NOTES - 1) {
                        span_y -= ctx_wr.num_rows_for_status_bars;
                    }
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){col,
                                   ctx_wr.bottom_row,
                                   ctx_wr.layers[LAYER_GRIDLINES]},
                        (float[2]){0, span_y},
                        color,
                        0,
                        0,
                        (float[2]){default_vertical_line_thickness, 0},
                        QUAD_LINE | QUAD_GRID);
                }
                memcpy(ctx_vk.quads_vertex_buffer_mapped,
                       quad_vertices,
                       sizeof(war_quad_vertex) * quad_vertices_count);
                memcpy(ctx_vk.quads_index_buffer_mapped,
                       quad_indices,
                       sizeof(uint16_t) * quad_indices_count);
                VkMappedMemoryRange quad_flush_ranges[2] = {
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.quads_vertex_buffer_memory,
                     .offset = 0,
                     .size = war_align64(sizeof(war_quad_vertex) *
                                         quad_vertices_count)},
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.quads_index_buffer_memory,
                     .offset = 0,
                     .size =
                         war_align64(sizeof(uint16_t) * quad_indices_count)}};
                vkFlushMappedMemoryRanges(ctx_vk.device, 2, quad_flush_ranges);
                VkDeviceSize quad_vertices_offsets[2] = {0};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       0,
                                       1,
                                       &ctx_vk.quads_vertex_buffer,
                                       quad_vertices_offsets);
                VkDeviceSize quad_instances_offsets[2] = {0};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       1,
                                       1,
                                       &ctx_vk.quads_instance_buffer,
                                       quad_instances_offsets);
                VkDeviceSize quad_indices_offset = 0;
                vkCmdBindIndexBuffer(ctx_vk.cmd_buffer,
                                     ctx_vk.quads_index_buffer,
                                     quad_indices_offset,
                                     VK_INDEX_TYPE_UINT16);
                war_quad_push_constants quad_push_constants = {
                    .bottom_left = {ctx_wr.left_col, ctx_wr.bottom_row},
                    .physical_size = {physical_width, physical_height},
                    .cell_size = {ctx_wr.cell_width, ctx_wr.cell_height},
                    .zoom = ctx_wr.zoom_scale,
                    .cell_offsets = {ctx_wr.num_cols_for_line_numbers,
                                     ctx_wr.num_rows_for_status_bars},
                    .scroll_margin = {ctx_wr.scroll_margin_cols,
                                      ctx_wr.scroll_margin_rows},
                    .anchor_cell = {ctx_wr.col, ctx_wr.row},
                    .top_right = {ctx_wr.right_col, ctx_wr.top_row},
                };
                vkCmdPushConstants(ctx_vk.cmd_buffer,
                                   ctx_vk.pipeline_layout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(war_quad_push_constants),
                                   &quad_push_constants);
                vkCmdDrawIndexed(
                    ctx_vk.cmd_buffer, quad_indices_count, 1, 0, 0, 0);
                // draw transparent quads
                vkCmdBindPipeline(ctx_vk.cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  ctx_vk.transparent_quad_pipeline);
                memcpy(ctx_vk.quads_vertex_buffer_mapped +
                           quad_vertices_count * sizeof(war_quad_vertex),
                       transparent_quad_vertices,
                       sizeof(war_quad_vertex) *
                           transparent_quad_vertices_count);
                memcpy(ctx_vk.quads_index_buffer_mapped +
                           quad_indices_count * sizeof(uint16_t),
                       transparent_quad_indices,
                       sizeof(uint16_t) * transparent_quad_indices_count);
                VkMappedMemoryRange transparent_quad_flush_ranges[2] = {
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.quads_vertex_buffer_memory,
                     .offset = war_align64(sizeof(war_quad_vertex) *
                                           quad_vertices_count),
                     .size = war_align64(sizeof(war_quad_vertex) *
                                         transparent_quad_vertices_count)},
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.quads_index_buffer_memory,
                     .offset =
                         war_align64(sizeof(uint16_t) * quad_indices_count),
                     .size = war_align64(sizeof(uint16_t) *
                                         transparent_quad_indices_count)}};
                vkFlushMappedMemoryRanges(
                    ctx_vk.device, 2, transparent_quad_flush_ranges);
                VkDeviceSize transparent_quad_vertices_offsets[2] = {
                    quad_vertices_count * sizeof(war_quad_vertex)};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       0,
                                       1,
                                       &ctx_vk.quads_vertex_buffer,
                                       transparent_quad_vertices_offsets);
                VkDeviceSize transparent_quad_instances_offsets[2] = {0};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       1,
                                       1,
                                       &ctx_vk.quads_instance_buffer,
                                       transparent_quad_instances_offsets);
                VkDeviceSize transparent_quad_indices_offset =
                    quad_indices_count * sizeof(uint16_t);
                vkCmdBindIndexBuffer(ctx_vk.cmd_buffer,
                                     ctx_vk.quads_index_buffer,
                                     transparent_quad_indices_offset,
                                     VK_INDEX_TYPE_UINT16);
                war_quad_push_constants transparent_quad_push_constants = {
                    .bottom_left = {ctx_wr.left_col, ctx_wr.bottom_row},
                    .physical_size = {physical_width, physical_height},
                    .cell_size = {ctx_wr.cell_width, ctx_wr.cell_height},
                    .zoom = ctx_wr.zoom_scale,
                    .cell_offsets = {ctx_wr.num_cols_for_line_numbers,
                                     ctx_wr.num_rows_for_status_bars},
                    .scroll_margin = {ctx_wr.scroll_margin_cols,
                                      ctx_wr.scroll_margin_rows},
                    .anchor_cell = {ctx_wr.col, ctx_wr.row},
                    .top_right = {ctx_wr.right_col, ctx_wr.top_row},
                };
                vkCmdPushConstants(ctx_vk.cmd_buffer,
                                   ctx_vk.pipeline_layout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(war_quad_push_constants),
                                   &transparent_quad_push_constants);
                vkCmdDrawIndexed(ctx_vk.cmd_buffer,
                                 transparent_quad_indices_count,
                                 1,
                                 0,
                                 0,
                                 0);
                //---------------------------------------------------------
                // TEXT PIPELINE
                //---------------------------------------------------------
                vkCmdBindPipeline(ctx_vk.cmd_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  ctx_vk.text_pipeline);
                vkCmdBindDescriptorSets(ctx_vk.cmd_buffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        ctx_vk.text_pipeline_layout,
                                        0,
                                        1,
                                        &ctx_vk.font_descriptor_set,
                                        0,
                                        NULL);
                // draw status bar text
                ctx_wr.text_status_bar_start_index = 0;
                ctx_wr.text_status_bar_middle_index = ctx_wr.viewport_cols / 2;
                ctx_wr.text_status_bar_end_index =
                    (ctx_wr.viewport_cols * 3) / 4;
                war_get_top_text(&ctx_wr);
                war_get_middle_text(&ctx_wr, &views, atomics);
                war_get_bottom_text(&ctx_wr);
                for (float col = 0; col < ctx_wr.viewport_cols; col++) {
                    if (ctx_wr.text_top_status_bar[(int)col] != 0) {
                        war_make_text_quad(
                            text_vertices,
                            text_indices,
                            &text_vertices_count,
                            &text_indices_count,
                            (float[3]){col + ctx_wr.left_col,
                                       2 + ctx_wr.bottom_row,
                                       ctx_wr.layers[LAYER_HUD_TEXT]},
                            (float[2]){1, 1},
                            ctx_wr.white_hex,
                            &ctx_vk
                                 .glyphs[ctx_wr.text_top_status_bar[(int)col]],
                            ctx_wr.text_thickness,
                            ctx_wr.text_feather,
                            0);
                    }
                    if (ctx_wr.text_middle_status_bar[(int)col] != 0) {
                        war_make_text_quad(
                            text_vertices,
                            text_indices,
                            &text_vertices_count,
                            &text_indices_count,
                            (float[3]){col + ctx_wr.left_col,
                                       1 + ctx_wr.bottom_row,
                                       ctx_wr.layers[LAYER_HUD_TEXT]},
                            (float[2]){1, 1},
                            ctx_wr.red_hex,
                            &ctx_vk.glyphs[ctx_wr.text_middle_status_bar[(
                                int)col]],
                            ctx_wr.text_thickness_bold,
                            ctx_wr.text_feather_bold,
                            0);
                    }
                    if (ctx_wr.text_bottom_status_bar[(int)col] != 0) {
                        war_make_text_quad(
                            text_vertices,
                            text_indices,
                            &text_vertices_count,
                            &text_indices_count,
                            (float[3]){col + ctx_wr.left_col,
                                       ctx_wr.bottom_row,
                                       ctx_wr.layers[LAYER_HUD_TEXT]},
                            (float[2]){1, 1},
                            ctx_wr.full_white_hex,
                            &ctx_vk.glyphs[ctx_wr.text_bottom_status_bar[(
                                int)col]],
                            ctx_wr.text_thickness,
                            ctx_wr.text_feather,
                            0);
                    }
                }
                // draw piano text
                char* piano_notes[12] = {"C",
                                         "C#",
                                         "D",
                                         "D#",
                                         "E",
                                         "F",
                                         "F#",
                                         "G",
                                         "G#",
                                         "A",
                                         "A#",
                                         "B"};
                for (uint32_t row = ctx_wr.bottom_row;
                     row <= ctx_wr.top_row &&
                     ctx_wr.hud_state != HUD_LINE_NUMBERS;
                     row++) {
                    uint32_t i_piano_notes = row % 12;
                    if (i_piano_notes == 1 || i_piano_notes == 3 ||
                        i_piano_notes == 6 || i_piano_notes == 8 ||
                        i_piano_notes == 10) {
                        continue;
                    }
                    int octave = row / 12 - 1;
                    if (octave < 0) { octave = '-' - '0'; }
                    war_make_text_quad(
                        text_vertices,
                        text_indices,
                        &text_vertices_count,
                        &text_indices_count,
                        (float[3]){1 + ctx_wr.left_col,
                                   row + ctx_wr.num_rows_for_status_bars,
                                   ctx_wr.layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr.black_hex,
                        &ctx_vk.glyphs[piano_notes[i_piano_notes][0]],
                        ctx_wr.text_thickness,
                        ctx_wr.text_feather,
                        0);
                    war_make_text_quad(
                        text_vertices,
                        text_indices,
                        &text_vertices_count,
                        &text_indices_count,
                        (float[3]){2 + ctx_wr.left_col,
                                   row + ctx_wr.num_rows_for_status_bars,
                                   ctx_wr.layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr.black_hex,
                        &ctx_vk.glyphs['0' + octave],
                        ctx_wr.text_thickness,
                        ctx_wr.text_feather,
                        0);
                }
                // draw line number text
                for (uint32_t row = ctx_wr.bottom_row;
                     row <= ctx_wr.top_row && ctx_wr.hud_state != HUD_PIANO;
                     row++) {
                    uint32_t digits[3];
                    digits[0] = (row / 100) % 10;
                    digits[1] = (row / 10) % 10;
                    digits[2] = row % 10;
                    int digit_count = 3;
                    if (digits[0] == 0) {
                        digit_count = (digits[1] == 0) ? (digit_count - 2) :
                                                         (digit_count - 1);
                    }
                    for (int col = ln_offset + 2;
                         col > (ln_offset + 2) - digit_count;
                         col--) {
                        war_make_text_quad(
                            text_vertices,
                            text_indices,
                            &text_vertices_count,
                            &text_indices_count,
                            (float[3]){ctx_wr.left_col + col,
                                       row + ctx_wr.num_rows_for_status_bars,
                                       ctx_wr.layers[LAYER_HUD_TEXT]},
                            (float[2]){1, 1},
                            ctx_wr.full_white_hex,
                            &ctx_vk.glyphs['0' + digits[col - ln_offset]],
                            ctx_wr.text_thickness,
                            ctx_wr.text_feather,
                            0);
                    }
                }
                memcpy(ctx_vk.text_vertex_buffer_mapped,
                       text_vertices,
                       sizeof(war_text_vertex) * text_vertices_count);
                memcpy(ctx_vk.text_index_buffer_mapped,
                       text_indices,
                       sizeof(uint16_t) * text_indices_count);
                VkMappedMemoryRange text_flush_ranges[2] = {
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.text_vertex_buffer_memory,
                     .offset = 0,
                     .size = war_align64(sizeof(war_text_vertex) *
                                         text_vertices_count)},
                    {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                     .memory = ctx_vk.text_index_buffer_memory,
                     .offset = 0,
                     .size =
                         war_align64(sizeof(uint16_t) * text_indices_count)}};
                vkFlushMappedMemoryRanges(ctx_vk.device, 2, text_flush_ranges);
                VkDeviceSize text_vertices_offsets[2] = {0};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       0,
                                       1,
                                       &ctx_vk.text_vertex_buffer,
                                       text_vertices_offsets);
                VkDeviceSize text_instances_offsets[2] = {0};
                vkCmdBindVertexBuffers(ctx_vk.cmd_buffer,
                                       1,
                                       1,
                                       &ctx_vk.text_instance_buffer,
                                       text_instances_offsets);
                VkDeviceSize text_indices_offset = 0;
                vkCmdBindIndexBuffer(ctx_vk.cmd_buffer,
                                     ctx_vk.text_index_buffer,
                                     text_indices_offset,
                                     VK_INDEX_TYPE_UINT16);
                war_text_push_constants text_push_constants = {
                    .bottom_left = {ctx_wr.left_col, ctx_wr.bottom_row},
                    .physical_size = {physical_width, physical_height},
                    .cell_size = {ctx_wr.cell_width, ctx_wr.cell_height},
                    .zoom = ctx_wr.zoom_scale,
                    .cell_offsets = {ctx_wr.num_cols_for_line_numbers,
                                     ctx_wr.num_rows_for_status_bars},
                    .scroll_margin = {ctx_wr.scroll_margin_cols,
                                      ctx_wr.scroll_margin_rows},
                    .anchor_cell = {ctx_wr.col, ctx_wr.row},
                    .top_right = {ctx_wr.right_col, ctx_wr.top_row},
                    .ascent = ctx_vk.ascent,
                    .descent = ctx_vk.descent,
                    .line_gap = ctx_vk.line_gap,
                    .baseline = ctx_vk.baseline,
                    .font_height = ctx_vk.font_height,
                };
                vkCmdPushConstants(ctx_vk.cmd_buffer,
                                   ctx_vk.text_pipeline_layout,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(war_text_push_constants),
                                   &text_push_constants);
                vkCmdDrawIndexed(
                    ctx_vk.cmd_buffer, text_indices_count, 1, 0, 0, 0);
                //---------------------------------------------------------
                //   END RENDER PASS
                //---------------------------------------------------------
                vkCmdEndRenderPass(ctx_vk.cmd_buffer);
                result = vkEndCommandBuffer(ctx_vk.cmd_buffer);
                assert(result == VK_SUCCESS);
                VkSubmitInfo submit_info = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .commandBufferCount = 1,
                    .pCommandBuffers = &ctx_vk.cmd_buffer,
                    .waitSemaphoreCount = 0,
                    .pWaitSemaphores = NULL,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores = NULL,
                };
                result = vkQueueSubmit(
                    ctx_vk.queue,
                    1,
                    &submit_info,
                    ctx_vk.in_flight_fences[ctx_vk.current_frame]);
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

                uint32_t cursor_w = ctx_vk.cell_width;
                uint32_t cursor_h = ctx_vk.cell_height;
                ctx_wr.cursor_x = ctx_wr.col * ctx_vk.cell_width;
                ctx_wr.cursor_y =
                    (physical_height - 1) - (ctx_wr.row * ctx_vk.cell_height);

                for (uint32_t y = ctx_wr.cursor_y;
                     y < ctx_wr.cursor_y + cursor_h;
                     ++y) {
                    if (y >= physical_height) break;
                    for (uint32_t x = ctx_wr.cursor_x;
                         x < ctx_wr.cursor_x + cursor_w;
                         ++x) {
                        if (x >= physical_width) break;
                        pixels[y * physical_width + x] = 0xFFFFFFFF; // white
                    }
                }
#endif
                // war_wayland_holy_trinity(fd,
                //                          wl_surface_id,
                //                          wl_buffer_id,
                //                          0,
                //                          0,
                //                          0,
                //                          0,
                //                          physical_width,
                //                          physical_height);
                goto wayland_done;
            wl_display_error:
                dump_bytes("wl_display::error event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_display_delete_id:
                // dump_bytes("wl_display::delete_id event",
                //            msg_buffer + msg_buffer_offset,
                //            size);

                if (war_read_le32(msg_buffer + msg_buffer_offset + 8) ==
                    wl_callback_id) {
                    war_wayland_wl_surface_frame(
                        fd, wl_surface_id, wl_callback_id);
                }
                goto wayland_done;
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
                goto wayland_done;
#endif
            wl_buffer_release:
                // dump_bytes("wl_buffer_release event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                goto wayland_done;
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
                goto wayland_done;
            xdg_surface_configure:
                // dump_bytes("xdg_surface_configure event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                assert(size == 12);

                uint8_t ack_configure[12];
                war_write_le32(ack_configure, xdg_surface_id);
                war_write_le16(ack_configure + 4, 4);
                war_write_le16(ack_configure + 6, 12);
                war_write_le32(
                    ack_configure + 8,
                    war_read_le32(msg_buffer + msg_buffer_offset + 8));
                // dump_bytes(
                //     "xdg_surface_ack_configure request", ack_configure, 12);
                ssize_t ack_configure_written = write(fd, ack_configure, 12);
                assert(ack_configure_written == 12);

                if (!wp_viewport_id) {
                    uint8_t get_viewport[16];
                    war_write_le32(get_viewport, wp_viewporter_id);
                    war_write_le16(get_viewport + 4, 1);
                    war_write_le16(get_viewport + 6, 16);
                    war_write_le32(get_viewport + 8, new_id);
                    war_write_le32(get_viewport + 12, wl_surface_id);
                    // dump_bytes("wp_viewporter::get_viewport request",
                    //            get_viewport,
                    //            16);
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
                    //    "wp_viewport::set_source request", set_source, 24);
                    // ssize_t set_source_written = write(fd, set_source, 24);
                    // assert(set_source_written == 24);

                    uint8_t set_destination[16];
                    war_write_le32(set_destination, wp_viewport_id);
                    war_write_le16(set_destination + 4, 2);
                    war_write_le16(set_destination + 6, 16);
                    war_write_le32(set_destination + 8, logical_width);
                    war_write_le32(set_destination + 12, logical_height);
                    // dump_bytes("wp_viewport::set_destination request",
                    //            set_destination,
                    //            16);
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
                goto wayland_done;
            xdg_toplevel_configure:
                // dump_bytes("xdg_toplevel_configure event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                uint32_t width =
                    *(uint32_t*)(msg_buffer + msg_buffer_offset + 0);
                uint32_t height =
                    *(uint32_t*)(msg_buffer + msg_buffer_offset + 4);
                // States array starts at offset 8
                uint8_t* states_ptr = msg_buffer + msg_buffer_offset + 8;
                size_t states_bytes =
                    size -
                    12; // subtract object_id/opcode/length + width/height
                size_t num_states = states_bytes / 4;
                ctx_wr.fullscreen = false;
                for (size_t i = 0; i < num_states; i++) {
                    uint32_t state = *(uint32_t*)(states_ptr + i * 4);
                    if (state == 2) { // XDG_TOPLEVEL_STATE_FULLSCREEN
                        ctx_wr.fullscreen = true;
                        // call_carmack("true fullscreen");
                        break;
                    }
                }
                if (ctx_wr.fullscreen) {
                    war_wl_surface_set_opaque_region(fd, wl_surface_id, 0);
                    ctx_wr.text_feather = default_text_feather;
                    ctx_wr.text_thickness = default_text_thickness;
                } else if (!ctx_wr.fullscreen) {
                    war_wl_surface_set_opaque_region(
                        fd, wl_surface_id, wl_region_id);
                    ctx_wr.text_feather = windowed_text_feather;
                    ctx_wr.text_thickness = windowed_text_thickness;
                }
                goto wayland_done;
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

                uint8_t wl_region_destroy[8];
                war_write_le32(wl_region_destroy, wl_region_id);
                war_write_le16(wl_region_destroy + 4, 0);
                war_write_le16(wl_region_destroy + 6, 8);
                ssize_t wl_region_destroy_written =
                    write(fd, wl_region_destroy, 8);
                dump_bytes("wl_region::destroy request", wl_region_destroy, 8);
                assert(wl_region_destroy_written == 8);

                uint8_t wl_surface_destroy[8];
                war_write_le32(wl_surface_destroy, wl_surface_id);
                war_write_le16(wl_surface_destroy + 4, 0);
                war_write_le16(wl_surface_destroy + 6, 8);
                ssize_t wl_surface_destroy_written =
                    write(fd, wl_surface_destroy, 8);
                dump_bytes(
                    "wl_surface::destroy request", wl_surface_destroy, 8);
                assert(wl_surface_destroy_written == 8);

                atomic_store(&atomics->state, AUDIO_CMD_END_WAR);
                usleep(500000);
                goto wayland_done;
            xdg_toplevel_configure_bounds:
                dump_bytes("xdg_toplevel_configure_bounds event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            xdg_toplevel_wm_capabilities:
                dump_bytes("xdg_toplevel_wm_capabilities event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
#if DMABUF
            zwp_linux_dmabuf_v1_format:
                dump_bytes("zwp_linux_dmabuf_v1_format event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_dmabuf_v1_modifier:
                dump_bytes("zwp_linux_dmabuf_v1_modifier event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_buffer_params_v1_created:
                dump_bytes(
                    "zwp_linux_buffer_params_v1_created", // COMMENT
                                                          // REFACTOR: to ::
                    msg_buffer + msg_buffer_offset,
                    size);
                goto wayland_done;
            zwp_linux_buffer_params_v1_failed:
                dump_bytes("zwp_linux_buffer_params_v1_failed event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
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
                *((int*)CMSG_DATA(cmsg)) = ctx_vk.dmabuf_fd;
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
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_format_table:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_format_table event",
                           msg_buffer + msg_buffer_offset,
                           size); // REFACTOR: event
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_main_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_main_device event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_tranche_done:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_tranche_target_device:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_target_"
                           "device event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_tranche_formats:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_formats event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_linux_dmabuf_feedback_v1_tranche_flags:
                dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_flags event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
#endif
            wp_linux_drm_syncobj_manager_v1_jump:
                dump_bytes("wp_linux_drm_syncobj_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_compositor_jump:
                dump_bytes("wl_compositor_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_surface_enter:
                dump_bytes("wl_surface_enter event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_surface_leave:
                dump_bytes("wl_surface_leave event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
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

                // war_wayland_holy_trinity(fd,
                //                          wl_surface_id,
                //                          wl_buffer_id,
                //                          0,
                //                          0,
                //                          0,
                //                          0,
                //                          physical_width,
                //                          physical_height);
                goto wayland_done;
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

                // war_wayland_holy_trinity(fd,
                //                          wl_surface_id,
                //                          wl_buffer_id,
                //                          0,
                //                          0,
                //                          0,
                //                          0,
                //                          physical_width,
                //                          physical_height);
                goto wayland_done;
            zwp_idle_inhibit_manager_v1_jump:
                dump_bytes("zwp_idle_inhibit_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwlr_layer_shell_v1_jump:
                dump_bytes("zwlr_layer_shell_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zxdg_decoration_manager_v1_jump:
                dump_bytes("zxdg_decoration_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zxdg_toplevel_decoration_v1_configure: {
                dump_bytes("zxdg_toplevel_decoration_v1_configure event",
                           msg_buffer + msg_buffer_offset,
                           size);
                uint8_t set_mode[12];
                war_write_le32(set_mode, zxdg_toplevel_decoration_v1_id);
                war_write_le16(set_mode + 4, 1);
                war_write_le16(set_mode + 6, 12);
                war_write_le32(set_mode + 8, 1);
                // war_write_le32(
                //     set_mode + 8,
                //     war_read_le32(msg_buffer + msg_buffer_offset + 8));
                dump_bytes("zxdg_toplevel_decoration_v1::set_mode request",
                           set_mode,
                           12);
                ssize_t set_mode_written = write(fd, set_mode, 12);
                assert(set_mode_written == 12);
                goto wayland_done;
            }
            zwp_relative_pointer_manager_v1_jump:
                dump_bytes("zwp_relative_pointer_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_pointer_constraints_v1_jump:
                dump_bytes("zwp_pointer_constraints_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wp_presentation_clock_id:
                dump_bytes("wp_presentation_clock_id event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwlr_output_manager_v1_head:
                dump_bytes("zwlr_output_manager_v1_head event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwlr_output_manager_v1_done:
                dump_bytes("zwlr_output_manager_v1_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            ext_foreign_toplevel_list_v1_toplevel:
                dump_bytes("ext_foreign_toplevel_list_v1_toplevel event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwlr_data_control_manager_v1_jump:
                dump_bytes("zwlr_data_control_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wp_viewporter_jump:
                dump_bytes("wp_viewporter_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wp_content_type_manager_v1_jump:
                dump_bytes("wp_content_type_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wp_fractional_scale_manager_v1_jump:
                dump_bytes("wp_fractional_scale_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            xdg_activation_v1_jump:
                dump_bytes("xdg_activation_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_virtual_keyboard_manager_v1_jump:
                dump_bytes("zwp_virtual_keyboard_manager_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            zwp_pointer_gestures_v1_jump:
                dump_bytes("zwp_pointer_gestures_v1_jump event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
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
                goto wayland_done;
            wl_seat_name:
                dump_bytes(
                    "wl_seat_name event", msg_buffer + msg_buffer_offset, size);
                call_carmack("seat: %s",
                             (const char*)msg_buffer + msg_buffer_offset + 12);
                goto wayland_done;
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
                            {.keysym = KEYSYM_MINUS, .mod = MOD_CTRL},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_equal,
                             .mod = MOD_CTRL | MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_MINUS, .mod = MOD_CTRL | MOD_ALT},
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
                            {.keysym = XKB_KEY_t, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_f, .mod = MOD_SHIFT},
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
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {.keysym = XKB_KEY_v, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
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
                            {.keysym = KEYSYM_SPACE, .mod = 0},
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
                            {.keysym = XKB_KEY_d, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_x, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_w, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_e, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_e, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_b, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_UP, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_DOWN, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_LEFT, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_RIGHT, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_UP, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_DOWN, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_LEFT, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_RIGHT, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_u, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_d, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_a, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_a, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_ESCAPE, .mod = MOD_ALT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_a, .mod = MOD_ALT | MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_a, .mod = MOD_CTRL},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_TAB, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_h, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_s, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_u, .mod = 0},
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {.keysym = XKB_KEY_w, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_g, .mod = 0},
                            {.keysym = XKB_KEY_a, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_TAB, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_v, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_k, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_j, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {.keysym = XKB_KEY_m, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_b, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_q, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_q, .mod = MOD_SHIFT},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_r, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_y, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_u, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_i, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_o, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_p, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_LEFTBRACKET, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_RIGHTBRACKET, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_MINUS, .mod = 0},
                            {0},
                        },
                        {
                            {0},
                        },
                        {
                            {.keysym = XKB_KEY_c, .mod = 0},
                            {0},
                        },
                        {
                            {.keysym = KEYSYM_SPACE, .mod = 0},
                            {0},
                        }};
                war_label key_labels[SEQUENCE_COUNT][MODE_COUNT] = {
                    // normal, views, visual_line, visual_block, insert,
                    // command, mode_m, mode_o, visual
                    // cmd, handle_release, handle_timeout, handle_repeat
                    {
                        {&&cmd_normal_k, 0, 1, 1},
                        {&&cmd_views_k, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_k, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_j, 0, 1, 1},
                        {&&cmd_views_j, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_j, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_h, 0, 1, 1},
                        {&&cmd_views_h, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_l, 0, 1, 1},
                        {&&cmd_views_l, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_l, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_k, 0, 1, 1},
                        {&&cmd_views_alt_k, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_j, 0, 1, 1},
                        {&&cmd_views_alt_j, 0, 1, 1},
                    },
                    {{&&cmd_normal_alt_h, 0, 1, 1},
                     {&&cmd_views_alt_h, 0, 1, 1}},
                    {{&&cmd_normal_alt_l, 0, 1, 1},
                     {&&cmd_views_alt_l, 0, 1, 1}},
                    {{&&cmd_normal_0, 0, 1, 1},
                     {NULL, 0, 1, 1},
                     {NULL, 0, 1, 1},

                     {&&cmd_record_0, 0, 1, 1}},
                    {
                        {&&cmd_normal_$, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_G, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_gg, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_1, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_1, 0, 1, 1},
                        {&&cmd_midi_1, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_2, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_2, 0, 1, 1},
                        {&&cmd_midi_2, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_3, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_3, 0, 1, 1},
                        {&&cmd_midi_3, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_4, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_4, 0, 1, 1},
                        {&&cmd_midi_4, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_5, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_5, 0, 1, 1},
                        {&&cmd_midi_5, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_6, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_6, 0, 1, 1},
                        {&&cmd_midi_6, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_7, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_7, 0, 1, 1},
                        {&&cmd_midi_7, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_8, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_8, 0, 1, 1},
                        {&&cmd_midi_8, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_9, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_9, 0, 1, 1},
                        {&&cmd_midi_9, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_equal, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_minus, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_alt_equal, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_alt_minus, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_0, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_esc, 0, 1, 1},
                        {&&cmd_views_esc, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_esc, 0, 1, 1},
                        {&&cmd_midi_esc, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_f, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_t, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_t, 0, 1, 1},
                        {&&cmd_midi_t, 1, 1, 1},
                    },
                    {
                        {&&cmd_normal_x, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_x, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_T, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_T, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_F, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_gb, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_gt, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_gm, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_s, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_z, 0, 1, 1},
                        {&&cmd_views_z, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_return, 0, 1, 1},
                        {&&cmd_views_return, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacediv, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacedov, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacediw, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceda, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacehov, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacehiv, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceha, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacesov, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacesiv, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacesa, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacemov, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacemiv, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacema, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceuov, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceuiv, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceua, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacea, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_g, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_t, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_n, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_s, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_m, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_y, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_z, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_q, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_e, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_a, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space1, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space2, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space3, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space4, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space5, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space6, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space7, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space8, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space9, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_space0, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_1, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_2, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_3, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_4, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_5, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_6, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_7, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_8, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_9, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_0, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacedspacea, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_K, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_J, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_H, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_L, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_d, 0, 1, 1},
                        {&&cmd_views_d, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_m, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_m, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_X, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_w, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_w, 0, 1, 1},
                        {&&cmd_midi_w, 1, 1, 1},
                    },
                    {
                        {&&cmd_normal_W, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_e, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_e, 0, 1, 1},
                        {&&cmd_midi_e, 1, 1, 1},
                    },
                    {
                        {&&cmd_normal_E, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_b, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_b, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_k, 0, 1, 1},
                        {&&cmd_views_k, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_j, 0, 1, 1},
                        {&&cmd_views_j, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_h, 0, 1, 1},
                        {&&cmd_views_h, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_l, 0, 1, 1},
                        {&&cmd_views_l, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_k, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_j, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_h, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_l, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_u, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_d, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_A, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_a, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_esc, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_alt_A, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ctrl_a, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_tab, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_tab, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacehiw, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacesiw, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spaceuiw, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_ga, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_shift_tab, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_V, 0, 1, 1},
                        {&&cmd_views_V, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_K, 0, 1, 1},
                        {&&cmd_views_K, 0, 1, 1},
                        {&&cmd_midi_K, 0, 1, 1},
                        {&&cmd_record_K, 0, 1, 1},
                        {&&cmd_midi_K, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_J, 0, 1, 1},
                        {&&cmd_views_J, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_J, 0, 1, 1},
                        {&&cmd_midi_J, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_spacem, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_B, 0, 1, 1},
                    },
                    {
                        {&&cmd_normal_q, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_q, 0, 1, 1},
                        {&&cmd_midi_q, 1, 1, 1},
                    },
                    {
                        {&&cmd_normal_Q, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_Q, 0, 1, 1},
                        {&&cmd_midi_Q, 0, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_r, 0, 1, 1},
                        {&&cmd_midi_r, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_y, 0, 1, 1},
                        {&&cmd_midi_y, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_u, 0, 1, 1},
                        {&&cmd_midi_u, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_i, 0, 1, 1},
                        {&&cmd_midi_i, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_o, 0, 1, 1},
                        {&&cmd_midi_o, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_p, 0, 1, 1},
                        {&&cmd_midi_p, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_leftbracket, 0, 1, 1},
                        {&&cmd_midi_leftbracket, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_rightbracket, 0, 1, 1},
                        {&&cmd_midi_rightbracket, 1, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_minus, 0, 1, 1},
                        {&&cmd_midi_minus, 0, 1, 1},
                    },
                    {
                        {&&cmd_void, 0, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_midi_c, 0, 1, 1},
                    },
                    {
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {NULL, 0, 1, 1},
                        {&&cmd_record_space, 0, 0, 1},
                        {&&cmd_midi_space, 0, 0, 1},
                    },
                };
                //// default to void command if unset
                // for (size_t s = 0; s < SEQUENCE_COUNT; s++) {
                //     for (size_t m = 0; m < MODE_COUNT; m++) {
                //         if (key_labels[s][m].command == NULL) {
                //             key_labels[s][m].command = &&cmd_void;
                //         }
                //     }
                // }
                size_t state_counter = 1; // 0 = root
                for (size_t seq_idx = 0; seq_idx < SEQUENCE_COUNT; seq_idx++) {
                    size_t parent = 0; // start at root

                    // compute sequence length
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

                            // Initialize per-mode terminal flag
                            for (size_t m = 0; m < MODE_COUNT; m++) {
                                fsm[next].is_terminal[m] = false;
                            }

                            memset(fsm[next].command,
                                   0,
                                   sizeof(fsm[next].command));
                            memset(fsm[next].next_state,
                                   0,
                                   sizeof(fsm[next].next_state));
                        }
                        parent = next;
                    }

                    // Set terminal & commands for each mode
                    for (size_t m = 0; m < MODE_COUNT; m++) {
                        fsm[parent].is_terminal[m] = true;
                        fsm[parent].command[m] = key_labels[seq_idx][m].command;
                        fsm[parent].handle_release[m] =
                            key_labels[seq_idx][m].handle_release;
                        fsm[parent].handle_timeout[m] =
                            key_labels[seq_idx][m].handle_timeout;
                        fsm[parent].handle_repeat[m] =
                            key_labels[seq_idx][m].handle_repeat;
                    }
                }
                assert(state_counter < MAX_STATES);
                munmap(keymap_map, keymap_size);
                close(keymap_fd);
                xkb_keymap_unref(xkb_keymap);
                xkb_keymap = NULL;
                goto wayland_done;
            wl_keyboard_enter:
                // dump_bytes("wl_keyboard_enter event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                goto wayland_done;
            wl_keyboard_leave:
                // dump_bytes("wl_keyboard_leave event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                goto wayland_done;
            wl_keyboard_key:
                // dump_bytes("wl_keyboard_key event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                if (ctx_wr.end_window_render) { goto wayland_done; }
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
                if (keysym == KEYSYM_DEFAULT) {
                    // repeats
                    key_down[repeat_keysym][repeat_mod] = false;
                    repeat_keysym = 0;
                    repeat_mod = 0;
                    repeating = false;
                    goto cmd_done;
                }
                // if (mods & (1 << mod_fn)) mod |= MOD_FN;
                bool pressed = (wl_key_state == 1);
                if (!pressed) {
                    key_down[keysym][mod] = false;
                    key_last_event_us[keysym][mod] = 0;
                    // repeats
                    if (repeat_keysym == keysym &&
                        fsm[current_state_index].handle_repeat[ctx_wr.mode]) {
                        repeat_keysym = 0;
                        repeat_mod = 0;
                        repeating = false;
                        // timeouts
                        timeout = false;
                        timeout_state_index = 0;
                        timeout_start_us = 0;
                    }
                    if (ctx_wr.skip_release) {
                        ctx_wr.skip_release = 0;
                        goto cmd_done;
                    }
                    uint16_t idx =
                        fsm[current_state_index].next_state[keysym][mod];
                    if (fsm[idx].handle_release[ctx_wr.mode] &&
                        !ctx_wr.trigger && fsm[idx].command[ctx_wr.mode]) {
                        goto* fsm[idx].command[ctx_wr.mode];
                    }
                    goto cmd_done;
                }
                if (ctx_wr.num_chars_in_sequence < MAX_SEQUENCE_LENGTH) {
                    ctx_wr.input_sequence[ctx_wr.num_chars_in_sequence] =
                        war_keysym_to_char(keysym, mod);
                }
                if (!key_down[keysym][mod]) {
                    key_down[keysym][mod] = true;
                    key_last_event_us[keysym][mod] = ctx_wr.now;
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
                fsm_state_last_event_us = ctx_wr.now;
                if (fsm[current_state_index].is_terminal[ctx_wr.mode] &&
                    !war_state_is_prefix(&ctx_wr, current_state_index, fsm)) {
                    uint16_t temp = current_state_index;
                    current_state_index = 0;
                    // repeats
                    if ((ctx_wr.mode != MODE_MIDI ||
                         (ctx_wr.mode == MODE_MIDI && ctx_wr.trigger)) &&
                        fsm[temp].handle_repeat[ctx_wr.mode]) {
                        repeat_keysym = keysym;
                        repeat_mod = mod;
                        repeating = false;
                    }
                    // timeouts
                    if (keysym != KEYSYM_ESCAPE && mod != 0) {
                        timeout_state_index = 0;
                    }
                    timeout_start_us = 0;
                    timeout = false;
                    if (fsm[temp].command[ctx_wr.mode]) {
                        goto* fsm[temp].command[ctx_wr.mode];
                    }
                } else if (fsm[current_state_index].is_terminal[ctx_wr.mode] &&
                           war_state_is_prefix(
                               &ctx_wr, current_state_index, fsm)) {
                    if (fsm[current_state_index].handle_timeout[ctx_wr.mode]) {
                        // repeats
                        repeat_keysym = 0;
                        repeat_mod = 0;
                        repeating = false;
                        // timeouts
                        timeout_state_index = current_state_index;
                        timeout_start_us = ctx_wr.now;
                        timeout = true;
                        current_state_index = 0;
                        goto cmd_done;
                    }
                    uint16_t temp = current_state_index;
                    current_state_index = 0;
                    // repeats
                    if ((ctx_wr.mode != MODE_MIDI ||
                         (ctx_wr.mode == MODE_MIDI && ctx_wr.trigger)) &&
                        fsm[current_state_index].handle_timeout[ctx_wr.mode]) {
                        repeat_keysym = keysym;
                        repeat_mod = mod;
                        repeating = false;
                    }
                    // timeouts
                    if (keysym != KEYSYM_ESCAPE && mod != 0) {
                        timeout_state_index = 0;
                    }
                    timeout_start_us = 0;
                    timeout = false;
                    if (fsm[temp].command[ctx_wr.mode]) {
                        goto* fsm[temp].command[ctx_wr.mode];
                    }
                }
                goto cmd_done;
            cmd_normal_k:
                call_carmack("cmd_normal_k");
                uint32_t increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                uint32_t scaled_whole =
                    (increment * ctx_wr.navigation_whole_number_row) /
                    ctx_wr.navigation_sub_cells_row;
                uint32_t scaled_frac =
                    (increment * ctx_wr.navigation_whole_number_row) %
                    ctx_wr.navigation_sub_cells_row;
                ctx_wr.row = war_clamp_add_uint32(
                    ctx_wr.row, scaled_whole, ctx_wr.max_row);
                ctx_wr.sub_row = war_clamp_add_uint32(
                    ctx_wr.sub_row, scaled_frac, ctx_wr.max_row);
                ctx_wr.row = war_clamp_add_uint32(
                    ctx_wr.row,
                    ctx_wr.sub_row / ctx_wr.navigation_sub_cells_row,
                    ctx_wr.max_row);
                ctx_wr.sub_row = war_clamp_uint32(
                    ctx_wr.sub_row % ctx_wr.navigation_sub_cells_row,
                    ctx_wr.min_row,
                    ctx_wr.max_row);
                if (ctx_wr.row > ctx_wr.top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_add_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.max_row);
                    ctx_wr.top_row = war_clamp_add_uint32(
                        ctx_wr.top_row, increment, ctx_wr.max_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_j:
                call_carmack("cmd_normal_j");
                increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                scaled_whole =
                    (increment * ctx_wr.navigation_whole_number_row) /
                    ctx_wr.navigation_sub_cells_row;
                scaled_frac = (increment * ctx_wr.navigation_whole_number_row) %
                              ctx_wr.navigation_sub_cells_row;
                ctx_wr.row = war_clamp_subtract_uint32(
                    ctx_wr.row, scaled_whole, ctx_wr.min_row);
                if (ctx_wr.sub_row < scaled_frac) {
                    ctx_wr.row = war_clamp_subtract_uint32(
                        ctx_wr.row, 1, ctx_wr.min_row);
                    ctx_wr.sub_row += ctx_wr.navigation_sub_cells_row;
                }
                ctx_wr.sub_row = war_clamp_subtract_uint32(
                    ctx_wr.sub_row, scaled_frac, ctx_wr.min_row);
                ctx_wr.row = war_clamp_subtract_uint32(
                    ctx_wr.row,
                    ctx_wr.sub_row / ctx_wr.navigation_sub_cells_row,
                    ctx_wr.min_row);
                ctx_wr.sub_row = war_clamp_uint32(
                    ctx_wr.sub_row % ctx_wr.navigation_sub_cells_row,
                    ctx_wr.min_row,
                    ctx_wr.max_row);
                if (ctx_wr.row <
                    ctx_wr.bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_subtract_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.min_row);
                    ctx_wr.top_row = war_clamp_subtract_uint32(
                        ctx_wr.top_row, increment, ctx_wr.min_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.top_row = war_clamp_add_uint32(
                            ctx_wr.top_row, diff, ctx_wr.max_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_l:
                call_carmack("cmd_normal_l");
                uint32_t initial = ctx_wr.col;
                increment = ctx_wr.col_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                scaled_whole =
                    (increment * ctx_wr.navigation_whole_number_col) /
                    ctx_wr.navigation_sub_cells_col;
                scaled_frac = (increment * ctx_wr.navigation_whole_number_col) %
                              ctx_wr.navigation_sub_cells_col;
                ctx_wr.col = war_clamp_add_uint32(
                    ctx_wr.col, scaled_whole, ctx_wr.max_col);
                ctx_wr.sub_col = war_clamp_add_uint32(
                    ctx_wr.sub_col, scaled_frac, ctx_wr.max_col);
                if (ctx_wr.sub_col >= ctx_wr.navigation_sub_cells_col) {
                    uint32_t carry =
                        ctx_wr.sub_col / ctx_wr.navigation_sub_cells_col;
                    ctx_wr.col =
                        war_clamp_add_uint32(ctx_wr.col, carry, ctx_wr.max_col);
                    ctx_wr.sub_col =
                        ctx_wr.sub_col % ctx_wr.navigation_sub_cells_col;
                }
                uint32_t pan = ctx_wr.col - initial;
                if (ctx_wr.col > ctx_wr.right_col - ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_add_uint32(
                        ctx_wr.left_col, pan, ctx_wr.max_col);
                    ctx_wr.right_col = war_clamp_add_uint32(
                        ctx_wr.right_col, pan, ctx_wr.max_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.left_col, diff, ctx_wr.min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_h:
                call_carmack("cmd_normal_h");
                initial = ctx_wr.col;
                increment = ctx_wr.col_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                scaled_whole =
                    (increment * ctx_wr.navigation_whole_number_col) /
                    ctx_wr.navigation_sub_cells_col;
                scaled_frac = (increment * ctx_wr.navigation_whole_number_col) %
                              ctx_wr.navigation_sub_cells_col;
                ctx_wr.col = war_clamp_subtract_uint32(
                    ctx_wr.col, scaled_whole, ctx_wr.min_col);
                if (ctx_wr.sub_col < scaled_frac) {
                    if (ctx_wr.col > ctx_wr.min_col) {
                        ctx_wr.col = war_clamp_subtract_uint32(
                            ctx_wr.col, 1, ctx_wr.min_col);
                        ctx_wr.sub_col += ctx_wr.navigation_sub_cells_col;
                    } else {
                        ctx_wr.sub_col = 0;
                    }
                }
                ctx_wr.sub_col = war_clamp_subtract_uint32(
                    ctx_wr.sub_col, scaled_frac, ctx_wr.min_col);
                ctx_wr.col = war_clamp_subtract_uint32(
                    ctx_wr.col,
                    ctx_wr.sub_col / ctx_wr.navigation_sub_cells_col,
                    ctx_wr.min_col);
                ctx_wr.sub_col = war_clamp_uint32(
                    ctx_wr.sub_col % ctx_wr.navigation_sub_cells_col,
                    ctx_wr.min_col,
                    ctx_wr.max_col);
                pan = initial - ctx_wr.col;
                if (ctx_wr.col < ctx_wr.left_col + ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_subtract_uint32(
                        ctx_wr.left_col, pan, ctx_wr.min_col);
                    ctx_wr.right_col = war_clamp_subtract_uint32(
                        ctx_wr.right_col, pan, ctx_wr.min_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.right_col, diff, ctx_wr.max_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_K: {
                call_carmack("cmd_normal_K");
                float gain =
                    atomic_load(&atomics->play_gain) + ctx_wr.gain_increment;
                gain = fminf(gain, 1.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_J: {
                call_carmack("cmd_normal_J");
                float gain =
                    atomic_load(&atomics->play_gain) - ctx_wr.gain_increment;
                gain = fmaxf(gain, 0.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_k:
                call_carmack("cmd_normal_alt_k");
                increment = ctx_wr.row_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                ctx_wr.row =
                    war_clamp_add_uint32(ctx_wr.row, increment, ctx_wr.max_row);
                if (ctx_wr.row > ctx_wr.top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_add_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.max_row);
                    ctx_wr.top_row = war_clamp_add_uint32(
                        ctx_wr.top_row, increment, ctx_wr.max_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_j:
                call_carmack("cmd_normal_alt_j");
                increment = ctx_wr.row_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                ctx_wr.row = war_clamp_subtract_uint32(
                    ctx_wr.row, increment, ctx_wr.min_row);
                if (ctx_wr.row <
                    ctx_wr.bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_subtract_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.min_row);
                    ctx_wr.top_row = war_clamp_subtract_uint32(
                        ctx_wr.top_row, increment, ctx_wr.min_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.top_row = war_clamp_add_uint32(
                            ctx_wr.top_row, diff, ctx_wr.max_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_l:
                call_carmack("cmd_normal_alt_l");
                increment = ctx_wr.col_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                ctx_wr.col =
                    war_clamp_add_uint32(ctx_wr.col, increment, ctx_wr.max_col);
                if (ctx_wr.col > ctx_wr.right_col - ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_add_uint32(
                        ctx_wr.left_col, increment, ctx_wr.max_col);
                    ctx_wr.right_col = war_clamp_add_uint32(
                        ctx_wr.right_col, increment, ctx_wr.max_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.left_col, diff, ctx_wr.min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_h:
                call_carmack("cmd_normal_alt_h");
                increment = ctx_wr.col_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                ctx_wr.col = war_clamp_subtract_uint32(
                    ctx_wr.col, increment, ctx_wr.min_col);
                if (ctx_wr.col < ctx_wr.left_col + ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_subtract_uint32(
                        ctx_wr.left_col, increment, ctx_wr.min_col);
                    ctx_wr.right_col = war_clamp_subtract_uint32(
                        ctx_wr.right_col, increment, ctx_wr.min_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.right_col, diff, ctx_wr.max_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_K:
                call_carmack("cmd_normal_alt_shift_k");
                increment =
                    ctx_wr.viewport_rows - ctx_wr.num_rows_for_status_bars;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                ctx_wr.row =
                    war_clamp_add_uint32(ctx_wr.row, increment, ctx_wr.max_row);
                if (ctx_wr.row > ctx_wr.top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_add_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.max_row);
                    ctx_wr.top_row = war_clamp_add_uint32(
                        ctx_wr.top_row, increment, ctx_wr.max_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_J:
                call_carmack("cmd_normal_alt_shift_j");
                increment =
                    ctx_wr.viewport_rows - ctx_wr.num_rows_for_status_bars;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_row);
                }
                ctx_wr.row = war_clamp_subtract_uint32(
                    ctx_wr.row, increment, ctx_wr.min_row);
                if (ctx_wr.row <
                    ctx_wr.bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    ctx_wr.bottom_row = war_clamp_subtract_uint32(
                        ctx_wr.bottom_row, increment, ctx_wr.min_row);
                    ctx_wr.top_row = war_clamp_subtract_uint32(
                        ctx_wr.top_row, increment, ctx_wr.min_row);
                    uint32_t new_viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        ctx_wr.top_row = war_clamp_add_uint32(
                            ctx_wr.top_row, diff, ctx_wr.max_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_L:
                call_carmack("cmd_normal_alt_shift_l");
                increment =
                    ctx_wr.viewport_cols - ctx_wr.num_cols_for_line_numbers;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                ctx_wr.col =
                    war_clamp_add_uint32(ctx_wr.col, increment, ctx_wr.max_col);
                if (ctx_wr.col > ctx_wr.right_col - ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_add_uint32(
                        ctx_wr.left_col, increment, ctx_wr.max_col);
                    ctx_wr.right_col = war_clamp_add_uint32(
                        ctx_wr.right_col, increment, ctx_wr.max_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.left_col, diff, ctx_wr.min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_H:
                call_carmack("cmd_normal_alt_shift_h");
                increment =
                    ctx_wr.viewport_cols - ctx_wr.num_cols_for_line_numbers;
                if (ctx_wr.numeric_prefix) {
                    increment = war_clamp_multiply_uint32(
                        increment, ctx_wr.numeric_prefix, ctx_wr.max_col);
                }
                ctx_wr.col = war_clamp_subtract_uint32(
                    ctx_wr.col, increment, ctx_wr.min_col);
                if (ctx_wr.col < ctx_wr.left_col + ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    ctx_wr.left_col = war_clamp_subtract_uint32(
                        ctx_wr.left_col, increment, ctx_wr.min_col);
                    ctx_wr.right_col = war_clamp_subtract_uint32(
                        ctx_wr.right_col, increment, ctx_wr.min_col);
                    uint32_t new_viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.right_col, diff, ctx_wr.max_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_0:
                call_carmack("cmd_normal_0");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.numeric_prefix = ctx_wr.numeric_prefix * 10;
                    goto cmd_done;
                }
                ctx_wr.col = ctx_wr.left_col;
                ctx_wr.sub_col = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_V: {
                call_carmack("cmd_normal_V");
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            }
            cmd_normal_ga: {
                call_carmack("cmd_normal_$");
                uint32_t col = ((float)atomic_load(&atomics->play_clock) /
                                ctx_a.sample_rate) /
                               ((60.0f / ctx_a.BPM) / 4.0f);
                ctx_wr.col =
                    war_clamp_uint32(col, ctx_wr.min_col, ctx_wr.max_col);
                ctx_wr.sub_col = 0;
                uint32_t viewport_width = ctx_wr.right_col - ctx_wr.left_col;
                uint32_t distance = viewport_width / 2;
                ctx_wr.left_col = war_clamp_subtract_uint32(
                    ctx_wr.col, distance, ctx_wr.min_col);
                ctx_wr.right_col =
                    war_clamp_add_uint32(ctx_wr.col, distance, ctx_wr.max_col);
                uint32_t new_viewport_width = war_clamp_subtract_uint32(
                    ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_width, new_viewport_width, ctx_wr.min_col);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr.right_col, diff, ctx_wr.max_col);
                    if (sum < ctx_wr.max_col) {
                        ctx_wr.right_col = sum;
                    } else {
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.left_col, diff, ctx_wr.min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_$:
                call_carmack("cmd_normal_$");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.col = war_clamp_uint32(
                        ctx_wr.numeric_prefix, ctx_wr.min_col, ctx_wr.max_col);
                    ctx_wr.sub_col = 0;
                    uint32_t viewport_width =
                        ctx_wr.right_col - ctx_wr.left_col;
                    uint32_t distance = viewport_width / 2;
                    ctx_wr.left_col = war_clamp_subtract_uint32(
                        ctx_wr.col, distance, ctx_wr.min_col);
                    ctx_wr.right_col = war_clamp_add_uint32(
                        ctx_wr.col, distance, ctx_wr.max_col);
                    uint32_t new_viewport_width = war_clamp_subtract_uint32(
                        ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = war_clamp_subtract_uint32(
                            viewport_width, new_viewport_width, ctx_wr.min_col);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx_wr.right_col, diff, ctx_wr.max_col);
                        if (sum < ctx_wr.max_col) {
                            ctx_wr.right_col = sum;
                        } else {
                            ctx_wr.left_col = war_clamp_subtract_uint32(
                                ctx_wr.left_col, diff, ctx_wr.min_col);
                        }
                    }
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.col = ctx_wr.right_col;
                ctx_wr.sub_col = 0;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_G:
                call_carmack("cmd_normal_G");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.row = war_clamp_uint32(
                        ctx_wr.numeric_prefix, ctx_wr.min_row, ctx_wr.max_row);
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    uint32_t distance = viewport_height / 2;
                    ctx_wr.bottom_row = war_clamp_subtract_uint32(
                        ctx_wr.row, distance, ctx_wr.min_row);
                    ctx_wr.top_row = war_clamp_add_uint32(
                        ctx_wr.row, distance, ctx_wr.max_row);
                    uint32_t new_viewport_height = war_clamp_subtract_uint32(
                        ctx_wr.top_row, ctx_wr.bottom_row, 0);
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = war_clamp_subtract_uint32(
                            viewport_height, new_viewport_height, 0);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx_wr.top_row, diff, ctx_wr.max_row);
                        if (sum < ctx_wr.max_row) {
                            ctx_wr.top_row = sum;
                        } else {
                            ctx_wr.bottom_row = war_clamp_subtract_uint32(
                                ctx_wr.bottom_row, diff, ctx_wr.min_row);
                        }
                    }
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.row = ctx_wr.bottom_row;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gg:
                call_carmack("cmd_normal_gg");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.row = war_clamp_uint32(
                        ctx_wr.numeric_prefix, ctx_wr.min_row, ctx_wr.max_row);
                    uint32_t viewport_height =
                        ctx_wr.top_row - ctx_wr.bottom_row;
                    uint32_t distance = viewport_height / 2;
                    ctx_wr.bottom_row = war_clamp_subtract_uint32(
                        ctx_wr.row, distance, ctx_wr.min_row);
                    ctx_wr.top_row = war_clamp_add_uint32(
                        ctx_wr.row, distance, ctx_wr.max_row);
                    uint32_t new_viewport_height = war_clamp_subtract_uint32(
                        ctx_wr.top_row, ctx_wr.bottom_row, ctx_wr.min_row);
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff =
                            war_clamp_subtract_uint32(viewport_height,
                                                      new_viewport_height,
                                                      ctx_wr.min_row);
                        uint32_t sum = war_clamp_add_uint32(
                            ctx_wr.top_row, diff, ctx_wr.max_row);
                        if (sum < ctx_wr.max_row) {
                            ctx_wr.top_row = sum;
                        } else {
                            ctx_wr.bottom_row = war_clamp_subtract_uint32(
                                ctx_wr.bottom_row, diff, ctx_wr.min_row);
                        }
                    }
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.row = ctx_wr.top_row;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_1:
                call_carmack("cmd_normal_1");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 1, UINT32_MAX);
                goto cmd_done;
            cmd_normal_2:
                call_carmack("cmd_normal_2");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 2, UINT32_MAX);
                goto cmd_done;
            cmd_normal_3:
                call_carmack("cmd_normal_3");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 3, UINT32_MAX);
                goto cmd_done;
            cmd_normal_4:
                call_carmack("cmd_normal_4");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 4, UINT32_MAX);
                goto cmd_done;
            cmd_normal_5:
                call_carmack("cmd_normal_5");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 5, UINT32_MAX);
                goto cmd_done;
            cmd_normal_6:
                call_carmack("cmd_normal_6");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 6, UINT32_MAX);
                goto cmd_done;
            cmd_normal_7:
                call_carmack("cmd_normal_7");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 7, UINT32_MAX);
                goto cmd_done;
            cmd_normal_8:
                call_carmack("cmd_normal_8");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 8, UINT32_MAX);
                goto cmd_done;
            cmd_normal_9:
                call_carmack("cmd_normal_9");
                ctx_wr.numeric_prefix = war_clamp_multiply_uint32(
                    ctx_wr.numeric_prefix, 10, UINT32_MAX);
                ctx_wr.numeric_prefix =
                    war_clamp_add_uint32(ctx_wr.numeric_prefix, 9, UINT32_MAX);
                goto cmd_done;
            cmd_normal_ctrl_equal:
                call_carmack("cmd_normal_ctrl_equal");
                // ctx_wr.zoom_scale +=
                // ctx_wr.zoom_increment; if
                // (ctx_wr.zoom_scale > 5.0f) {
                // ctx_wr.zoom_scale = 5.0f; }
                // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
                // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
                // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
                // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_minus:
                call_carmack("cmd_normal_ctrl_minus");
                // ctx_wr.zoom_scale -=
                // ctx_wr.zoom_increment; if
                // (ctx_wr.zoom_scale < 0.1f) {
                // ctx_wr.zoom_scale = 0.1f; }
                // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
                // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
                // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
                // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_alt_equal:
                call_carmack("cmd_normal_ctrl_alt_equal");
                // ctx_wr.zoom_scale +=
                // ctx_wr.zoom_leap_increment; if
                // (ctx_wr.zoom_scale > 5.0f) {
                // ctx_wr.zoom_scale = 5.0f; }
                // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
                // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
                // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
                // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_alt_minus:
                call_carmack("cmd_normal_ctrl_alt_minus");
                // ctx_wr.zoom_scale -=
                // ctx_wr.zoom_leap_increment; if
                // (ctx_wr.zoom_scale < 0.1f) {
                // ctx_wr.zoom_scale = 0.1f; }
                // ctx_wr.panning_x = ctx_wr.anchor_ndc_x
                // - ctx_wr.anchor_x * ctx_wr.zoom_scale;
                // ctx_wr.panning_y = ctx_wr.anchor_ndc_y
                // - ctx_wr.anchor_y * ctx_wr.zoom_scale;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_ctrl_0:
                call_carmack("cmd_normal_ctrl_0");
                ctx_wr.zoom_scale = 1.0f;
                ctx_wr.left_col = 0;
                ctx_wr.bottom_row = 0;
                ctx_wr.right_col =
                    (uint32_t)((ctx_wr.physical_width -
                                ((float)ctx_wr.num_cols_for_line_numbers *
                                 ctx_wr.cell_width)) /
                               ctx_wr.cell_width) -
                    1;
                ctx_wr.top_row =
                    (uint32_t)((ctx_wr.physical_height -
                                ((float)ctx_wr.num_rows_for_status_bars *
                                 ctx_wr.cell_height)) /
                               ctx_wr.cell_height) -
                    1;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_esc:
                call_carmack("cmd_normal_esc");
                if (timeout_state_index) {
                    if (fsm[timeout_state_index].command[ctx_wr.mode]) {
                        goto* fsm[timeout_state_index].command[ctx_wr.mode];
                        timeout_state_index = 0;
                    }
                    goto cmd_done;
                }
                if (atomic_load(&atomics->state) == AUDIO_CMD_RECORD) {}
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_s:
                call_carmack("cmd_normal_s");
                ctx_wr.cursor_width_sub_cells = 1;
                ctx_wr.cursor_width_whole_number = 1;
                ctx_wr.cursor_width_sub_col = 1;
                ctx_wr.navigation_whole_number_col = 1;
                ctx_wr.navigation_sub_cells_col = 1;
                if (ctx_wr.navigation_sub_cells_col !=
                    ctx_wr.previous_navigation_sub_cells_col) {
                    ctx_wr.sub_col =
                        (ctx_wr.sub_col * ctx_wr.navigation_sub_cells_col) /
                        ctx_wr.previous_navigation_sub_cells_col;
                    ctx_wr.sub_col = war_clamp_uint32(
                        ctx_wr.sub_col, 0, ctx_wr.navigation_sub_cells_col - 1);

                    ctx_wr.previous_navigation_sub_cells_col =
                        ctx_wr.navigation_sub_cells_col;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_f:
                call_carmack("cmd_normal_f");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.cursor_width_whole_number = ctx_wr.numeric_prefix;
                    ctx_wr.f_cursor_width_whole_number = ctx_wr.numeric_prefix;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    ctx_wr.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx_wr.cursor_width_whole_number = 1;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_t:
                call_carmack("cmd_normal_t");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.cursor_width_sub_cells = ctx_wr.numeric_prefix;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    ctx_wr.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx_wr.cursor_width_sub_cells = 1;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
            cmd_normal_T:
                call_carmack("cmd_normal_T");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.previous_navigation_sub_cells_col =
                        ctx_wr.navigation_sub_cells_col;
                    ctx_wr.navigation_sub_cells_col = ctx_wr.numeric_prefix;
                    ctx_wr.t_navigation_sub_cells = ctx_wr.numeric_prefix;
                    if (ctx_wr.navigation_sub_cells_col !=
                        ctx_wr.previous_navigation_sub_cells_col) {
                        ctx_wr.sub_col =
                            (ctx_wr.sub_col * ctx_wr.navigation_sub_cells_col) /
                            ctx_wr.previous_navigation_sub_cells_col;
                        ctx_wr.sub_col = war_clamp_uint32(
                            ctx_wr.sub_col,
                            0,
                            ctx_wr.navigation_sub_cells_col - 1);

                        ctx_wr.previous_navigation_sub_cells_col =
                            ctx_wr.navigation_sub_cells_col;
                    }
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    ctx_wr.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx_wr.navigation_sub_cells_col = 1;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_F:
                call_carmack("cmd_normal_F");
                if (ctx_wr.numeric_prefix) {
                    ctx_wr.navigation_whole_number_col = ctx_wr.numeric_prefix;
                    ctx_wr.f_navigation_whole_number = ctx_wr.numeric_prefix;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    ctx_wr.numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx_wr.navigation_whole_number_col = 1;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                ctx_wr.numeric_prefix = 0;
                goto cmd_done;
            cmd_normal_gb:
                call_carmack("cmd_normal_gb");
                ctx_wr.row = ctx_wr.min_row;
                uint32_t viewport_height = ctx_wr.top_row - ctx_wr.bottom_row;
                uint32_t distance = viewport_height / 2;
                ctx_wr.bottom_row = war_clamp_subtract_uint32(
                    ctx_wr.row, distance, ctx_wr.min_row);
                ctx_wr.top_row =
                    war_clamp_add_uint32(ctx_wr.row, distance, ctx_wr.max_row);
                uint32_t new_viewport_height = war_clamp_subtract_uint32(
                    ctx_wr.top_row, ctx_wr.bottom_row, ctx_wr.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx_wr.min_row);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr.top_row, diff, ctx_wr.max_row);
                    if (sum < ctx_wr.max_row) {
                        ctx_wr.top_row = sum;
                    } else {
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gt:
                call_carmack("cmd_normal_gt");
                ctx_wr.row = ctx_wr.max_row;
                viewport_height = ctx_wr.top_row - ctx_wr.bottom_row;
                distance = viewport_height / 2;
                ctx_wr.bottom_row = war_clamp_subtract_uint32(
                    ctx_wr.row, distance, ctx_wr.min_row);
                ctx_wr.top_row =
                    war_clamp_add_uint32(ctx_wr.row, distance, ctx_wr.max_row);
                new_viewport_height = war_clamp_subtract_uint32(
                    ctx_wr.top_row, ctx_wr.bottom_row, ctx_wr.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx_wr.min_row);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr.top_row, diff, ctx_wr.max_row);
                    if (sum < ctx_wr.max_row) {
                        ctx_wr.top_row = sum;
                    } else {
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_gm:
                call_carmack("cmd_normal_gm");
                ctx_wr.row = 60;
                viewport_height = ctx_wr.top_row - ctx_wr.bottom_row;
                distance = viewport_height / 2;
                ctx_wr.bottom_row = war_clamp_subtract_uint32(
                    ctx_wr.row, distance, ctx_wr.min_row);
                ctx_wr.top_row =
                    war_clamp_add_uint32(ctx_wr.row, distance, ctx_wr.max_row);
                new_viewport_height = war_clamp_subtract_uint32(
                    ctx_wr.top_row, ctx_wr.bottom_row, ctx_wr.min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx_wr.min_row);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr.top_row, diff, ctx_wr.max_row);
                    if (sum < ctx_wr.max_row) {
                        ctx_wr.top_row = sum;
                    } else {
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.bottom_row, diff, ctx_wr.min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                goto cmd_done;
            cmd_normal_z:
                call_carmack("cmd_normal_z");
                if (ctx_wr.numeric_prefix) {
                    for (uint32_t i = 0; i < ctx_wr.numeric_prefix; i++) {
                        war_note_quads_add(&note_quads,
                                           &note_quads_count,
                                           pc,
                                           &ctx_wr,
                                           ctx_wr.color_cursor,
                                           ctx_wr.color_note_outline_default,
                                           100.0f,
                                           AUDIO_VOICE_GRAND_PIANO,
                                           false,
                                           false);
                    }
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_note_quads_add(&note_quads,
                                   &note_quads_count,
                                   pc,
                                   &ctx_wr,
                                   ctx_wr.color_cursor,
                                   ctx_wr.color_note_outline_default,
                                   100.0f,
                                   AUDIO_VOICE_GRAND_PIANO,
                                   false,
                                   false);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_return:
                call_carmack("cmd_normal_return");
                if (ctx_wr.numeric_prefix) {
                    for (uint32_t i = 0; i < ctx_wr.numeric_prefix; i++) {
                        war_note_quads_add(&note_quads,
                                           &note_quads_count,
                                           pc,
                                           &ctx_wr,
                                           ctx_wr.color_cursor,
                                           ctx_wr.color_note_outline_default,
                                           100.0f,
                                           AUDIO_VOICE_GRAND_PIANO,
                                           false,
                                           false);
                    }
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_note_quads_add(&note_quads,
                                   &note_quads_count,
                                   pc,
                                   &ctx_wr,
                                   ctx_wr.color_cursor,
                                   ctx_wr.color_note_outline_default,
                                   100.0f,
                                   AUDIO_VOICE_GRAND_PIANO,
                                   false,
                                   false);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_x:
                call_carmack("cmd_normal_x");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_trim = note_quads_in_x[i];
                    if (note_quads.hidden[i_trim]) { continue; }
                    war_note_quads_trim_right_at_i(
                        &note_quads, &note_quads_count, &ctx_wr, pc, i_trim);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_X:
                call_carmack("cmd_normal_X");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_trim = note_quads_in_x[i];
                    if (note_quads.hidden[i_trim]) { continue; }
                    war_note_quads_trim_left_at_i(
                        &note_quads, &note_quads_count, &ctx_wr, pc, i_trim);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_d:
                call_carmack("cmd_normal_d");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    if (note_quads.hidden[i_delete]) { continue; }
                    war_note_quads_delete_at_i(
                        &note_quads, &note_quads_count, pc, i_delete);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacediv:
                call_carmack("cmd_normal_spacediv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (int32_t i = (int32_t)note_quads_in_x_count - 1; i >= 0;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    if (note_quads.hidden[i_delete]) { continue; }
                    war_note_quads_delete_at_i(
                        &note_quads, &note_quads_count, pc, i_delete);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacedov:
                call_carmack("cmd_normal_spacedov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (int32_t i = (int32_t)note_quads_in_x_count - 1; i >= 0;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    if (note_quads.hidden[i_delete]) { continue; }
                    war_note_quads_delete_at_i(
                        &note_quads, &note_quads_count, pc, i_delete);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacediw:
                call_carmack("cmd_normal_spacediw");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_delete = note_quads_in_x[i];
                    if (note_quads.hidden[i_delete]) { continue; }
                    war_note_quads_delete_at_i(
                        &note_quads, &note_quads_count, pc, i_delete);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceda:
                call_carmack("cmd_normal_spaceda");
                note_quads_count = 0;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacehov:
                call_carmack("cmd_normal_spacehov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacehiv:
                call_carmack("cmd_normal_spacehiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacehiw:
                call_carmack("cmd_normal_spacehiw");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_hide = note_quads_in_x[i];
                    note_quads.hidden[i_hide] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceha:
                call_carmack("cmd_normal_spaceha");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.hidden[i] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesov:
                call_carmack("cmd_normal_spacesov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesiv:
                call_carmack("cmd_normal_spacesiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    note_quads.hidden[note_quads_in_x[i]] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesiw:
                call_carmack("cmd_normal_spacesiw");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_hide = note_quads_in_x[i];
                    note_quads.hidden[i_hide] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacesa:
                call_carmack("cmd_normal_spacesa");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.hidden[i] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacem:
                call_carmack("cmd_normal_spacem");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (note_quads
                        .hidden[note_quads_in_x[note_quads_in_x_count - 1]]) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                note_quads.mute[note_quads_in_x[note_quads_in_x_count - 1]] =
                    !note_quads
                         .mute[note_quads_in_x[note_quads_in_x_count - 1]];
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacemov:
                call_carmack("cmd_normal_spacemov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    uint32_t i_mute = note_quads_in_x[i];
                    if (note_quads.hidden[i_mute]) { continue; }
                    note_quads.mute[i_mute] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacemiv:
                call_carmack("cmd_normal_spacemiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    uint32_t i_mute = note_quads_in_x[i];
                    if (note_quads.hidden[i_mute]) { continue; }
                    note_quads.mute[i_mute] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacema:
                call_carmack("cmd_normal_spacema");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.mute[i] = true;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_m:
                call_carmack("cmd_normal_m");
                ctx_wr.mode = MODE_MIDI;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceuov:
                call_carmack("cmd_normal_spaceuov");
                note_quads_in_x_count = 0;
                war_note_quads_outside_view(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    uint32_t i_unmute = note_quads_in_x[i];
                    if (note_quads.hidden[i_unmute]) { continue; }
                    note_quads.mute[i_unmute] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceuiv:
                call_carmack("cmd_normal_spaceuiv");
                note_quads_in_x_count = 0;
                war_note_quads_in_view(&note_quads,
                                       note_quads_count,
                                       &ctx_wr,
                                       note_quads_in_x,
                                       &note_quads_in_x_count);
                for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                    uint32_t i_unmute = note_quads_in_x[i];
                    if (note_quads.hidden[i_unmute]) { continue; }
                    note_quads.mute[i_unmute] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceuiw:
                call_carmack("cmd_normal_spaceuiw");
                note_quads_in_x_count = 0;
                war_note_quads_under_cursor(&note_quads,
                                            note_quads_count,
                                            &ctx_wr,
                                            note_quads_in_x,
                                            &note_quads_in_x_count);
                if (!note_quads_in_x_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                for (int32_t i = (int32_t)note_quads_in_x_count - 1;
                     i >= (int32_t)note_quads_in_x_count - 1 -
                              (int32_t)ctx_wr.numeric_prefix;
                     i--) {
                    uint32_t i_mute = note_quads_in_x[i];
                    note_quads.mute[i_mute] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spaceua:
                call_carmack("cmd_normal_spaceua");
                for (uint32_t i = 0; i < note_quads_count; i++) {
                    note_quads.mute[note_quads_in_x[i]] = false;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacea:
                call_carmack("cmd_normal_spacea");
                if (views.views_count < MAX_VIEWS_SAVED) {
                    views.col[views.views_count] = ctx_wr.col;
                    views.row[views.views_count] = ctx_wr.row;
                    views.left_col[views.views_count] = ctx_wr.left_col;
                    views.bottom_row[views.views_count] = ctx_wr.bottom_row;
                    views.right_col[views.views_count] = ctx_wr.right_col;
                    views.top_row[views.views_count] = ctx_wr.top_row;
                    views.views_count++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_spacedspacea:
                call_carmack("cmd_normal_spacedspacea");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_g:
                call_carmack("cmd_normal_alt_g");
                if (views.views_count > 0) {
                    uint32_t i_views = 0;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_t:
                call_carmack("cmd_normal_alt_t");
                if (views.views_count > 1) {
                    uint32_t i_views = 1;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_n:
                call_carmack("cmd_normal_alt_n");
                if (views.views_count > 2) {
                    uint32_t i_views = 2;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_s:
                call_carmack("cmd_normal_alt_s");
                if (views.views_count > 3) {
                    uint32_t i_views = 3;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_m:
                call_carmack("cmd_normal_alt_m");
                if (views.views_count > 4) {
                    uint32_t i_views = 4;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_y:
                call_carmack("cmd_normal_alt_y");
                if (views.views_count > 5) {
                    uint32_t i_views = 5;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_z:
                call_carmack("cmd_normal_alt_z");
                if (views.views_count > 6) {
                    uint32_t i_views = 6;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_q:
                call_carmack("cmd_normal_alt_q");
                if (views.views_count > 7) {
                    uint32_t i_views = 7;
                    ctx_wr.col = views.col[i_views];
                    ctx_wr.row = views.row[i_views];
                    ctx_wr.left_col = views.left_col[i_views];
                    ctx_wr.bottom_row = views.bottom_row[i_views];
                    ctx_wr.right_col = views.right_col[i_views];
                    ctx_wr.top_row = views.top_row[i_views];
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_e:
                call_carmack("cmd_normal_alt_e");
                ctx_wr.mode =
                    (ctx_wr.mode != MODE_VIEWS) ? MODE_VIEWS : MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                call_carmack("mode: %u", ctx_wr.mode);
                goto cmd_done;
            //-----------------------------------------------------------------
            // PLAYBACK COMMANDS
            //-----------------------------------------------------------------
            cmd_normal_a: {
                call_carmack("cmd_normal_a");
                atomic_fetch_xor(&atomics->play, 1);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_a: {
                call_carmack("cmd_normal_alt_a");
                uint64_t seek_viewport = (uint64_t)((
                    float)(ctx_wr.left_col * ((60.0f / ctx_a.BPM) / 4.0f) *
                           ctx_a.sample_rate));
                war_pc_to_a(
                    pc, AUDIO_CMD_SEEK, sizeof(uint64_t), &seek_viewport);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_A: {
                call_carmack("cmd_normal_alt_A");
                uint64_t seek = (uint64_t)((
                    float)(war_cursor_pos_x(&ctx_wr) *
                           ((60.0f / ctx_a.BPM) / 4.0f) * ctx_a.sample_rate));
                war_pc_to_a(pc, AUDIO_CMD_SEEK, sizeof(uint64_t), &seek);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_A: {
                call_carmack("cmd_normal_A");
                if (ctx_wr.numeric_prefix) {
                    uint64_t seek = (uint64_t)((float)ctx_wr.numeric_prefix *
                                               ((60.0f / ctx_a.BPM) / 4.0f) *
                                               ctx_a.sample_rate);
                    war_pc_to_a(pc, AUDIO_CMD_SEEK, sizeof(uint64_t), &seek);
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_esc: {
                call_carmack("cmd_normal_alt_esc");
                atomic_store(&atomics->state, AUDIO_CMD_STOP);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_ctrl_a: {
                call_carmack("cmd_normal_ctrl_a");
                uint64_t seek_start = 0;
                war_pc_to_a(pc, AUDIO_CMD_SEEK, sizeof(uint64_t), &seek_start);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_space1:
                call_carmack("cmd_normal_space1");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space2:
                call_carmack("cmd_normal_space2");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space3:
                call_carmack("cmd_normal_space3");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space4:
                call_carmack("cmd_normal_space4");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space5:
                call_carmack("cmd_normal_space5");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space6:
                call_carmack("cmd_normal_space6");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space7:
                call_carmack("cmd_normal_space7");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space8:
                call_carmack("cmd_normal_space8");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space9:
                call_carmack("cmd_normal_space9");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_space0:
                call_carmack("cmd_normal_space0");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_1:
                call_carmack("cmd_normal_alt_1");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_2:
                call_carmack("cmd_normal_alt_2");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_3:
                call_carmack("cmd_normal_alt_3");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_4:
                call_carmack("cmd_normal_alt_4");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_5:
                call_carmack("cmd_normal_alt_5");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_6:
                call_carmack("cmd_normal_alt_6");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_7:
                call_carmack("cmd_normal_alt_7");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_8:
                call_carmack("cmd_normal_alt_8");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_9:
                call_carmack("cmd_normal_alt_9");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_alt_0:
                call_carmack("cmd_normal_alt_0");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_w:
                call_carmack("cmd_normal_w");
                note_quads_in_x_count = 0;
                war_note_quads_in_row(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                if (note_quads_in_x_count == 0) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                const float EPS = 1e-2f;
                int i_next_note = -1;
                uint32_t count = 1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                uint32_t temp_i = 0;
                float cursor_pos_x = war_cursor_pos_x(&ctx_wr);
                while (temp_i < count) {
                    float next_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        float next_note_distance_temp =
                            war_note_pos_x(&note_quads, note_quads_in_x[i]) -
                            cursor_pos_x;
                        if (next_note_distance_temp < 0.0f) { continue; }
                        if ((next_note_distance < 0.0f &&
                             next_note_distance_temp > 0.0f) ||
                            (next_note_distance_temp > 0.0f &&
                             next_note_distance_temp <
                                 next_note_distance + EPS)) {
                            next_note_distance = next_note_distance_temp;
                            i_next_note = note_quads_in_x[i];
                        }
                    }
                    if (i_next_note < 0) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.col = war_clamp_uint32(note_quads.col[i_next_note],
                                                  ctx_wr.min_col,
                                                  ctx_wr.max_col);
                    ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                    ctx_wr.navigation_sub_cells_col =
                        note_quads.sub_cells_col[i_next_note];
                    if (ctx_wr.col > ctx_wr.right_col ||
                        ctx_wr.col < ctx_wr.left_col) {
                        uint32_t viewport_width =
                            ctx_wr.right_col - ctx_wr.left_col;
                        distance = viewport_width / 2;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.col, distance, ctx_wr.min_col);
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.col, distance, ctx_wr.max_col);
                        ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                        uint32_t new_viewport_width = war_clamp_subtract_uint32(
                            ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                        if (new_viewport_width < viewport_width) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_width,
                                                          new_viewport_width,
                                                          ctx_wr.min_col);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.right_col, diff, ctx_wr.max_col);
                            if (sum < ctx_wr.max_col) {
                                ctx_wr.right_col = sum;
                            } else {
                                ctx_wr.left_col = war_clamp_subtract_uint32(
                                    ctx_wr.left_col, diff, ctx_wr.min_col);
                            }
                        }
                    }
                    cursor_pos_x = war_note_pos_x(&note_quads, i_next_note);
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_W:
                call_carmack("cmd_normal_W");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_e:
                call_carmack("cmd_normal_e");
                note_quads_in_x_count = 0;
                war_note_quads_in_row(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                if (note_quads_in_x_count == 0) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                count = 1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                temp_i = 0;
                float cursor_pos_x_end = war_cursor_pos_x_end(&ctx_wr);
                while (temp_i < count) {
                    i_next_note = -1;
                    float next_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        float next_note_distance_temp =
                            war_note_pos_x_end(&note_quads,
                                               note_quads_in_x[i]) -
                            cursor_pos_x;
                        if (next_note_distance_temp < 0.0f) { continue; }
                        if ((next_note_distance < 0.0f &&
                             next_note_distance_temp > 0.0f) ||
                            (next_note_distance_temp > 0.0f &&
                             next_note_distance_temp <
                                 next_note_distance + EPS)) {
                            next_note_distance = next_note_distance_temp;
                            i_next_note = note_quads_in_x[i];
                        }
                    }
                    if (i_next_note < 0) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.col =
                        war_clamp_uint32(note_quads.col[i_next_note] +
                                             (uint32_t)war_note_span_x(
                                                 &note_quads, i_next_note) -
                                             1,
                                         ctx_wr.min_col,
                                         ctx_wr.max_col);
                    ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                    ctx_wr.navigation_sub_cells_col =
                        note_quads.sub_cells_col[i_next_note];
                    if (ctx_wr.col > ctx_wr.right_col ||
                        ctx_wr.col < ctx_wr.left_col) {
                        uint32_t viewport_width =
                            ctx_wr.right_col - ctx_wr.left_col;
                        distance = viewport_width / 2;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.col, distance, ctx_wr.min_col);
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.col, distance, ctx_wr.max_col);
                        ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                        uint32_t new_viewport_width = war_clamp_subtract_uint32(
                            ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                        if (new_viewport_width < viewport_width) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_width,
                                                          new_viewport_width,
                                                          ctx_wr.min_col);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.right_col, diff, ctx_wr.max_col);
                            if (sum < ctx_wr.max_col) {
                                ctx_wr.right_col = sum;
                            } else {
                                ctx_wr.left_col = war_clamp_subtract_uint32(
                                    ctx_wr.left_col, diff, ctx_wr.min_col);
                            }
                        }
                    }
                    cursor_pos_x_end =
                        war_note_pos_x_end(&note_quads, i_next_note);
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_E: {
                call_carmack("cmd_normal_E");
                note_quads_in_x_count = 0;
                war_note_quads_in_row(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                if (note_quads_in_x_count == 0) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                count = 1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                temp_i = 0;
                float cursor_pos_x_end = war_cursor_pos_x_end(&ctx_wr);
                while (temp_i < count) {
                    i_next_note = -1;
                    float next_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        float next_note_distance_temp =
                            war_note_pos_x_end(&note_quads,
                                               note_quads_in_x[i]) -
                            cursor_pos_x_end;
                        if (next_note_distance_temp < 0.0f) { continue; }
                        if ((next_note_distance < 0.0f &&
                             next_note_distance_temp >= 0.0f) ||
                            (next_note_distance_temp > 0.0f &&
                             next_note_distance_temp <=
                                 next_note_distance + EPS)) {
                            next_note_distance = next_note_distance_temp;
                            i_next_note = note_quads_in_x[i];
                        }
                    }
                    if (i_next_note < 0) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.col = war_clamp_uint32(
                        note_quads.col[i_next_note] +
                            (uint32_t)war_note_span_x(&note_quads, i_next_note),
                        ctx_wr.min_col,
                        ctx_wr.max_col);
                    ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                    ctx_wr.navigation_sub_cells_col =
                        note_quads.sub_cells_col[i_next_note];
                    if (ctx_wr.col > ctx_wr.right_col ||
                        ctx_wr.col < ctx_wr.left_col) {
                        uint32_t viewport_width =
                            ctx_wr.right_col - ctx_wr.left_col;
                        distance = viewport_width / 2;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.col, distance, ctx_wr.min_col);
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.col, distance, ctx_wr.max_col);
                        ctx_wr.sub_col = note_quads.sub_col[i_next_note];
                        uint32_t new_viewport_width = war_clamp_subtract_uint32(
                            ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                        if (new_viewport_width < viewport_width) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_width,
                                                          new_viewport_width,
                                                          ctx_wr.min_col);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.right_col, diff, ctx_wr.max_col);
                            if (sum < ctx_wr.max_col) {
                                ctx_wr.right_col = sum;
                            } else {
                                ctx_wr.left_col = war_clamp_subtract_uint32(
                                    ctx_wr.left_col, diff, ctx_wr.min_col);
                            }
                        }
                    }
                    cursor_pos_x_end =
                        war_note_pos_x_end(&note_quads, i_next_note);
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_b:
                call_carmack("cmd_normal_b");
                note_quads_in_x_count = 0;
                war_note_quads_in_row(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                if (note_quads_in_x_count == 0) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                int i_previous_note = -1;
                count = 1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                temp_i = 0;
                cursor_pos_x = war_cursor_pos_x(&ctx_wr);
                while (temp_i < count) {
                    float previous_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        float previous_note_distance_temp =
                            cursor_pos_x -
                            war_note_pos_x(&note_quads, note_quads_in_x[i]);
                        if (previous_note_distance_temp < 0.0f) { continue; }
                        if ((previous_note_distance < 0.0f &&
                             previous_note_distance_temp > 0.0f) ||
                            (previous_note_distance_temp > 0.0f &&
                             previous_note_distance_temp <
                                 previous_note_distance + EPS)) {
                            previous_note_distance =
                                previous_note_distance_temp;
                            i_previous_note = note_quads_in_x[i];
                        }
                    }
                    if (i_previous_note < 0) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.col =
                        war_clamp_uint32(note_quads.col[i_previous_note],
                                         ctx_wr.min_col,
                                         ctx_wr.max_col);
                    ctx_wr.sub_col = note_quads.sub_col[i_previous_note];
                    ctx_wr.navigation_sub_cells_col =
                        note_quads.sub_cells_col[i_previous_note];
                    if (ctx_wr.col > ctx_wr.right_col ||
                        ctx_wr.col < ctx_wr.left_col) {
                        uint32_t viewport_width =
                            ctx_wr.right_col - ctx_wr.left_col;
                        distance = viewport_width / 2;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.col, distance, ctx_wr.min_col);
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.col, distance, ctx_wr.max_col);
                        ctx_wr.sub_col = note_quads.sub_col[i_previous_note];
                        uint32_t new_viewport_width = war_clamp_subtract_uint32(
                            ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                        if (new_viewport_width < viewport_width) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_width,
                                                          new_viewport_width,
                                                          ctx_wr.min_col);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.right_col, diff, ctx_wr.max_col);
                            if (sum < ctx_wr.max_col) {
                                ctx_wr.right_col = sum;
                            } else {
                                ctx_wr.left_col = war_clamp_subtract_uint32(
                                    ctx_wr.left_col, diff, ctx_wr.min_col);
                            }
                        }
                    }
                    cursor_pos_x = war_note_pos_x(&note_quads, i_previous_note);
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_normal_B: {
                call_carmack("cmd_normal_B");
                note_quads_in_x_count = 0;
                war_note_quads_in_row(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                if (note_quads_in_x_count == 0) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                int i_previous_note = -1;
                count = 1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                temp_i = 0;
                cursor_pos_x = war_cursor_pos_x(&ctx_wr);
                while (temp_i < count) {
                    float previous_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        float previous_note_distance_temp =
                            cursor_pos_x -
                            war_note_pos_x(&note_quads, note_quads_in_x[i]);
                        if (previous_note_distance_temp < 0.0f) { continue; }
                        if ((previous_note_distance <= 0.0f &&
                             previous_note_distance_temp >= 0.0f) ||
                            (previous_note_distance_temp >= 0.0f &&
                             previous_note_distance_temp <=
                                 previous_note_distance + EPS)) {
                            previous_note_distance =
                                previous_note_distance_temp;
                            i_previous_note = note_quads_in_x[i];
                        }
                    }
                    if (i_previous_note < 0) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.col =
                        war_clamp_uint32(note_quads.col[i_previous_note],
                                         ctx_wr.min_col,
                                         ctx_wr.max_col);
                    ctx_wr.col = war_clamp_subtract_uint32(
                        ctx_wr.col, 1, ctx_wr.min_col);
                    ctx_wr.sub_col = note_quads.sub_col[i_previous_note];
                    ctx_wr.navigation_sub_cells_col =
                        note_quads.sub_cells_col[i_previous_note];
                    if (ctx_wr.col > ctx_wr.right_col ||
                        ctx_wr.col < ctx_wr.left_col) {
                        uint32_t viewport_width =
                            ctx_wr.right_col - ctx_wr.left_col;
                        distance = viewport_width / 2;
                        ctx_wr.left_col = war_clamp_subtract_uint32(
                            ctx_wr.col, distance, ctx_wr.min_col);
                        ctx_wr.right_col = war_clamp_add_uint32(
                            ctx_wr.col, distance, ctx_wr.max_col);
                        ctx_wr.sub_col = note_quads.sub_col[i_previous_note];
                        uint32_t new_viewport_width = war_clamp_subtract_uint32(
                            ctx_wr.right_col, ctx_wr.left_col, ctx_wr.min_col);
                        if (new_viewport_width < viewport_width) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_width,
                                                          new_viewport_width,
                                                          ctx_wr.min_col);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.right_col, diff, ctx_wr.max_col);
                            if (sum < ctx_wr.max_col) {
                                ctx_wr.right_col = sum;
                            } else {
                                ctx_wr.left_col = war_clamp_subtract_uint32(
                                    ctx_wr.left_col, diff, ctx_wr.min_col);
                            }
                        }
                    }
                    cursor_pos_x = war_note_pos_x(&note_quads, i_previous_note);
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_u: {
                // TODO REFACTOR: add the blocks to each jump label like above
                call_carmack("cmd_normal_alt_u");
                note_quads_in_x_count = 0;
                war_note_quads_in_col(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                uint32_t cursor_row = ctx_wr.row;
                uint32_t count = 1;
                int i_above = -1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                uint32_t temp_i = 0;
                while (temp_i < count) {
                    float previous_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        uint32_t note_row = note_quads.row[note_quads_in_x[i]];
                        float previous_note_distance_temp =
                            (int)note_row - (int)cursor_row;
                        if (previous_note_distance_temp < 0.0f) { continue; }
                        if ((previous_note_distance < 0.0f &&
                             previous_note_distance_temp > 0.0f) ||
                            (previous_note_distance_temp > 0.0f &&
                             previous_note_distance_temp <
                                 previous_note_distance)) {
                            previous_note_distance =
                                previous_note_distance_temp;
                            i_above = note_quads_in_x[i];
                        }
                    }
                    if (i_above < 0.0f) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.row = war_clamp_uint32(note_quads.row[i_above],
                                                  ctx_wr.min_row,
                                                  ctx_wr.max_row);
                    if (ctx_wr.row > ctx_wr.top_row ||
                        ctx_wr.row < ctx_wr.bottom_row) {
                        uint32_t viewport_height =
                            ctx_wr.top_row - ctx_wr.bottom_row;
                        uint32_t distance = viewport_height / 2;
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.row, distance, ctx_wr.min_row);
                        ctx_wr.top_row = war_clamp_add_uint32(
                            ctx_wr.row, distance, ctx_wr.max_row);
                        uint32_t new_viewport_height =
                            war_clamp_subtract_uint32(ctx_wr.top_row,
                                                      ctx_wr.bottom_row,
                                                      ctx_wr.min_row);
                        if (new_viewport_height < viewport_height) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_height,
                                                          new_viewport_height,
                                                          ctx_wr.min_row);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.top_row, diff, ctx_wr.max_row);
                            if (sum < ctx_wr.max_row) {
                                ctx_wr.top_row = sum;
                            } else {
                                ctx_wr.bottom_row = war_clamp_subtract_uint32(
                                    ctx_wr.bottom_row, diff, ctx_wr.min_row);
                            }
                        }
                    }
                    cursor_row = note_quads.row[i_above];
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_alt_d: {
                call_carmack("cmd_normal_alt_d");
                note_quads_in_x_count = 0;
                war_note_quads_in_col(&note_quads,
                                      note_quads_count,
                                      &ctx_wr,
                                      note_quads_in_x,
                                      &note_quads_in_x_count);
                uint32_t cursor_row = ctx_wr.row;
                uint32_t count = 1;
                int i_above = -1;
                if (ctx_wr.numeric_prefix) { count = ctx_wr.numeric_prefix; }
                uint32_t temp_i = 0;
                while (temp_i < count) {
                    float previous_note_distance = -1.0f;
                    for (uint32_t i = 0; i < note_quads_in_x_count; i++) {
                        uint32_t note_row = note_quads.row[note_quads_in_x[i]];
                        float previous_note_distance_temp =
                            (int)cursor_row - (int)note_row;
                        if (previous_note_distance_temp < 0.0f) { continue; }
                        if ((previous_note_distance < 0.0f &&
                             previous_note_distance_temp > 0.0f) ||
                            (previous_note_distance_temp > 0.0f &&
                             previous_note_distance_temp <
                                 previous_note_distance)) {
                            previous_note_distance =
                                previous_note_distance_temp;
                            i_above = note_quads_in_x[i];
                        }
                    }
                    if (i_above < 0.0f) {
                        ctx_wr.numeric_prefix = 0;
                        memset(ctx_wr.input_sequence,
                               0,
                               sizeof(ctx_wr.input_sequence));
                        ctx_wr.num_chars_in_sequence = 0;
                        goto cmd_done;
                    }
                    ctx_wr.row = war_clamp_uint32(note_quads.row[i_above],
                                                  ctx_wr.min_row,
                                                  ctx_wr.max_row);
                    if (ctx_wr.row > ctx_wr.top_row ||
                        ctx_wr.row < ctx_wr.bottom_row) {
                        uint32_t viewport_height =
                            ctx_wr.top_row - ctx_wr.bottom_row;
                        uint32_t distance = viewport_height / 2;
                        ctx_wr.bottom_row = war_clamp_subtract_uint32(
                            ctx_wr.row, distance, ctx_wr.min_row);
                        ctx_wr.top_row = war_clamp_add_uint32(
                            ctx_wr.row, distance, ctx_wr.max_row);
                        uint32_t new_viewport_height =
                            war_clamp_subtract_uint32(ctx_wr.top_row,
                                                      ctx_wr.bottom_row,
                                                      ctx_wr.min_row);
                        if (new_viewport_height < viewport_height) {
                            uint32_t diff =
                                war_clamp_subtract_uint32(viewport_height,
                                                          new_viewport_height,
                                                          ctx_wr.min_row);
                            uint32_t sum = war_clamp_add_uint32(
                                ctx_wr.top_row, diff, ctx_wr.max_row);
                            if (sum < ctx_wr.max_row) {
                                ctx_wr.top_row = sum;
                            } else {
                                ctx_wr.bottom_row = war_clamp_subtract_uint32(
                                    ctx_wr.bottom_row, diff, ctx_wr.min_row);
                            }
                        }
                    }
                    cursor_row = note_quads.row[i_above];
                    temp_i++;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_tab: {
                call_carmack("cmd_normal_tab");
                switch (ctx_wr.cursor_blink_state) {
                case CURSOR_BLINK:
                    ctx_wr.cursor_blink_state = CURSOR_BLINK_BPM;
                    ctx_wr.cursor_blinking = false;
                    ctx_wr.cursor_blink_previous_us = ctx_wr.now;
                    break;
                case CURSOR_BLINK_BPM:
                    ctx_wr.cursor_blink_state = 0;
                    ctx_wr.cursor_blinking = false;
                    ctx_wr.cursor_blink_previous_us = ctx_wr.now;
                    break;
                case 0:
                    ctx_wr.cursor_blink_state = CURSOR_BLINK;
                    ctx_wr.cursor_blinking = false;
                    ctx_wr.cursor_blink_previous_us = ctx_wr.now;
                    ctx_wr.cursor_blink_duration_us =
                        DEFAULT_CURSOR_BLINK_DURATION;
                    break;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_shift_tab: {
                call_carmack("cmd_normal_shift_tab");
                switch (ctx_wr.hud_state) {
                case HUD_PIANO:
                    ctx_wr.hud_state = HUD_PIANO_AND_LINE_NUMBERS;
                    ctx_wr.num_cols_for_line_numbers = 6;
                    ctx_wr.right_col -= 3;
                    ctx_wr.col =
                        war_clamp_uint32(ctx_wr.col, 0, ctx_wr.right_col);
                    break;
                case HUD_PIANO_AND_LINE_NUMBERS:
                    ctx_wr.hud_state = HUD_LINE_NUMBERS;
                    ctx_wr.num_cols_for_line_numbers = 3;
                    ctx_wr.right_col += 3;
                    ctx_wr.col =
                        war_clamp_uint32(ctx_wr.col, 0, ctx_wr.right_col);
                    break;
                case HUD_LINE_NUMBERS:
                    ctx_wr.hud_state = HUD_PIANO;
                    ctx_wr.num_cols_for_line_numbers = 3;
                    break;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_q: {
                call_carmack("cmd_normal_q");
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_Q: {
                call_carmack("cmd_normal_Q");
                ctx_wr.mode = MODE_RECORD;
                if (atomic_load(&atomics->state) != AUDIO_CMD_RECORD_WAIT &&
                    atomic_load(&atomics->state) != AUDIO_CMD_RECORD) {
                    atomic_store(&atomics->state, AUDIO_CMD_RECORD_WAIT);
                    atomic_store(&atomics->record, 1);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->state, AUDIO_CMD_RECORD_MAP);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_normal_space: {
                call_carmack("cmd_normal_space");
                ctx_wr.mode = MODE_RECORD;
                if (atomic_load(&atomics->state) != AUDIO_CMD_RECORD_WAIT &&
                    atomic_load(&atomics->state) != AUDIO_CMD_RECORD) {
                    atomic_store(&atomics->state, AUDIO_CMD_RECORD_WAIT);
                    atomic_store(&atomics->record, 1);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->state, AUDIO_CMD_RECORD_MAP);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            //------------------------------------------------------------------
            // RECORD COMMANDS
            //------------------------------------------------------------------
            cmd_record_tab: {
                call_carmack("cmd_record_tab");
                atomic_fetch_xor(&atomics->record_monitor, 1);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_K: {
                call_carmack("cmd_record_K");
                float gain =
                    atomic_load(&atomics->play_gain) + ctx_wr.gain_increment;
                gain = fminf(gain, 1.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_J: {
                call_carmack("cmd_record_J");
                float gain =
                    atomic_load(&atomics->play_gain) - ctx_wr.gain_increment;
                gain = fmaxf(gain, 0.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_k: {
                call_carmack("cmd_record_k");
                float gain =
                    atomic_load(&atomics->record_gain) + ctx_wr.gain_increment;
                gain = fminf(gain, 1.0f);
                atomic_store(&atomics->record_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_j: {
                call_carmack("cmd_record_j");
                float gain =
                    atomic_load(&atomics->record_gain) - ctx_wr.gain_increment;
                gain = fmaxf(gain, 0.0f);
                atomic_store(&atomics->record_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_Q: {
                call_carmack("cmd_record_Q");
                atomic_store(&atomics->record, 0);
                atomic_store(&atomics->state, AUDIO_CMD_RECORD_MAP);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_space: {
                call_carmack("cmd_record_space");
                atomic_store(&atomics->record, 0);
                atomic_store(&atomics->state, AUDIO_CMD_RECORD_MAP);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_q: {
                call_carmack("cmd_record_q");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             0 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_w: {
                call_carmack("cmd_record_w");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             1 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_e: {
                call_carmack("cmd_record_e");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             2 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_r: {
                call_carmack("cmd_record_r");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             3 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_t: {
                call_carmack("cmd_record_t");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             4 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_y: {
                call_carmack("cmd_record_y");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             5 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_u: {
                call_carmack("cmd_record_u");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             6 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_i: {
                call_carmack("cmd_record_i");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note,
                             7 + 12 * (ctx_wr.record_octave + 1));
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_o: {
                call_carmack("cmd_record_o");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                float note = 8 + 12 * (ctx_wr.record_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note, note);
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_p: {
                call_carmack("cmd_record_p");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                float note = 9 + 12 * (ctx_wr.record_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note, note);
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_leftbracket: {
                call_carmack("cmd_record_leftbracket");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                float note = 10 + 12 * (ctx_wr.record_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note, note);
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_rightbracket: {
                call_carmack("cmd_record_rightbracket");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                float note = 11 + 12 * (ctx_wr.record_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->map_note, note);
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_record_minus:
                call_carmack("cmd_record_minus");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = -1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_0:
                call_carmack("cmd_record_0");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 0;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_1:
                call_carmack("cmd_record_1");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_2:
                call_carmack("cmd_record_2");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 2;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_3:
                call_carmack("cmd_record_3");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 3;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_4:
                call_carmack("cmd_record_4");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 4;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_5:
                call_carmack("cmd_record_5");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 5;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_6:
                call_carmack("cmd_record_6");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 6;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_7:
                call_carmack("cmd_record_7");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 7;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_8:
                call_carmack("cmd_record_8");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 8;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_9:
                call_carmack("cmd_record_9");
                if (atomic_load(&atomics->record)) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.record_octave = 9;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            cmd_record_esc:
                call_carmack("cmd_record_esc");
                ctx_wr.mode = MODE_NORMAL;
                atomic_store(&atomics->record, 0);
                atomic_store(&atomics->map_note, -1);
                atomic_store(&atomics->state, AUDIO_CMD_STOP);
                war_pc_to_a(pc, AUDIO_CMD_RECORD_MAP, 0, NULL);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            //-----------------------------------------------------------------
            // COMMANDS VIEWS
            //-----------------------------------------------------------------
            cmd_views_k: {
                call_carmack("cmd_views_k");
                uint32_t increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_add_uint32(
                    views.warpoon_row, increment, views.warpoon_max_row);
                if (views.warpoon_row >
                    views.warpoon_top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_add_uint32(views.warpoon_bottom_row,
                                             increment,
                                             views.warpoon_max_row);
                    views.warpoon_top_row =
                        war_clamp_add_uint32(views.warpoon_top_row,
                                             increment,
                                             views.warpoon_max_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_bottom_row =
                            war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                      diff,
                                                      views.warpoon_min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_j: {
                call_carmack("cmd_views_j");
                uint32_t increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_subtract_uint32(
                    views.warpoon_row, increment, views.warpoon_min_row);
                if (views.warpoon_row <
                    views.warpoon_bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    views.warpoon_top_row =
                        war_clamp_subtract_uint32(views.warpoon_top_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_top_row = war_clamp_add_uint32(
                            views.warpoon_top_row, diff, views.warpoon_max_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_h: {
                call_carmack("cmd_views_h");
                if (views.warpoon_mode == MODE_VISUAL_LINE) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                uint32_t increment = ctx_wr.col_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_col);
                }
                views.warpoon_col = war_clamp_subtract_uint32(
                    views.warpoon_col, increment, views.warpoon_min_col);
                if (views.warpoon_col <
                    views.warpoon_left_col + ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    views.warpoon_left_col =
                        war_clamp_subtract_uint32(views.warpoon_left_col,
                                                  increment,
                                                  views.warpoon_min_col);
                    views.warpoon_right_col =
                        war_clamp_subtract_uint32(views.warpoon_right_col,
                                                  increment,
                                                  views.warpoon_min_col);
                    uint32_t new_viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        views.warpoon_right_col =
                            war_clamp_add_uint32(views.warpoon_right_col,
                                                 diff,
                                                 views.warpoon_max_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_l: {
                call_carmack("cmd_views_l");
                if (views.warpoon_mode == MODE_VISUAL_LINE) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                uint32_t increment = ctx_wr.col_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_col);
                }
                views.warpoon_col = war_clamp_add_uint32(
                    views.warpoon_col, increment, views.warpoon_max_col);
                if (views.warpoon_col >
                    views.warpoon_right_col - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    views.warpoon_left_col =
                        war_clamp_add_uint32(views.warpoon_left_col,
                                             increment,
                                             views.warpoon_max_col);
                    views.warpoon_right_col =
                        war_clamp_add_uint32(views.warpoon_right_col,
                                             increment,
                                             views.warpoon_max_col);
                    uint32_t new_viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        views.warpoon_left_col =
                            war_clamp_subtract_uint32(views.warpoon_left_col,
                                                      diff,
                                                      views.warpoon_min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                call_carmack("warpoon col: %u", views.warpoon_col);
                goto cmd_done;
            }
            cmd_views_alt_k: {
                call_carmack("cmd_views_alt_k");
                uint32_t increment = ctx_wr.row_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_add_uint32(
                    views.warpoon_row, increment, views.warpoon_max_row);
                if (views.warpoon_row >
                    views.warpoon_top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_add_uint32(views.warpoon_bottom_row,
                                             increment,
                                             views.warpoon_max_row);
                    views.warpoon_top_row =
                        war_clamp_add_uint32(views.warpoon_top_row,
                                             increment,
                                             views.warpoon_max_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_bottom_row =
                            war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                      diff,
                                                      views.warpoon_min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_alt_j: {
                call_carmack("cmd_views_alt_j");
                uint32_t increment = ctx_wr.row_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_subtract_uint32(
                    views.warpoon_row, increment, views.warpoon_min_row);
                if (views.warpoon_row <
                    views.warpoon_bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    views.warpoon_top_row =
                        war_clamp_subtract_uint32(views.warpoon_top_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_top_row = war_clamp_add_uint32(
                            views.warpoon_top_row, diff, views.warpoon_max_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_alt_h: {
                call_carmack("cmd_views_alt_h");
                if (views.warpoon_mode == MODE_VISUAL_LINE) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                uint32_t increment = ctx_wr.col_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_col);
                }
                views.warpoon_col = war_clamp_subtract_uint32(
                    views.warpoon_col, increment, views.warpoon_min_col);
                if (views.warpoon_col <
                    views.warpoon_left_col + ctx_wr.scroll_margin_cols) {
                    uint32_t viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    views.warpoon_left_col =
                        war_clamp_subtract_uint32(views.warpoon_left_col,
                                                  increment,
                                                  views.warpoon_min_col);
                    views.warpoon_right_col =
                        war_clamp_subtract_uint32(views.warpoon_right_col,
                                                  increment,
                                                  views.warpoon_min_col);
                    uint32_t new_viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        views.warpoon_right_col =
                            war_clamp_add_uint32(views.warpoon_right_col,
                                                 diff,
                                                 views.warpoon_max_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_alt_l: {
                call_carmack("cmd_views_alt_l");
                if (views.warpoon_mode == MODE_VISUAL_LINE) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                uint32_t increment = ctx_wr.col_leap_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_col);
                }
                views.warpoon_col = war_clamp_add_uint32(
                    views.warpoon_col, increment, views.warpoon_max_col);
                if (views.warpoon_col >
                    views.warpoon_right_col - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    views.warpoon_left_col =
                        war_clamp_add_uint32(views.warpoon_left_col,
                                             increment,
                                             views.warpoon_max_col);
                    views.warpoon_right_col =
                        war_clamp_add_uint32(views.warpoon_right_col,
                                             increment,
                                             views.warpoon_max_col);
                    uint32_t new_viewport_width =
                        views.warpoon_right_col - views.warpoon_left_col;
                    if (new_viewport_width < viewport_width) {
                        uint32_t diff = viewport_width - new_viewport_width;
                        views.warpoon_left_col =
                            war_clamp_subtract_uint32(views.warpoon_left_col,
                                                      diff,
                                                      views.warpoon_min_col);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_K: {
                call_carmack("cmd_views_K");
                war_warpoon_shift_up(&views);
                uint32_t increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_add_uint32(
                    views.warpoon_row, increment, views.warpoon_max_row);
                if (views.warpoon_row >
                    views.warpoon_top_row - ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_add_uint32(views.warpoon_bottom_row,
                                             increment,
                                             views.warpoon_max_row);
                    views.warpoon_top_row =
                        war_clamp_add_uint32(views.warpoon_top_row,
                                             increment,
                                             views.warpoon_max_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_bottom_row =
                            war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                      diff,
                                                      views.warpoon_min_row);
                    }
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_J: {
                call_carmack("cmd_views_J");
                war_warpoon_shift_down(&views);
                uint32_t increment = ctx_wr.row_increment;
                if (ctx_wr.numeric_prefix) {
                    increment =
                        war_clamp_multiply_uint32(increment,
                                                  ctx_wr.numeric_prefix,
                                                  views.warpoon_max_row);
                }
                views.warpoon_row = war_clamp_subtract_uint32(
                    views.warpoon_row, increment, views.warpoon_min_row);
                if (views.warpoon_row <
                    views.warpoon_bottom_row + ctx_wr.scroll_margin_rows) {
                    uint32_t viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    views.warpoon_bottom_row =
                        war_clamp_subtract_uint32(views.warpoon_bottom_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    views.warpoon_top_row =
                        war_clamp_subtract_uint32(views.warpoon_top_row,
                                                  increment,
                                                  views.warpoon_min_row);
                    uint32_t new_viewport_height =
                        views.warpoon_top_row - views.warpoon_bottom_row;
                    if (new_viewport_height < viewport_height) {
                        uint32_t diff = viewport_height - new_viewport_height;
                        views.warpoon_top_row = war_clamp_add_uint32(
                            views.warpoon_top_row, diff, views.warpoon_max_row);
                    }
                }

                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_d: {
                call_carmack("cmd_views_d");
                uint32_t i_views = views.warpoon_max_row - views.warpoon_row;
                if (i_views >= views.views_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_warpoon_delete_at_i(&views, i_views);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_V: {
                call_carmack("cmd_views_V");
                switch (views.warpoon_mode) {
                case MODE_NORMAL:
                    views.warpoon_mode = MODE_VISUAL_LINE;
                    views.warpoon_visual_line_row = views.warpoon_row;
                    break;
                case MODE_VISUAL_LINE:
                    views.warpoon_mode = MODE_NORMAL;
                    break;
                }
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_esc: {
                call_carmack("cmd_views_esc");
                if (views.warpoon_mode == MODE_VISUAL_LINE) {
                    views.warpoon_mode = MODE_NORMAL;
                    // logic for undoing the selection
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_z: {
                call_carmack("cmd_views_z");
                ctx_wr.mode = MODE_NORMAL;
                uint32_t i_views = views.warpoon_max_row - views.warpoon_row;
                if (i_views >= views.views_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.col = views.col[i_views];
                ctx_wr.row = views.row[i_views];
                ctx_wr.left_col = views.left_col[i_views];
                ctx_wr.bottom_row = views.bottom_row[i_views];
                ctx_wr.right_col = views.right_col[i_views];
                ctx_wr.top_row = views.top_row[i_views];
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_views_return: {
                call_carmack("cmd_views_return");
                ctx_wr.mode = MODE_NORMAL;
                uint32_t i_views = views.warpoon_max_row - views.warpoon_row;
                if (i_views >= views.views_count) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                ctx_wr.col = views.col[i_views];
                ctx_wr.row = views.row[i_views];
                ctx_wr.left_col = views.left_col[i_views];
                ctx_wr.bottom_row = views.bottom_row[i_views];
                ctx_wr.right_col = views.right_col[i_views];
                ctx_wr.top_row = views.top_row[i_views];
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            // midi
            cmd_midi_m: {
                call_carmack("cmd_midi_m");
                ctx_wr.mode = MODE_NORMAL;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_T: {
                call_carmack("cmd_midi_T");
                ctx_wr.trigger ^= 1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_b: {
                call_carmack("cmd_midi_b");
                ctx_wr.trigger ^= 1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_x: {
                call_carmack("cmd_midi_x");
                war_pc_to_a(pc, AUDIO_CMD_RESET_MAPPINGS, 0, NULL);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_c: {
                call_carmack("cmd_midi_c");
                war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF_ALL, 0, NULL);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_K: {
                call_carmack("cmd_midi_K");
                float gain =
                    atomic_load(&atomics->play_gain) + ctx_wr.gain_increment;
                gain = fminf(gain, 1.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_J: {
                call_carmack("cmd_midi_J");
                float gain =
                    atomic_load(&atomics->play_gain) - ctx_wr.gain_increment;
                gain = fmaxf(gain, 0.0f);
                atomic_store(&atomics->play_gain, gain);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_Q: {
                call_carmack("cmd_midi_Q");
                uint8_t previous = atomic_fetch_xor(&atomics->midi_record, 1);
                if (previous) {
                    atomic_store(&atomics->state, AUDIO_CMD_MIDI_RECORD_MAP);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->state, AUDIO_CMD_MIDI_RECORD_WAIT);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_space: {
                call_carmack("cmd_midi_space");
                uint8_t previous = atomic_fetch_xor(&atomics->midi_record, 1);
                if (previous) {
                    atomic_store(&atomics->state, AUDIO_CMD_MIDI_RECORD_MAP);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                atomic_store(&atomics->state, AUDIO_CMD_MIDI_RECORD_WAIT);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_q: {
                call_carmack("cmd_midi_q");
                int note = 0 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_w: {
                call_carmack("cmd_midi_w");
                int note = 1 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_e: {
                call_carmack("cmd_midi_e");
                int note = 2 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_r: {
                call_carmack("cmd_midi_r");
                int note = 3 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_t: {
                call_carmack("cmd_midi_t");
                int note = 4 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_y: {
                call_carmack("cmd_midi_y");
                int note = 5 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_u: {
                call_carmack("cmd_midi_u");
                int note = 6 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_i: {
                call_carmack("cmd_midi_i");
                int note = 7 + 12 * ((int)ctx_wr.midi_octave + 1);
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_o: {
                call_carmack("cmd_midi_o");
                int note = 8 + 12 * (ctx_wr.midi_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_p: {
                call_carmack("cmd_midi_p");
                int note = 9 + 12 * (ctx_wr.midi_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_leftbracket: {
                call_carmack("cmd_midi_leftbracket");
                int note = 10 + 12 * (ctx_wr.midi_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_rightbracket: {
                call_carmack("cmd_midi_rightbracket");
                int note = 11 + 12 * (ctx_wr.midi_octave + 1);
                if (note > 127) {
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->state) == AUDIO_CMD_MIDI_RECORD_MAP) {
                    atomic_store(&atomics->map_note, note);
                    war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                if (atomic_load(&atomics->notes_on[note])) {
                    war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF, sizeof(note), &note);
                    ctx_wr.numeric_prefix = 0;
                    memset(ctx_wr.input_sequence,
                           0,
                           sizeof(ctx_wr.input_sequence));
                    ctx_wr.num_chars_in_sequence = 0;
                    goto cmd_done;
                }
                war_pc_to_a(pc, AUDIO_CMD_NOTE_ON, sizeof(note), &note);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_l: {
                call_carmack("cmd_midi_l");
                atomic_fetch_xor(&atomics->loop, 1);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_minus: {
                call_carmack("cmd_midi_minus");
                ctx_wr.midi_octave = -1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_esc: {
                call_carmack("cmd_midi_esc");
                ctx_wr.mode = MODE_NORMAL;
                war_pc_to_a(pc, AUDIO_CMD_NOTE_OFF_ALL, 0, NULL);
                atomic_store(&atomics->midi_record, 0);
                atomic_store(&atomics->map_note, -1);
                atomic_store(&atomics->state, AUDIO_CMD_STOP);
                // war_pc_to_a(pc, AUDIO_CMD_MIDI_RECORD_MAP, 0, NULL);
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_0: {
                call_carmack("cmd_midi_0");
                ctx_wr.midi_octave = 0;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_1: {
                call_carmack("cmd_midi_1");
                ctx_wr.midi_octave = 1;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_2: {
                call_carmack("cmd_midi_2");
                ctx_wr.midi_octave = 2;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_3: {
                call_carmack("cmd_midi_3");
                ctx_wr.midi_octave = 3;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_4: {
                call_carmack("cmd_midi_4");
                ctx_wr.midi_octave = 4;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_5: {
                call_carmack("cmd_midi_5");
                ctx_wr.midi_octave = 5;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_6: {
                call_carmack("cmd_midi_6");
                ctx_wr.midi_octave = 6;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_7: {
                call_carmack("cmd_midi_7");
                ctx_wr.midi_octave = 7;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_8: {
                call_carmack("cmd_midi_8");
                ctx_wr.midi_octave = 8;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_midi_9: {
                call_carmack("cmd_midi_9");
                ctx_wr.midi_octave = 9;
                ctx_wr.numeric_prefix = 0;
                memset(ctx_wr.input_sequence, 0, sizeof(ctx_wr.input_sequence));
                ctx_wr.num_chars_in_sequence = 0;
                goto cmd_done;
            }
            cmd_void:
                goto cmd_done;
            cmd_done: {
                ctx_wr.cursor_blink_previous_us = ctx_wr.now;
                ctx_wr.cursor_blinking = false;
                ctx_wr.trinity = true;
                if (goto_cmd_repeat_done) {
                    goto_cmd_repeat_done = false;
                    goto cmd_repeat_done;
                }
                if (goto_cmd_timeout_done) {
                    goto_cmd_timeout_done = false;
                    goto cmd_timeout_done;
                }
                goto wayland_done;
            }
            wl_keyboard_modifiers:
                // dump_bytes("wl_keyboard_modifiers event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
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
                goto wayland_done;
            wl_keyboard_repeat_info:
                dump_bytes("wl_keyboard_repeat_info event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_enter:
                // dump_bytes("wl_pointer_enter event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                goto wayland_done;
            wl_pointer_leave:
                // dump_bytes("wl_pointer_leave event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                goto wayland_done;
            wl_pointer_motion:
                // dump_bytes("wl_pointer_motion event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                ctx_wr.cursor_x = (float)(int32_t)war_read_le32(
                                      msg_buffer + msg_buffer_offset + 12) /
                                  256.0f * scale_factor;
                ctx_wr.cursor_y = (float)(int32_t)war_read_le32(
                                      msg_buffer + msg_buffer_offset + 16) /
                                  256.0f * scale_factor;
                goto wayland_done;
            wl_pointer_button:
                // dump_bytes("wl_pointer_button event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                switch (
                    war_read_le32(msg_buffer + msg_buffer_offset + 8 + 12)) {
                case 1:
                    if (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 8) ==
                        BTN_LEFT) {
                        if (((int)(ctx_wr.cursor_x / ctx_vk.cell_width) -
                             (int)ctx_wr.num_cols_for_line_numbers) < 0) {
                            ctx_wr.col = ctx_wr.left_col;
                            break;
                        }
                        if ((((physical_height - ctx_wr.cursor_y) /
                              ctx_vk.cell_height) -
                             ctx_wr.num_rows_for_status_bars) < 0) {
                            ctx_wr.row = ctx_wr.bottom_row;
                            break;
                        }
                        ctx_wr.col =
                            (uint32_t)(ctx_wr.cursor_x / ctx_vk.cell_width) -
                            ctx_wr.num_cols_for_line_numbers + ctx_wr.left_col;
                        ctx_wr.row =
                            (uint32_t)((physical_height - ctx_wr.cursor_y) /
                                       ctx_vk.cell_height) -
                            ctx_wr.num_rows_for_status_bars + ctx_wr.bottom_row;
                        ctx_wr.cursor_blink_previous_us = ctx_wr.now;
                        ctx_wr.cursor_blinking = false;
                        if (ctx_wr.row > ctx_wr.max_row) {
                            ctx_wr.row = ctx_wr.max_row;
                        }
                        if (ctx_wr.row > ctx_wr.top_row) {
                            ctx_wr.row = ctx_wr.top_row;
                        }
                        if (ctx_wr.row < ctx_wr.bottom_row) {
                            ctx_wr.row = ctx_wr.bottom_row;
                        }
                        if (ctx_wr.col > ctx_wr.max_col) {
                            ctx_wr.col = ctx_wr.max_col;
                        }
                        if (ctx_wr.col > ctx_wr.right_col) {
                            ctx_wr.col = ctx_wr.right_col;
                        }
                        if (ctx_wr.col < ctx_wr.left_col) {
                            ctx_wr.col = ctx_wr.left_col;
                        }
                    }
                }
                goto wayland_done;
            wl_pointer_axis:
                dump_bytes("wl_pointer_axis event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_frame:
                // dump_bytes("wl_pointer_frame event",
                //            msg_buffer + msg_buffer_offset,
                //            size);
                //  war_wayland_holy_trinity(fd,
                //                           wl_surface_id,
                //                           wl_buffer_id,
                //                           0,
                //                           0,
                //                           0,
                //                           0,
                //                           physical_width,
                //                           physical_height);
                goto wayland_done;
            wl_pointer_axis_source:
                dump_bytes("wl_pointer_axis_source event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_axis_stop:
                dump_bytes("wl_pointer_axis_stop event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_axis_discrete:
                dump_bytes("wl_pointer_axis_discrete event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_axis_value120:
                dump_bytes("wl_pointer_axis_value120 event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_pointer_axis_relative_direction:
                dump_bytes("wl_pointer_axis_relative_direction event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_down:
                dump_bytes("wl_touch_down event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_up:
                dump_bytes(
                    "wl_touch_up event", msg_buffer + msg_buffer_offset, size);
                goto wayland_done;
            wl_touch_motion:
                dump_bytes("wl_touch_motion event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_frame:
                dump_bytes("wl_touch_frame event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_cancel:
                dump_bytes("wl_touch_cancel event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_shape:
                dump_bytes("wl_touch_shape event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_touch_orientation:
                dump_bytes("wl_touch_orientation event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_geometry:
                dump_bytes("wl_output_geometry event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_mode:
                dump_bytes("wl_output_mode event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_done:
                dump_bytes("wl_output_done event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_scale:
                dump_bytes("wl_output_scale event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_name:
                dump_bytes("wl_output_name event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wl_output_description:
                dump_bytes("wl_output_description event",
                           msg_buffer + msg_buffer_offset,
                           size);
                goto wayland_done;
            wayland_default:
                dump_bytes(
                    "default event", msg_buffer + msg_buffer_offset, size);
                goto wayland_done;
            wayland_done:
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
    close(ctx_vk.dmabuf_fd);
    ctx_vk.dmabuf_fd = -1;
#endif
    xkb_state_unref(xkb_state);
    xkb_context_unref(xkb_context);
    end("war_window_render");
    return 0;
}
static void war_play(void* userdata) {
    void** data = (void**)userdata;
    war_audio_context* ctx_a = data[0];
    war_producer_consumer* pc = data[1];
    war_atomics* atomics = data[2];
    war_samples* samples = data[3];
    war_samples* record_samples = data[4];
    int16_t* sample_pool = data[5];
    int32_t* record_samples_notes_indices = data[6];

    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_a->play_stream);
    if (!b) return;

    struct spa_buffer* buf = b->buffer;
    if (!buf || !buf->datas[0].data) {
        if (buf && buf->datas[0].maxsize > 0) {
            memset(buf->datas[0].data, 0, buf->datas[0].maxsize);
            if (buf->datas[0].chunk) {
                buf->datas[0].chunk->offset = 0;
                buf->datas[0].chunk->stride =
                    sizeof(int16_t) * ctx_a->channel_count;
                buf->datas[0].chunk->size = buf->datas[0].maxsize;
            }
        }
        pw_stream_queue_buffer(ctx_a->play_stream, b);
        return;
    }

    int16_t* dst = (int16_t*)buf->datas[0].data;
    size_t stride = sizeof(int16_t) * ctx_a->channel_count;
    uint32_t n_frames = buf->datas[0].maxsize / stride;
    if (b->requested) n_frames = SPA_MIN(b->requested, n_frames);
    memset(dst, 0, n_frames * stride);

    float gain = atomic_load(&atomics->play_gain);
    uint64_t global_frame = atomic_load(&atomics->play_clock);
    uint8_t midi_record = atomic_load(&atomics->midi_record);
    uint8_t play = atomic_load(&atomics->play);

    // -------------------------------------------------------------------------
    // Iterate over all notes
    // Inside the main note loop
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        uint8_t note_on = atomic_load(&atomics->notes_on[note]);
        uint8_t prev_state = atomic_load(&atomics->notes_on_previous[note]);

        // --- Start note timing
        if (note_on && !prev_state) {
            samples->notes_frames_start[note] = global_frame;
        }

        // --- MIDI recording: note just started
        if (midi_record && note_on && !prev_state) {
            war_pc_to_wr(pc, AUDIO_CMD_MIDI_RECORD, 0, NULL);
            uint64_t record_start = atomic_load(&atomics->midi_record_frames);

            for (uint32_t s = 0; s < samples->samples_count[note]; s++) {
                uint32_t rec_i = record_samples->samples_count[0];
                if (rec_i >= MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE) break;

                int samples_i = note * MAX_SAMPLES_PER_NOTE + s;

                record_samples->samples[rec_i] = samples->samples[samples_i];
                record_samples->samples_frames_start[rec_i] = record_start;
                record_samples->samples_frames[rec_i] = 0;

                record_samples->samples_attack[rec_i] =
                    samples->samples_attack[samples_i];
                record_samples->samples_sustain[rec_i] =
                    samples->samples_sustain[samples_i];
                record_samples->samples_release[rec_i] =
                    samples->samples_release[samples_i];
                record_samples->samples_gain[rec_i] =
                    samples->samples_gain[samples_i];

                record_samples->notes_attack[0] = samples->notes_attack[note];
                record_samples->notes_sustain[0] = samples->notes_sustain[note];
                record_samples->notes_release[0] = samples->notes_release[note];
                record_samples->notes_gain[0] = samples->notes_gain[note];

                record_samples->samples_count[0] = rec_i + 1;

                // Store the recording index for this note
                record_samples_notes_indices[note] = rec_i;
            }
        }

        // --- MIDI recording: note just released
        if (midi_record && !note_on && prev_state) {
            int32_t rec_i = record_samples_notes_indices[note];
            if (rec_i >= 0 &&
                rec_i < (int32_t)(MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE)) {
                record_samples->samples_frames_duration[rec_i] =
                    atomic_load(&atomics->midi_record_frames) -
                    record_samples->samples_frames_start[rec_i];

                // Reset note index
                record_samples_notes_indices[note] = -1;
            }
        }

        // --- Skip notes that are off or have no samples
        if (!note_on || samples->samples_count[note] == 0) { continue; }
        atomic_store(&atomics->notes_on_previous[note], 1);

        uint64_t note_start = samples->notes_frames_start[note];
        uint64_t note_duration = samples->notes_frames_duration[note];
        uint8_t loop_note = atomic_load(&atomics->loop);

        // --- Mix each sample
        for (uint32_t f = 0; f < n_frames; f++) {
            uint64_t global_pos = global_frame + f;
            uint64_t note_elapsed = global_pos - note_start;

            if (loop_note && note_duration > 0) {
                note_elapsed %= note_duration;
            }

            for (uint32_t s = 0; s < samples->samples_count[note]; s++) {
                int samples_i = note * MAX_SAMPLES_PER_NOTE + s;
                int16_t* sample_ptr = samples->samples[samples_i];
                if (!sample_ptr) continue;

                uint64_t sample_start =
                    samples->samples_frames_start[samples_i];
                uint64_t sample_duration =
                    samples->samples_frames_duration[samples_i];

                if (note_elapsed < sample_start ||
                    note_elapsed >= sample_start + sample_duration)
                    continue;

                uint64_t sample_phase = note_elapsed - sample_start;

                for (uint32_t c = 0; c < ctx_a->channel_count; c++) {
                    uint64_t idx = sample_phase * ctx_a->channel_count + c;
                    int32_t mixed = dst[f * ctx_a->channel_count + c] +
                                    (int32_t)(sample_ptr[idx] * gain);
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    dst[f * ctx_a->channel_count + c] = (int16_t)mixed;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Queue buffer
    if (buf->datas[0].chunk) {
        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = stride;
        buf->datas[0].chunk->size = n_frames * stride;
    }
    pw_stream_queue_buffer(ctx_a->play_stream, b);

    // -------------------------------------------------------------------------
    // Frame counters
    atomic_fetch_add(&atomics->play_clock, n_frames);
    if (play) { atomic_fetch_add(&atomics->play_frames, n_frames); }
    if (midi_record && record_samples->samples_count[0] > 0) {
        atomic_fetch_add(&atomics->midi_record_frames, n_frames);
    } else if (!midi_record) {
        atomic_store(&atomics->midi_record_frames, 0);
    }
}
static void war_record(void* userdata) {
    void** data = (void**)userdata;
    war_audio_context* ctx_a = data[0];
    war_producer_consumer* pc = data[1];
    war_atomics* atomics = data[2];
    war_samples* samples = data[3];
    war_samples* record_samples = data[4];
    int16_t* sample_pool = data[5];
    int32_t* record_samples_notes_indices = data[6];
    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_a->record_stream);
    if (!b) return;
    struct spa_buffer* buf = b->buffer;
    if (!buf || !buf->datas[0].data) {
        pw_stream_queue_buffer(ctx_a->record_stream, b);
        return;
    }
    size_t stride = sizeof(int16_t) * ctx_a->channel_count;
    uint32_t n_frames_in = buf->datas[0].chunk->size / stride;
    int16_t* src = (int16_t*)buf->datas[0].data;
    if (!atomic_load(&atomics->record)) {
        ctx_a->over_threshold = 0;
        ctx_a->warmup_frames =
            ctx_a->sample_rate / AUDIO_DEFAULT_WARMUP_FRAMES_FACTOR;
        pw_stream_queue_buffer(ctx_a->record_stream, b);
        return;
    }
    if (ctx_a->warmup_frames > 0) {
        uint32_t frames_to_consume =
            SPA_MIN(n_frames_in, (uint32_t)ctx_a->warmup_frames);
        ctx_a->warmup_frames -= frames_to_consume;
        pw_stream_queue_buffer(ctx_a->record_stream, b);
        return;
    }
    uint64_t total_samples = (uint64_t)n_frames_in * ctx_a->channel_count;
    float threshold = atomic_load(&atomics->record_threshold);
    float sum_sq = 0.0f;
    for (uint64_t i = 0; i < total_samples; i++) {
        float s = (float)src[i] / 32767.0f;
        sum_sq += s * s;
    }
    float rms = sqrtf(sum_sq / total_samples);
    if (rms >= threshold && !ctx_a->over_threshold) {
        ctx_a->over_threshold = 1;
        war_pc_to_wr(pc, AUDIO_CMD_RECORD, 0, NULL);
    }
    if (!ctx_a->over_threshold) {
        pw_stream_queue_buffer(ctx_a->record_stream, b);
        return;
    }
    uint64_t frame_offset =
        atomic_fetch_add(&atomics->record_frames, n_frames_in);
    uint64_t buffer_frames =
        (ctx_a->sample_rate * ctx_a->sample_duration_seconds);
    uint64_t start_frame = frame_offset % buffer_frames;
    float gain = atomic_load(&atomics->record_gain);
    uint64_t first_part = buffer_frames - start_frame;
    if (first_part > n_frames_in) first_part = n_frames_in;
    uint64_t second_part = n_frames_in - first_part;
    for (uint64_t i = 0; i < first_part * ctx_a->channel_count; i++) {
        ctx_a->record_buffer[start_frame * ctx_a->channel_count + i] =
            (int16_t)(src[i] * gain);
    }
    for (uint64_t i = 0; i < second_part * ctx_a->channel_count; i++) {
        ctx_a->record_buffer[i] =
            (int16_t)(src[first_part * ctx_a->channel_count + i] * gain);
    }
    if (atomic_load(&atomics->record_monitor) && ctx_a->play_stream) {
        struct pw_buffer* out_b = pw_stream_dequeue_buffer(ctx_a->play_stream);
        if (out_b && out_b->buffer && out_b->buffer->datas[0].data) {
            uint32_t max_frames_out = out_b->buffer->datas[0].maxsize / stride;
            uint32_t copy_frames = SPA_MIN(n_frames_in, max_frames_out);
            memcpy(out_b->buffer->datas[0].data, src, copy_frames * stride);
            if (out_b->buffer->datas[0].chunk) {
                out_b->buffer->datas[0].chunk->offset = 0;
                out_b->buffer->datas[0].chunk->stride = stride;
                out_b->buffer->datas[0].chunk->size = copy_frames * stride;
            }
            pw_stream_queue_buffer(ctx_a->play_stream, out_b);
        }
    }
    pw_stream_queue_buffer(ctx_a->record_stream, b);
}
//-----------------------------------------------------------------------------
// THREAD AUDIO
//-----------------------------------------------------------------------------
void* war_audio(void* args) {
    header("war_audio");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];
    atomics->notes_on = malloc(sizeof(uint8_t) * MAX_MIDI_NOTES);
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        atomic_init(&atomics->notes_on[i], 0);
    }
    atomics->notes_on_previous = malloc(sizeof(uint8_t) * MAX_MIDI_NOTES);
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        atomic_init(&atomics->notes_on_previous[i], 0);
    }
    struct sched_param param = {.sched_priority = 10};
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        call_carmack("AUDIO THREAD ERROR WITH SCHEDULING FIFO");
        perror("pthread_setschedparam");
    }
    //-------------------------------------------------------------------------
    // AUDIO CONTEXT
    //-------------------------------------------------------------------------
    war_audio_context* ctx_a = malloc(sizeof(war_audio_context));
    assert(ctx_a);
    ctx_a->sample_frames = malloc(MAX_MIDI_NOTES * sizeof(uint64_t));
    memset(ctx_a->sample_frames, 0, sizeof(uint64_t) * MAX_MIDI_NOTES);
    assert(ctx_a->sample_frames);
    ctx_a->sample_frames_duration = malloc(MAX_MIDI_NOTES * sizeof(uint64_t));
    memset(ctx_a->sample_frames_duration, 0, sizeof(uint64_t) * MAX_MIDI_NOTES);
    assert(ctx_a->sample_frames_duration);
    ctx_a->sample_phase = malloc(MAX_MIDI_NOTES * sizeof(float));
    memset(ctx_a->sample_phase, 0, sizeof(float) * MAX_MIDI_NOTES);
    assert(ctx_a->sample_phase);
    ctx_a->sample_rate = AUDIO_DEFAULT_SAMPLE_RATE;
    ctx_a->period_size = AUDIO_DEFAULT_PERIOD_SIZE;
    ctx_a->sub_period_size =
        AUDIO_DEFAULT_PERIOD_SIZE / AUDIO_DEFAULT_SUB_PERIOD_FACTOR;
    ctx_a->BPM = AUDIO_DEFAULT_BPM;
    ctx_a->channel_count = AUDIO_DEFAULT_CHANNEL_COUNT;
    ctx_a->phase = 0.0f;
    ctx_a->sample_duration_seconds = AUDIO_DEFAULT_SAMPLE_DURATION;
    ctx_a->over_threshold = 0;
    ctx_a->record_buffer =
        malloc(ctx_a->sample_rate * ctx_a->sample_duration_seconds *
               ctx_a->channel_count * sizeof(int16_t));
    memset(ctx_a->record_buffer,
           0,
           ctx_a->sample_rate * ctx_a->sample_duration_seconds *
               ctx_a->channel_count * sizeof(int16_t));
    assert(ctx_a->record_buffer);
    ctx_a->warmup_frames = 0;
    ctx_a->default_attack = 0.0f;
    ctx_a->default_sustain = 1.0f;
    ctx_a->default_release = 0.0f;
    ctx_a->default_gain = 1.0f;
    ctx_a->resample_buffer =
        malloc(ctx_a->sample_rate * ctx_a->sample_duration_seconds *
               ctx_a->channel_count * sizeof(int16_t));
    memset(ctx_a->resample_buffer,
           0,
           ctx_a->sample_rate * ctx_a->sample_duration_seconds *
               ctx_a->channel_count * sizeof(int16_t));
    assert(ctx_a->resample_buffer);
    //-------------------------------------------------------------------------
    // SAMPLE POOL ~338 MB.
    //-------------------------------------------------------------------------
    int16_t* sample_pool = malloc(MAX_MIDI_NOTES * ctx_a->sample_rate *
                                  ctx_a->sample_duration_seconds *
                                  ctx_a->channel_count * sizeof(int16_t));
    memset(sample_pool,
           0,
           MAX_MIDI_NOTES * ctx_a->sample_rate *
               ctx_a->sample_duration_seconds * ctx_a->channel_count *
               sizeof(int16_t));
    assert(sample_pool);
    //-------------------------------------------------------------------------
    // SAMPLES ~1 MB.
    //-------------------------------------------------------------------------
    war_samples* samples;
    samples = malloc(sizeof(war_samples));
    assert(samples);
    samples->samples =
        malloc(sizeof(int16_t*) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_frames_start =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_frames_duration =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_frames =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_attack =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_sustain =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_release =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->samples_gain =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    samples->notes_attack = malloc(sizeof(float) * MAX_MIDI_NOTES);
    samples->notes_sustain = malloc(sizeof(float) * MAX_MIDI_NOTES);
    samples->notes_release = malloc(sizeof(float) * MAX_MIDI_NOTES);
    samples->notes_gain = malloc(sizeof(float) * MAX_MIDI_NOTES);
    samples->notes_frames_start = malloc(sizeof(uint64_t) * MAX_MIDI_NOTES);
    samples->notes_frames_duration = malloc(sizeof(uint64_t) * MAX_MIDI_NOTES);
    samples->samples_count = malloc(sizeof(uint32_t) * MAX_MIDI_NOTES);
    memset(samples->samples,
           0,
           sizeof(int16_t*) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_frames_start,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_frames_duration,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_frames,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_attack,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_sustain,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_release,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->samples_gain,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(samples->notes_attack, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(samples->notes_sustain, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(samples->notes_release, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(samples->notes_gain, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(samples->notes_frames_start, 0, sizeof(uint64_t) * MAX_MIDI_NOTES);
    memset(
        samples->notes_frames_duration, 0, sizeof(uint64_t) * MAX_MIDI_NOTES);
    memset(samples->samples_count, 0, sizeof(uint32_t) * MAX_MIDI_NOTES);
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        samples->notes_attack[i] = ctx_a->default_attack;
        samples->notes_sustain[i] = ctx_a->default_sustain;
        samples->notes_release[i] = ctx_a->default_release;
        samples->notes_gain[i] = ctx_a->default_gain;
        samples->notes_frames_start[i] = 0;
        samples->notes_frames_duration[i] = 0;
        for (int k = 0; k < MAX_SAMPLES_PER_NOTE; k++) {
            int samples_i = i * MAX_SAMPLES_PER_NOTE + k;
            samples->samples_attack[samples_i] = ctx_a->default_attack;
            samples->samples_sustain[samples_i] = ctx_a->default_sustain;
            samples->samples_release[samples_i] = ctx_a->default_release;
            samples->samples_gain[samples_i] = ctx_a->default_gain;
        }
    }
    war_samples* record_samples = malloc(sizeof(war_samples));
    assert(record_samples);
    record_samples->samples =
        malloc(sizeof(int16_t*) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_frames_start =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_frames_duration =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_frames =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_attack =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_sustain =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_release =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->samples_gain =
        malloc(sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    record_samples->notes_attack = malloc(sizeof(float) * MAX_MIDI_NOTES);
    record_samples->notes_sustain = malloc(sizeof(float) * MAX_MIDI_NOTES);
    record_samples->notes_release = malloc(sizeof(float) * MAX_MIDI_NOTES);
    record_samples->notes_gain = malloc(sizeof(float) * MAX_MIDI_NOTES);
    record_samples->notes_frames_start =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES);
    record_samples->notes_frames_duration =
        malloc(sizeof(uint64_t) * MAX_MIDI_NOTES);
    record_samples->samples_count = malloc(sizeof(uint32_t) * MAX_MIDI_NOTES);
    memset(record_samples->samples,
           0,
           sizeof(int16_t*) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_frames_start,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_frames_duration,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_frames,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_attack,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_sustain,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_release,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->samples_gain,
           0,
           sizeof(float) * MAX_MIDI_NOTES * MAX_SAMPLES_PER_NOTE);
    memset(record_samples->notes_attack, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(record_samples->notes_sustain, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(record_samples->notes_release, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(record_samples->notes_gain, 0, sizeof(float) * MAX_MIDI_NOTES);
    memset(record_samples->notes_frames_start,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES);
    memset(record_samples->notes_frames_duration,
           0,
           sizeof(uint64_t) * MAX_MIDI_NOTES);
    memset(record_samples->samples_count, 0, sizeof(uint32_t) * MAX_MIDI_NOTES);
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        record_samples->notes_attack[i] = ctx_a->default_attack;
        record_samples->notes_sustain[i] = ctx_a->default_sustain;
        record_samples->notes_release[i] = ctx_a->default_release;
        record_samples->notes_gain[i] = ctx_a->default_gain;
        record_samples->notes_frames_start[i] = 0;
        record_samples->notes_frames_duration[i] = 0;
        for (int k = 0; k < MAX_SAMPLES_PER_NOTE; k++) {
            int samples_i = i * MAX_SAMPLES_PER_NOTE + k;
            record_samples->samples_attack[samples_i] = ctx_a->default_attack;
            record_samples->samples_sustain[samples_i] = ctx_a->default_sustain;
            record_samples->samples_release[samples_i] = ctx_a->default_release;
            record_samples->samples_gain[samples_i] = ctx_a->default_gain;
        }
    }
    //-------------------------------------------------------------------------
    // SINE PER NOTE FOR TESTING
    //-------------------------------------------------------------------------
    float sine_table[AUDIO_SINE_TABLE_SIZE];
    for (int i = 0; i < AUDIO_SINE_TABLE_SIZE; i++) {
        sine_table[i] = sinf(2.0f * M_PI * i / AUDIO_SINE_TABLE_SIZE);
    }
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        int16_t* note_sample =
            sample_pool + note * ctx_a->sample_rate *
                              ctx_a->sample_duration_seconds *
                              ctx_a->channel_count;
        float freq = 440.0f * powf(2.0f, (note - 69) / 12.0f); // MIDI to Hz
        uint32_t n_samples =
            ctx_a->sample_rate * ctx_a->sample_duration_seconds;
        float table_phase_inc =
            (freq * AUDIO_SINE_TABLE_SIZE) / ctx_a->sample_rate;
        float table_phase = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            int index = (int)table_phase % AUDIO_SINE_TABLE_SIZE;
            int16_t sample = (int16_t)(sine_table[index] * 3000);
            for (uint64_t c = 0; c < ctx_a->channel_count; c++) {
                note_sample[i * ctx_a->channel_count + c] = sample;
            }
            table_phase += table_phase_inc;
            if (table_phase >= AUDIO_SINE_TABLE_SIZE)
                table_phase -= AUDIO_SINE_TABLE_SIZE;
        }
        int samples_i = note * MAX_SAMPLES_PER_NOTE + 0;
        samples->samples[samples_i] = note_sample;
        samples->samples_frames_duration[samples_i] = n_samples;
        samples->samples_frames_start[samples_i] = 0;
        samples->samples_frames[samples_i] = 0;
        samples->samples_count[note] = 1;
        samples->notes_frames_start[note] = 0;
        samples->notes_frames_duration[note] = n_samples;
    }
    // TEST FOR MULTIPLE SAMPLES PER NOTE AT NOTE 60
    // samples->samples[60 * MAX_SAMPLES_PER_NOTE + 1] =
    //     samples->samples[64 * MAX_SAMPLES_PER_NOTE + 0];
    // samples->samples_frames_duration[60 * MAX_SAMPLES_PER_NOTE + 1] =
    //     ctx_a->sample_rate * ctx_a->sample_duration_seconds;
    // samples->samples_frames_start[60 * MAX_SAMPLES_PER_NOTE + 1] = 0;
    // samples->samples_frames[60 * MAX_SAMPLES_PER_NOTE + 1] = 0;
    // samples->samples_count[60] = 2;
    int32_t* record_samples_notes_indices =
        malloc(sizeof(uint32_t) * MAX_MIDI_NOTES);
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        record_samples_notes_indices[i] = -1;
    }
    assert(record_samples_notes_indices);
    void** userdata = malloc(sizeof(void*) * 6);
    assert(userdata);
    userdata[0] = ctx_a;
    userdata[1] = pc;
    userdata[2] = atomics;
    userdata[3] = samples;
    userdata[4] = record_samples;
    userdata[5] = sample_pool;
    userdata[6] = record_samples_notes_indices;
    //-------------------------------------------------------------------------
    // PIPEWIRE
    //-------------------------------------------------------------------------
    pw_init(NULL, NULL);
    ctx_a->pw_loop = pw_loop_new(NULL);
    struct spa_audio_info_raw play_info = {
        .format = SPA_AUDIO_FORMAT_S16,
        .rate = ctx_a->sample_rate,
        .channels = ctx_a->channel_count,
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    uint8_t play_buffer[1024];
    struct spa_pod_builder play_builder;
    spa_pod_builder_init(&play_builder, play_buffer, sizeof(play_buffer));
    const struct spa_pod* play_params[1];
    play_params[0] = spa_format_audio_raw_build(
        &play_builder, SPA_PARAM_EnumFormat, &play_info);
    struct pw_stream_events play_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_play,
    };
    ctx_a->play_stream = pw_stream_new_simple(
        ctx_a->pw_loop, "WAR_play", NULL, &play_events, userdata);
    uint32_t sink_id = PW_ID_ANY;
    pw_stream_connect(ctx_a->play_stream,
                      PW_DIRECTION_OUTPUT,
                      sink_id,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                      play_params,
                      1);
    struct spa_audio_info_raw record_info = {
        .format = SPA_AUDIO_FORMAT_S16,
        .rate = ctx_a->sample_rate,
        .channels = ctx_a->channel_count,
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    uint8_t record_buffer[1024];
    struct spa_pod_builder record_builder;
    spa_pod_builder_init(&record_builder, record_buffer, sizeof(record_buffer));
    const struct spa_pod* record_params[1];
    record_params[0] = spa_format_audio_raw_build(
        &record_builder, SPA_PARAM_EnumFormat, &record_info);
    struct pw_stream_events record_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_record,
    };
    ctx_a->record_stream = pw_stream_new_simple(
        ctx_a->pw_loop, "WAR_record", NULL, &record_events, userdata);
    uint32_t source_id = PW_ID_ANY;
    pw_stream_connect(ctx_a->record_stream,
                      PW_DIRECTION_INPUT,
                      source_id,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                      record_params,
                      1);
    while (pw_stream_get_state(ctx_a->record_stream, NULL) !=
           PW_STREAM_STATE_PAUSED) {
        pw_loop_iterate(ctx_a->pw_loop, 1);
        struct timespec ts = {0, 500000};
        nanosleep(&ts, NULL);
    }
    //-------------------------------------------------------------------------
    // PC AUDIO
    //-------------------------------------------------------------------------
    uint32_t header;
    uint32_t size;
    uint8_t payload[PC_BUFFER_SIZE];
    void* pc_audio[AUDIO_CMD_COUNT];
    pc_audio[AUDIO_CMD_STOP] = &&pc_stop;
    pc_audio[AUDIO_CMD_PLAY] = &&pc_play;
    pc_audio[AUDIO_CMD_PAUSE] = &&pc_pause;
    pc_audio[AUDIO_CMD_GET_FRAMES] = &&pc_get_frames;
    pc_audio[AUDIO_CMD_ADD_NOTE] = &&pc_add_note;
    pc_audio[AUDIO_CMD_END_WAR] = &&pc_end_war;
    pc_audio[AUDIO_CMD_SEEK] = &&pc_seek;
    pc_audio[AUDIO_CMD_RECORD_WAIT] = &&pc_record_wait;
    pc_audio[AUDIO_CMD_RECORD] = &&pc_record;
    pc_audio[AUDIO_CMD_RECORD_MAP] = &&pc_record_map;
    pc_audio[AUDIO_CMD_SET_THRESHOLD] = &&pc_set_threshold;
    pc_audio[AUDIO_CMD_NOTE_ON] = &&pc_note_on;
    pc_audio[AUDIO_CMD_NOTE_OFF] = &&pc_note_off;
    pc_audio[AUDIO_CMD_NOTE_OFF_ALL] = &&pc_note_off_all;
    pc_audio[AUDIO_CMD_RESET_MAPPINGS] = &&pc_reset_mappings;
    pc_audio[AUDIO_CMD_MIDI_RECORD] = &&pc_midi_record;
    pc_audio[AUDIO_CMD_MIDI_RECORD_MAP] = &&pc_midi_record_map;
    pc_audio[AUDIO_CMD_MIDI_RECORD_WAIT] = &&pc_midi_record_wait;
    atomic_store(&atomics->start_war, 1);
    struct timespec ts = {0, 500000}; // 0.5 ms
pc_audio:
    if (war_pc_from_wr(pc, &header, &size, payload)) { goto* pc_audio[header]; }
    goto pc_audio_done;
pc_stop:
    goto pc_audio_done;
pc_play:
    goto pc_audio_done;
pc_pause:
    goto pc_audio_done;
pc_get_frames:
    goto pc_audio_done;
pc_add_note:
    goto pc_audio_done;
pc_end_war:
    goto pc_audio_done;
pc_seek:
    uint64_t seek_frames;
    memcpy(&seek_frames, payload, size);
    atomic_store(&atomics->play_frames, seek_frames);
    goto pc_audio_done;
pc_record_wait:
    goto pc_audio_done;
pc_record:
    goto pc_audio_done;
pc_record_map:
    int16_t map_note = atomic_exchange(&atomics->map_note, -1);
    ctx_a->over_threshold = 0;
    if (map_note == -1) {
        war_pc_to_wr(pc, AUDIO_CMD_STOP, 0, NULL);
        memset(ctx_a->record_buffer,
               0,
               ctx_a->sample_rate * ctx_a->sample_duration_seconds *
                   ctx_a->channel_count * sizeof(int16_t));
        goto pc_audio_done;
    }
    int16_t* note_sample = sample_pool + map_note * ctx_a->sample_rate *
                                             ctx_a->sample_duration_seconds *
                                             ctx_a->channel_count;
    uint64_t record_frames = atomic_exchange(&atomics->record_frames, 0);
    if (record_frames > ctx_a->sample_rate * ctx_a->sample_duration_seconds) {
        record_frames = ctx_a->sample_rate * ctx_a->sample_duration_seconds;
    }
    memcpy(note_sample,
           ctx_a->record_buffer,
           record_frames * ctx_a->channel_count * sizeof(int16_t));
    samples->samples[map_note * MAX_SAMPLES_PER_NOTE + 0] = note_sample;
    samples->samples_frames_duration[map_note * MAX_SAMPLES_PER_NOTE + 0] =
        record_frames;
    samples->samples_frames_start[map_note * MAX_SAMPLES_PER_NOTE + 0] = 0;
    samples->samples_count[map_note] = 1;
    samples->notes_frames_start[map_note] = 0;
    samples->notes_frames_duration[map_note] = record_frames;
    war_pc_to_wr(pc, AUDIO_CMD_STOP, 0, NULL);
    memset(ctx_a->record_buffer,
           0,
           ctx_a->sample_rate * ctx_a->sample_duration_seconds *
               ctx_a->channel_count * sizeof(int16_t));
    goto pc_audio_done;
pc_set_threshold:
    goto pc_audio_done;
pc_note_on: {
    int note;
    memcpy(&note, payload, size);
    atomic_store(&atomics->notes_on_previous[note],
                 atomic_load(&atomics->notes_on[note]));
    atomic_store(&atomics->notes_on[note], 1);
    goto pc_audio_done;
}
pc_note_off: {
    int note;
    memcpy(&note, payload, size);
    atomic_store(&atomics->notes_on_previous[note],
                 atomic_load(&atomics->notes_on[note]));
    atomic_store(&atomics->notes_on[note], 0);
    goto pc_audio_done;
}
pc_note_off_all: {
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        atomic_store(&atomics->notes_on_previous[i],
                     atomic_load(&atomics->notes_on[i]));
        atomic_store(&atomics->notes_on[i], 0);
    }
    goto pc_audio_done;
}
pc_reset_mappings:
    for (int note = 0; note < MAX_MIDI_NOTES; note++) {
        int16_t* note_sample =
            sample_pool + note * ctx_a->sample_rate *
                              ctx_a->sample_duration_seconds *
                              ctx_a->channel_count;
        float freq = 440.0f * powf(2.0f, (note - 69) / 12.0f); // MIDI to Hz
        uint32_t n_samples =
            ctx_a->sample_rate * ctx_a->sample_duration_seconds;
        float table_phase_inc =
            (freq * AUDIO_SINE_TABLE_SIZE) / ctx_a->sample_rate;
        float table_phase = 0.0f;
        for (uint32_t i = 0; i < n_samples; i++) {
            int index = (int)table_phase % AUDIO_SINE_TABLE_SIZE;
            int16_t sample = (int16_t)(sine_table[index] * 3000);
            for (uint64_t c = 0; c < ctx_a->channel_count; c++) {
                note_sample[i * ctx_a->channel_count + c] = sample;
            }
            table_phase += table_phase_inc;
            if (table_phase >= AUDIO_SINE_TABLE_SIZE)
                table_phase -= AUDIO_SINE_TABLE_SIZE;
        }
        int samples_i = note * MAX_SAMPLES_PER_NOTE + 0;
        samples->samples[samples_i] = note_sample;
        samples->samples_frames_duration[samples_i] = n_samples;
        samples->samples_frames_start[samples_i] = 0;
        samples->samples_frames[samples_i] = 0;
        samples->samples_count[note] = 1;
    }
    goto pc_audio_done;
pc_midi_record: { goto pc_audio_done; }
pc_midi_record_wait: { goto pc_audio_done; }
pc_midi_record_map: {
    int16_t map_note = atomic_exchange(&atomics->map_note, -1);
    if (map_note == -1) {
        war_pc_to_wr(pc, AUDIO_CMD_STOP, 0, NULL);
        record_samples->samples_count[0] = 0;
        goto pc_audio_done;
    }
    uint64_t notes_frames_duration = 0;
    for (uint32_t i = 0; i < record_samples->samples_count[0]; i++) {
        int samples_i = map_note * MAX_SAMPLES_PER_NOTE + i;
        samples->samples[samples_i] = record_samples->samples[i];
        samples->samples_frames_start[samples_i] =
            record_samples->samples_frames_start[i];
        samples->samples_frames_duration[samples_i] =
            record_samples->samples_frames_duration[i];
        samples->samples_frames[samples_i] = 0;
        samples->samples_attack[samples_i] = record_samples->samples_attack[i];
        samples->samples_sustain[samples_i] =
            record_samples->samples_sustain[i];
        samples->samples_release[samples_i] =
            record_samples->samples_release[i];
        samples->samples_gain[samples_i] = record_samples->samples_gain[i];
        if (samples->samples_frames_start[samples_i] +
                samples->samples_frames_duration[samples_i] >=
            notes_frames_duration) {
            notes_frames_duration = samples->samples_frames_start[samples_i] +
                                    samples->samples_frames_duration[samples_i];
        }
    }
    samples->notes_attack[map_note] = record_samples->notes_attack[0];
    samples->notes_sustain[map_note] = record_samples->notes_sustain[0];
    samples->notes_release[map_note] = record_samples->notes_release[0];
    samples->notes_gain[map_note] = record_samples->notes_gain[0];
    samples->notes_frames_start[map_note] = 0;
    samples->notes_frames_duration[map_note] = notes_frames_duration;
    samples->samples_count[map_note] = record_samples->samples_count[0];
    record_samples->samples_count[0] = 0;
    for (int i = 0; i < MAX_MIDI_NOTES; i++) {
        record_samples_notes_indices[i] = -1;
    }
    war_pc_to_wr(pc, AUDIO_CMD_STOP, 0, NULL);
    goto pc_audio_done;
}
pc_audio_done:
    switch (atomics->state) {
    case AUDIO_CMD_END_WAR: {
        goto end_audio;
    }
    }
    pw_loop_iterate(ctx_a->pw_loop, 1);
    nanosleep(&ts, NULL);
    goto pc_audio;
end_audio:
    pw_stream_destroy(ctx_a->play_stream);
    pw_stream_destroy(ctx_a->record_stream);
    pw_loop_destroy(ctx_a->pw_loop);
    pw_deinit();
    end("war_audio");
    return 0;
}
