//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Monaco
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
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and
// LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

#include "war_vulkan.c"
#include "war_wayland.c"

#include "../build/war_keymap_macros.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_keymap_functions.h"
#include "h/war_main.h"

#include <errno.h>
#include <fcntl.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
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
#include <strings.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int main() {
    CALL_TERRY_DAVIS("war");
    //-------------------------------------------------------------------------
    // LUA
    //-------------------------------------------------------------------------
    war_lua_context ctx_lua;
    ctx_lua.L = luaL_newstate();
    if (!ctx_lua.L) {
        call_terry_davis("failed to create Lua state");
        return -1;
    }
    luaL_openlibs(ctx_lua.L);
    war_load_lua_config(&ctx_lua, "src/lua/war_main.lua");
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    war_producer_consumer pc_control;
    pc_control.size = atomic_load(&ctx_lua.PC_CONTROL_BUFFER_SIZE);
    pc_control.to_a = mmap(NULL,
                           pc_control.size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1,
                           0);
    assert(pc_control.to_a);
    memset(pc_control.to_a, 0, pc_control.size);
    pc_control.to_wr = mmap(NULL,
                            pc_control.size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
    assert(pc_control.to_wr);
    memset(pc_control.to_wr, 0, pc_control.size);
    pc_control.i_to_a = 0;
    pc_control.i_to_wr = 0;
    pc_control.i_from_a = 0;
    pc_control.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // PC PLAY
    //-------------------------------------------------------------------------
    war_producer_consumer pc_play;
    pc_play.size = atomic_load(&ctx_lua.PC_PLAY_BUFFER_SIZE);
    pc_play.to_a = mmap(NULL,
                        pc_play.size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    assert(pc_play.to_a);
    memset(pc_play.to_a, 0, pc_play.size);
    pc_play.to_wr = mmap(NULL,
                         pc_play.size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);
    assert(pc_play.to_wr);
    memset(pc_play.to_wr, 0, pc_play.size);
    pc_play.i_to_a = 0;
    pc_play.i_to_wr = 0;
    pc_play.i_from_a = 0;
    pc_play.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // PC CAPTURE
    //-------------------------------------------------------------------------
    war_producer_consumer pc_capture;
    pc_capture.size = atomic_load(&ctx_lua.PC_CAPTURE_BUFFER_SIZE);
    pc_capture.to_a = mmap(NULL,
                           pc_capture.size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1,
                           0);
    assert(pc_capture.to_a);
    memset(pc_capture.to_a, 0, pc_capture.size);
    pc_capture.to_wr = mmap(NULL,
                            pc_capture.size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
    assert(pc_capture.to_wr);
    memset(pc_capture.to_wr, 0, pc_capture.size);
    pc_capture.i_to_a = 0;
    pc_capture.i_to_wr = 0;
    pc_capture.i_from_a = 0;
    pc_capture.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // ATOMICS
    //-------------------------------------------------------------------------
    war_atomics atomics = {
        .play_clock = 0,
        .play_frames = 0,
        .capture_frames = 0,
        .capture_monitor = 0,
        .capture_threshold = 0.01f,
        .play_writer_rate = 0,
        .play_reader_rate = 0,
        .capture_writer_rate = 0,
        .capture_reader_rate = 0,
        .play_gain = 1.0f,
        .capture_gain = 1.0f,
        .capture = 0,
        .play = 0,
        .map = 0,
        .map_note = -1,
        .loop = 0,
        .start_war = 0,
        .resample = 0,
        .repeat_section = 0,
        .repeat_start_frames = 0,
        .repeat_end_frames = 0,
        .note_next_id = 1,
        .cache_next_id = 1,
        .cache_next_timestamp = 1,
        .layer = 0,
    };
    //-------------------------------------------------------------------------
    // THREADS
    //-------------------------------------------------------------------------
    war_pool pool_wr;
    war_pool pool_a;
    pthread_t war_window_render_thread;
    pthread_create(
        &war_window_render_thread,
        NULL,
        war_window_render,
        (void* [6]){
            &pc_control, &atomics, &pool_wr, &ctx_lua, &pc_play, &pc_capture});
    pthread_t war_audio_thread;
    pthread_create(
        &war_audio_thread,
        NULL,
        war_audio,
        (void* [6]){
            &pc_control, &atomics, &pool_a, &ctx_lua, &pc_play, &pc_capture});
    pthread_join(war_window_render_thread, NULL);
    pthread_join(war_audio_thread, NULL);
    END("war");
    return 0;
}
//-----------------------------------------------------------------------------
// THREAD WINDOW RENDER
//-----------------------------------------------------------------------------
void* war_window_render(void* args) {
    header("war_window_render");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc_control = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];
    while (!atomic_load(&atomics->start_war)) { usleep(1000); }
    war_pool* pool_wr = args_ptrs[2];
    war_lua_context* ctx_lua = args_ptrs[3];
    war_producer_consumer* pc_play = args_ptrs[4];
    war_producer_consumer* pc_capture = args_ptrs[5];
    call_terry_davis("ctx_lua WR_STATES: %i", atomic_load(&ctx_lua->WR_STATES));
    pool_wr->pool_alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    pool_wr->pool_size =
        war_get_pool_wr_size(pool_wr, ctx_lua, "src/lua/war_main.lua");
    pool_wr->pool_size += pool_wr->pool_alignment * 2000;
    call_terry_davis("pool_wr hack size: %zu", pool_wr->pool_size);
    pool_wr->pool_size = ALIGN_UP(pool_wr->pool_size, pool_wr->pool_alignment);
    int pool_result = posix_memalign(
        &pool_wr->pool, pool_wr->pool_alignment, pool_wr->pool_size);
    assert(pool_result == 0 && pool_wr->pool);
    memset(pool_wr->pool, 0, pool_wr->pool_size);
    pool_wr->pool_ptr = (uint8_t*)pool_wr->pool;
    //-------------------------------------------------------------------------
    // VULKAN CONTEXT
    //-------------------------------------------------------------------------
    // const uint32_t internal_width = 1920;
    // const uint32_t internal_height = 1080;
    double physical_width = 2560;
    double physical_height = 1600;
    const uint32_t stride = physical_width * 4;
    war_vulkan_context ctx_vk_stack =
        war_vulkan_init(ctx_lua, physical_width, physical_height);
    war_vulkan_context* ctx_vk = &ctx_vk_stack;
    assert(ctx_vk->dmabuf_fd >= 0);
    //-------------------------------------------------------------------------
    // COLOR CONTEXT
    //-------------------------------------------------------------------------
    // constants
    const uint32_t light_gray_hex = 0xFF454950;
    const uint32_t darker_light_gray_hex = 0xFF36383C; // gutter / line numbers
    const uint32_t dark_gray_hex = 0xFF282828;
    // rainbow
    war_color_context* ctx_color =
        war_pool_alloc(pool_wr, sizeof(war_color_context));
    ctx_color->colors_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
    ctx_color->colors =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_color->colors_count);
    const uint32_t red_hex = 0xFF0000DE;
    const uint32_t orange_hex = 0xFF0080FF;
    const uint32_t yellow_hex = 0xFF00FFFF;
    const uint32_t green_hex = 0xFF00FF00;
    const uint32_t cyan_hex = 0xFFFFFF00;
    const uint32_t blue_hex = 0xFFFF0000;
    const uint32_t magenta_hex = 0xFFFF00FF;
    const uint32_t purple_hex = 0xFF8000FF;
    const uint32_t gray_hex = 0xFF808080;
    const uint32_t white_hex = 0xFFB1D9E9;
    const uint32_t black_hex = 0xFF000000;
    const uint32_t full_white_hex = 0xFFFFFFFF;
    const uint32_t super_light_gray_hex = 0xFFD0D0D0;
    ctx_color->colors[0] = red_hex;
    ctx_color->colors[1] = orange_hex;
    ctx_color->colors[2] = yellow_hex;
    ctx_color->colors[3] = green_hex;
    ctx_color->colors[4] = cyan_hex;
    ctx_color->colors[5] = blue_hex;
    ctx_color->colors[6] = magenta_hex;
    ctx_color->colors[7] = purple_hex;
    ctx_color->colors[8] = gray_hex;
    ctx_color->full_white_hex = full_white_hex;
    ctx_color->white_hex = white_hex;
    //-------------------------------------------------------------------------
    // DEFAULTS
    //-------------------------------------------------------------------------
    const float default_horizontal_line_thickness = 0.018;
    const float piano_horizontal_line_thickness = 0.018;
    const float default_vertical_line_thickness = 0.018; // default: 0.018
    const float default_outline_thickness =
        0.04; // 0.027 is minimum for preventing 1/4, 1/7, 1/9, max = 0.075f
              // sub_cursor right outline from disappearing
              // defualt outline = 0.04f
    float default_alpha_scale = atomic_load(&ctx_lua->DEFAULT_ALPHA_SCALE);
    float default_cursor_alpha_scale =
        atomic_load(&ctx_lua->DEFAULT_CURSOR_ALPHA_SCALE);
    float default_playback_bar_thickness =
        atomic_load(&ctx_lua->DEFAULT_PLAYBACK_BAR_THICKNESS);
    float default_text_feather = atomic_load(&ctx_lua->DEFAULT_TEXT_FEATHER);
    float default_text_thickness =
        atomic_load(&ctx_lua->DEFAULT_TEXT_THICKNESS);
    float windowed_text_feather = atomic_load(&ctx_lua->WINDOWED_TEXT_FEATHER);
    float windowed_text_thickness =
        atomic_load(&ctx_lua->WINDOWED_TEXT_THICKNESS);
    float default_windowed_alpha_scale =
        atomic_load(&ctx_lua->DEFAULT_WINDOWED_ALPHA_SCALE);
    float default_windowed_cursor_alpha_scale =
        atomic_load(&ctx_lua->DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE);
    //-------------------------------------------------------------------------
    // VIEWPORT
    //-------------------------------------------------------------------------
    double scale_factor = 1.483333; // 1.483333
    const uint32_t logical_width =
        (uint32_t)floor(physical_width / scale_factor);
    const uint32_t logical_height =
        (uint32_t)floor(physical_height / scale_factor);
    uint32_t num_rows_for_status_bars = 3;
    uint32_t num_cols_for_line_numbers = 3;
    uint32_t default_viewport_cols =
        (uint32_t)(physical_width / ctx_vk->cell_width);
    uint32_t default_viewport_rows =
        (uint32_t)(physical_height / ctx_vk->cell_height);
    double viewport_bound_x = physical_width / ctx_vk->cell_width;
    double viewport_bound_y = physical_height / ctx_vk->cell_height;
    uint32_t visible_rows =
        (uint32_t)((physical_height -
                    ((float)num_rows_for_status_bars * ctx_vk->cell_height)) /
                   ctx_vk->cell_height);
    //-------------------------------------------------------------------------
    // UNDO TREE
    //-------------------------------------------------------------------------
    war_undo_tree* undo_tree = war_pool_alloc(pool_wr, sizeof(war_undo_tree));
    undo_tree->root = NULL;
    undo_tree->current = NULL;
    undo_tree->next_id = 1;
    undo_tree->next_seq_num = 1;
    undo_tree->next_branch_id = 1;
    //-------------------------------------------------------------------------
    // CWD
    //-------------------------------------------------------------------------
    chdir(atomic_load(&ctx_lua->CWD));
    //-------------------------------------------------------------------------
    // STATUS CONTEXT
    //-------------------------------------------------------------------------
    war_status_context* ctx_status =
        war_pool_alloc(pool_wr, sizeof(war_status_context));
    ctx_status->capacity = atomic_load(&ctx_lua->A_PATH_LIMIT);
    ctx_status->top =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_status->capacity);
    ctx_status->top_size = 0;
    getcwd(ctx_status->top, ctx_status->capacity);
    ctx_status->middle =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_status->capacity);
    ctx_status->middle_size = 0;
    ctx_status->bottom =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_status->capacity);
    ctx_status->bottom_size = 0;
    ctx_status->layers_active_size = atomic_load(&ctx_lua->A_LAYER_COUNT);
    ctx_status->layers_active =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_status->layers_active_size);
    //-------------------------------------------------------------------------
    // COMMAND CONTEXT
    //-------------------------------------------------------------------------
    war_command_context* ctx_command =
        war_pool_alloc(pool_wr, sizeof(war_command_context));
    ctx_command->capacity = atomic_load(&ctx_lua->A_PATH_LIMIT);
    ctx_command->input =
        war_pool_alloc(pool_wr, sizeof(int) * ctx_command->capacity);
    ctx_command->text =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_command->capacity);
    ctx_command->prompt =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_command->capacity);
    ctx_command->prompt_size = 0;
    ctx_command->command = 0;
    ctx_command->input_write_index = 0;
    ctx_command->text_write_index = 0;
    ctx_command->input_read_index = 0;
    ctx_command->text_size = 0;
    //-------------------------------------------------------------------------
    // WINDOW RENDER CONTEXT
    //-------------------------------------------------------------------------
    war_window_render_context ctx_wr_stack = {
        .prompt = 0,
        .default_viewport_cols = default_viewport_cols,
        .default_viewport_rows = default_viewport_rows,
        .layer_flux = 0,
        .layers_active_count = 0,
        .skip_release = 0,
        .midi_toggle = 0,
        .midi_octave = 4,
        .record_octave = 4,
        .gain_increment = 0.05f,
        .trinity = false,
        .fullscreen = false,
        .end_window_render = false,
        .FPS = atomic_load(&ctx_lua->WR_FPS),
        .now = 0,
        .mode = 0,
        .hud_state = HUD_PIANO,
        .cursor_blink_state = 0,
        .cursor_blink_duration_us =
            atomic_load(&ctx_lua->WR_CURSOR_BLINK_DURATION_US),
        .cursor_pos_x = 0,
        .cursor_pos_y = 60,
        .cursor_size_x = 1.0,
        .cursor_navigation_x = 1.0,
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
                                          ctx_vk->cell_width)) /
                       ctx_vk->cell_width) -
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
        .viewport_cols = default_viewport_cols,
        .viewport_rows = default_viewport_rows,
        .scroll_margin_cols = 0, // cols from visible min/max col
        .scroll_margin_rows = 0, // rows from visible min/max row
        .cell_width = ctx_vk->cell_width,
        .cell_height = ctx_vk->cell_height,
        .physical_width = physical_width,
        .physical_height = physical_height,
        .logical_width = logical_width,
        .logical_height = logical_height,
        .max_col = 144635, // shaking start happening after 289290,
                           // line numbers thinning happens after 144635
        .max_row = atomic_load(&ctx_lua->A_NOTE_COUNT) - 1,
        .min_col = 0,
        .min_row = 0,
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
        .text_feather_bold = atomic_load(&ctx_lua->DEFAULT_BOLD_TEXT_FEATHER),
        .text_thickness_bold =
            atomic_load(&ctx_lua->DEFAULT_BOLD_TEXT_THICKNESS),
        .color_note_default = white_hex,
        .color_note_outline_default = full_white_hex,
        .color_cursor = white_hex,
        .color_cursor_transparent = white_hex,
    };
    war_window_render_context* ctx_wr = &ctx_wr_stack;
    for (int i = 0; i < LAYER_COUNT; i++) {
        ctx_wr->layers[i] = i / ctx_wr->layer_count;
    }
    uint32_t max_viewport_cols =
        (uint32_t)(physical_width /
                   (ctx_vk->cell_width * ctx_wr->min_zoom_scale));
    uint32_t max_viewport_rows =
        (uint32_t)(physical_height /
                   (ctx_vk->cell_height * ctx_wr->min_zoom_scale));
    uint32_t max_gridlines_per_split = max_viewport_cols + max_viewport_rows;
    war_views views_stack;
    {
        views_stack.views_saved_max = atomic_load(&ctx_lua->WR_VIEWS_SAVED);
        views_stack.text_size = atomic_load(&ctx_lua->WR_WARPOON_TEXT_COLS);
        uint32_t* views_col = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        uint32_t* views_row = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        uint32_t* views_left_col = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        uint32_t* views_right_col = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        uint32_t* views_bottom_row = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        uint32_t* views_top_row = war_pool_alloc(
            pool_wr, sizeof(uint32_t) * views_stack.views_saved_max);
        char** warpoon_text = war_pool_alloc(
            pool_wr, sizeof(char*) * views_stack.views_saved_max);
        for (uint32_t i = 0; i < views_stack.views_saved_max; i++) {
            warpoon_text[i] =
                war_pool_alloc(pool_wr, sizeof(char) * views_stack.text_size);
        }
        uint32_t warpoon_viewport_cols = 25;
        uint32_t warpoon_viewport_rows = 8;
        uint32_t warpoon_hud_cols = 2;
        uint32_t warpoon_hud_rows = 0;
        uint32_t warpoon_max_col = views_stack.text_size - 1 - warpoon_hud_cols;
        uint32_t warpoon_max_row =
            views_stack.views_saved_max - 1 - warpoon_hud_rows;
        views_stack = (war_views){
            .col = views_col,
            .row = views_row,
            .left_col = views_left_col,
            .right_col = views_right_col,
            .bottom_row = views_bottom_row,
            .top_row = views_top_row,
            .views_count = 0,
            .warpoon_text = warpoon_text,
            .warpoon_mode = 0,
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
            .warpoon_color_bg = ctx_wr->darker_light_gray_hex,
            .warpoon_color_outline = ctx_wr->white_hex,
            .warpoon_color_hud = ctx_wr->red_hex,
            .warpoon_color_hud_text = ctx_wr->full_white_hex,
            .warpoon_color_text = ctx_wr->white_hex,
            .warpoon_color_cursor = ctx_wr->white_hex,
        };
    }
    war_views* views = &views_stack;
    //-------------------------------------------------------------------------
    // FSM CONTEXT + REPEATS + TIMEOUTS
    //-------------------------------------------------------------------------
    // Watch for overflow
    war_fsm_context* ctx_fsm = war_pool_alloc(pool_wr, sizeof(war_fsm_context));
    // Set counts
    ctx_fsm->keysym_count = atomic_load(&ctx_lua->WR_KEYSYM_COUNT);
    ctx_fsm->mod_count = atomic_load(&ctx_lua->WR_MOD_COUNT);
    ctx_fsm->state_count = atomic_load(&ctx_lua->WR_STATES);
    ctx_fsm->mode_count = atomic_load(&ctx_lua->WR_MODE_COUNT);
    ctx_fsm->name_limit = atomic_load(&ctx_lua->WR_FN_NAME_LIMIT);
    // FSM data allocations (per state per mode)
    ctx_fsm->next_state =
        war_pool_alloc(pool_wr,
                       sizeof(uint64_t) * ctx_fsm->state_count *
                           ctx_fsm->keysym_count * ctx_fsm->mod_count);
    ctx_fsm->is_terminal = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->is_prefix = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_release = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_repeat = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_timeout = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->function =
        war_pool_alloc(pool_wr,
                       sizeof(war_function_union) * ctx_fsm->state_count *
                           ctx_fsm->mode_count);
    ctx_fsm->type = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->name =
        war_pool_alloc(pool_wr,
                       sizeof(char) * ctx_fsm->state_count *
                           ctx_fsm->mode_count * ctx_fsm->name_limit);
    // Runtime state allocations
    ctx_fsm->key_down = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->keysym_count * ctx_fsm->mod_count);
    ctx_fsm->key_last_event_us = war_pool_alloc(
        pool_wr, sizeof(uint64_t) * ctx_fsm->keysym_count * ctx_fsm->mod_count);
    // repeats
    ctx_fsm->repeat_delay_us = 150000;
    ctx_fsm->repeat_rate_us = 40000;
    ctx_fsm->repeat_keysym = 0;
    ctx_fsm->repeat_mod = 0;
    ctx_fsm->repeating = 0;
    ctx_fsm->goto_cmd_repeat_done = 0;
    // timeouts
    ctx_fsm->timeout_duration_us = 500000;
    ctx_fsm->timeout_state_index = 0;
    ctx_fsm->timeout_start_us = 0;
    ctx_fsm->timeout = 0;
    ctx_fsm->goto_cmd_timeout_done = 0;
    // Initialize runtime values
    ctx_fsm->state_last_event_us = 0;
    ctx_fsm->current_state = 0;
    // modes
    ctx_fsm->current_mode = 0;
    // Initialize modifiers
    ctx_fsm->mod_shift = 0;
    ctx_fsm->mod_ctrl = 0;
    ctx_fsm->mod_alt = 0;
    ctx_fsm->mod_logo = 0;
    ctx_fsm->mod_caps = 0;
    ctx_fsm->mod_num = 0;
    ctx_fsm->mod_fn = 0;
    // XKB initialization (separate from pool allocation)
    ctx_fsm->xkb_context = NULL;
    ctx_fsm->xkb_state = NULL;
    //-----------------------------------------------------------------------------
    // WAYLAND
    //-----------------------------------------------------------------------------
    uint32_t zwp_linux_dmabuf_v1_id = 0;
    uint32_t zwp_linux_buffer_params_v1_id = 0;
    uint32_t zwp_linux_dmabuf_feedback_v1_id = 0;
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
    uint32_t new_id;
    uint8_t get_registry[12];
    war_write_le32(get_registry, wl_display_id);
    war_write_le16(get_registry + 4, 1);
    war_write_le16(get_registry + 6, 12);
    war_write_le32(get_registry + 8, wl_registry_id);
    ssize_t written = write(fd, get_registry, 12);
    call_terry_davis("written size: %lu", written);
    dump_bytes("written", get_registry, 12);
    assert(written == 12);
    new_id = wl_registry_id + 1;
    uint32_t msg_buffer_alloc_size =
        atomic_load(&ctx_lua->WR_WAYLAND_MSG_BUFFER_SIZE);
    uint8_t* msg_buffer =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * msg_buffer_alloc_size);
    size_t msg_buffer_size = 0;
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };
    uint32_t max_wayland_objects =
        atomic_load(&ctx_lua->WR_WAYLAND_MAX_OBJECTS);
    uint32_t max_wayland_opcodes =
        atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES);
    void** obj_op = war_pool_alloc(
        pool_wr, sizeof(void*) * max_wayland_objects * max_wayland_opcodes);
    obj_op[wl_display_id * max_wayland_opcodes + 0] = &&wl_display_error;
    obj_op[wl_display_id * max_wayland_opcodes + 1] = &&wl_display_delete_id;
    obj_op[wl_registry_id * max_wayland_opcodes + 0] = &&wl_registry_global;
    obj_op[wl_registry_id * max_wayland_opcodes + 1] =
        &&wl_registry_global_remove;
    //-------------------------------------------------------------------------
    // NOTE QUADS
    //-------------------------------------------------------------------------
    war_note_quads* note_quads =
        war_pool_alloc(pool_wr, sizeof(war_note_quads));
    note_quads->note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
    note_quads->alive =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * note_quads->note_quads_max);
    note_quads->id =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * note_quads->note_quads_max);
    note_quads->pos_x =
        war_pool_alloc(pool_wr, sizeof(double) * note_quads->note_quads_max);
    note_quads->pos_y =
        war_pool_alloc(pool_wr, sizeof(double) * note_quads->note_quads_max);
    note_quads->layer =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * note_quads->note_quads_max);
    note_quads->size_x =
        war_pool_alloc(pool_wr, sizeof(double) * note_quads->note_quads_max);
    note_quads->navigation_x =
        war_pool_alloc(pool_wr, sizeof(double) * note_quads->note_quads_max);
    note_quads->navigation_x_numerator =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->navigation_x_denominator =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->size_x_numerator =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->size_x_denominator =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->color =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->outline_color =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->gain =
        war_pool_alloc(pool_wr, sizeof(float) * note_quads->note_quads_max);
    note_quads->voice =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->hidden =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->mute =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * note_quads->note_quads_max);
    note_quads->count = 0;
    uint32_t quads_max = atomic_load(&ctx_lua->WR_QUADS_MAX);
    uint32_t text_quads_max = atomic_load(&ctx_lua->WR_TEXT_QUADS_MAX);
    war_quad_vertex* quad_vertices =
        war_pool_alloc(pool_wr, sizeof(war_quad_vertex) * quads_max);
    uint32_t quad_vertices_count = 0;
    uint16_t* quad_indices =
        war_pool_alloc(pool_wr, sizeof(uint16_t) * quads_max);
    uint32_t quad_indices_count = 0;
    war_quad_vertex* transparent_quad_vertices =
        war_pool_alloc(pool_wr, sizeof(war_quad_vertex) * quads_max);
    uint32_t transparent_quad_vertices_count = 0;
    uint16_t* transparent_quad_indices =
        war_pool_alloc(pool_wr, sizeof(uint16_t) * quads_max);
    uint32_t transparent_quad_indices_count = 0;
    war_text_vertex* text_vertices =
        war_pool_alloc(pool_wr, sizeof(war_text_vertex) * text_quads_max);
    uint32_t text_vertices_count = 0;
    uint16_t* text_indices =
        war_pool_alloc(pool_wr, sizeof(uint16_t) * text_quads_max);
    uint32_t text_indices_count = 0;
    //-------------------------------------------------------------------------
    // RENDERING FPS
    //-------------------------------------------------------------------------
    const double microsecond_conversion = 1000000.0;
    ctx_wr->sleep_duration_us = 50000;
    ctx_wr->frame_duration_us =
        (uint64_t)round((1.0 / (double)ctx_wr->FPS) * microsecond_conversion);
    uint64_t last_frame_time = war_get_monotonic_time_us();
    ctx_wr->cursor_blink_previous_us = last_frame_time;
    ctx_wr->cursor_blinking = false;
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    uint32_t header;
    uint32_t size;
    uint32_t control_cmd_count = atomic_load(&ctx_lua->CMD_COUNT);
    uint8_t* control_payload =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * pc_control->size);
    uint8_t* tmp_control_payload =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * pc_control->size);
    void** pc_control_cmd =
        war_pool_alloc(pool_wr, sizeof(void*) * control_cmd_count);
    pc_control_cmd[0] = &&end_wr;
    //-------------------------------------------------------------------------
    // CAPTURE WAV
    //-------------------------------------------------------------------------
    war_wav* capture_wav = war_pool_alloc(pool_wr, sizeof(war_wav));
    capture_wav->memfd_size = 44;
    capture_wav->name_limit = atomic_load(&ctx_lua->A_PATH_LIMIT);
    uint64_t init_capacity = 44 + sizeof(float) *
                                      atomic_load(&ctx_lua->A_SAMPLE_RATE) *
                                      atomic_load(&ctx_lua->A_SAMPLE_DURATION) *
                                      atomic_load(&ctx_lua->A_CHANNEL_COUNT);
    capture_wav->memfd_capacity = init_capacity;
    capture_wav->fname =
        war_pool_alloc(pool_wr, sizeof(char) * capture_wav->name_limit);
    capture_wav->fname_size = sizeof("capture.wav") - 1;
    memcpy(capture_wav->fname, "capture.wav", capture_wav->fname_size);
    capture_wav->memfd = memfd_create(capture_wav->fname, MFD_CLOEXEC);
    if (capture_wav->memfd < 0) {
        call_terry_davis("memfd failed to open: %s", capture_wav->fname);
    }
    if (ftruncate(capture_wav->memfd, capture_wav->memfd_capacity) == -1) {
        call_terry_davis("memfd ftruncate failed: %s", capture_wav->fname);
    }
    capture_wav->wav = mmap(NULL,
                            capture_wav->memfd_capacity,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            capture_wav->memfd,
                            0);
    if (capture_wav->wav == MAP_FAILED) {
        call_terry_davis("mmap failed: %s", capture_wav->fname);
    }
    war_riff_header init_riff_header = (war_riff_header){
        .chunk_id = "RIFF",
        .chunk_size = init_capacity - 8,
        .format = "WAVE",
    };
    war_fmt_chunk init_fmt_chunk = (war_fmt_chunk){
        .subchunk1_id = "fmt ",
        .subchunk1_size = 16,
        .audio_format = 3,
        .num_channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .byte_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE) *
                     atomic_load(&ctx_lua->A_CHANNEL_COUNT) * sizeof(float),
        .block_align = atomic_load(&ctx_lua->A_CHANNEL_COUNT) * sizeof(float),
        .bits_per_sample = 32,
    };
    war_data_chunk init_data_chunk = (war_data_chunk){
        .subchunk2_id = "data",
        .subchunk2_size = init_capacity - 44,
    };
    *(war_riff_header*)capture_wav->wav = init_riff_header;
    *(war_fmt_chunk*)(capture_wav->wav + sizeof(war_riff_header)) =
        init_fmt_chunk;
    *(war_data_chunk*)(capture_wav->wav + sizeof(war_riff_header) +
                       sizeof(war_fmt_chunk)) = init_data_chunk;
    capture_wav->fd =
        open(capture_wav->fname, O_TRUNC | O_CREAT | O_RDWR, 0644);
    if (capture_wav->fd < 0) {
        call_terry_davis("fd failed to open: %s", capture_wav->fname);
    }
    capture_wav->fd_size = 0;
    //-------------------------------------------------------------------------
    // CACHE WAV
    //-------------------------------------------------------------------------
    war_cache_wav* cache_wav = war_pool_alloc(pool_wr, sizeof(war_cache_wav));
    cache_wav->capacity = atomic_load(&ctx_lua->A_CACHE_SIZE);
    cache_wav->id =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * cache_wav->capacity);
    cache_wav->timestamp =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * cache_wav->capacity);
    cache_wav->wav =
        war_pool_alloc(pool_wr, sizeof(uint8_t*) * cache_wav->capacity);
    cache_wav->memfd =
        war_pool_alloc(pool_wr, sizeof(int) * cache_wav->capacity);
    cache_wav->memfd_size =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * cache_wav->capacity);
    cache_wav->memfd_capacity =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * cache_wav->capacity);
    cache_wav->fd = war_pool_alloc(pool_wr, sizeof(int) * cache_wav->capacity);
    cache_wav->fd_size =
        war_pool_alloc(pool_wr, sizeof(uint64_t) * cache_wav->capacity);
    cache_wav->count = 0;
    cache_wav->next_id = 1;
    cache_wav->next_timestamp = 1;
    //-------------------------------------------------------------------------
    // MAP WAV
    //-------------------------------------------------------------------------
    war_map_wav* map_wav = war_pool_alloc(pool_wr, sizeof(war_map_wav));
    map_wav->note_count = atomic_load(&ctx_lua->A_NOTE_COUNT);
    map_wav->layer_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
    map_wav->name_limit = atomic_load(&ctx_lua->A_PATH_LIMIT);
    map_wav->id = war_pool_alloc(
        pool_wr, sizeof(uint64_t) * map_wav->note_count * map_wav->layer_count);
    map_wav->fname =
        war_pool_alloc(pool_wr,
                       sizeof(char) * map_wav->note_count *
                           map_wav->layer_count * map_wav->name_limit);
    map_wav->fname_size = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * map_wav->note_count * map_wav->layer_count);
    map_wav->note = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * map_wav->note_count * map_wav->layer_count);
    map_wav->layer = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * map_wav->note_count * map_wav->layer_count);
    //-------------------------------------------------------------------------
    // CAPTURE CONTEXT
    //-------------------------------------------------------------------------
    war_capture_context* ctx_capture =
        war_pool_alloc(pool_wr, sizeof(war_capture_context));
    ctx_capture->fps = atomic_load(&ctx_lua->WR_CAPTURE_CALLBACK_FPS);
    // rate
    ctx_capture->rate_us = (uint64_t)round((1.0 / (double)ctx_capture->fps) *
                                           microsecond_conversion);
    ctx_capture->last_frame_time = war_get_monotonic_time_us();
    ctx_capture->last_read_time = 0;
    ctx_capture->read_count = 0;
    // misc
    ctx_capture->capture = 0;
    ctx_capture->capture_wait = 1;
    ctx_capture->capture_delay = 0;
    ctx_capture->state = CAPTURE_WAITING;
    ctx_capture->threshold = atomic_load(&ctx_lua->WR_CAPTURE_THRESHOLD);
    ctx_capture->prompt = 1;
    ctx_capture->prompt_step = 0; // 0 = fname, 1 = note, 2 = layer
    ctx_capture->monitor = 0;
    //-------------------------------------------------------------------------
    // PLAY CONTEXT
    //-------------------------------------------------------------------------
    war_play_context* ctx_play =
        war_pool_alloc(pool_wr, sizeof(war_capture_context));
    ctx_play->fps = atomic_load(&ctx_lua->WR_PLAY_CALLBACK_FPS);
    // rate
    ctx_play->rate_us =
        (uint64_t)round((1.0 / (double)ctx_play->fps) * microsecond_conversion);
    ctx_play->last_frame_time = war_get_monotonic_time_us();
    ctx_play->last_write_time = 0;
    ctx_play->write_count = 0;
    // misc
    ctx_play->play = 0;
    ctx_play->note_layers = war_pool_alloc(
        pool_wr, sizeof(uint64_t) * atomic_load(&ctx_lua->A_NOTE_COUNT));
    //-------------------------------------------------------------------------
    // ENV
    //-------------------------------------------------------------------------
    war_env* env = war_pool_alloc(pool_wr, sizeof(war_env));
    env->ctx_wr = ctx_wr;
    env->atomics = atomics;
    env->ctx_color = ctx_color;
    env->ctx_lua = ctx_lua;
    env->views = views;
    env->ctx_play = ctx_play;
    env->ctx_capture = ctx_capture;
    env->ctx_command = ctx_command;
    env->ctx_status = ctx_status;
    env->undo_tree = undo_tree;
    env->note_quads = note_quads;
    env->pool_wr = pool_wr;
    env->ctx_vk = ctx_vk;
    env->capture_wav = capture_wav;
    env->ctx_fsm = ctx_fsm;
wr: {

    if (war_pc_from_a(pc_control, &header, &size, control_payload)) {
        goto* pc_control_cmd[header];
    }
    ctx_wr->now = war_get_monotonic_time_us();
    //-------------------------------------------------------------------------
    // PLAY WRITER
    //-------------------------------------------------------------------------
    if (ctx_wr->now - ctx_play->last_frame_time >= ctx_play->rate_us) {
        ctx_play->last_frame_time += ctx_play->rate_us;
        if (!ctx_play->play) { goto skip_play; }
        if (ctx_wr->now - ctx_play->last_write_time >= 1000000) {
            atomic_store(&atomics->play_writer_rate,
                         (double)ctx_play->write_count);
            ctx_play->write_count = 0;
            ctx_play->last_write_time = ctx_wr->now;
        }
        ctx_play->write_count++;
        uint64_t write_pos = pc_play->i_to_a;
        uint64_t read_pos = pc_play->i_from_wr;
        int64_t used_bytes = write_pos - read_pos;
        if (used_bytes < 0) used_bytes += pc_play->size;
        float buffer_percent = ((float)used_bytes / pc_play->size) * 100.0f;
        // uint64_t target_samples =
        //     (uint64_t)(((double)atomic_load(&ctx_lua->A_BYTES_NEEDED) *
        //                 atomic_load(&atomics->writer_rate)) /
        //                ((double)atomic_load(&atomics->reader_rate) * 8.0));
        uint64_t target_samples =
            (uint64_t)((double)atomic_load(&ctx_lua->A_BYTES_NEEDED) / 8.0);
        //---------------------------------------------------------------------
        // MIX SAMPLES
        //---------------------------------------------------------------------
        static float phase = 0.0f;
        const float phase_inc = 2.0f * M_PI * 440.0f / 44100.0f;
        for (uint64_t i = 0; i < target_samples; i++) {
            float sample = sinf(phase) * 0.1f;
            uint64_t byte_offset = pc_play->i_to_a;
            uint8_t* audio_ptr = pc_play->to_a + byte_offset;
            ((float*)audio_ptr)[0] = sample;
            ((float*)audio_ptr)[1] = sample;
            pc_play->i_to_a = (byte_offset + 8) & (pc_play->size - 1);
            phase += phase_inc;
            if (phase >= 2.0f * M_PI) { phase -= 2.0f * M_PI; }
        }
    }
skip_play:
    //-------------------------------------------------------------------------
    // CAPTURE READER
    //-------------------------------------------------------------------------
    if (ctx_wr->now - ctx_capture->last_frame_time >= ctx_capture->rate_us) {
        ctx_capture->last_frame_time += ctx_capture->rate_us;
        if (!ctx_capture->capture) {
            pc_capture->i_from_a = pc_capture->i_to_a;
            ctx_capture->state = CAPTURE_WAITING;
            goto skip_capture;
        }
        if (ctx_wr->now - ctx_capture->last_read_time >= 1000000) {
            atomic_store(&atomics->capture_reader_rate,
                         (double)ctx_capture->read_count);
            // call_terry_davis("capture_reader_rate: %.2f Hz",
            //              atomic_load(&atomics->capture_reader_rate));
            ctx_capture->read_count = 0;
            ctx_capture->last_read_time = ctx_wr->now;
        }
        ctx_capture->read_count++;
        uint64_t write_pos = pc_capture->i_to_a;
        uint64_t read_pos = pc_capture->i_from_a;
        int64_t available_bytes;
        if (write_pos >= read_pos) {
            available_bytes = write_pos - read_pos;
        } else {
            available_bytes = pc_capture->size + write_pos - read_pos;
        }
        if (ctx_capture->capture_wait &&
            ctx_capture->state == CAPTURE_WAITING) {
            float max_amplitude = 0.0f;
            uint64_t read_idx = read_pos;
            uint64_t samples_to_check = available_bytes / sizeof(float);
            for (uint64_t i = 0; i < samples_to_check; i++) {
                float sample = ((float*)pc_capture->to_a)[read_idx / 4];
                float amplitude = fabsf(sample);
                if (amplitude > max_amplitude) { max_amplitude = amplitude; }
                read_idx = (read_idx + 4) & (pc_capture->size - 1);
            }
            if (max_amplitude > ctx_capture->threshold) {
                ctx_capture->state = CAPTURE_CAPTURING;
                call_terry_davis("Sound detected - starting recording");
            }
        } else if (!ctx_capture->capture_wait &&
                   ctx_capture->state == CAPTURE_WAITING) {
        } else if (ctx_capture->state == CAPTURE_PROMPT) {
            switch (ctx_capture->prompt_step) {
            case 0: {
                break;
            }
            case 1: {
                break;
            }
            case 2: {
                break;
            }
            }
        }
        if (available_bytes > 0) {
            uint64_t space_left =
                capture_wav->memfd_capacity - capture_wav->memfd_size;
            uint64_t bytes_to_copy =
                available_bytes < space_left ? available_bytes : space_left;
            if (bytes_to_copy > 0) {
                uint64_t read_idx = read_pos;
                uint64_t samples_to_copy = bytes_to_copy / sizeof(float);
                float* wav_samples =
                    (float*)(capture_wav->wav + capture_wav->memfd_size);
                for (uint64_t i = 0; i < samples_to_copy; i++) {
                    wav_samples[i] = ((float*)pc_capture->to_a)[read_idx / 4];
                    read_idx = (read_idx + 4) & (pc_capture->size - 1);
                }
                pc_capture->i_from_a = read_idx;
                if (ctx_capture->state == CAPTURE_CAPTURING) {
                    capture_wav->memfd_size += bytes_to_copy;
                    war_riff_header* riff_header =
                        (war_riff_header*)capture_wav->wav;
                    riff_header->chunk_size = capture_wav->memfd_size - 8;
                    war_data_chunk* data_chunk =
                        (war_data_chunk*)(capture_wav->wav +
                                          sizeof(war_riff_header) +
                                          sizeof(war_fmt_chunk));
                    data_chunk->subchunk2_size = capture_wav->memfd_size - 44;
                }
            }
        }
    }
skip_capture:
    //-------------------------------------------------------------------------
    // FPS
    //-------------------------------------------------------------------------
    if (ctx_wr->now - last_frame_time >= ctx_wr->frame_duration_us) {
        last_frame_time += ctx_wr->frame_duration_us;
        if (ctx_wr->trinity) {
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
    //-------------------------------------------------------------------------
    // COMMAND MODE HANDLING
    //-------------------------------------------------------------------------
    if (ctx_command->command) {
        war_command_status(ctx_command, ctx_status);
        if (ctx_command->input_write_index == 0 ||
            ctx_command->input_read_index >= ctx_command->input_write_index) {
            goto skip_command;
        }
        int input = ctx_command->input[ctx_command->input_read_index];
        if (input == 8) { // ASCII backspace
            if (ctx_command->text_write_index > 0) {
                for (int i = ctx_command->text_write_index - 1;
                     i < ctx_command->text_size;
                     i++) {
                    ctx_command->text[i] = ctx_command->text[i + 1];
                }
                ctx_command->text_write_index--;
                ctx_command->text_size--;
                ctx_command->text[ctx_command->text_size] = '\0';
            }
        } else if (input == 10) { // ASCII newline
            ctx_command->command = 0;
            printf("- ");
            for (uint32_t i = 0; i < ctx_command->text_size; i++) {
                printf("%c", ctx_command->text[i]);
            }
            printf("\n");
            ctx_command->text_write_index = 0;
            ctx_command->text_size = 0;
            ctx_command->text[0] = '\0';
        } else if (input == 27) { // ASCII escape
            ctx_command->command = 0;
            ctx_command->text_write_index = 0;
            war_command_reset(ctx_command, ctx_status);
        } else if (input == 3) { // Arrow left
            if (ctx_command->text_write_index > 0) {
                ctx_command->text_write_index--;
            }
        } else if (input == 4) { // Arrow right
            if (ctx_command->text_write_index < ctx_command->text_size) {
                ctx_command->text_write_index++;
            }
        } else {
            if (ctx_command->text_size < ctx_command->capacity - 1) {
                if (ctx_command->text_write_index < ctx_command->text_size) {
                    for (int i = ctx_command->text_size;
                         i > ctx_command->text_write_index;
                         i--) {
                        ctx_command->text[i] = ctx_command->text[i - 1];
                    }
                }

                ctx_command->text[ctx_command->text_write_index] = (char)input;
                ctx_command->text_write_index++;
                ctx_command->text_size++;
                ctx_command->text[ctx_command->text_size] = '\0';
            }
        }
        ctx_command->input_read_index++;
    }
skip_command:
    // cursor blink
    if (ctx_wr->cursor_blink_state &&
        ctx_wr->now - ctx_wr->cursor_blink_previous_us >=
            ctx_wr->cursor_blink_duration_us &&
        (ctx_fsm->current_mode == ctx_fsm->MODE_ROLL ||
         (ctx_fsm->current_mode == ctx_fsm->MODE_VIEWS &&
          views->warpoon_mode != ctx_fsm->MODE_VISUAL_LINE))) {
        ctx_wr->cursor_blink_duration_us =
            (ctx_wr->cursor_blink_state == CURSOR_BLINK) ?
                atomic_load(&ctx_lua->WR_CURSOR_BLINK_DURATION_US) :
                (uint64_t)round(
                    (60.0 / ((double)atomic_load(&ctx_lua->A_BPM))) *
                    microsecond_conversion);
        ;
        ctx_wr->cursor_blink_previous_us += ctx_wr->cursor_blink_duration_us;
        ctx_wr->cursor_blinking = !ctx_wr->cursor_blinking;
    }
    //---------------------------------------------------------------------
    // KEY REPEATS
    //---------------------------------------------------------------------
    if (ctx_fsm->repeat_keysym) {
        uint32_t k = ctx_fsm->repeat_keysym;
        uint8_t m = ctx_fsm->repeat_mod;
        if (ctx_fsm->key_down[k * atomic_load(&ctx_lua->WR_MOD_COUNT) + m]) {
            uint64_t elapsed =
                ctx_wr->now - ctx_fsm->key_last_event_us
                                  [k * atomic_load(&ctx_lua->WR_MOD_COUNT) + m];
            if (!ctx_fsm->repeating) {
                // still waiting for initial delay
                if (elapsed >= ctx_fsm->repeat_delay_us) {
                    ctx_fsm->repeating = true;
                    ctx_fsm->key_last_event_us[k * atomic_load(
                                                       &ctx_lua->WR_MOD_COUNT) +
                                               m] = ctx_wr->now; // reset timer
                }
            } else {
                if (elapsed >= ctx_fsm->repeat_rate_us) {
                    ctx_fsm->key_last_event_us[k * atomic_load(
                                                       &ctx_lua->WR_MOD_COUNT) +
                                               m] = ctx_wr->now;
                    //---------------------------------------------------------
                    // COMMAND INPUT REPEATS
                    //---------------------------------------------------------
                    if (ctx_command->command) {
                        // Write repeated key to command input buffer
                        if (ctx_command->input_write_index <
                            ctx_command->capacity) {
                            int keysym_to_int = war_keysym_to_int(k, m);
                            ctx_command->input[ctx_command->input_write_index] =
                                keysym_to_int;
                            ctx_command->input_write_index++;
                        }
                        goto cmd_repeat_done; // Skip FSM processing
                    }
                    uint16_t next_state_index =
                        ctx_fsm->next_state[FSM_3D_INDEX(
                            ctx_fsm->current_state, k, m)];
                    if (next_state_index != 0) {
                        ctx_fsm->current_state = next_state_index;
                        ctx_fsm->state_last_event_us = ctx_wr->now;
                        if (ctx_fsm->is_terminal[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)] &&
                            !ctx_fsm->is_prefix[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)] &&
                            ctx_fsm->handle_repeat[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)]) {
                            uint16_t temp = ctx_fsm->current_state;
                            ctx_fsm->current_state = 0;
                            ctx_fsm->goto_cmd_repeat_done = true;
                            if (ctx_fsm->type[FSM_2D_MODE(
                                    temp, ctx_fsm->current_mode)] ==
                                ctx_fsm->FUNCTION_NONE) {
                                goto cmd_repeat_done;
                            }
                            if (ctx_fsm->type[FSM_2D_MODE(
                                    temp, ctx_fsm->current_mode)] ==
                                ctx_fsm->FUNCTION_C) {
                                ctx_fsm
                                    ->function[FSM_2D_MODE(
                                        temp, ctx_fsm->current_mode)]
                                    .c(env);
                                goto cmd_done;
                            }
                            // handle lua logic
                            goto cmd_done;
                        }
                    }
                }
            }
        }
    } else {
        ctx_fsm->repeat_keysym = 0;
        ctx_fsm->repeat_mod = 0;
        ctx_fsm->repeating = false;
    }
cmd_repeat_done:
    //--------------------------------------------------------------------
    // KEY TIMEOUTS
    //--------------------------------------------------------------------
    if (ctx_fsm->timeout && ctx_wr->now >= ctx_fsm->timeout_start_us +
                                               ctx_fsm->timeout_duration_us) {
        uint16_t temp = ctx_fsm->timeout_state_index;
        ctx_fsm->timeout = false;
        ctx_fsm->timeout_state_index = 0;
        ctx_fsm->timeout_start_us = 0;
        ctx_fsm->goto_cmd_timeout_done = true;
        // clear current
        ctx_fsm->current_state = 0;
        ctx_fsm->state_last_event_us = ctx_wr->now;
        if (ctx_fsm->type[FSM_2D_MODE(temp, ctx_fsm->current_mode)] ==
            ctx_fsm->FUNCTION_NONE) {
            goto cmd_timeout_done;
        }
        if (ctx_fsm->type[FSM_2D_MODE(temp, ctx_fsm->current_mode)] ==
            ctx_fsm->FUNCTION_C) {
            ctx_fsm->function[FSM_2D_MODE(temp, ctx_fsm->current_mode)].c(env);
            goto cmd_done;
        }
        // handle lua logic
        goto cmd_done;
    }
cmd_timeout_done:
    //---------------------------------------------------------------------
    // WAYLAND MESSAGE PARSING
    //---------------------------------------------------------------------
    int ret = poll(&pfd, 1, 0);
    assert(ret >= 0);
    // if (ret == 0) { call_terry_davis("timeout"); }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        call_terry_davis("wayland socket error or hangup: %s", strerror(errno));
        goto end_wr;
    }
    if (pfd.revents & POLLIN) {
        struct msghdr poll_msg_hdr = {0};
        struct iovec poll_iov;
        poll_iov.iov_base = msg_buffer + msg_buffer_size;
        poll_iov.iov_len =
            atomic_load(&ctx_lua->WR_WAYLAND_MSG_BUFFER_SIZE) - msg_buffer_size;
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
            uint16_t size = war_read_le16(msg_buffer + msg_buffer_offset + 6);
            if ((size < 8) || (size > (msg_buffer_size - msg_buffer_offset))) {
                break;
            };
            uint32_t object_id = war_read_le32(msg_buffer + msg_buffer_offset);
            uint16_t opcode = war_read_le16(msg_buffer + msg_buffer_offset + 4);
            if (object_id >=
                    (uint32_t)atomic_load(&ctx_lua->WR_WAYLAND_MAX_OBJECTS) ||
                opcode >=
                    (uint16_t)atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES)) {
                // COMMENT CONCERN: INVALID OBJECT/OP 27 TIMES!
                // call_terry_davis(
                //    "invalid object/op: id=%u, op=%u", object_id,
                //    opcode);
                goto wayland_done;
            }
            size_t idx =
                object_id * atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                opcode;
            if (obj_op[idx]) { goto* obj_op[idx]; }
            goto wayland_default;
        wl_registry_global:
            dump_bytes("global event", msg_buffer + msg_buffer_offset, size);
            call_terry_davis("iname: %s",
                             (const char*)msg_buffer + msg_buffer_offset + 16);

            const char* iname = (const char*)msg_buffer + msg_buffer_offset +
                                16; // COMMENT OPTIMIZE: perfect hash
            if (strcmp(iname, "wl_compositor") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_compositor_id = new_id;
                obj_op[wl_compositor_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_compositor_jump;
                new_id++;
            } else if (strcmp(iname, "wl_output") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_output_id = new_id;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_output_geometry;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_output_mode;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&wl_output_done;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&wl_output_scale;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&wl_output_name;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&wl_output_description;
                new_id++;
            } else if (strcmp(iname, "wl_seat") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_seat_id = new_id;
                obj_op[wl_seat_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_seat_capabilities;
                obj_op[wl_seat_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_seat_name;
                new_id++;
            } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_linux_dmabuf_v1_id = new_id;
                obj_op[zwp_linux_dmabuf_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_linux_dmabuf_v1_format;
                obj_op[zwp_linux_dmabuf_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&zwp_linux_dmabuf_v1_modifier;
                new_id++;
            } else if (strcmp(iname, "xdg_wm_base") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                xdg_wm_base_id = new_id;
                obj_op[xdg_wm_base_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&xdg_wm_base_ping;
                new_id++;
            } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_linux_drm_syncobj_manager_v1_id = new_id;
                obj_op[wp_linux_drm_syncobj_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wp_linux_drm_syncobj_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_idle_inhibit_manager_v1_id = new_id;
                obj_op[zwp_idle_inhibit_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_idle_inhibit_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zxdg_decoration_manager_v1_id = new_id;
                obj_op[zxdg_decoration_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zxdg_decoration_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_relative_pointer_manager_v1_id = new_id;
                obj_op[zwp_relative_pointer_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_relative_pointer_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_pointer_constraints_v1_id = new_id;
                obj_op[zwp_pointer_constraints_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_pointer_constraints_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_output_manager_v1_id = new_id;
                obj_op[zwlr_output_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwlr_output_manager_v1_head;
                obj_op[zwlr_output_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&zwlr_output_manager_v1_done;
                new_id++;
            } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_data_control_manager_v1_id = new_id;
                obj_op[zwlr_data_control_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwlr_data_control_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_virtual_keyboard_manager_v1_id = new_id;
                obj_op[zwp_virtual_keyboard_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_virtual_keyboard_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "wp_viewporter") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_viewporter_id = new_id;
                obj_op[wp_viewporter_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wp_viewporter_jump;
                new_id++;
            } else if (strcmp(iname, "wp_fractional_scale_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_fractional_scale_manager_v1_id = new_id;
                obj_op[wp_fractional_scale_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wp_fractional_scale_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_pointer_gestures_v1_id = new_id;
                obj_op[zwp_pointer_gestures_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_pointer_gestures_v1_jump;
                new_id++;
            } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                xdg_activation_v1_id = new_id;
                obj_op[xdg_activation_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&xdg_activation_v1_jump;
                new_id++;
            } else if (strcmp(iname, "wp_presentation") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_presentation_id = new_id;
                obj_op[wp_presentation_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wp_presentation_clock_id;
                new_id++;
            } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_layer_shell_v1_id = new_id;
                obj_op[zwlr_layer_shell_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwlr_layer_shell_v1_jump;
                new_id++;
            } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                ext_foreign_toplevel_list_v1_id = new_id;
                obj_op[ext_foreign_toplevel_list_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&ext_foreign_toplevel_list_v1_toplevel;
                new_id++;
            } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_content_type_manager_v1_id = new_id;
                obj_op[wp_content_type_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wp_content_type_manager_v1_jump;
                new_id++;
            }
            if (!wl_surface_id && wl_compositor_id) {
                uint8_t create_surface[12];
                war_write_le32(create_surface, wl_compositor_id);
                war_write_le16(create_surface + 4, 0);
                war_write_le16(create_surface + 6, 12);
                war_write_le32(create_surface + 8, new_id);
                dump_bytes("create_surface request", create_surface, 12);
                call_terry_davis("bound: wl_surface");
                ssize_t create_surface_written = write(fd, create_surface, 12);
                assert(create_surface_written == 12);
                wl_surface_id = new_id;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_surface_enter;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_surface_leave;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&wl_surface_preferred_buffer_scale;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&wl_surface_preferred_buffer_transform;
                new_id++;
            }
            if (!wl_region_id && wl_surface_id && wl_compositor_id) {
                uint8_t create_region[12];
                war_write_le32(create_region, wl_compositor_id);
                war_write_le16(create_region + 4, 1);
                war_write_le16(create_region + 6, 12);
                war_write_le32(create_region + 8, new_id);
                dump_bytes("create_region request", create_region, 12);
                call_terry_davis("bound: wl_region");
                ssize_t create_region_written = write(fd, create_region, 12);
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
            if (!zwp_linux_dmabuf_feedback_v1_id && zwp_linux_dmabuf_v1_id &&
                wl_surface_id) {
                uint8_t get_surface_feedback[16];
                war_write_le32(get_surface_feedback, zwp_linux_dmabuf_v1_id);
                war_write_le16(get_surface_feedback + 4, 3);
                war_write_le16(get_surface_feedback + 6, 16);
                war_write_le32(get_surface_feedback + 8, new_id);
                war_write_le32(get_surface_feedback + 12, wl_surface_id);
                dump_bytes("zwp_linux_dmabuf_v1::get_surface_feedback request",
                           get_surface_feedback,
                           16);
                call_terry_davis("bound: xdg_surface");
                ssize_t get_surface_feedback_written =
                    write(fd, get_surface_feedback, 16);
                assert(get_surface_feedback_written == 16);
                zwp_linux_dmabuf_feedback_v1_id = new_id;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zwp_linux_dmabuf_feedback_v1_done;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&zwp_linux_dmabuf_feedback_v1_format_table;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&zwp_linux_dmabuf_feedback_v1_main_device;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&zwp_linux_dmabuf_feedback_v1_tranche_done;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] =
                    &&zwp_linux_dmabuf_feedback_v1_tranche_target_device;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&zwp_linux_dmabuf_feedback_v1_tranche_formats;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] = &&zwp_linux_dmabuf_feedback_v1_tranche_flags;
                new_id++;
            }
            if (!xdg_surface_id && xdg_wm_base_id && wl_surface_id) {
                uint8_t get_xdg_surface[16];
                war_write_le32(get_xdg_surface, xdg_wm_base_id);
                war_write_le16(get_xdg_surface + 4, 2);
                war_write_le16(get_xdg_surface + 6, 16);
                war_write_le32(get_xdg_surface + 8, new_id);
                war_write_le32(get_xdg_surface + 12, wl_surface_id);
                dump_bytes("get_xdg_surface request", get_xdg_surface, 16);
                call_terry_davis("bound: xdg_surface");
                ssize_t get_xdg_surface_written =
                    write(fd, get_xdg_surface, 16);
                assert(get_xdg_surface_written == 16);
                xdg_surface_id = new_id;
                obj_op[xdg_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&xdg_surface_configure;
                new_id++;
                uint8_t get_toplevel[12];
                war_write_le32(get_toplevel, xdg_surface_id);
                war_write_le16(get_toplevel + 4, 1);
                war_write_le16(get_toplevel + 6, 12);
                war_write_le32(get_toplevel + 8, new_id);
                dump_bytes("get_xdg_toplevel request", get_toplevel, 12);
                call_terry_davis("bound: xdg_toplevel");
                ssize_t get_toplevel_written = write(fd, get_toplevel, 12);
                assert(get_toplevel_written == 12);
                xdg_toplevel_id = new_id;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&xdg_toplevel_configure;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&xdg_toplevel_close;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&xdg_toplevel_configure_bounds;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&xdg_toplevel_wm_capabilities;
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
                war_write_le32(get_toplevel_decoration + 12, xdg_toplevel_id);
                dump_bytes("get_toplevel_decoration request",
                           get_toplevel_decoration,
                           16);
                call_terry_davis("bound: zxdg_toplevel_decoration_v1");
                ssize_t get_toplevel_decoration_written =
                    write(fd, get_toplevel_decoration, 16);
                assert(get_toplevel_decoration_written == 16);
                zxdg_toplevel_decoration_v1_id = new_id;
                obj_op[zxdg_toplevel_decoration_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&zxdg_toplevel_decoration_v1_configure;
                new_id++;

                //---------------------------------------------------------
                // initial commit
                //---------------------------------------------------------
                war_wayland_wl_surface_commit(fd, wl_surface_id);
            }
            goto wayland_done;
        wl_registry_global_remove:
            dump_bytes("global_rm event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_callback_done: {
            //-------------------------------------------------------------
            // RENDERING WITH VULKAN
            //-------------------------------------------------------------
            // dump_bytes("wl_callback::wayland_done event",
            //           msg_buffer + msg_buffer_offset,
            //           size);
            assert(ctx_vk->current_frame == 0);
            vkWaitForFences(ctx_vk->device,
                            1,
                            &ctx_vk->in_flight_fences[ctx_vk->current_frame],
                            VK_TRUE,
                            UINT64_MAX);
            vkResetFences(ctx_vk->device,
                          1,
                          &ctx_vk->in_flight_fences[ctx_vk->current_frame]);
            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            VkResult result =
                vkBeginCommandBuffer(ctx_vk->cmd_buffer, &begin_info);
            assert(result == VK_SUCCESS);
            VkClearValue clear_values[2];
            clear_values[0].color =
                (VkClearColorValue){{0.1569f, 0.1569f, 0.1569f, 1.0f}};
            clear_values[1].depthStencil = (VkClearDepthStencilValue){
                ctx_wr->layers[LAYER_OPAQUE_REGION], 0.0f};
            VkRenderPassBeginInfo render_pass_info = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = ctx_vk->render_pass,
                .framebuffer = ctx_vk->frame_buffer,
                .renderArea =
                    {
                        .offset = {0, 0},
                        .extent = {physical_width, physical_height},
                    },
                .clearValueCount = 2,
                .pClearValues = clear_values,
            };
            vkCmdBeginRenderPass(ctx_vk->cmd_buffer,
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
            vkCmdBindPipeline(ctx_vk->cmd_buffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              ctx_vk->quad_pipeline);
            uint32_t cursor_color = ctx_wr->color_cursor;
            float alpha_factor = ctx_wr->alpha_scale_cursor;
            uint8_t color_alpha =
                (ctx_wr->color_cursor_transparent >> 24) & 0xFF;
            uint32_t cursor_color_transparent =
                ((uint8_t)(color_alpha * alpha_factor) << 24) |
                (ctx_wr->color_cursor_transparent & 0x00FFFFFF);
            // draw note quads and figure out if cursor should be
            // transparent
            // TODO: spillover
            for (uint32_t i = 0; i < note_quads->count; i++) {
                if (note_quads->alive[i] == 0 || note_quads->hidden[i]) {
                    continue;
                }
                double cursor_pos_x = ctx_wr->cursor_pos_x;
                double cursor_pos_y = ctx_wr->cursor_pos_y;
                double cursor_end_x = cursor_pos_x + ctx_wr->cursor_size_x;
                double pos_x = note_quads->pos_x[i];
                double pos_y = note_quads->pos_y[i];
                double size_x = note_quads->size_x[i];
                double end_x = pos_x + size_x;
                double left_bound = ctx_wr->left_col;
                double right_bound = ctx_wr->right_col + 1;
                double top_bound = ctx_wr->top_row + 1;
                double bottom_bound = ctx_wr->bottom_row;
                if (pos_y > top_bound || pos_y < bottom_bound ||
                    pos_x > right_bound || end_x < left_bound) {
                    continue;
                }
                uint32_t color = note_quads->color[i];
                uint32_t outline_color = note_quads->outline_color[i];
                if (note_quads->mute[i]) {
                    float alpha_factor = ctx_wr->alpha_scale;
                    uint8_t color_alpha = (color >> 24) & 0xFF;
                    uint8_t outline_color_alpha = (outline_color >> 24) & 0xFF;
                    color = ((uint8_t)(color_alpha * alpha_factor) << 24) |
                            (color & 0x00FFFFFF);
                    outline_color =
                        ((uint8_t)(outline_color_alpha * alpha_factor) << 24) |
                        (outline_color & 0x00FFFFFF);
                }
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){(float)pos_x,
                                         (float)note_quads->pos_y[i],
                                         ctx_wr->layers[LAYER_NOTES]},
                              (float[2]){(float)size_x, 1},
                              color,
                              default_outline_thickness,
                              outline_color,
                              (float[2]){0.0f, 0.0f},
                              QUAD_GRID);
                if (cursor_pos_y != pos_y || cursor_pos_x >= end_x ||
                    cursor_end_x <= pos_x) {
                    continue;
                }
                cursor_color = cursor_color_transparent;
            }
            if (ctx_fsm->current_mode == ctx_fsm->MODE_ROLL &&
                !ctx_command->command && !ctx_wr->cursor_blinking) {
                war_make_transparent_quad(
                    transparent_quad_vertices,
                    transparent_quad_indices,
                    &transparent_quad_vertices_count,
                    &transparent_quad_indices_count,
                    (float[3]){ctx_wr->cursor_pos_x +
                                   (float)ctx_wr->sub_col /
                                       ctx_wr->navigation_sub_cells_col,
                               ctx_wr->cursor_pos_y,
                               ctx_wr->layers[LAYER_CURSOR]},
                    (float[2]){(float)ctx_wr->cursor_size_x, 1},
                    cursor_color,
                    0,
                    0,
                    (float[2]){0.0f, 0.0f},
                    QUAD_GRID);
            } else if (ctx_fsm->current_mode == ctx_fsm->MODE_VIEWS) {
                // draw views
                uint32_t offset_col = ctx_wr->left_col +
                                      ((ctx_wr->viewport_cols +
                                        ctx_wr->num_cols_for_line_numbers - 1) /
                                           2 -
                                       views->warpoon_viewport_cols / 2);
                uint32_t offset_row = ctx_wr->bottom_row +
                                      ((ctx_wr->viewport_rows +
                                        ctx_wr->num_rows_for_status_bars - 1) /
                                           2 -
                                       views->warpoon_viewport_rows / 2);
                // draw views background
                war_make_quad(
                    quad_vertices,
                    quad_indices,
                    &quad_vertices_count,
                    &quad_indices_count,
                    (float[3]){offset_col,
                               offset_row,
                               ctx_wr->layers[LAYER_POPUP_BACKGROUND]},
                    (float[2]){views->warpoon_viewport_cols,
                               views->warpoon_viewport_rows},
                    views->warpoon_color_bg,
                    ctx_wr->outline_thickness,
                    views->warpoon_color_outline,
                    (float[2]){0.0f, 0.0f},
                    QUAD_OUTLINE);
                // draw views gutter
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){offset_col,
                                         offset_row,
                                         ctx_wr->layers[LAYER_POPUP_HUD]},
                              (float[2]){views->warpoon_hud_cols,
                                         views->warpoon_viewport_rows},
                              views->warpoon_color_hud,
                              ctx_wr->outline_thickness,
                              views->warpoon_color_outline,
                              (float[2]){0.0f, 0.0f},
                              QUAD_OUTLINE);
                // draw views cursor
                if (!ctx_wr->cursor_blinking && !ctx_command->command) {
                    uint32_t cursor_span_x = 1;
                    uint32_t cursor_pos_x = views->warpoon_col;
                    if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                        cursor_span_x = views->warpoon_viewport_cols -
                                        views->warpoon_hud_cols;
                        cursor_pos_x = 0;
                    }
                    float alpha_factor = ctx_wr->alpha_scale;
                    uint8_t color_alpha = (cursor_color >> 24) & 0xFF;
                    cursor_color =
                        ((uint8_t)(color_alpha * alpha_factor) << 24) |
                        (cursor_color & 0x00FFFFFF);
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){offset_col + views->warpoon_hud_cols +
                                       cursor_pos_x - views->warpoon_left_col,
                                   offset_row + views->warpoon_hud_rows +
                                       views->warpoon_row -
                                       views->warpoon_bottom_row,
                                   ctx_wr->layers[LAYER_POPUP_CURSOR]},
                        (float[2]){cursor_span_x, 1},
                        cursor_color_transparent,
                        0,
                        0,
                        (float[2]){0.0f, 0.0f},
                        0);
                }
                // draw views line numbers text
                int number = views->warpoon_max_row - views->warpoon_top_row +
                             views->warpoon_viewport_rows;
                for (uint32_t row = views->warpoon_bottom_row;
                     row <= views->warpoon_top_row;
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
                                           views->warpoon_bottom_row +
                                           views->warpoon_hud_rows,
                                       ctx_wr->layers[LAYER_POPUP_HUD_TEXT]},
                            (float[2]){1, 1},
                            views->warpoon_color_hud_text,
                            &ctx_vk->glyphs['0' + digits[col - 1]],
                            ctx_wr->text_thickness,
                            ctx_wr->text_feather,
                            0);
                    }
                }
                // draw views text
                war_get_warpoon_text(views);
                uint32_t row = views->warpoon_max_row;
                for (uint32_t i_views = 0; i_views < views->views_count;
                     i_views++, row--) {
                    if (row > views->warpoon_top_row ||
                        row < views->warpoon_bottom_row) {
                        continue;
                    }
                    for (uint32_t col = 0;
                         col <= views->warpoon_right_col &&
                         views->warpoon_text[i_views][col] != '\0';
                         col++) {
                        war_make_text_quad(
                            text_vertices,
                            text_indices,
                            &text_vertices_count,
                            &text_indices_count,
                            (float[3]){offset_col + views->warpoon_hud_cols +
                                           col,
                                       offset_row + views->warpoon_hud_rows +
                                           row - views->warpoon_bottom_row,
                                       ctx_wr->layers[LAYER_POPUP_CURSOR]},
                            (float[2]){1, 1},
                            views->warpoon_color_text,
                            &ctx_vk->glyphs[(int)views
                                                ->warpoon_text[i_views][col]],
                            ctx_wr->text_thickness,
                            ctx_wr->text_feather,
                            0);
                    }
                }
                break;
            }
            if (ctx_command->command) {
                war_make_transparent_quad(
                    transparent_quad_vertices,
                    transparent_quad_indices,
                    &transparent_quad_vertices_count,
                    &transparent_quad_indices_count,
                    (float[3]){ctx_wr->left_col +
                                   ctx_command->text_write_index +
                                   ctx_command->prompt_size + 1,
                               ctx_wr->bottom_row + 1,
                               ctx_wr->layers[LAYER_CURSOR]},
                    (float[2]){(float)ctx_wr->cursor_size_x, 1},
                    cursor_color,
                    0,
                    0,
                    (float[2]){0.0f, 0.0f},
                    0);
            }
            // draw playback bar
            uint32_t playback_bar_color = ctx_wr->red_hex;
            float span_y = ctx_wr->viewport_rows;
            if (ctx_wr->top_row == atomic_load(&ctx_lua->A_NOTE_COUNT) - 1) {
                span_y -= ctx_wr->num_rows_for_status_bars;
            }
            war_make_quad(
                quad_vertices,
                quad_indices,
                &quad_vertices_count,
                &quad_indices_count,
                (float[3]){
                    ((float)atomic_load(&atomics->play_frames) /
                     atomic_load(&ctx_lua->A_SAMPLE_RATE)) /
                        ((60.0f / atomic_load(&ctx_lua->A_BPM)) /
                         atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT)),
                    ctx_wr->bottom_row,
                    ctx_wr->layers[LAYER_PLAYBACK_BAR]},
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
                          (float[3]){ctx_wr->left_col,
                                     ctx_wr->bottom_row,
                                     ctx_wr->layers[LAYER_HUD]},
                          (float[2]){ctx_wr->viewport_cols + 1, 1},
                          ctx_wr->red_hex,
                          0,
                          0,
                          (float[2]){0.0f, 0.0f},
                          0);
            war_make_quad(quad_vertices,
                          quad_indices,
                          &quad_vertices_count,
                          &quad_indices_count,
                          (float[3]){ctx_wr->left_col,
                                     ctx_wr->bottom_row + 1,
                                     ctx_wr->layers[LAYER_HUD]},
                          (float[2]){ctx_wr->viewport_cols + 1, 1},
                          ctx_wr->dark_gray_hex,
                          0,
                          0,
                          (float[2]){0.0f, 0.0f},
                          0);
            war_make_quad(quad_vertices,
                          quad_indices,
                          &quad_vertices_count,
                          &quad_indices_count,
                          (float[3]){ctx_wr->left_col,
                                     ctx_wr->bottom_row + 2,
                                     ctx_wr->layers[LAYER_HUD]},
                          (float[2]){ctx_wr->viewport_cols + 1, 1},
                          ctx_wr->darker_light_gray_hex,
                          0,
                          0,
                          (float[2]){0.0f, 0.0f},
                          0);
            // draw piano quads
            float gutter_end_span_inset = 5 * default_vertical_line_thickness;
            switch (ctx_wr->hud_state) {
            case HUD_PIANO_AND_LINE_NUMBERS:
            case HUD_PIANO:
                float span_y = ctx_wr->viewport_rows;
                if (ctx_wr->top_row == ctx_wr->max_row) {
                    span_y -= ctx_wr->num_rows_for_status_bars;
                }
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){ctx_wr->left_col,
                                         ctx_wr->bottom_row +
                                             ctx_wr->num_rows_for_status_bars,
                                         ctx_wr->layers[LAYER_HUD]},
                              (float[2]){3 - gutter_end_span_inset, span_y},
                              ctx_wr->full_white_hex,
                              0,
                              0,
                              (float[2]){0.0f, 0.0f},
                              0);
                for (uint32_t row = ctx_wr->bottom_row; row <= ctx_wr->top_row;
                     row++) {
                    if (row < ctx_wr->max_row) {
                        war_make_quad(
                            quad_vertices,
                            quad_indices,
                            &quad_vertices_count,
                            &quad_indices_count,
                            (float[3]){ctx_wr->left_col,
                                       row + ctx_wr->num_rows_for_status_bars +
                                           1,
                                       ctx_wr->layers[LAYER_HUD]},
                            (float[2]){3 - gutter_end_span_inset, 0},
                            super_light_gray_hex,
                            0,
                            0,
                            (float[2]){0.0f, piano_horizontal_line_thickness},
                            QUAD_LINE);
                    }
                    uint32_t note = row % 12;
                    if (note == 1 || note == 3 || note == 6 || note == 8 ||
                        note == 10) {
                        war_make_quad(
                            quad_vertices,
                            quad_indices,
                            &quad_vertices_count,
                            &quad_indices_count,
                            (float[3]){ctx_wr->left_col,
                                       row + ctx_wr->num_rows_for_status_bars,
                                       ctx_wr->layers[LAYER_HUD]},
                            (float[2]){2 - gutter_end_span_inset, 1},
                            ctx_wr->black_hex,
                            0,
                            0,
                            (float[2]){0.0f, 0.0f},
                            0);
                    }
                }
                break;
            }
            // draw line number quad
            int ln_offset = (ctx_wr->hud_state == HUD_LINE_NUMBERS) ? 0 : 3;
            if (ctx_wr->hud_state != HUD_PIANO) {
                uint32_t span_y = ctx_wr->viewport_rows;
                if (ctx_wr->top_row ==
                    atomic_load(&ctx_lua->A_NOTE_COUNT) - 1) {
                    span_y -= ctx_wr->num_rows_for_status_bars;
                }
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){ctx_wr->left_col + ln_offset -
                                             default_vertical_line_thickness,
                                         ctx_wr->bottom_row +
                                             ctx_wr->num_rows_for_status_bars,
                                         ctx_wr->layers[LAYER_HUD]},
                              (float[2]){3 - gutter_end_span_inset, span_y},
                              ctx_wr->red_hex,
                              0,
                              0,
                              (float[2]){0.0f, 0.0f},
                              0);
                for (uint32_t row = ctx_wr->bottom_row; row <= ctx_wr->top_row;
                     row++) {
                    if (row >= ctx_wr->max_row) { continue; }
                    war_make_quad(
                        quad_vertices,
                        quad_indices,
                        &quad_vertices_count,
                        &quad_indices_count,
                        (float[3]){ctx_wr->left_col + ln_offset,
                                   row + ctx_wr->num_rows_for_status_bars + 1,
                                   ctx_wr->layers[LAYER_HUD]},
                        (float[2]){3 - gutter_end_span_inset, 0},
                        ctx_wr->full_white_hex,
                        0,
                        0,
                        (float[2]){0.0f, piano_horizontal_line_thickness},
                        QUAD_LINE);
                }
            }
            // draw gridline quads
            for (uint32_t row = ctx_wr->bottom_row + 1;
                 row <= ctx_wr->top_row + 1;
                 row++) {
                war_make_quad(
                    quad_vertices,
                    quad_indices,
                    &quad_vertices_count,
                    &quad_indices_count,
                    (float[3]){
                        ctx_wr->left_col, row, ctx_wr->layers[LAYER_GRIDLINES]},
                    (float[2]){ctx_wr->viewport_cols, 0},
                    ctx_wr->darker_light_gray_hex,
                    0,
                    0,
                    (float[2]){0.0f, default_horizontal_line_thickness},
                    QUAD_LINE | QUAD_GRID);
            }
            qsort(ctx_wr->gridline_splits,
                  MAX_GRIDLINE_SPLITS,
                  sizeof(uint32_t),
                  war_compare_desc_uint32);
            for (uint32_t col = ctx_wr->left_col + 1;
                 col <= ctx_wr->right_col + 1;
                 col++) {
                bool draw_vertical_line = false;
                uint32_t color;
                for (uint32_t i = 0; i < MAX_GRIDLINE_SPLITS; i++) {
                    draw_vertical_line =
                        (col % ctx_wr->gridline_splits[i]) == 0;
                    if (draw_vertical_line) {
                        switch (i) {
                        case 0:
                            color = ctx_wr->white_hex;
                            break;
                        case 1:
                            color = ctx_wr->darker_light_gray_hex;
                            break;
                        case 2:
                            color = ctx_wr->red_hex;
                            break;
                        case 3:
                            color = ctx_wr->black_hex;
                            break;
                        }
                        break;
                    }
                }
                if (!draw_vertical_line) { continue; }
                uint32_t span_y = ctx_wr->viewport_rows;
                if (ctx_wr->top_row ==
                    atomic_load(&ctx_lua->A_NOTE_COUNT) - 1) {
                    span_y -= ctx_wr->num_rows_for_status_bars;
                }
                war_make_quad(quad_vertices,
                              quad_indices,
                              &quad_vertices_count,
                              &quad_indices_count,
                              (float[3]){col,
                                         ctx_wr->bottom_row,
                                         ctx_wr->layers[LAYER_GRIDLINES]},
                              (float[2]){0, span_y},
                              color,
                              0,
                              0,
                              (float[2]){default_vertical_line_thickness, 0},
                              QUAD_LINE | QUAD_GRID);
            }
            memcpy(ctx_vk->quads_vertex_buffer_mapped,
                   quad_vertices,
                   sizeof(war_quad_vertex) * quad_vertices_count);
            memcpy(ctx_vk->quads_index_buffer_mapped,
                   quad_indices,
                   sizeof(uint16_t) * quad_indices_count);
            VkMappedMemoryRange quad_flush_ranges[2] = {
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->quads_vertex_buffer_memory,
                 .offset = 0,
                 .size = war_align64(sizeof(war_quad_vertex) *
                                     quad_vertices_count)},
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->quads_index_buffer_memory,
                 .offset = 0,
                 .size = war_align64(sizeof(uint16_t) * quad_indices_count)}};
            vkFlushMappedMemoryRanges(ctx_vk->device, 2, quad_flush_ranges);
            VkDeviceSize quad_vertices_offsets[2] = {0};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   0,
                                   1,
                                   &ctx_vk->quads_vertex_buffer,
                                   quad_vertices_offsets);
            VkDeviceSize quad_instances_offsets[2] = {0};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   1,
                                   1,
                                   &ctx_vk->quads_instance_buffer,
                                   quad_instances_offsets);
            VkDeviceSize quad_indices_offset = 0;
            vkCmdBindIndexBuffer(ctx_vk->cmd_buffer,
                                 ctx_vk->quads_index_buffer,
                                 quad_indices_offset,
                                 VK_INDEX_TYPE_UINT16);
            war_quad_push_constants quad_push_constants = {
                .bottom_left = {ctx_wr->left_col, ctx_wr->bottom_row},
                .physical_size = {physical_width, physical_height},
                .cell_size = {ctx_wr->cell_width, ctx_wr->cell_height},
                .zoom = ctx_wr->zoom_scale,
                .cell_offsets = {ctx_wr->num_cols_for_line_numbers,
                                 ctx_wr->num_rows_for_status_bars},
                .scroll_margin = {ctx_wr->scroll_margin_cols,
                                  ctx_wr->scroll_margin_rows},
                .anchor_cell = {ctx_wr->cursor_pos_x, ctx_wr->cursor_pos_y},
                .top_right = {ctx_wr->right_col, ctx_wr->top_row},
            };
            vkCmdPushConstants(ctx_vk->cmd_buffer,
                               ctx_vk->pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(war_quad_push_constants),
                               &quad_push_constants);
            vkCmdDrawIndexed(
                ctx_vk->cmd_buffer, quad_indices_count, 1, 0, 0, 0);
            // draw transparent quads
            vkCmdBindPipeline(ctx_vk->cmd_buffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              ctx_vk->transparent_quad_pipeline);
            memcpy(ctx_vk->quads_vertex_buffer_mapped +
                       quad_vertices_count * sizeof(war_quad_vertex),
                   transparent_quad_vertices,
                   sizeof(war_quad_vertex) * transparent_quad_vertices_count);
            memcpy(ctx_vk->quads_index_buffer_mapped +
                       quad_indices_count * sizeof(uint16_t),
                   transparent_quad_indices,
                   sizeof(uint16_t) * transparent_quad_indices_count);
            VkMappedMemoryRange transparent_quad_flush_ranges[2] = {
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->quads_vertex_buffer_memory,
                 .offset =
                     war_align64(sizeof(war_quad_vertex) * quad_vertices_count),
                 .size = war_align64(sizeof(war_quad_vertex) *
                                     transparent_quad_vertices_count)},
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->quads_index_buffer_memory,
                 .offset = war_align64(sizeof(uint16_t) * quad_indices_count),
                 .size = war_align64(sizeof(uint16_t) *
                                     transparent_quad_indices_count)}};
            vkFlushMappedMemoryRanges(
                ctx_vk->device, 2, transparent_quad_flush_ranges);
            VkDeviceSize transparent_quad_vertices_offsets[2] = {
                quad_vertices_count * sizeof(war_quad_vertex)};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   0,
                                   1,
                                   &ctx_vk->quads_vertex_buffer,
                                   transparent_quad_vertices_offsets);
            VkDeviceSize transparent_quad_instances_offsets[2] = {0};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   1,
                                   1,
                                   &ctx_vk->quads_instance_buffer,
                                   transparent_quad_instances_offsets);
            VkDeviceSize transparent_quad_indices_offset =
                quad_indices_count * sizeof(uint16_t);
            vkCmdBindIndexBuffer(ctx_vk->cmd_buffer,
                                 ctx_vk->quads_index_buffer,
                                 transparent_quad_indices_offset,
                                 VK_INDEX_TYPE_UINT16);
            war_quad_push_constants transparent_quad_push_constants = {
                .bottom_left = {ctx_wr->left_col, ctx_wr->bottom_row},
                .physical_size = {physical_width, physical_height},
                .cell_size = {ctx_wr->cell_width, ctx_wr->cell_height},
                .zoom = ctx_wr->zoom_scale,
                .cell_offsets = {ctx_wr->num_cols_for_line_numbers,
                                 ctx_wr->num_rows_for_status_bars},
                .scroll_margin = {ctx_wr->scroll_margin_cols,
                                  ctx_wr->scroll_margin_rows},
                .anchor_cell = {ctx_wr->cursor_pos_x, ctx_wr->cursor_pos_y},
                .top_right = {ctx_wr->right_col, ctx_wr->top_row},
            };
            vkCmdPushConstants(ctx_vk->cmd_buffer,
                               ctx_vk->pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(war_quad_push_constants),
                               &transparent_quad_push_constants);
            vkCmdDrawIndexed(
                ctx_vk->cmd_buffer, transparent_quad_indices_count, 1, 0, 0, 0);
            //---------------------------------------------------------
            // TEXT PIPELINE
            //---------------------------------------------------------
            vkCmdBindPipeline(ctx_vk->cmd_buffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              ctx_vk->text_pipeline);
            vkCmdBindDescriptorSets(ctx_vk->cmd_buffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    ctx_vk->text_pipeline_layout,
                                    0,
                                    1,
                                    &ctx_vk->font_descriptor_set,
                                    0,
                                    NULL);
            //---------------------------------------------------------
            // STATUS BARS
            //---------------------------------------------------------
            for (float col = 0; col < ctx_wr->viewport_cols; col++) {
                if (ctx_status->top[(int)col] != 0) {
                    war_make_text_quad(
                        text_vertices,
                        text_indices,
                        &text_vertices_count,
                        &text_indices_count,
                        (float[3]){col + ctx_wr->left_col,
                                   2 + ctx_wr->bottom_row,
                                   ctx_wr->layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr->white_hex,
                        &ctx_vk->glyphs[(int)ctx_status->top[(int)col]],
                        ctx_wr->text_thickness,
                        ctx_wr->text_feather,
                        0);
                }
                if (ctx_status->middle[(int)col] != 0) {
                    war_make_text_quad(
                        text_vertices,
                        text_indices,
                        &text_vertices_count,
                        &text_indices_count,
                        (float[3]){col + ctx_wr->left_col,
                                   1 + ctx_wr->bottom_row,
                                   ctx_wr->layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr->red_hex,
                        &ctx_vk->glyphs[(int)ctx_status->middle[(int)col]],
                        ctx_wr->text_thickness_bold,
                        ctx_wr->text_feather_bold,
                        0);
                }
                if (ctx_status->bottom[(int)col] != 0) {
                    war_make_text_quad(
                        text_vertices,
                        text_indices,
                        &text_vertices_count,
                        &text_indices_count,
                        (float[3]){col + ctx_wr->left_col,
                                   ctx_wr->bottom_row,
                                   ctx_wr->layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr->full_white_hex,
                        &ctx_vk->glyphs[(int)ctx_status->bottom[(int)col]],
                        ctx_wr->text_thickness,
                        ctx_wr->text_feather,
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
            for (uint32_t row = ctx_wr->bottom_row;
                 row <= ctx_wr->top_row &&
                 ctx_wr->hud_state != HUD_LINE_NUMBERS;
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
                    (float[3]){1 + ctx_wr->left_col,
                               row + ctx_wr->num_rows_for_status_bars,
                               ctx_wr->layers[LAYER_HUD_TEXT]},
                    (float[2]){1, 1},
                    ctx_wr->black_hex,
                    &ctx_vk->glyphs[piano_notes[i_piano_notes][0]],
                    ctx_wr->text_thickness,
                    ctx_wr->text_feather,
                    0);
                war_make_text_quad(
                    text_vertices,
                    text_indices,
                    &text_vertices_count,
                    &text_indices_count,
                    (float[3]){2 + ctx_wr->left_col,
                               row + ctx_wr->num_rows_for_status_bars,
                               ctx_wr->layers[LAYER_HUD_TEXT]},
                    (float[2]){1, 1},
                    ctx_wr->black_hex,
                    &ctx_vk->glyphs['0' + octave],
                    ctx_wr->text_thickness,
                    ctx_wr->text_feather,
                    0);
            }
            // draw line number text
            for (uint32_t row = ctx_wr->bottom_row;
                 row <= ctx_wr->top_row && ctx_wr->hud_state != HUD_PIANO;
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
                        (float[3]){ctx_wr->left_col + col,
                                   row + ctx_wr->num_rows_for_status_bars,
                                   ctx_wr->layers[LAYER_HUD_TEXT]},
                        (float[2]){1, 1},
                        ctx_wr->full_white_hex,
                        &ctx_vk->glyphs['0' + digits[col - ln_offset]],
                        ctx_wr->text_thickness,
                        ctx_wr->text_feather,
                        0);
                }
            }
            memcpy(ctx_vk->text_vertex_buffer_mapped,
                   text_vertices,
                   sizeof(war_text_vertex) * text_vertices_count);
            memcpy(ctx_vk->text_index_buffer_mapped,
                   text_indices,
                   sizeof(uint16_t) * text_indices_count);
            VkMappedMemoryRange text_flush_ranges[2] = {
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->text_vertex_buffer_memory,
                 .offset = 0,
                 .size = war_align64(sizeof(war_text_vertex) *
                                     text_vertices_count)},
                {.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                 .memory = ctx_vk->text_index_buffer_memory,
                 .offset = 0,
                 .size = war_align64(sizeof(uint16_t) * text_indices_count)}};
            vkFlushMappedMemoryRanges(ctx_vk->device, 2, text_flush_ranges);
            VkDeviceSize text_vertices_offsets[2] = {0};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   0,
                                   1,
                                   &ctx_vk->text_vertex_buffer,
                                   text_vertices_offsets);
            VkDeviceSize text_instances_offsets[2] = {0};
            vkCmdBindVertexBuffers(ctx_vk->cmd_buffer,
                                   1,
                                   1,
                                   &ctx_vk->text_instance_buffer,
                                   text_instances_offsets);
            VkDeviceSize text_indices_offset = 0;
            vkCmdBindIndexBuffer(ctx_vk->cmd_buffer,
                                 ctx_vk->text_index_buffer,
                                 text_indices_offset,
                                 VK_INDEX_TYPE_UINT16);
            war_text_push_constants text_push_constants = {
                .bottom_left = {ctx_wr->left_col, ctx_wr->bottom_row},
                .physical_size = {physical_width, physical_height},
                .cell_size = {ctx_wr->cell_width, ctx_wr->cell_height},
                .zoom = ctx_wr->zoom_scale,
                .cell_offsets = {ctx_wr->num_cols_for_line_numbers,
                                 ctx_wr->num_rows_for_status_bars},
                .scroll_margin = {ctx_wr->scroll_margin_cols,
                                  ctx_wr->scroll_margin_rows},
                .anchor_cell = {ctx_wr->cursor_pos_x, ctx_wr->cursor_pos_y},
                .top_right = {ctx_wr->right_col, ctx_wr->top_row},
                .ascent = ctx_vk->ascent,
                .descent = ctx_vk->descent,
                .line_gap = ctx_vk->line_gap,
                .baseline = ctx_vk->baseline,
                .font_height = ctx_vk->font_height,
            };
            vkCmdPushConstants(ctx_vk->cmd_buffer,
                               ctx_vk->text_pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(war_text_push_constants),
                               &text_push_constants);
            vkCmdDrawIndexed(
                ctx_vk->cmd_buffer, text_indices_count, 1, 0, 0, 0);
            //---------------------------------------------------------
            //   END RENDER PASS
            //---------------------------------------------------------
            vkCmdEndRenderPass(ctx_vk->cmd_buffer);
            result = vkEndCommandBuffer(ctx_vk->cmd_buffer);
            assert(result == VK_SUCCESS);
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &ctx_vk->cmd_buffer,
                .waitSemaphoreCount = 0,
                .pWaitSemaphores = NULL,
                .signalSemaphoreCount = 0,
                .pSignalSemaphores = NULL,
            };
            result =
                vkQueueSubmit(ctx_vk->queue,
                              1,
                              &submit_info,
                              ctx_vk->in_flight_fences[ctx_vk->current_frame]);
            assert(result == VK_SUCCESS);
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
        }
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
                war_wayland_wl_surface_frame(fd, wl_surface_id, wl_callback_id);
            }
            goto wayland_done;
        wl_buffer_release:
            // dump_bytes("wl_buffer_release event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto wayland_done;
        xdg_wm_base_ping:
            dump_bytes(
                "xdg_wm_base_ping event", msg_buffer + msg_buffer_offset, size);
            assert(size == 12);
            uint8_t pong[12];
            war_write_le32(pong, xdg_wm_base_id);
            war_write_le16(pong + 4, 3);
            war_write_le16(pong + 6, 12);
            war_write_le32(pong + 8,
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
            war_write_le32(ack_configure + 8,
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
                call_terry_davis("bound: wp_viewport");
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
                obj_op[wl_callback_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_callback_done;
                new_id++;
            }
            war_wayland_wl_surface_commit(fd, wl_surface_id);
            goto wayland_done;
        xdg_toplevel_configure:
            // dump_bytes("xdg_toplevel_configure event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            uint32_t width = *(uint32_t*)(msg_buffer + msg_buffer_offset + 0);
            uint32_t height = *(uint32_t*)(msg_buffer + msg_buffer_offset + 4);
            // States array starts at offset 8
            uint8_t* states_ptr = msg_buffer + msg_buffer_offset + 8;
            size_t states_bytes =
                size - 12; // subtract object_id/opcode/length + width/height
            size_t num_states = states_bytes / 4;
            ctx_wr->fullscreen = false;
            for (size_t i = 0; i < num_states; i++) {
                uint32_t state = *(uint32_t*)(states_ptr + i * 4);
                if (state == 2) { // XDG_TOPLEVEL_STATE_FULLSCREEN
                    ctx_wr->fullscreen = true;
                    // call_terry_davis("true fullscreen");
                    break;
                }
            }
            if (ctx_wr->fullscreen) {
                war_wl_surface_set_opaque_region(fd, wl_surface_id, 0);
                ctx_wr->text_feather = default_text_feather;
                ctx_wr->text_thickness = default_text_thickness;
                ctx_wr->alpha_scale = default_alpha_scale;
                ctx_wr->alpha_scale_cursor = default_cursor_alpha_scale;
                goto wayland_done;
            }
            war_wl_surface_set_opaque_region(fd, wl_surface_id, wl_region_id);
            ctx_wr->text_feather = windowed_text_feather;
            ctx_wr->text_thickness = windowed_text_thickness;
            ctx_wr->alpha_scale = default_windowed_alpha_scale;
            ctx_wr->alpha_scale_cursor = default_windowed_cursor_alpha_scale;
            goto wayland_done;
        xdg_toplevel_close:
            // dump_bytes("xdg_toplevel_close event", msg_buffer +
            // msg_buffer_offset, size);

            uint8_t xdg_toplevel_destroy[8];
            war_write_le32(xdg_toplevel_destroy, xdg_toplevel_id);
            war_write_le16(xdg_toplevel_destroy + 4, 0);
            war_write_le16(xdg_toplevel_destroy + 6, 8);
            ssize_t xdg_toplevel_destroy_written =
                write(fd, xdg_toplevel_destroy, 8);
            // dump_bytes("xdg_toplevel::destroy request",
            // xdg_toplevel_destroy, 8);
            assert(xdg_toplevel_destroy_written == 8);

            uint8_t xdg_surface_destroy[8];
            war_write_le32(xdg_surface_destroy, xdg_surface_id);
            war_write_le16(xdg_surface_destroy + 4, 0);
            war_write_le16(xdg_surface_destroy + 6, 8);
            ssize_t xdg_surface_destroy_written =
                write(fd, xdg_surface_destroy, 8);
            // dump_bytes("xdg_surface::destroy request",
            // xdg_surface_destroy, 8);
            assert(xdg_surface_destroy_written == 8);

            uint8_t wl_buffer_destroy[8];
            war_write_le32(wl_buffer_destroy, wl_buffer_id);
            war_write_le16(wl_buffer_destroy + 4, 0);
            war_write_le16(wl_buffer_destroy + 6, 8);
            ssize_t wl_buffer_destroy_written = write(fd, wl_buffer_destroy, 8);
            // dump_bytes("wl_buffer::destroy request", wl_buffer_destroy,
            // 8);
            assert(wl_buffer_destroy_written == 8);

            uint8_t wl_region_destroy[8];
            war_write_le32(wl_region_destroy, wl_region_id);
            war_write_le16(wl_region_destroy + 4, 0);
            war_write_le16(wl_region_destroy + 6, 8);
            ssize_t wl_region_destroy_written = write(fd, wl_region_destroy, 8);
            // dump_bytes("wl_region::destroy request", wl_region_destroy,
            // 8);
            assert(wl_region_destroy_written == 8);

            uint8_t wl_surface_destroy[8];
            war_write_le32(wl_surface_destroy, wl_surface_id);
            war_write_le16(wl_surface_destroy + 4, 0);
            war_write_le16(wl_surface_destroy + 6, 8);
            ssize_t wl_surface_destroy_written =
                write(fd, wl_surface_destroy, 8);
            // dump_bytes("wl_surface::destroy request",
            // wl_surface_destroy, 8);
            assert(wl_surface_destroy_written == 8);

            war_pc_to_a(pc_control, CONTROL_END_WAR, 0, NULL);
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
            dump_bytes("zwp_linux_buffer_params_v1_created", // COMMENT
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
            dump_bytes(
                "zwp_linux_dmabuf_v1_create_params request", create_params, 12);
            call_terry_davis("bound: zwp_linux_buffer_params_v1");
            ssize_t create_params_written = write(fd, create_params, 12);
            assert(create_params_written == 12);
            zwp_linux_buffer_params_v1_id = new_id;
            obj_op[zwp_linux_buffer_params_v1_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   0] = &&zwp_linux_buffer_params_v1_created;
            obj_op[zwp_linux_buffer_params_v1_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   1] = &&zwp_linux_buffer_params_v1_failed;
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
            *((int*)CMSG_DATA(cmsg)) = ctx_vk->dmabuf_fd;
            ssize_t dmabuf_sent = sendmsg(fd, &msg, 0);
            if (dmabuf_sent < 0) perror("sendmsg");
            assert(dmabuf_sent == 28);
#if DEBUG
            uint8_t full_msg[32] = {0};
            memcpy(full_msg, header, 8);
            memcpy(full_msg + 8, tail, 20);
#endif
            dump_bytes("zwp_linux_buffer_params_v1::add request", full_msg, 28);

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
            call_terry_davis("bound: wl_buffer");
            ssize_t create_immed_written = write(fd, create_immed, 28);
            assert(create_immed_written == 28);
            wl_buffer_id = new_id;
            obj_op[wl_buffer_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   0] = &&wl_buffer_release;
            new_id++;

            uint8_t destroy[8];
            war_write_le32(destroy, zwp_linux_buffer_params_v1_id);
            war_write_le16(destroy + 4, 0);
            war_write_le16(destroy + 6, 8);
            ssize_t destroy_written = write(fd, destroy, 8);
            assert(destroy_written == 8);
            dump_bytes(
                "zwp_linux_buffer_params_v1_id::destroy request", destroy, 8);
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
            dump_bytes(
                "wl_surface_enter event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_surface_leave:
            dump_bytes(
                "wl_surface_leave event", msg_buffer + msg_buffer_offset, size);
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
            war_write_le32(set_buffer_scale + 8,
                           war_read_le32(msg_buffer + msg_buffer_offset + 8));
            dump_bytes(
                "wl_surface::set_buffer_scale request", set_buffer_scale, 12);
            ssize_t set_buffer_scale_written = write(fd, set_buffer_scale, 12);
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
            war_write_le32(set_buffer_transform + 8,
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
            dump_bytes(
                "zxdg_toplevel_decoration_v1::set_mode request", set_mode, 12);
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
                call_terry_davis("keyboard detected");
                assert(size == 12);
                uint8_t get_keyboard[12];
                war_write_le32(get_keyboard, wl_seat_id);
                war_write_le16(get_keyboard + 4, 1);
                war_write_le16(get_keyboard + 6, 12);
                war_write_le32(get_keyboard + 8, new_id);
                dump_bytes("get_keyboard request", get_keyboard, 12);
                call_terry_davis("bound: wl_keyboard",
                                 (const char*)msg_buffer + msg_buffer_offset +
                                     12);
                ssize_t get_keyboard_written = write(fd, get_keyboard, 12);
                assert(get_keyboard_written == 12);
                wl_keyboard_id = new_id;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_keyboard_keymap;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_keyboard_enter;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&wl_keyboard_leave;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&wl_keyboard_key;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&wl_keyboard_modifiers;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&wl_keyboard_repeat_info;
                new_id++;
            }
            if (capabilities & wl_seat_pointer) {
                call_terry_davis("pointer detected");
                assert(size == 12);
                uint8_t get_pointer[12];
                war_write_le32(get_pointer, wl_seat_id);
                war_write_le16(get_pointer + 4, 0);
                war_write_le16(get_pointer + 6, 12);
                war_write_le32(get_pointer + 8, new_id);
                dump_bytes("get_pointer request", get_pointer, 12);
                call_terry_davis("bound: wl_pointer");
                ssize_t get_pointer_written = write(fd, get_pointer, 12);
                assert(get_pointer_written == 12);
                wl_pointer_id = new_id;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_pointer_enter;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_pointer_leave;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&wl_pointer_motion;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&wl_pointer_button;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&wl_pointer_axis;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&wl_pointer_frame;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] = &&wl_pointer_axis_source;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       7] = &&wl_pointer_axis_stop;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       8] = &&wl_pointer_axis_discrete;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       9] = &&wl_pointer_axis_value120;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       10] = &&wl_pointer_axis_relative_direction;
                new_id++;
            }
            if (capabilities & wl_seat_touch) {
                call_terry_davis("touch detected");
                assert(size == 12);
                uint8_t get_touch[12];
                war_write_le32(get_touch, wl_seat_id);
                war_write_le16(get_touch + 4, 2);
                war_write_le16(get_touch + 6, 12);
                war_write_le32(get_touch + 8, new_id);
                dump_bytes("get_touch request", get_touch, 12);
                call_terry_davis("bound: wl_touch");
                ssize_t get_touch_written = write(fd, get_touch, 12);
                assert(get_touch_written == 12);
                wl_touch_id = new_id;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&wl_touch_down;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&wl_touch_up;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&wl_touch_motion;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&wl_touch_frame;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&wl_touch_cancel;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&wl_touch_shape;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] = &&wl_touch_orientation;
                new_id++;
            }
            goto wayland_done;
        wl_seat_name:
            dump_bytes(
                "wl_seat_name event", msg_buffer + msg_buffer_offset, size);
            call_terry_davis("seat: %s",
                             (const char*)msg_buffer + msg_buffer_offset + 12);
            goto wayland_done;
        wl_keyboard_keymap: {
            dump_bytes("wl_keyboard_keymap event", msg_buffer, size);
            assert(size == 16);
            ctx_fsm->keymap_fd = -1;
            for (struct cmsghdr* poll_cmsg = CMSG_FIRSTHDR(&poll_msg_hdr);
                 poll_cmsg != NULL;
                 poll_cmsg = CMSG_NXTHDR(&poll_msg_hdr, poll_cmsg)) {
                if (poll_cmsg->cmsg_level == SOL_SOCKET &&
                    poll_cmsg->cmsg_type == SCM_RIGHTS) {
                    ctx_fsm->keymap_fd = *(int*)CMSG_DATA(poll_cmsg);
                    break;
                }
            }
            assert(ctx_fsm->keymap_fd >= 0);
            ctx_fsm->keymap_format =
                war_read_le32(msg_buffer + msg_buffer_offset + 8);
            assert(ctx_fsm->keymap_format == XKB_KEYMAP_FORMAT_TEXT_V1);
            ctx_fsm->keymap_size =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4);
            assert(ctx_fsm->keymap_size > 0);
            //-----------------------------------------------------------------
            // FSM INIT
            //-----------------------------------------------------------------
            ctx_fsm->xkb_context = xkb_context_new(0);
            assert(ctx_fsm->xkb_context);
            ctx_fsm->keymap_map = mmap(NULL,
                                       ctx_fsm->keymap_size,
                                       PROT_READ,
                                       MAP_PRIVATE,
                                       ctx_fsm->keymap_fd,
                                       0);
            assert(ctx_fsm->keymap_map != MAP_FAILED);
            ctx_fsm->xkb_keymap =
                xkb_keymap_new_from_string(ctx_fsm->xkb_context,
                                           ctx_fsm->keymap_map,
                                           XKB_KEYMAP_FORMAT_TEXT_V1,
                                           0);
            assert(ctx_fsm->xkb_keymap);
            ctx_fsm->xkb_state = xkb_state_new(ctx_fsm->xkb_keymap);
            assert(ctx_fsm->xkb_state);
            ctx_fsm->mod_shift = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                          XKB_MOD_NAME_SHIFT);
            ctx_fsm->mod_ctrl = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_CTRL);
            ctx_fsm->mod_alt =
                xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap, XKB_MOD_NAME_ALT);
            ctx_fsm->mod_logo = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_LOGO);
            ctx_fsm->mod_caps = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_CAPS);
            ctx_fsm->mod_num =
                xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap, XKB_MOD_NAME_NUM);
            //-----------------------------------------------------------------
            // LUA DEFAULT MODES
            //-----------------------------------------------------------------
            lua_getglobal(ctx_lua->L, "war");
            if (!lua_istable(ctx_lua->L, -1)) {
                call_terry_davis("war not a table");
                lua_pop(ctx_lua->L, 1);
            }
            lua_getfield(ctx_lua->L, -1, "modes");
            if (!lua_istable(ctx_lua->L, -1)) {
                call_terry_davis("modes not a field");
                lua_pop(ctx_lua->L, 2);
            }
            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                const char* mode_name = lua_tostring(ctx_lua->L, -2);
                int mode_value = (int)lua_tointeger(ctx_lua->L, -1);
                if (strcmp(mode_name, "roll") == 0) {
                    ctx_fsm->MODE_ROLL = mode_value;
                } else if (strcmp(mode_name, "views") == 0) {
                    ctx_fsm->MODE_VIEWS = mode_value;
                } else if (strcmp(mode_name, "visual_line") == 0) {
                    ctx_fsm->MODE_VISUAL_LINE = mode_value;
                } else if (strcmp(mode_name, "capture") == 0) {
                    ctx_fsm->MODE_CAPTURE = mode_value;
                } else if (strcmp(mode_name, "midi") == 0) {
                    ctx_fsm->MODE_MIDI = mode_value;
                } else if (strcmp(mode_name, "command") == 0) {
                    ctx_fsm->MODE_COMMAND = mode_value;
                } else if (strcmp(mode_name, "visual_block") == 0) {
                    ctx_fsm->MODE_VISUAL_BLOCK = mode_value;
                } else if (strcmp(mode_name, "insert") == 0) {
                    ctx_fsm->MODE_INSERT = mode_value;
                } else if (strcmp(mode_name, "o") == 0) {
                    ctx_fsm->MODE_O = mode_value;
                } else if (strcmp(mode_name, "visual") == 0) {
                    ctx_fsm->MODE_VISUAL = mode_value;
                } else if (strcmp(mode_name, "wav") == 0) {
                    ctx_fsm->MODE_WAV = mode_value;
                }
                call_terry_davis("mode: %s, value: %i", mode_name, mode_value);
                lua_pop(ctx_lua->L, 1);
            }
            lua_pop(ctx_lua->L, 2);
            //-----------------------------------------------------------------
            // LUA DEFAULT FUNCTION TYPES
            //-----------------------------------------------------------------
            lua_getglobal(ctx_lua->L, "war");
            if (!lua_istable(ctx_lua->L, -1)) {
                call_terry_davis("war not a table");
                lua_pop(ctx_lua->L, 1);
            }
            lua_getfield(ctx_lua->L, -1, "function_types");
            if (!lua_istable(ctx_lua->L, -1)) {
                call_terry_davis("function_types not a field");
                lua_pop(ctx_lua->L, 2);
            }
            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                const char* type_name = lua_tostring(ctx_lua->L, -2);
                int type_value = (int)lua_tointeger(ctx_lua->L, -1);
                if (strcmp(type_name, "none") == 0) {
                    ctx_fsm->FUNCTION_NONE = type_value;
                } else if (strcmp(type_name, "c") == 0) {
                    ctx_fsm->FUNCTION_C = type_value;
                } else if (strcmp(type_name, "lua") == 0) {
                    ctx_fsm->FUNCTION_LUA = type_value;
                }
                call_terry_davis("type: %s, value: %i", type_name, type_value);
                lua_pop(ctx_lua->L, 1);
            }
            lua_pop(ctx_lua->L, 2);
            //-----------------------------------------------------------------
            // LUA DEFAULT KEYMAP
            //-----------------------------------------------------------------
            lua_getglobal(ctx_lua->L, "war_flattened");
            if (!lua_istable(ctx_lua->L, -1)) {
                call_terry_davis("war_flattened not a table");
                lua_pop(ctx_lua->L, 1);
                goto wayland_done;
            }

            uint16_t max_states = ctx_fsm->state_count;
            uint8_t state_has_children[max_states];
            memset(state_has_children, 0, sizeof(state_has_children));
            uint16_t next_state_counter = 1;
            uint32_t sequence_count = 0;

            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                if (!lua_istable(ctx_lua->L, -1)) {
                    lua_pop(ctx_lua->L, 1);
                    continue;
                }

                lua_getfield(ctx_lua->L, -1, "keys");
                int idx_keys = lua_gettop(ctx_lua->L);
                lua_getfield(ctx_lua->L, -2, "commands");
                int idx_commands = lua_gettop(ctx_lua->L);
                if (!lua_istable(ctx_lua->L, idx_keys) ||
                    !lua_istable(ctx_lua->L, idx_commands)) {
                    lua_pop(ctx_lua->L, 2);
                    lua_pop(ctx_lua->L, 1);
                    continue;
                }

                uint16_t current_state = 0;
                size_t keys_len = lua_objlen(ctx_lua->L, idx_keys);
                for (size_t i = 1; i <= keys_len; i++) {
                    lua_rawgeti(ctx_lua->L, idx_keys, (int)i);
                    const char* token = lua_tostring(ctx_lua->L, -1);
                    uint16_t keysym = 0;
                    uint8_t mod = 0;
                    if (token &&
                        war_parse_token_to_keysym_mod(token, &keysym, &mod)) {
                        size_t idx3d = FSM_3D_INDEX(current_state, keysym, mod);
                        uint16_t next_state = ctx_fsm->next_state[idx3d];
                        if (next_state == 0) {
                            if (next_state_counter < max_states) {
                                next_state = next_state_counter++;
                            } else {
                                next_state = max_states - 1;
                                call_terry_davis("fsm state overflow");
                            }
                            ctx_fsm->next_state[idx3d] = next_state;
                        }
                        if (next_state != current_state) {
                            state_has_children[current_state] = 1;
                        }
                        current_state = next_state;
                    } else {
                        call_terry_davis("invalid key token: %s",
                                         token ? token : "(nil)");
                    }
                    lua_pop(ctx_lua->L, 1);
                }

                sequence_count++;
                size_t commands_len = lua_objlen(ctx_lua->L, idx_commands);
                for (size_t j = 1; j <= commands_len; j++) {
                    lua_rawgeti(ctx_lua->L, idx_commands, (int)j);
                    if (!lua_istable(ctx_lua->L, -1)) {
                        lua_pop(ctx_lua->L, 1);
                        continue;
                    }

                    lua_getfield(ctx_lua->L, -1, "cmd");
                    const char* cmd_name = lua_tostring(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "mode");
                    int mode = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "type");
                    int type = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "handle_release");
                    int handle_release = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "handle_repeat");
                    int handle_repeat = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "handle_timeout");
                    int handle_timeout = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    if (cmd_name && mode < (int)ctx_fsm->mode_count) {
                        size_t mode_idx = FSM_2D_MODE(current_state, mode);
                        ctx_fsm->is_terminal[mode_idx] = 1;
                        ctx_fsm->handle_release[mode_idx] = handle_release;
                        ctx_fsm->handle_repeat[mode_idx] = handle_repeat;
                        ctx_fsm->handle_timeout[mode_idx] = handle_timeout;
                        ctx_fsm->type[mode_idx] = type;
                        size_t name_offset = FSM_3D_NAME(current_state, mode);
                        strncpy(ctx_fsm->name + name_offset,
                                cmd_name,
                                ctx_fsm->name_limit - 1);
                        ctx_fsm->name[name_offset + ctx_fsm->name_limit - 1] =
                            '\0';
                        if (type == ctx_fsm->FUNCTION_C) {
                            ctx_fsm->function[mode_idx].c =
                                war_get_keymap_function(cmd_name);
                        }
                    }

                    lua_pop(ctx_lua->L, 1);
                }

                lua_pop(ctx_lua->L, 2); // commands, keys
                lua_pop(ctx_lua->L, 1); // entry table
            }

            lua_pop(ctx_lua->L, 1); // war_flattened table

            for (uint16_t s = 0; s < next_state_counter; s++) {
                if (!state_has_children[s]) { continue; }
                for (uint32_t m = 0; m < ctx_fsm->mode_count; m++) {
                    ctx_fsm->is_prefix[FSM_2D_MODE(s, m)] = 1;
                }
            }

            atomic_store(&ctx_lua->WR_SEQUENCE_COUNT, sequence_count);
            goto wayland_done;
        }
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
        wl_keyboard_key: {
            // dump_bytes("wl_keyboard_key event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            if (ctx_wr->end_window_render) { goto wayland_done; }
            uint32_t wl_key_state =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4);
            uint32_t keycode =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4) +
                8; // + 8 cuz wayland
            xkb_keysym_t keysym =
                xkb_state_key_get_one_sym(ctx_fsm->xkb_state, keycode);
            // TODO: Normalize the other things like '<' (regarding
            // MOD_SHIFT and XKB)
            keysym = war_normalize_keysym(keysym); // normalize to lowercase and
                                                   // prevent out of bounds
            xkb_mod_mask_t mods = xkb_state_serialize_mods(
                ctx_fsm->xkb_state, XKB_STATE_MODS_DEPRESSED);
            uint8_t mod = 0;
            if (mods & (1 << ctx_fsm->mod_shift)) mod |= MOD_SHIFT;
            if (mods & (1 << ctx_fsm->mod_ctrl)) mod |= MOD_CTRL;
            if (mods & (1 << ctx_fsm->mod_alt)) mod |= MOD_ALT;
            if (mods & (1 << ctx_fsm->mod_logo)) mod |= MOD_LOGO;
            if (mods & (1 << ctx_fsm->mod_caps)) mod |= MOD_CAPS;
            if (mods & (1 << ctx_fsm->mod_num)) mod |= MOD_NUM;
            if (keysym == KEYSYM_DEFAULT) {
                // repeats
                ctx_fsm->key_down[FSM_3D_INDEX(ctx_fsm->current_state,
                                               ctx_fsm->repeat_keysym,
                                               ctx_fsm->repeat_mod)] = false;
                ctx_fsm->repeat_keysym = 0;
                ctx_fsm->repeat_mod = 0;
                ctx_fsm->repeating = false;
                goto cmd_done;
            }
            // if (mods & (1 << mod_fn)) mod |= MOD_FN;
            bool pressed = (wl_key_state == 1);
            if (!pressed) {
                ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = false;
                ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = 0;
                // repeats
                if (ctx_fsm->repeat_keysym == keysym &&
                    ctx_fsm->handle_repeat[FSM_2D_MODE(
                        ctx_fsm->current_state, ctx_fsm->current_mode)] &&
                    !ctx_command->command) {
                    ctx_fsm->repeat_keysym = 0;
                    ctx_fsm->repeat_mod = 0;
                    ctx_fsm->repeating = false;
                    // timeouts
                    ctx_fsm->timeout = false;
                    ctx_fsm->timeout_state_index = 0;
                    ctx_fsm->timeout_start_us = 0;
                }
                if (ctx_wr->skip_release) {
                    ctx_wr->skip_release = 0;
                    goto cmd_done;
                }
                uint16_t idx = ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)];
                if (idx && ctx_fsm->handle_release[FSM_2D_MODE(
                               idx, ctx_fsm->current_mode)]) {
                    war_fsm_execute_command(env, ctx_fsm, idx);
                }
                goto cmd_done;
            }
            if (!ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)]) {
                ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = 1;
                ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = ctx_wr->now;
            }
            //-----------------------------------------------------------------
            // COMMAND INPUT
            //-----------------------------------------------------------------
            if (ctx_command->command && pressed) {
                // Write to input buffer for command mode
                if (ctx_command->input_write_index < ctx_command->capacity) {
                    int keysym_to_int = war_keysym_to_int(keysym, mod);
                    ctx_command->input[ctx_command->input_write_index] =
                        keysym_to_int;
                    ctx_command->input_write_index++;
                }
                // SET UP REPEATS FOR COMMAND MODE
                ctx_fsm->repeat_keysym = keysym;
                ctx_fsm->repeat_mod = mod;
                ctx_fsm->repeating = 0;
                ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = ctx_wr->now;
                goto cmd_done;
            }
            uint16_t next_state_index = ctx_fsm->next_state[FSM_3D_INDEX(
                ctx_fsm->current_state, keysym, mod)];
            if (ctx_fsm->timeout &&
                ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->timeout_state_index, keysym, mod)]) {
                next_state_index = ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->timeout_state_index, keysym, mod)];
            }
            if (next_state_index == 0) {
                ctx_fsm->current_state = 0;
                // timeouts
                ctx_fsm->timeout = 0;
                ctx_fsm->timeout_state_index = 0;
                ctx_fsm->timeout_start_us = 0;
                goto cmd_done;
            }
            ctx_fsm->current_state = next_state_index;
            ctx_fsm->state_last_event_us = ctx_wr->now;
            size_t mode_idx =
                FSM_2D_MODE(ctx_fsm->current_state, ctx_fsm->current_mode);
            if (ctx_fsm->is_terminal[mode_idx] &&
                !ctx_fsm->is_prefix[mode_idx]) {
                uint16_t temp = ctx_fsm->current_state;
                ctx_fsm->current_state = 0;
                // repeats
                if ((ctx_fsm->current_mode != ctx_fsm->MODE_MIDI ||
                     (ctx_fsm->current_mode == ctx_fsm->MODE_MIDI &&
                      ctx_wr->midi_toggle)) &&
                    ctx_fsm->handle_repeat[mode_idx]) {
                    ctx_fsm->repeat_keysym = keysym;
                    ctx_fsm->repeat_mod = mod;
                    ctx_fsm->repeating = 0;
                }
                // timeouts
                if (keysym != KEYSYM_ESCAPE && mod != 0) {
                    ctx_fsm->timeout_state_index = 0;
                }
                ctx_fsm->timeout_start_us = 0;
                ctx_fsm->timeout = 0;
                war_fsm_execute_command(env, ctx_fsm, temp);
            } else if (ctx_fsm->is_terminal[mode_idx] &&
                       ctx_fsm->is_prefix[mode_idx]) {
                if (ctx_fsm->handle_timeout[mode_idx]) {
                    // repeats
                    ctx_fsm->repeat_keysym = 0;
                    ctx_fsm->repeat_mod = 0;
                    ctx_fsm->repeating = 0;
                    // timeouts
                    ctx_fsm->timeout_state_index = ctx_fsm->current_state;
                    ctx_fsm->timeout_start_us = ctx_wr->now;
                    ctx_fsm->timeout = 1;
                    ctx_fsm->current_state = 0;
                    goto cmd_done;
                }
                uint16_t temp = ctx_fsm->current_state;
                ctx_fsm->current_state = 0;
                // repeats
                if ((ctx_fsm->current_mode != ctx_fsm->MODE_MIDI ||
                     (ctx_fsm->current_mode == ctx_fsm->MODE_MIDI &&
                      ctx_wr->midi_toggle)) &&
                    ctx_fsm->handle_repeat[mode_idx]) {
                    ctx_fsm->repeat_keysym = keysym;
                    ctx_fsm->repeat_mod = mod;
                    ctx_fsm->repeating = 0;
                }
                // timeouts
                if (keysym != KEYSYM_ESCAPE && mod != 0) {
                    ctx_fsm->timeout_state_index = 0;
                }
                ctx_fsm->timeout_start_us = 0;
                ctx_fsm->timeout = false;
                war_fsm_execute_command(env, ctx_fsm, temp);
            }
            goto cmd_done;
        }
        cmd_normal_k:
            call_terry_davis("cmd_normal_k");
            uint32_t increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            uint32_t scaled_whole =
                (increment * ctx_wr->navigation_whole_number_row) /
                ctx_wr->navigation_sub_cells_row;
            uint32_t scaled_frac =
                (increment * ctx_wr->navigation_whole_number_row) %
                ctx_wr->navigation_sub_cells_row;
            ctx_wr->cursor_pos_y = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, scaled_whole, ctx_wr->max_row);
            ctx_wr->sub_row = war_clamp_add_uint32(
                ctx_wr->sub_row, scaled_frac, ctx_wr->max_row);
            ctx_wr->cursor_pos_y = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y,
                ctx_wr->sub_row / ctx_wr->navigation_sub_cells_row,
                ctx_wr->max_row);
            ctx_wr->sub_row = war_clamp_uint32(
                ctx_wr->sub_row % ctx_wr->navigation_sub_cells_row,
                ctx_wr->min_row,
                ctx_wr->max_row);
            if (ctx_wr->cursor_pos_y >
                ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_add_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->max_row);
                ctx_wr->top_row = war_clamp_add_uint32(
                    ctx_wr->top_row, increment, ctx_wr->max_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_j:
            call_terry_davis("cmd_normal_j");
            increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            scaled_whole = (increment * ctx_wr->navigation_whole_number_row) /
                           ctx_wr->navigation_sub_cells_row;
            scaled_frac = (increment * ctx_wr->navigation_whole_number_row) %
                          ctx_wr->navigation_sub_cells_row;
            ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, scaled_whole, ctx_wr->min_row);
            if (ctx_wr->sub_row < scaled_frac) {
                ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
                    ctx_wr->cursor_pos_y, 1, ctx_wr->min_row);
                ctx_wr->sub_row += ctx_wr->navigation_sub_cells_row;
            }
            ctx_wr->sub_row = war_clamp_subtract_uint32(
                ctx_wr->sub_row, scaled_frac, ctx_wr->min_row);
            ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y,
                ctx_wr->sub_row / ctx_wr->navigation_sub_cells_row,
                ctx_wr->min_row);
            ctx_wr->sub_row = war_clamp_uint32(
                ctx_wr->sub_row % ctx_wr->navigation_sub_cells_row,
                ctx_wr->min_row,
                ctx_wr->max_row);
            if (ctx_wr->cursor_pos_y <
                ctx_wr->bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->min_row);
                ctx_wr->top_row = war_clamp_subtract_uint32(
                    ctx_wr->top_row, increment, ctx_wr->min_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->top_row = war_clamp_add_uint32(
                        ctx_wr->top_row, diff, ctx_wr->max_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_l: {
            call_terry_davis("cmd_normal_l");
            double initial = ctx_wr->cursor_pos_x;
            double increment =
                (double)ctx_wr->col_increment * ctx_wr->cursor_navigation_x;
            if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
            ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x + increment;
            if (ctx_wr->cursor_pos_x > ctx_wr->max_col) {
                ctx_wr->cursor_pos_x = ctx_wr->max_col;
            }
            double pan = ctx_wr->cursor_pos_x - initial;
            if (ctx_wr->cursor_pos_x >
                ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_add_uint32(
                    ctx_wr->left_col, pan, ctx_wr->max_col);
                ctx_wr->right_col = war_clamp_add_uint32(
                    ctx_wr->right_col, pan, ctx_wr->max_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->left_col = war_clamp_subtract_uint32(
                        ctx_wr->left_col, diff, ctx_wr->min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_h: {
            call_terry_davis("cmd_normal_h");
            double initial = ctx_wr->cursor_pos_x;
            double increment =
                (double)ctx_wr->col_increment * ctx_wr->cursor_navigation_x;
            if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
            ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x - increment;
            if (ctx_wr->cursor_pos_x < ctx_wr->min_col) {
                ctx_wr->cursor_pos_x = ctx_wr->min_col;
            }
            double pan = initial - ctx_wr->cursor_pos_x;
            if (ctx_wr->cursor_pos_x <
                ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_subtract_uint32(
                    ctx_wr->left_col, pan, ctx_wr->min_col);
                ctx_wr->right_col = war_clamp_subtract_uint32(
                    ctx_wr->right_col, pan, ctx_wr->min_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->right_col = war_clamp_add_uint32(
                        ctx_wr->right_col, diff, ctx_wr->max_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_gd: {
            call_terry_davis("cmd_normal_gd");
            ctx_fsm->current_mode = ctx_fsm->MODE_WAV;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_K: {
            call_terry_davis("cmd_normal_K");
            float gain =
                atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
            gain = fminf(gain, 1.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_J: {
            call_terry_davis("cmd_normal_J");
            float gain =
                atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
            gain = fmaxf(gain, 0.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_k:
            call_terry_davis("cmd_normal_alt_k");
            increment = ctx_wr->row_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            ctx_wr->cursor_pos_y = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, increment, ctx_wr->max_row);
            if (ctx_wr->cursor_pos_y >
                ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_add_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->max_row);
                ctx_wr->top_row = war_clamp_add_uint32(
                    ctx_wr->top_row, increment, ctx_wr->max_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_j:
            call_terry_davis("cmd_normal_alt_j");
            increment = ctx_wr->row_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, increment, ctx_wr->min_row);
            if (ctx_wr->cursor_pos_y <
                ctx_wr->bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->min_row);
                ctx_wr->top_row = war_clamp_subtract_uint32(
                    ctx_wr->top_row, increment, ctx_wr->min_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->top_row = war_clamp_add_uint32(
                        ctx_wr->top_row, diff, ctx_wr->max_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_h: {
            call_terry_davis("cmd_normal_h");
            double initial = ctx_wr->cursor_pos_x;
            double increment = (double)ctx_wr->col_leap_increment *
                               ctx_wr->cursor_navigation_x;
            if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
            ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x - increment;
            if (ctx_wr->cursor_pos_x < ctx_wr->min_col) {
                ctx_wr->cursor_pos_x = ctx_wr->min_col;
            }
            double pan = initial - ctx_wr->cursor_pos_x;
            if (ctx_wr->cursor_pos_x <
                ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_subtract_uint32(
                    ctx_wr->left_col, pan, ctx_wr->min_col);
                ctx_wr->right_col = war_clamp_subtract_uint32(
                    ctx_wr->right_col, pan, ctx_wr->min_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->right_col = war_clamp_add_uint32(
                        ctx_wr->right_col, diff, ctx_wr->max_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_l: {
            call_terry_davis("cmd_normal_l");
            double initial = ctx_wr->cursor_pos_x;
            double increment = (double)ctx_wr->col_leap_increment *
                               ctx_wr->cursor_navigation_x;
            if (ctx_wr->numeric_prefix) { increment *= ctx_wr->numeric_prefix; }
            ctx_wr->cursor_pos_x = ctx_wr->cursor_pos_x + increment;
            if (ctx_wr->cursor_pos_x > ctx_wr->max_col) {
                ctx_wr->cursor_pos_x = ctx_wr->max_col;
            }
            double pan = ctx_wr->cursor_pos_x - initial;
            if (ctx_wr->cursor_pos_x >
                ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_add_uint32(
                    ctx_wr->left_col, pan, ctx_wr->max_col);
                ctx_wr->right_col = war_clamp_add_uint32(
                    ctx_wr->right_col, pan, ctx_wr->max_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->left_col = war_clamp_subtract_uint32(
                        ctx_wr->left_col, diff, ctx_wr->min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_i: {
            call_terry_davis("cmd_normal_i");
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_play->note_layers[(int)ctx_wr->cursor_pos_y] = layer;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_L: {
            call_terry_davis("cmd_normal_L");
            war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_tab: {
            call_terry_davis("cmd_normal_alt_tab");
            ctx_wr->layer_flux = !ctx_wr->layer_flux;
            war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_K:
            call_terry_davis("cmd_normal_alt_shift_k");
            increment =
                ctx_wr->viewport_rows - ctx_wr->num_rows_for_status_bars;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            ctx_wr->cursor_pos_y = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, increment, ctx_wr->max_row);
            if (ctx_wr->cursor_pos_y >
                ctx_wr->top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_add_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->max_row);
                ctx_wr->top_row = war_clamp_add_uint32(
                    ctx_wr->top_row, increment, ctx_wr->max_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_J:
            call_terry_davis("cmd_normal_alt_shift_j");
            increment =
                ctx_wr->viewport_rows - ctx_wr->num_rows_for_status_bars;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_row);
            }
            ctx_wr->cursor_pos_y = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, increment, ctx_wr->min_row);
            if (ctx_wr->cursor_pos_y <
                ctx_wr->bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->bottom_row, increment, ctx_wr->min_row);
                ctx_wr->top_row = war_clamp_subtract_uint32(
                    ctx_wr->top_row, increment, ctx_wr->min_row);
                uint32_t new_viewport_height =
                    ctx_wr->top_row - ctx_wr->bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    ctx_wr->top_row = war_clamp_add_uint32(
                        ctx_wr->top_row, diff, ctx_wr->max_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_L:
            call_terry_davis("cmd_normal_alt_shift_l");
            increment =
                ctx_wr->viewport_cols - ctx_wr->num_cols_for_line_numbers;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_col);
            }
            ctx_wr->cursor_pos_x = war_clamp_add_uint32(
                ctx_wr->cursor_pos_x, increment, ctx_wr->max_col);
            if (ctx_wr->cursor_pos_x >
                ctx_wr->right_col - ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_add_uint32(
                    ctx_wr->left_col, increment, ctx_wr->max_col);
                ctx_wr->right_col = war_clamp_add_uint32(
                    ctx_wr->right_col, increment, ctx_wr->max_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->left_col = war_clamp_subtract_uint32(
                        ctx_wr->left_col, diff, ctx_wr->min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_H:
            call_terry_davis("cmd_normal_alt_shift_h");
            increment =
                ctx_wr->viewport_cols - ctx_wr->num_cols_for_line_numbers;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, ctx_wr->max_col);
            }
            ctx_wr->cursor_pos_x = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_x, increment, ctx_wr->min_col);
            if (ctx_wr->cursor_pos_x <
                ctx_wr->left_col + ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                ctx_wr->left_col = war_clamp_subtract_uint32(
                    ctx_wr->left_col, increment, ctx_wr->min_col);
                ctx_wr->right_col = war_clamp_subtract_uint32(
                    ctx_wr->right_col, increment, ctx_wr->min_col);
                uint32_t new_viewport_width =
                    ctx_wr->right_col - ctx_wr->left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    ctx_wr->right_col = war_clamp_add_uint32(
                        ctx_wr->right_col, diff, ctx_wr->max_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_0:
            call_terry_davis("cmd_normal_0");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->numeric_prefix = ctx_wr->numeric_prefix * 10;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_x = ctx_wr->left_col;
            ctx_wr->sub_col = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_V: {
            call_terry_davis("cmd_normal_V");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_ga: {
            call_terry_davis("cmd_normal_$");
            uint32_t col = ((float)atomic_load(&atomics->play_clock) /
                            atomic_load(&ctx_lua->A_SAMPLE_RATE)) /
                           ((60.0f / atomic_load(&ctx_lua->A_BPM)) /
                            atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT));
            ctx_wr->cursor_pos_x =
                war_clamp_uint32(col, ctx_wr->min_col, ctx_wr->max_col);
            ctx_wr->sub_col = 0;
            uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
            uint32_t distance = viewport_width / 2;
            ctx_wr->left_col = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_x, distance, ctx_wr->min_col);
            ctx_wr->right_col = war_clamp_add_uint32(
                ctx_wr->cursor_pos_x, distance, ctx_wr->max_col);
            uint32_t new_viewport_width = war_clamp_subtract_uint32(
                ctx_wr->right_col, ctx_wr->left_col, ctx_wr->min_col);
            if (new_viewport_width < viewport_width) {
                uint32_t diff = war_clamp_subtract_uint32(
                    viewport_width, new_viewport_width, ctx_wr->min_col);
                uint32_t sum = war_clamp_add_uint32(
                    ctx_wr->right_col, diff, ctx_wr->max_col);
                if (sum < ctx_wr->max_col) {
                    ctx_wr->right_col = sum;
                } else {
                    ctx_wr->left_col = war_clamp_subtract_uint32(
                        ctx_wr->left_col, diff, ctx_wr->min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_$:
            call_terry_davis("cmd_normal_$");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->cursor_pos_x = war_clamp_uint32(
                    ctx_wr->numeric_prefix, ctx_wr->min_col, ctx_wr->max_col);
                ctx_wr->sub_col = 0;
                uint32_t viewport_width = ctx_wr->right_col - ctx_wr->left_col;
                uint32_t distance = viewport_width / 2;
                ctx_wr->left_col = war_clamp_subtract_uint32(
                    ctx_wr->cursor_pos_x, distance, ctx_wr->min_col);
                ctx_wr->right_col = war_clamp_add_uint32(
                    ctx_wr->cursor_pos_x, distance, ctx_wr->max_col);
                uint32_t new_viewport_width = war_clamp_subtract_uint32(
                    ctx_wr->right_col, ctx_wr->left_col, ctx_wr->min_col);
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_width, new_viewport_width, ctx_wr->min_col);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr->right_col, diff, ctx_wr->max_col);
                    if (sum < ctx_wr->max_col) {
                        ctx_wr->right_col = sum;
                    } else {
                        ctx_wr->left_col = war_clamp_subtract_uint32(
                            ctx_wr->left_col, diff, ctx_wr->min_col);
                    }
                }
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_x = ctx_wr->right_col;
            ctx_wr->sub_col = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_G:
            call_terry_davis("cmd_normal_G");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->cursor_pos_y = war_clamp_uint32(
                    ctx_wr->numeric_prefix, ctx_wr->min_row, ctx_wr->max_row);
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                uint32_t distance = viewport_height / 2;
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
                ctx_wr->top_row = war_clamp_add_uint32(
                    ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
                uint32_t new_viewport_height = war_clamp_subtract_uint32(
                    ctx_wr->top_row, ctx_wr->bottom_row, 0);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, 0);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr->top_row, diff, ctx_wr->max_row);
                    if (sum < ctx_wr->max_row) {
                        ctx_wr->top_row = sum;
                    } else {
                        ctx_wr->bottom_row = war_clamp_subtract_uint32(
                            ctx_wr->bottom_row, diff, ctx_wr->min_row);
                    }
                }
                if (ctx_wr->layer_flux) {
                    war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
                }
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_y = ctx_wr->bottom_row;
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_gg:
            call_terry_davis("cmd_normal_gg");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->cursor_pos_y = war_clamp_uint32(
                    ctx_wr->numeric_prefix, ctx_wr->min_row, ctx_wr->max_row);
                uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
                uint32_t distance = viewport_height / 2;
                ctx_wr->bottom_row = war_clamp_subtract_uint32(
                    ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
                ctx_wr->top_row = war_clamp_add_uint32(
                    ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
                uint32_t new_viewport_height = war_clamp_subtract_uint32(
                    ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = war_clamp_subtract_uint32(
                        viewport_height, new_viewport_height, ctx_wr->min_row);
                    uint32_t sum = war_clamp_add_uint32(
                        ctx_wr->top_row, diff, ctx_wr->max_row);
                    if (sum < ctx_wr->max_row) {
                        ctx_wr->top_row = sum;
                    } else {
                        ctx_wr->bottom_row = war_clamp_subtract_uint32(
                            ctx_wr->bottom_row, diff, ctx_wr->min_row);
                    }
                }
                if (ctx_wr->layer_flux) {
                    war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
                }
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_y = ctx_wr->top_row;
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_1:
            call_terry_davis("cmd_normal_1");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 1, UINT32_MAX);
            goto cmd_done;
        cmd_normal_2:
            call_terry_davis("cmd_normal_2");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 2, UINT32_MAX);
            goto cmd_done;
        cmd_normal_3:
            call_terry_davis("cmd_normal_3");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 3, UINT32_MAX);
            goto cmd_done;
        cmd_normal_4:
            call_terry_davis("cmd_normal_4");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 4, UINT32_MAX);
            goto cmd_done;
        cmd_normal_5:
            call_terry_davis("cmd_normal_5");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 5, UINT32_MAX);
            goto cmd_done;
        cmd_normal_6:
            call_terry_davis("cmd_normal_6");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 6, UINT32_MAX);
            goto cmd_done;
        cmd_normal_7:
            call_terry_davis("cmd_normal_7");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 7, UINT32_MAX);
            goto cmd_done;
        cmd_normal_8:
            call_terry_davis("cmd_normal_8");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 8, UINT32_MAX);
            goto cmd_done;
        cmd_normal_9:
            call_terry_davis("cmd_normal_9");
            ctx_wr->numeric_prefix = war_clamp_multiply_uint32(
                ctx_wr->numeric_prefix, 10, UINT32_MAX);
            ctx_wr->numeric_prefix =
                war_clamp_add_uint32(ctx_wr->numeric_prefix, 9, UINT32_MAX);
            goto cmd_done;
        cmd_normal_r:
            call_terry_davis("cmd_normal_r");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_ctrl_equal: {
            call_terry_davis("cmd_normal_ctrl_equal");
            ctx_wr->zoom_scale += ctx_wr->zoom_increment;
            if (ctx_wr->zoom_scale > 5.0f) { ctx_wr->zoom_scale = 5.0f; }
            float viewport_cols_f = ctx_wr->physical_width /
                                    (ctx_wr->cell_width * ctx_wr->zoom_scale);
            float viewport_rows_f = ctx_wr->physical_height /
                                    (ctx_wr->cell_height * ctx_wr->zoom_scale);
            int viewport_cols = fmax(5, (int)round(viewport_cols_f));
            int viewport_rows = fmax(5, (int)round(viewport_rows_f));
            ctx_wr->viewport_cols = viewport_cols;
            ctx_wr->viewport_rows = viewport_rows;
            ctx_wr->right_col = fmin(ctx_wr->max_col,
                                     ctx_wr->left_col + viewport_cols - 1 -
                                         ctx_wr->num_cols_for_line_numbers);
            ctx_wr->top_row = fmin(ctx_wr->max_row,
                                   ctx_wr->bottom_row + viewport_rows - 1 -
                                       ctx_wr->num_rows_for_status_bars);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_ctrl_minus: {
            call_terry_davis("cmd_normal_ctrl_minus");
            ctx_wr->zoom_scale -= ctx_wr->zoom_increment;
            if (ctx_wr->zoom_scale <= 0.1f) { ctx_wr->zoom_scale = 0.1f; }
            float viewport_cols_f = ctx_wr->physical_width /
                                    (ctx_wr->cell_width * ctx_wr->zoom_scale);
            float viewport_rows_f = ctx_wr->physical_height /
                                    (ctx_wr->cell_height * ctx_wr->zoom_scale);
            int viewport_cols = fmax(5, (int)round(viewport_cols_f));
            int viewport_rows = fmax(5, (int)round(viewport_rows_f));
            ctx_wr->viewport_cols = viewport_cols;
            ctx_wr->viewport_rows = viewport_rows;
            ctx_wr->right_col = fmin(ctx_wr->max_col,
                                     ctx_wr->left_col + viewport_cols - 1 -
                                         ctx_wr->num_cols_for_line_numbers);
            ctx_wr->top_row = fmin(ctx_wr->max_row,
                                   ctx_wr->bottom_row + viewport_rows - 1 -
                                       ctx_wr->num_rows_for_status_bars);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_ctrl_alt_equal:
            call_terry_davis("cmd_normal_ctrl_alt_equal");
            // ctx_wr->zoom_scale +=
            // ctx_wr->zoom_leap_increment; if
            // (ctx_wr->zoom_scale > 5.0f) {
            // ctx_wr->zoom_scale = 5.0f; }
            // ctx_wr->panning_x = ctx_wr->anchor_ndc_x
            // - ctx_wr->anchor_x * ctx_wr->zoom_scale;
            // ctx_wr->panning_y = ctx_wr->anchor_ndc_y
            // - ctx_wr->anchor_y * ctx_wr->zoom_scale;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_ctrl_alt_minus:
            call_terry_davis("cmd_normal_ctrl_alt_minus");
            // ctx_wr->zoom_scale -=
            // ctx_wr->zoom_leap_increment; if
            // (ctx_wr->zoom_scale < 0.1f) {
            // ctx_wr->zoom_scale = 0.1f; }
            // ctx_wr->panning_x = ctx_wr->anchor_ndc_x
            // - ctx_wr->anchor_x * ctx_wr->zoom_scale;
            // ctx_wr->panning_y = ctx_wr->anchor_ndc_y
            // - ctx_wr->anchor_y * ctx_wr->zoom_scale;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_ctrl_0: {
            call_terry_davis("cmd_normal_ctrl_0");
            ctx_wr->zoom_scale = 1.0f;
            ctx_wr->viewport_cols =
                (uint32_t)(physical_width / ctx_vk->cell_width);
            ctx_wr->viewport_rows =
                (uint32_t)(physical_height / ctx_vk->cell_height);
            ctx_wr->right_col = fmin(ctx_wr->max_col,
                                     ctx_wr->left_col + default_viewport_cols -
                                         1 - ctx_wr->num_cols_for_line_numbers);
            ctx_wr->top_row = fmin(ctx_wr->max_row,
                                   ctx_wr->bottom_row + default_viewport_rows -
                                       1 - ctx_wr->num_rows_for_status_bars);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_esc: {
            call_terry_davis("cmd_normal_esc");
            if (ctx_fsm->timeout_state_index) {
                // if (fsm[timeout_state_index].command[ctx_fsm->current_mode])
                // {
                //     fsm[timeout_state_index].command[ctx_fsm->current_mode](
                //         env);
                //     timeout_state_index = 0;
                // }
                goto cmd_done;
            }
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_S: {
            call_terry_davis("cmd_normal_S");
            goto cmd_done;
        }
        cmd_normal_s: {
            call_terry_davis("cmd_normal_s");
            ctx_wr->cursor_width_sub_cells = 1;
            ctx_wr->cursor_width_whole_number = 1;
            ctx_wr->cursor_width_sub_col = 1;
            ctx_wr->navigation_whole_number_col = 1;
            ctx_wr->navigation_sub_cells_col = 1;
            ctx_wr->cursor_size_x = 1.0;
            ctx_wr->cursor_navigation_x = 1.0;
            uint32_t whole_pos_x = llround(ctx_wr->cursor_pos_x);
            ctx_wr->cursor_pos_x = whole_pos_x;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_f:
            call_terry_davis("cmd_normal_f");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->cursor_width_whole_number = ctx_wr->numeric_prefix;
                ctx_wr->cursor_size_x =
                    (double)ctx_wr->cursor_width_whole_number /
                    ctx_wr->cursor_width_sub_cells;
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_width_whole_number = 1;
            ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                                    ctx_wr->cursor_width_sub_cells;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_t:
            call_terry_davis("cmd_normal_t");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->cursor_width_sub_cells = ctx_wr->numeric_prefix;
                ctx_wr->cursor_size_x =
                    (double)ctx_wr->cursor_width_whole_number /
                    ctx_wr->cursor_width_sub_cells;
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_width_sub_cells = 1;
            ctx_wr->cursor_size_x = (double)ctx_wr->cursor_width_whole_number /
                                    ctx_wr->cursor_width_sub_cells;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_T:
            call_terry_davis("cmd_normal_T");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->navigation_sub_cells_col = ctx_wr->numeric_prefix;
                ctx_wr->cursor_navigation_x =
                    (double)ctx_wr->navigation_whole_number_col /
                    ctx_wr->navigation_sub_cells_col;
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->navigation_sub_cells_col = 1;
            ctx_wr->cursor_navigation_x =
                (double)ctx_wr->navigation_whole_number_col /
                ctx_wr->navigation_sub_cells_col;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_F:
            call_terry_davis("cmd_normal_F");
            if (ctx_wr->numeric_prefix) {
                ctx_wr->navigation_whole_number_col = ctx_wr->numeric_prefix;
                ctx_wr->cursor_navigation_x =
                    (double)ctx_wr->navigation_whole_number_col /
                    ctx_wr->navigation_sub_cells_col;
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->navigation_whole_number_col = 1;
            ctx_wr->cursor_navigation_x =
                (double)ctx_wr->navigation_whole_number_col /
                ctx_wr->navigation_sub_cells_col;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_f: {
            call_terry_davis("cmd_normal_alt_f");
            goto cmd_done;
        }
        cmd_normal_gb:
            call_terry_davis("cmd_normal_gb");
            ctx_wr->cursor_pos_y = ctx_wr->min_row;
            uint32_t viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
            uint32_t distance = viewport_height / 2;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
            ctx_wr->top_row = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
            uint32_t new_viewport_height = war_clamp_subtract_uint32(
                ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
            if (new_viewport_height < viewport_height) {
                uint32_t diff = war_clamp_subtract_uint32(
                    viewport_height, new_viewport_height, ctx_wr->min_row);
                uint32_t sum = war_clamp_add_uint32(
                    ctx_wr->top_row, diff, ctx_wr->max_row);
                if (sum < ctx_wr->max_row) {
                    ctx_wr->top_row = sum;
                } else {
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_gt:
            call_terry_davis("cmd_normal_gt");
            ctx_wr->cursor_pos_y = ctx_wr->max_row;
            viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
            distance = viewport_height / 2;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
            ctx_wr->top_row = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
            new_viewport_height = war_clamp_subtract_uint32(
                ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
            if (new_viewport_height < viewport_height) {
                uint32_t diff = war_clamp_subtract_uint32(
                    viewport_height, new_viewport_height, ctx_wr->min_row);
                uint32_t sum = war_clamp_add_uint32(
                    ctx_wr->top_row, diff, ctx_wr->max_row);
                if (sum < ctx_wr->max_row) {
                    ctx_wr->top_row = sum;
                } else {
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_gm:
            call_terry_davis("cmd_normal_gm");
            ctx_wr->cursor_pos_y = 60;
            viewport_height = ctx_wr->top_row - ctx_wr->bottom_row;
            distance = viewport_height / 2;
            ctx_wr->bottom_row = war_clamp_subtract_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->min_row);
            ctx_wr->top_row = war_clamp_add_uint32(
                ctx_wr->cursor_pos_y, distance, ctx_wr->max_row);
            new_viewport_height = war_clamp_subtract_uint32(
                ctx_wr->top_row, ctx_wr->bottom_row, ctx_wr->min_row);
            if (new_viewport_height < viewport_height) {
                uint32_t diff = war_clamp_subtract_uint32(
                    viewport_height, new_viewport_height, ctx_wr->min_row);
                uint32_t sum = war_clamp_add_uint32(
                    ctx_wr->top_row, diff, ctx_wr->max_row);
                if (sum < ctx_wr->max_row) {
                    ctx_wr->top_row = sum;
                } else {
                    ctx_wr->bottom_row = war_clamp_subtract_uint32(
                        ctx_wr->bottom_row, diff, ctx_wr->min_row);
                }
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_return:
        cmd_normal_z: {
            // TODO: spillover swapfile
            call_terry_davis("cmd_normal_z");
            uint64_t id = atomic_fetch_add(&atomics->note_next_id, 1);
            war_note_quad note_quad = {
                .alive = 1,
                .id = id,
                .pos_x = ctx_wr->cursor_pos_x,
                .pos_y = ctx_wr->cursor_pos_y,
                .layer = atomic_load(&atomics->layer),
                .size_x = ctx_wr->cursor_size_x,
                .size_x_numerator = ctx_wr->cursor_width_whole_number,
                .size_x_denominator = ctx_wr->cursor_width_sub_cells,
                .navigation_x = ctx_wr->cursor_navigation_x,
                .navigation_x_numerator = ctx_wr->navigation_whole_number_col,
                .navigation_x_denominator = ctx_wr->navigation_sub_cells_col,
                .color = ctx_wr->color_cursor,
                .outline_color = ctx_wr->color_note_outline_default,
                .gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN),
                .voice = 0,
            };
            double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
            double bpm = atomic_load(&ctx_lua->A_BPM);
            double frames_per_beat = sample_rate * 60.0 / bpm;
            double columns_per_beat =
                atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
            war_note note;
            double start_beats = note_quad.pos_x / columns_per_beat;
            note.note_start_frames =
                (uint64_t)(start_beats * frames_per_beat + 0.5);
            double duration_beats = note_quad.size_x / columns_per_beat;
            note.note_duration_frames =
                (uint64_t)(duration_beats * frames_per_beat + 0.5);
            note.note = note_quad.pos_y;
            note.layer = note_quad.layer;
            note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
            note.note_sustain = atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
            note.note_release = atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
            note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
            note.note_phase_increment = 0;
            note.alive = note_quad.alive;
            note.id = note_quad.id;
            uint32_t note_quads_max = atomic_load(&ctx_lua->WR_NOTE_QUADS_MAX);
            uint32_t undo_notes_batch_max =
                atomic_load(&ctx_lua->WR_UNDO_NOTES_BATCH_MAX);
            if (ctx_wr->numeric_prefix) {
                // add notes
                if (ctx_wr->numeric_prefix >= undo_notes_batch_max) {
                    // swapfile logic (memmove probably)
                    ctx_wr->numeric_prefix = 0;
                }
                // --- Compaction if needed ---
                if (note_quads->count + ctx_wr->numeric_prefix >=
                    note_quads_max) {
                    uint32_t write_idx = 0;
                    for (uint32_t read_idx = 0; read_idx < note_quads->count;
                         read_idx++) {
                        if (note_quads->alive[read_idx]) {
                            if (write_idx != read_idx) {
                                // Copy all fields
                                note_quads->pos_x[write_idx] =
                                    note_quads->pos_x[read_idx];
                                note_quads->pos_y[write_idx] =
                                    note_quads->pos_y[read_idx];
                                note_quads->layer[write_idx] =
                                    note_quads->layer[read_idx];
                                note_quads->size_x[write_idx] =
                                    note_quads->size_x[read_idx];
                                note_quads->size_x_numerator[write_idx] =
                                    note_quads->size_x_numerator[read_idx];
                                note_quads->size_x_denominator[write_idx] =
                                    note_quads->size_x_denominator[read_idx];
                                note_quads->navigation_x[write_idx] =
                                    note_quads->navigation_x[read_idx];
                                note_quads->navigation_x_numerator[write_idx] =
                                    note_quads
                                        ->navigation_x_numerator[read_idx];
                                note_quads
                                    ->navigation_x_denominator[write_idx] =
                                    note_quads
                                        ->navigation_x_denominator[read_idx];
                                note_quads->color[write_idx] =
                                    note_quads->color[read_idx];
                                note_quads->outline_color[write_idx] =
                                    note_quads->outline_color[read_idx];
                                note_quads->gain[write_idx] =
                                    note_quads->gain[read_idx];
                                note_quads->voice[write_idx] =
                                    note_quads->voice[read_idx];
                                note_quads->alive[write_idx] =
                                    note_quads->alive[read_idx];
                                note_quads->id[write_idx] =
                                    note_quads->id[read_idx];
                            }
                            write_idx++;
                        }
                    }
                    if (write_idx >= note_quads_max) {
                        call_terry_davis("TODO: implement spillover swapfile");
                        ctx_wr->numeric_prefix = 0;
                        goto cmd_done;
                    };
                    note_quads->count = write_idx;
                }
                war_undo_node* node =
                    war_pool_alloc(pool_wr, sizeof(war_undo_node));
                node->id = undo_tree->next_id++;
                node->seq_num = undo_tree->next_seq_num++;
                node->command = CMD_ADD_NOTES_SAME;
                node->payload.delete_notes_same.note = note;
                node->payload.delete_notes_same.note_quad = note_quad;
                node->payload.delete_notes_same.ids = war_pool_alloc(
                    pool_wr, sizeof(uint64_t) * ctx_wr->numeric_prefix);
                node->payload.delete_notes_same.count = ctx_wr->numeric_prefix;
                node->cursor_pos_x = ctx_wr->cursor_pos_x;
                node->cursor_pos_y = ctx_wr->cursor_pos_y;
                node->left_col = ctx_wr->left_col;
                node->right_col = ctx_wr->right_col;
                node->top_row = ctx_wr->top_row;
                node->bottom_row = ctx_wr->bottom_row;
                node->parent = NULL;
                node->next = NULL;
                node->prev = NULL;
                node->alt_next = NULL;
                node->alt_prev = NULL;
                if (!undo_tree->root) {
                    node->branch_id = undo_tree->next_branch_id++;
                    undo_tree->root = node;
                    undo_tree->current = node;
                } else if (!undo_tree->current) {
                    node->branch_id = undo_tree->next_branch_id++;
                    node->parent = NULL;
                    undo_tree->root = node;
                    undo_tree->current = node;
                } else {
                    war_undo_node* cur = undo_tree->current;
                    if (cur->next) { cur->next = NULL; }
                    node->parent = cur;
                    node->branch_id = cur->branch_id;
                    cur->next = node;
                    node->prev = cur;
                    undo_tree->current = node;
                }
                // batch add
                for (uint32_t i = 0; i < ctx_wr->numeric_prefix; i++) {
                    note_quads->pos_x[note_quads->count + i] = note_quad.pos_x;
                    note_quads->pos_y[note_quads->count + i] = note_quad.pos_y;
                    note_quads->layer[note_quads->count + i] = note_quad.layer;
                    note_quads->size_x[note_quads->count + i] =
                        note_quad.size_x;
                    note_quads->size_x_numerator[note_quads->count + i] =
                        note_quad.size_x_numerator;
                    note_quads->size_x_denominator[note_quads->count + i] =
                        note_quad.size_x_denominator;
                    note_quads->navigation_x[note_quads->count + i] =
                        note_quad.navigation_x;
                    note_quads->navigation_x_numerator[note_quads->count + i] =
                        note_quad.navigation_x_numerator;
                    note_quads
                        ->navigation_x_denominator[note_quads->count + i] =
                        note_quad.navigation_x_denominator;
                    note_quads->color[note_quads->count + i] = note_quad.color;
                    note_quads->outline_color[note_quads->count + i] =
                        note_quad.outline_color;
                    note_quads->gain[note_quads->count + i] = note_quad.gain;
                    note_quads->voice[note_quads->count + i] = note_quad.voice;
                    note_quads->alive[note_quads->count + i] = note_quad.alive;
                    note_quads->id[note_quads->count + i] = id;
                    node->payload.delete_notes_same.ids[i] = id;
                    id = atomic_fetch_add(&atomics->note_next_id, 1);
                }
                note_quads->count += ctx_wr->numeric_prefix;
                // note, count, ids
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            //-------------------------------------------------------------
            // ADD SINGLE NOTE
            //-------------------------------------------------------------
            // --- Compaction if needed ---
            if (note_quads->count + 1 >= note_quads_max) {
                uint32_t write_idx = 0;
                for (uint32_t read_idx = 0; read_idx < note_quads->count;
                     read_idx++) {
                    if (note_quads->alive[read_idx]) {
                        if (write_idx != read_idx) {
                            // Copy all fields
                            note_quads->pos_x[write_idx] =
                                note_quads->pos_x[read_idx];
                            note_quads->pos_y[write_idx] =
                                note_quads->pos_y[read_idx];
                            note_quads->layer[write_idx] =
                                note_quads->layer[read_idx];
                            note_quads->size_x[write_idx] =
                                note_quads->size_x[read_idx];
                            note_quads->size_x_numerator[write_idx] =
                                note_quads->size_x_numerator[read_idx];
                            note_quads->size_x_denominator[write_idx] =
                                note_quads->size_x_denominator[read_idx];
                            note_quads->navigation_x[write_idx] =
                                note_quads->navigation_x[read_idx];
                            note_quads->navigation_x_numerator[write_idx] =
                                note_quads->navigation_x_numerator[read_idx];
                            note_quads->navigation_x_denominator[write_idx] =
                                note_quads->navigation_x_denominator[read_idx];
                            note_quads->color[write_idx] =
                                note_quads->color[read_idx];
                            note_quads->outline_color[write_idx] =
                                note_quads->outline_color[read_idx];
                            note_quads->gain[write_idx] =
                                note_quads->gain[read_idx];
                            note_quads->voice[write_idx] =
                                note_quads->voice[read_idx];
                            note_quads->alive[write_idx] =
                                note_quads->alive[read_idx];
                            note_quads->id[write_idx] =
                                note_quads->id[read_idx];
                        }
                        write_idx++;
                    }
                }
                assert(write_idx < note_quads_max);
                note_quads->count = write_idx;
            }
            note_quads->pos_x[note_quads->count] = note_quad.pos_x;
            note_quads->pos_y[note_quads->count] = note_quad.pos_y;
            note_quads->layer[note_quads->count] = note_quad.layer;
            note_quads->size_x[note_quads->count] = note_quad.size_x;
            note_quads->size_x_numerator[note_quads->count] =
                note_quad.size_x_numerator;
            note_quads->size_x_denominator[note_quads->count] =
                note_quad.size_x_denominator;
            note_quads->navigation_x[note_quads->count] =
                note_quad.navigation_x;
            note_quads->navigation_x_numerator[note_quads->count] =
                note_quad.navigation_x_numerator;
            note_quads->navigation_x_denominator[note_quads->count] =
                note_quad.navigation_x_denominator;
            note_quads->color[note_quads->count] = note_quad.color;
            note_quads->outline_color[note_quads->count] =
                note_quad.outline_color;
            note_quads->gain[note_quads->count] = note_quad.gain;
            note_quads->voice[note_quads->count] = note_quad.voice;
            note_quads->alive[note_quads->count] = note_quad.alive;
            note_quads->id[note_quads->count] = note_quad.id;
            note_quads->count++;
            war_undo_node* node =
                war_pool_alloc(pool_wr, sizeof(war_undo_node));
            node->id = undo_tree->next_id++;
            node->seq_num = undo_tree->next_seq_num++;
            node->command = CMD_ADD_NOTE;
            node->payload.delete_note.note = note;
            node->payload.delete_note.note_quad = note_quad;
            node->cursor_pos_x = ctx_wr->cursor_pos_x;
            node->cursor_pos_y = ctx_wr->cursor_pos_y;
            node->left_col = ctx_wr->left_col;
            node->right_col = ctx_wr->right_col;
            node->top_row = ctx_wr->top_row;
            node->bottom_row = ctx_wr->bottom_row;
            node->parent = NULL;
            node->next = NULL;
            node->prev = NULL;
            node->alt_next = NULL;
            node->alt_prev = NULL;
            if (!undo_tree->root) {
                node->branch_id = undo_tree->next_branch_id++;
                undo_tree->root = node;
                undo_tree->current = node;
            } else if (!undo_tree->current) {
                node->branch_id = undo_tree->next_branch_id++;
                node->parent = NULL;
                undo_tree->root = node;
                undo_tree->current = node;
            } else {
                war_undo_node* cur = undo_tree->current;
                if (cur->next) { cur->next = NULL; }
                node->parent = cur;
                node->branch_id = cur->branch_id;
                cur->next = node;
                node->prev = cur;
                undo_tree->current = node;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_x:
            call_terry_davis("cmd_normal_x");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_X: {
            call_terry_davis("cmd_normal_X");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_d: {
            // TODO: spillover swapfile
            call_terry_davis("cmd_normal_d");
            if (note_quads->count == 0) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            uint64_t layer = atomic_load(&atomics->layer);
            if (ctx_wr->numeric_prefix) {
                // TODO: spillover swapfile
                // TODO batch delete
                uint32_t delete_count = 0;
                uint32_t undo_notes_batch_max =
                    atomic_load(&ctx_lua->WR_UNDO_NOTES_BATCH_MAX);
                war_undo_node* node;
                for (int32_t i = note_quads->count - 1; i >= 0; i--) {
                    if (note_quads->alive[i] == 0 || note_quads->hidden[i] ||
                        note_quads->layer[i] != layer) {
                        continue;
                    }
                    double cursor_pos_x = ctx_wr->cursor_pos_x;
                    double cursor_pos_y = ctx_wr->cursor_pos_y;
                    double cursor_end_x = cursor_pos_x + ctx_wr->cursor_size_x;
                    double note_pos_x = note_quads->pos_x[i];
                    double note_pos_y = note_quads->pos_y[i];
                    double note_end_x = note_pos_x + note_quads->size_x[i];
                    if (cursor_pos_y != note_pos_y ||
                        cursor_pos_x >= note_end_x ||
                        cursor_end_x <= note_pos_x) {
                        continue;
                    }
                    if (delete_count == 0) {
                        node = war_pool_alloc(pool_wr, sizeof(war_undo_node));
                        node->id = undo_tree->next_id++;
                        node->seq_num = undo_tree->next_seq_num++;
                        node->command = CMD_DELETE_NOTES;
                        node->payload.add_notes.note = war_pool_alloc(
                            pool_wr, sizeof(war_note) * undo_notes_batch_max);
                        node->payload.add_notes.note_quad = war_pool_alloc(
                            pool_wr,
                            sizeof(war_note_quad) * undo_notes_batch_max);
                        node->cursor_pos_x = ctx_wr->cursor_pos_x;
                        node->cursor_pos_y = ctx_wr->cursor_pos_y;
                        node->left_col = ctx_wr->left_col;
                        node->right_col = ctx_wr->right_col;
                        node->top_row = ctx_wr->top_row;
                        node->bottom_row = ctx_wr->bottom_row;
                        node->parent = NULL;
                        node->next = NULL;
                        node->prev = NULL;
                        node->alt_next = NULL;
                        node->alt_prev = NULL;
                        if (!undo_tree->root) {
                            node->branch_id = undo_tree->next_branch_id++;
                            undo_tree->root = node;
                            undo_tree->current = node;
                        } else if (!undo_tree->current) {
                            node->branch_id = undo_tree->next_branch_id++;
                            node->parent = NULL;
                            undo_tree->root = node;
                            undo_tree->current = node;
                        } else {
                            war_undo_node* cur = undo_tree->current;
                            if (cur->next) { cur->next = NULL; }
                            node->parent = cur;
                            node->branch_id = cur->branch_id;
                            cur->next = node;
                            node->prev = cur;
                            undo_tree->current = node;
                        }
                    }
                    war_note_quad note_quad;
                    note_quad.alive = note_quads->alive[i];
                    note_quad.id = note_quads->id[i];
                    note_quad.pos_x = note_quads->pos_x[i];
                    note_quad.pos_y = note_quads->pos_y[i];
                    note_quad.layer = note_quads->layer[i];
                    note_quad.size_x = note_quads->size_x[i];
                    note_quad.navigation_x = note_quads->navigation_x[i];
                    note_quad.navigation_x_numerator =
                        note_quads->navigation_x_numerator[i];
                    note_quad.navigation_x_denominator =
                        note_quads->navigation_x_denominator[i];
                    note_quad.size_x_numerator =
                        note_quads->size_x_numerator[i];
                    note_quad.size_x_denominator =
                        note_quads->size_x_denominator[i];
                    note_quad.color = note_quads->color[i];
                    note_quad.outline_color = note_quads->outline_color[i];
                    note_quad.gain = note_quads->gain[i];
                    note_quad.voice = note_quads->voice[i];
                    note_quad.hidden = note_quads->hidden[i];
                    note_quad.mute = note_quads->mute[i];
                    double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
                    double bpm = atomic_load(&ctx_lua->A_BPM);
                    double frames_per_beat = sample_rate * 60.0 / bpm;
                    double columns_per_beat =
                        atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
                    war_note note;
                    double start_beats = note_quad.pos_x / columns_per_beat;
                    note.note_start_frames =
                        (uint64_t)(start_beats * frames_per_beat + 0.5);
                    double duration_beats = note_quad.size_x / columns_per_beat;
                    note.note_duration_frames =
                        (uint64_t)(duration_beats * frames_per_beat + 0.5);
                    note.note = note_quad.pos_y;
                    note.layer = note_quad.layer;
                    note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
                    note.note_sustain =
                        atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
                    note.note_release =
                        atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
                    note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
                    note.note_phase_increment = 0;
                    note.id = note_quad.id;
                    note.alive = note_quad.alive;
                    node->payload.add_notes.note[delete_count] = note;
                    node->payload.add_notes.note_quad[delete_count] = note_quad;
                    note_quads->alive[i] = 0;
                    delete_count++;
                    if (delete_count >= undo_notes_batch_max) {
                        // spillover
                        ctx_wr->numeric_prefix = 0;
                        goto cmd_done;
                    }
                    if (delete_count >= ctx_wr->numeric_prefix) { break; }
                }
                if (delete_count == 0) {
                    ctx_wr->numeric_prefix = 0;
                    goto cmd_done;
                }
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            int32_t delete_idx = -1; // overflow impossible
            for (uint32_t i = 0; i < note_quads->count; i++) {
                if (note_quads->alive[i] == 0 || note_quads->hidden[i] ||
                    note_quads->layer[i] != layer) {
                    continue;
                }
                double cursor_pos_x = ctx_wr->cursor_pos_x;
                double cursor_pos_y = ctx_wr->cursor_pos_y;
                double cursor_end_x = cursor_pos_x + ctx_wr->cursor_size_x;
                double note_pos_x = note_quads->pos_x[i];
                double note_pos_y = note_quads->pos_y[i];
                double note_end_x = note_pos_x + note_quads->size_x[i];
                if (cursor_pos_y != note_pos_y || cursor_pos_x >= note_end_x ||
                    cursor_end_x <= note_pos_x) {
                    continue;
                }
                delete_idx = i;
            }
            if (delete_idx == -1) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            war_note_quad note_quad;
            note_quad.alive = note_quads->alive[delete_idx];
            note_quad.id = note_quads->id[delete_idx];
            note_quad.pos_x = note_quads->pos_x[delete_idx];
            note_quad.pos_y = note_quads->pos_y[delete_idx];
            note_quad.layer = note_quads->layer[delete_idx];
            note_quad.size_x = note_quads->size_x[delete_idx];
            note_quad.navigation_x = note_quads->navigation_x[delete_idx];
            note_quad.navigation_x_numerator =
                note_quads->navigation_x_numerator[delete_idx];
            note_quad.navigation_x_denominator =
                note_quads->navigation_x_denominator[delete_idx];
            note_quad.size_x_numerator =
                note_quads->size_x_numerator[delete_idx];
            note_quad.size_x_denominator =
                note_quads->size_x_denominator[delete_idx];
            note_quad.color = note_quads->color[delete_idx];
            note_quad.outline_color = note_quads->outline_color[delete_idx];
            note_quad.gain = note_quads->gain[delete_idx];
            note_quad.voice = note_quads->voice[delete_idx];
            note_quad.hidden = note_quads->hidden[delete_idx];
            note_quad.mute = note_quads->mute[delete_idx];
            double sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
            double bpm = atomic_load(&ctx_lua->A_BPM);
            double frames_per_beat = sample_rate * 60.0 / bpm;
            double columns_per_beat =
                atomic_load(&ctx_lua->A_DEFAULT_COLUMNS_PER_BEAT);
            war_note note;
            double start_beats = note_quad.pos_x / columns_per_beat;
            note.note_start_frames =
                (uint64_t)(start_beats * frames_per_beat + 0.5);
            double duration_beats = note_quad.size_x / columns_per_beat;
            note.note_duration_frames =
                (uint64_t)(duration_beats * frames_per_beat + 0.5);
            note.note = note_quad.pos_y;
            note.layer = note_quad.layer;
            note.note_attack = atomic_load(&ctx_lua->A_DEFAULT_ATTACK);
            note.note_sustain = atomic_load(&ctx_lua->A_DEFAULT_SUSTAIN);
            note.note_release = atomic_load(&ctx_lua->A_DEFAULT_RELEASE);
            note.note_gain = atomic_load(&ctx_lua->A_DEFAULT_GAIN);
            note.note_phase_increment = 0;
            note.id = note_quad.id;
            note.alive = note_quad.alive;
            note_quads->alive[delete_idx] = 0;
            war_undo_node* node =
                war_pool_alloc(pool_wr, sizeof(war_undo_node));
            node->id = undo_tree->next_id++;
            node->seq_num = undo_tree->next_seq_num++;
            node->command = CMD_DELETE_NOTE;
            node->payload.add_note.note = note;
            node->payload.add_note.note_quad = note_quad;
            node->cursor_pos_x = ctx_wr->cursor_pos_x;
            node->cursor_pos_y = ctx_wr->cursor_pos_y;
            node->left_col = ctx_wr->left_col;
            node->right_col = ctx_wr->right_col;
            node->top_row = ctx_wr->top_row;
            node->bottom_row = ctx_wr->bottom_row;
            node->parent = NULL;
            node->next = NULL;
            node->prev = NULL;
            node->alt_next = NULL;
            node->alt_prev = NULL;
            if (!undo_tree->root) {
                node->branch_id = undo_tree->next_branch_id++;
                undo_tree->root = node;
                undo_tree->current = node;
            } else if (!undo_tree->current) {
                node->branch_id = undo_tree->next_branch_id++;
                node->parent = NULL;
                undo_tree->root = node;
                undo_tree->current = node;
            } else {
                war_undo_node* cur = undo_tree->current;
                if (cur->next) { cur->next = NULL; }
                node->parent = cur;
                node->branch_id = cur->branch_id;
                cur->next = node;
                node->prev = cur;
                undo_tree->current = node;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_spacediv: {
            call_terry_davis("cmd_normal_spacediv");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_spacedov:
            call_terry_davis("cmd_normal_spacedov");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacediw: {
            call_terry_davis("cmd_normal_spacediw");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_spaceda:
            call_terry_davis("cmd_normal_spaceda");
            note_quads->count = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacehov:
            call_terry_davis("cmd_normal_spacehov");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacehiv:
            call_terry_davis("cmd_normal_spacehiv");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacehiw:
            call_terry_davis("cmd_normal_spacehiw");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spaceha:
            call_terry_davis("cmd_normal_spaceha");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacesov:
            call_terry_davis("cmd_normal_spacesov");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacesiv:
            call_terry_davis("cmd_normal_spacesiv");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacesiw:
            call_terry_davis("cmd_normal_spacesiw");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacesa:
            call_terry_davis("cmd_normal_spacesa");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacem:
            call_terry_davis("cmd_normal_spacem");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacemov:
            call_terry_davis("cmd_normal_spacemov");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacemiv:
            call_terry_davis("cmd_normal_spacemiv");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacema:
            call_terry_davis("cmd_normal_spacema");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_m:
            call_terry_davis("cmd_normal_m");
            ctx_fsm->current_mode = ctx_fsm->MODE_MIDI;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spaceumov:
            call_terry_davis("cmd_normal_spaceumov");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spaceumiv:
            call_terry_davis("cmd_normal_spaceumiv");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spaceumiw:
            call_terry_davis("cmd_normal_spaceumiw");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spaceuma:
            call_terry_davis("cmd_normal_spaceuma");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacea:
            call_terry_davis("cmd_normal_spacea");
            if (views->views_count <
                (uint32_t)atomic_load(&ctx_lua->WR_VIEWS_SAVED)) {
                views->col[views->views_count] = ctx_wr->cursor_pos_x;
                views->row[views->views_count] = ctx_wr->cursor_pos_y;
                views->left_col[views->views_count] = ctx_wr->left_col;
                views->bottom_row[views->views_count] = ctx_wr->bottom_row;
                views->right_col[views->views_count] = ctx_wr->right_col;
                views->top_row[views->views_count] = ctx_wr->top_row;
                views->views_count++;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_spacedspacea:
            call_terry_davis("cmd_normal_spacedspacea");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_g:
            call_terry_davis("cmd_normal_alt_g");
            if (views->views_count > 0) {
                uint32_t i_views = 0;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_t:
            call_terry_davis("cmd_normal_alt_t");
            if (views->views_count > 1) {
                uint32_t i_views = 1;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_n:
            call_terry_davis("cmd_normal_alt_n");
            if (views->views_count > 2) {
                uint32_t i_views = 2;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_s:
            call_terry_davis("cmd_normal_alt_s");
            if (views->views_count > 3) {
                uint32_t i_views = 3;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_m:
            call_terry_davis("cmd_normal_alt_m");
            if (views->views_count > 4) {
                uint32_t i_views = 4;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_y:
            call_terry_davis("cmd_normal_alt_y");
            if (views->views_count > 5) {
                uint32_t i_views = 5;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_z:
            call_terry_davis("cmd_normal_alt_z");
            if (views->views_count > 6) {
                uint32_t i_views = 6;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_q:
            call_terry_davis("cmd_normal_alt_q");
            if (views->views_count > 7) {
                uint32_t i_views = 7;
                ctx_wr->cursor_pos_x = views->col[i_views];
                ctx_wr->cursor_pos_y = views->row[i_views];
                ctx_wr->left_col = views->left_col[i_views];
                ctx_wr->bottom_row = views->bottom_row[i_views];
                ctx_wr->right_col = views->right_col[i_views];
                ctx_wr->top_row = views->top_row[i_views];
            }
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_e:
            call_terry_davis("cmd_normal_alt_e");
            ctx_fsm->current_mode =
                (ctx_fsm->current_mode != ctx_fsm->MODE_VIEWS) ?
                    ctx_fsm->MODE_VIEWS :
                    ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        //-----------------------------------------------------------------
        // PLAYBACK COMMANDS
        //-----------------------------------------------------------------
        cmd_normal_a: {
            call_terry_davis("cmd_normal_a");
            ctx_play->play = !ctx_play->play;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_a: {
            call_terry_davis("cmd_normal_alt_a");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_A: {
            call_terry_davis("cmd_normal_alt_A");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_A: {
            call_terry_davis("cmd_normal_A");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_esc: {
            call_terry_davis("cmd_normal_alt_esc");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_ctrl_a: {
            call_terry_davis("cmd_normal_ctrl_a");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_space1:
            call_terry_davis("cmd_normal_space1");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space2:
            call_terry_davis("cmd_normal_space2");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space3:
            call_terry_davis("cmd_normal_space3");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space4:
            call_terry_davis("cmd_normal_space4");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space5:
            call_terry_davis("cmd_normal_space5");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space6:
            call_terry_davis("cmd_normal_space6");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space7:
            call_terry_davis("cmd_normal_space7");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space8:
            call_terry_davis("cmd_normal_space8");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space9:
            call_terry_davis("cmd_normal_space9");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_space0:
            call_terry_davis("cmd_normal_space0");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_alt_1: {
            call_terry_davis("cmd_normal_alt_1");
            int idx = 0;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_2: {
            call_terry_davis("cmd_normal_alt_2");
            int idx = 1;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_3: {
            call_terry_davis("cmd_normal_alt_3");
            int idx = 2;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_4: {
            call_terry_davis("cmd_normal_alt_4");
            int idx = 3;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_5: {
            call_terry_davis("cmd_normal_alt_5");
            int idx = 4;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_6: {
            call_terry_davis("cmd_normal_alt_6");
            int idx = 5;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_7: {
            call_terry_davis("cmd_normal_alt_7");
            int idx = 6;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_8: {
            call_terry_davis("cmd_normal_alt_8");
            int idx = 7;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_9: {
            call_terry_davis("cmd_normal_alt_9");
            int idx = 8;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_0: {
            call_terry_davis("cmd_normal_alt_0");
            int layer_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
            ctx_wr->color_cursor = full_white_hex;
            ctx_wr->color_cursor_transparent = white_hex;
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << layer_count) - 1);
            ctx_wr->layers_active_count = 9;
            for (int i = 0; i < ctx_wr->layers_active_count; i++) {
                ctx_wr->layers_active[i] = (i + 1) + '0';
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_1: {
            call_terry_davis("cmd_normal_alt_shift_1");
            int idx = 0;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_2: {
            call_terry_davis("cmd_normal_alt_shift_2");
            int idx = 1;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_3: {
            call_terry_davis("cmd_normal_alt_shift_3");
            int idx = 2;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_4: {
            call_terry_davis("cmd_normal_alt_shift_4");
            int idx = 3;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_5: {
            call_terry_davis("cmd_normal_alt_shift_5");
            int idx = 4;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_6: {
            call_terry_davis("cmd_normal_alt_shift_6");
            int idx = 5;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_7: {
            call_terry_davis("cmd_normal_alt_shift_7");
            int idx = 6;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_8: {
            call_terry_davis("cmd_normal_alt_shift_8");
            int idx = 7;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_9: {
            call_terry_davis("cmd_normal_alt_shift_9");
            int idx = 8;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_shift_0: {
            call_terry_davis("cmd_normal_alt_shift_0");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_w: {
            call_terry_davis("cmd_normal_w");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_W:
            call_terry_davis("cmd_normal_W");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_normal_e: {
            call_terry_davis("cmd_normal_e");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_E: {
            call_terry_davis("cmd_normal_E");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_b: {
            call_terry_davis("cmd_normal_b");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_B: {
            call_terry_davis("cmd_normal_B");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_u: {
            call_terry_davis("cmd_normal_alt_u");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_alt_d: {
            call_terry_davis("cmd_normal_alt_d");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_tab: {
            call_terry_davis("cmd_normal_tab");
            switch (ctx_wr->cursor_blink_state) {
            case CURSOR_BLINK:
                ctx_wr->cursor_blink_state = CURSOR_BLINK_BPM;
                ctx_wr->cursor_blinking = false;
                ctx_wr->cursor_blink_previous_us = ctx_wr->now;
                break;
            case CURSOR_BLINK_BPM:
                ctx_wr->cursor_blink_state = 0;
                ctx_wr->cursor_blinking = false;
                ctx_wr->cursor_blink_previous_us = ctx_wr->now;
                break;
            case 0:
                ctx_wr->cursor_blink_state = CURSOR_BLINK;
                ctx_wr->cursor_blinking = false;
                ctx_wr->cursor_blink_previous_us = ctx_wr->now;
                ctx_wr->cursor_blink_duration_us =
                    DEFAULT_CURSOR_BLINK_DURATION;
                break;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_shift_tab: {
            call_terry_davis("cmd_normal_shift_tab");
            switch (ctx_wr->hud_state) {
            case HUD_PIANO:
                ctx_wr->hud_state = HUD_PIANO_AND_LINE_NUMBERS;
                ctx_wr->num_cols_for_line_numbers = 6;
                ctx_wr->right_col -= 3;
                ctx_wr->cursor_pos_x = war_clamp_uint32(
                    ctx_wr->cursor_pos_x, 0, ctx_wr->right_col);
                break;
            case HUD_PIANO_AND_LINE_NUMBERS:
                ctx_wr->hud_state = HUD_LINE_NUMBERS;
                ctx_wr->num_cols_for_line_numbers = 3;
                ctx_wr->right_col += 3;
                ctx_wr->cursor_pos_x = war_clamp_uint32(
                    ctx_wr->cursor_pos_x, 0, ctx_wr->right_col);
                break;
            case HUD_LINE_NUMBERS:
                ctx_wr->hud_state = HUD_PIANO;
                ctx_wr->num_cols_for_line_numbers = 3;
                break;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_q: {
            call_terry_davis("cmd_normal_q");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_Q: {
            call_terry_davis("cmd_normal_Q");
            ctx_capture->capture = 1;
            ctx_fsm->current_mode = ctx_fsm->MODE_WAV;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_space: {
            call_terry_davis("cmd_normal_space");
            ctx_fsm->current_mode = ctx_fsm->MODE_CAPTURE;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_colon: {
            call_terry_davis("cmd_normal_colon");
            ctx_command->command = 1;
            war_command_reset(ctx_command, ctx_status);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_u: {
            call_terry_davis("cmd_normal_u");
            assert(undo_tree != NULL);
            if (undo_tree->current) {
                war_undo_node* node = undo_tree->current;
                assert(node != NULL);
                switch (node->command) {
                case CMD_ADD_NOTE: {
                    break;
                }
                case CMD_DELETE_NOTE: {
                    break;
                }
                case CMD_ADD_NOTES: {
                    break;
                }
                case CMD_DELETE_NOTES: {
                    break;
                }
                case CMD_ADD_NOTES_SAME: {
                    break;
                }
                case CMD_DELETE_NOTES_SAME: {
                    break;
                }
                }
                undo_tree->current = node->prev ? node->prev : NULL;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_normal_ctrl_r: {
            call_terry_davis("cmd_normal_ctrl_r");
            assert(undo_tree != NULL);
            war_undo_node* next_node = NULL;
            if (!undo_tree->current) {
                next_node = undo_tree->root;
            } else if (undo_tree->current->next) {
                next_node = undo_tree->current->next;
            } else if (undo_tree->current->alt_next) {
                next_node = undo_tree->current->alt_next;
            }
            if (next_node) {
                assert(next_node != NULL);
                switch (next_node->command) {
                case CMD_ADD_NOTE: {
                    break;
                }
                case CMD_DELETE_NOTE: {
                    break;
                }
                case CMD_ADD_NOTES: {
                    break;
                }
                case CMD_DELETE_NOTES: {
                    break;
                }
                case CMD_ADD_NOTES_SAME: {
                    break;
                }
                case CMD_DELETE_NOTES_SAME: {
                    break;
                }
                }
                undo_tree->current = next_node;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        //------------------------------------------------------------------
        // RECORD COMMANDS
        //------------------------------------------------------------------
        cmd_record_tab: {
            call_terry_davis("cmd_record_tab");
            atomic_fetch_xor(&atomics->capture_monitor, 1);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_K: {
            call_terry_davis("cmd_record_K");
            float gain =
                atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
            gain = fminf(gain, 1.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_J: {
            call_terry_davis("cmd_record_J");
            float gain =
                atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
            gain = fmaxf(gain, 0.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_k: {
            call_terry_davis("cmd_record_k");
            float gain =
                atomic_load(&atomics->capture_gain) + ctx_wr->gain_increment;
            gain = fminf(gain, 1.0f);
            atomic_store(&atomics->capture_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_j: {
            call_terry_davis("cmd_record_j");
            float gain =
                atomic_load(&atomics->capture_gain) - ctx_wr->gain_increment;
            gain = fmaxf(gain, 0.0f);
            atomic_store(&atomics->capture_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_Q: {
            call_terry_davis("cmd_record_Q");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_space: {
            call_terry_davis("cmd_record_space");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_q: {
            call_terry_davis("cmd_record_q");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_w: {
            call_terry_davis("cmd_record_w");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_e: {
            call_terry_davis("cmd_record_e");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_r: {
            call_terry_davis("cmd_record_r");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_t: {
            call_terry_davis("cmd_record_t");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_y: {
            call_terry_davis("cmd_record_y");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_u: {
            call_terry_davis("cmd_record_u");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_i: {
            call_terry_davis("cmd_record_i");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_o: {
            call_terry_davis("cmd_record_o");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_p: {
            call_terry_davis("cmd_record_p");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_leftbracket: {
            call_terry_davis("cmd_record_leftbracket");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            float note = 10 + 12 * (ctx_wr->record_octave + 1);
            if (note > 127) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            atomic_store(&atomics->map_note, note);
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_rightbracket: {
            call_terry_davis("cmd_record_rightbracket");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            float note = 11 + 12 * (ctx_wr->record_octave + 1);
            if (note > 127) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            atomic_store(&atomics->map_note, note);
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_record_minus:
            call_terry_davis("cmd_record_minus");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = -1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_0:
            call_terry_davis("cmd_record_0");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_1:
            call_terry_davis("cmd_record_1");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_2:
            call_terry_davis("cmd_record_2");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 2;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_3:
            call_terry_davis("cmd_record_3");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 3;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_4:
            call_terry_davis("cmd_record_4");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 4;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_5:
            call_terry_davis("cmd_record_5");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 5;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_6:
            call_terry_davis("cmd_record_6");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 6;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_7:
            call_terry_davis("cmd_record_7");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 7;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_8:
            call_terry_davis("cmd_record_8");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 8;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_9:
            call_terry_davis("cmd_record_9");
            if (atomic_load(&atomics->capture)) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->record_octave = 9;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        cmd_record_esc:
            call_terry_davis("cmd_record_esc");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            atomic_store(&atomics->capture, 0);
            atomic_store(&atomics->map_note, -1);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        //-----------------------------------------------------------------
        // COMMANDS VIEWS
        //-----------------------------------------------------------------
        cmd_views_k: {
            call_terry_davis("cmd_views_k");
            uint32_t increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_add_uint32(
                views->warpoon_row, increment, views->warpoon_max_row);
            if (views->warpoon_row >
                views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_add_uint32(views->warpoon_bottom_row,
                                         increment,
                                         views->warpoon_max_row);
                views->warpoon_top_row = war_clamp_add_uint32(
                    views->warpoon_top_row, increment, views->warpoon_max_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_bottom_row =
                        war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                                  diff,
                                                  views->warpoon_min_row);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_j: {
            call_terry_davis("cmd_views_j");
            uint32_t increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_subtract_uint32(
                views->warpoon_row, increment, views->warpoon_min_row);
            if (views->warpoon_row <
                views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                              increment,
                                              views->warpoon_min_row);
                views->warpoon_top_row = war_clamp_subtract_uint32(
                    views->warpoon_top_row, increment, views->warpoon_min_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_top_row = war_clamp_add_uint32(
                        views->warpoon_top_row, diff, views->warpoon_max_row);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_h: {
            call_terry_davis("cmd_views_h");
            if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            uint32_t increment = ctx_wr->col_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
            }
            views->warpoon_col = war_clamp_subtract_uint32(
                views->warpoon_col, increment, views->warpoon_min_col);
            if (views->warpoon_col <
                views->warpoon_left_col + ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                views->warpoon_left_col = war_clamp_subtract_uint32(
                    views->warpoon_left_col, increment, views->warpoon_min_col);
                views->warpoon_right_col =
                    war_clamp_subtract_uint32(views->warpoon_right_col,
                                              increment,
                                              views->warpoon_min_col);
                uint32_t new_viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    views->warpoon_right_col = war_clamp_add_uint32(
                        views->warpoon_right_col, diff, views->warpoon_max_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_l: {
            call_terry_davis("cmd_views_l");
            if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            uint32_t increment = ctx_wr->col_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
            }
            views->warpoon_col = war_clamp_add_uint32(
                views->warpoon_col, increment, views->warpoon_max_col);
            if (views->warpoon_col >
                views->warpoon_right_col - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                views->warpoon_left_col = war_clamp_add_uint32(
                    views->warpoon_left_col, increment, views->warpoon_max_col);
                views->warpoon_right_col =
                    war_clamp_add_uint32(views->warpoon_right_col,
                                         increment,
                                         views->warpoon_max_col);
                uint32_t new_viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    views->warpoon_left_col = war_clamp_subtract_uint32(
                        views->warpoon_left_col, diff, views->warpoon_min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            call_terry_davis("warpoon col: %u", views->warpoon_col);
            goto cmd_done;
        }
        cmd_views_alt_k: {
            call_terry_davis("cmd_views_alt_k");
            uint32_t increment = ctx_wr->row_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_add_uint32(
                views->warpoon_row, increment, views->warpoon_max_row);
            if (views->warpoon_row >
                views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_add_uint32(views->warpoon_bottom_row,
                                         increment,
                                         views->warpoon_max_row);
                views->warpoon_top_row = war_clamp_add_uint32(
                    views->warpoon_top_row, increment, views->warpoon_max_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_bottom_row =
                        war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                                  diff,
                                                  views->warpoon_min_row);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_alt_j: {
            call_terry_davis("cmd_views_alt_j");
            uint32_t increment = ctx_wr->row_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_subtract_uint32(
                views->warpoon_row, increment, views->warpoon_min_row);
            if (views->warpoon_row <
                views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                              increment,
                                              views->warpoon_min_row);
                views->warpoon_top_row = war_clamp_subtract_uint32(
                    views->warpoon_top_row, increment, views->warpoon_min_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_top_row = war_clamp_add_uint32(
                        views->warpoon_top_row, diff, views->warpoon_max_row);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_alt_h: {
            call_terry_davis("cmd_views_alt_h");
            if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            uint32_t increment = ctx_wr->col_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
            }
            views->warpoon_col = war_clamp_subtract_uint32(
                views->warpoon_col, increment, views->warpoon_min_col);
            if (views->warpoon_col <
                views->warpoon_left_col + ctx_wr->scroll_margin_cols) {
                uint32_t viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                views->warpoon_left_col = war_clamp_subtract_uint32(
                    views->warpoon_left_col, increment, views->warpoon_min_col);
                views->warpoon_right_col =
                    war_clamp_subtract_uint32(views->warpoon_right_col,
                                              increment,
                                              views->warpoon_min_col);
                uint32_t new_viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    views->warpoon_right_col = war_clamp_add_uint32(
                        views->warpoon_right_col, diff, views->warpoon_max_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_alt_l: {
            call_terry_davis("cmd_views_alt_l");
            if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            uint32_t increment = ctx_wr->col_leap_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_col);
            }
            views->warpoon_col = war_clamp_add_uint32(
                views->warpoon_col, increment, views->warpoon_max_col);
            if (views->warpoon_col >
                views->warpoon_right_col - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                views->warpoon_left_col = war_clamp_add_uint32(
                    views->warpoon_left_col, increment, views->warpoon_max_col);
                views->warpoon_right_col =
                    war_clamp_add_uint32(views->warpoon_right_col,
                                         increment,
                                         views->warpoon_max_col);
                uint32_t new_viewport_width =
                    views->warpoon_right_col - views->warpoon_left_col;
                if (new_viewport_width < viewport_width) {
                    uint32_t diff = viewport_width - new_viewport_width;
                    views->warpoon_left_col = war_clamp_subtract_uint32(
                        views->warpoon_left_col, diff, views->warpoon_min_col);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_K: {
            call_terry_davis("cmd_views_K");
            war_warpoon_shift_up(views);
            uint32_t increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_add_uint32(
                views->warpoon_row, increment, views->warpoon_max_row);
            if (views->warpoon_row >
                views->warpoon_top_row - ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_add_uint32(views->warpoon_bottom_row,
                                         increment,
                                         views->warpoon_max_row);
                views->warpoon_top_row = war_clamp_add_uint32(
                    views->warpoon_top_row, increment, views->warpoon_max_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_bottom_row =
                        war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                                  diff,
                                                  views->warpoon_min_row);
                }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_J: {
            call_terry_davis("cmd_views_J");
            war_warpoon_shift_down(views);
            uint32_t increment = ctx_wr->row_increment;
            if (ctx_wr->numeric_prefix) {
                increment = war_clamp_multiply_uint32(
                    increment, ctx_wr->numeric_prefix, views->warpoon_max_row);
            }
            views->warpoon_row = war_clamp_subtract_uint32(
                views->warpoon_row, increment, views->warpoon_min_row);
            if (views->warpoon_row <
                views->warpoon_bottom_row + ctx_wr->scroll_margin_rows) {
                uint32_t viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                views->warpoon_bottom_row =
                    war_clamp_subtract_uint32(views->warpoon_bottom_row,
                                              increment,
                                              views->warpoon_min_row);
                views->warpoon_top_row = war_clamp_subtract_uint32(
                    views->warpoon_top_row, increment, views->warpoon_min_row);
                uint32_t new_viewport_height =
                    views->warpoon_top_row - views->warpoon_bottom_row;
                if (new_viewport_height < viewport_height) {
                    uint32_t diff = viewport_height - new_viewport_height;
                    views->warpoon_top_row = war_clamp_add_uint32(
                        views->warpoon_top_row, diff, views->warpoon_max_row);
                }
            }

            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_d: {
            call_terry_davis("cmd_views_d");
            uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
            if (i_views >= views->views_count) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            war_warpoon_delete_at_i(views, i_views);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_V: {
            call_terry_davis("cmd_views_V");
            // switch (views->warpoon_mode) {
            // case ctx_fsm->MODE_ROLL:
            //     views->warpoon_mode = ctx_fsm->ctx_fsm->MODE_VISUAL_LINE;
            //     views->warpoon_visual_line_row = views->warpoon_row;
            //     break;
            // case ctx_fsm->ctx_fsm->MODE_VISUAL_LINE:
            //     views->warpoon_mode = ctx_fsm->MODE_ROLL;
            //     break;
            // }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_esc: {
            call_terry_davis("cmd_views_esc");
            if (views->warpoon_mode == ctx_fsm->MODE_VISUAL_LINE) {
                views->warpoon_mode = ctx_fsm->MODE_ROLL;
                // logic for undoing the selection
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_z: {
            call_terry_davis("cmd_views_z");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
            if (i_views >= views->views_count) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_x = views->col[i_views];
            ctx_wr->cursor_pos_y = views->row[i_views];
            ctx_wr->left_col = views->left_col[i_views];
            ctx_wr->bottom_row = views->bottom_row[i_views];
            ctx_wr->right_col = views->right_col[i_views];
            ctx_wr->top_row = views->top_row[i_views];
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_views_return: {
            call_terry_davis("cmd_views_return");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            uint32_t i_views = views->warpoon_max_row - views->warpoon_row;
            if (i_views >= views->views_count) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->cursor_pos_x = views->col[i_views];
            ctx_wr->cursor_pos_y = views->row[i_views];
            ctx_wr->left_col = views->left_col[i_views];
            ctx_wr->bottom_row = views->bottom_row[i_views];
            ctx_wr->right_col = views->right_col[i_views];
            ctx_wr->top_row = views->top_row[i_views];
            if (ctx_wr->layer_flux) {
                war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        // midi
        cmd_midi_alt_1: {
            call_terry_davis("cmd_normal_alt_1");
            int idx = 0;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_2: {
            call_terry_davis("cmd_normal_alt_2");
            int idx = 1;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_3: {
            call_terry_davis("cmd_normal_alt_3");
            int idx = 2;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_4: {
            call_terry_davis("cmd_normal_alt_4");
            int idx = 3;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_5: {
            call_terry_davis("cmd_normal_alt_5");
            int idx = 4;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_6: {
            call_terry_davis("cmd_normal_alt_6");
            int idx = 5;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_7: {
            call_terry_davis("cmd_normal_alt_7");
            int idx = 6;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_8: {
            call_terry_davis("cmd_normal_alt_8");
            int idx = 7;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_9: {
            call_terry_davis("cmd_normal_alt_9");
            int idx = 8;
            ctx_wr->color_cursor = ctx_color->colors[idx];
            ctx_wr->color_cursor_transparent = ctx_color->colors[idx];
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << idx));
            ctx_wr->layers_active_count = 1;
            ctx_wr->layers_active[0] = (idx + 1) + '0';
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_0: {
            call_terry_davis("cmd_normal_alt_0");
            int layer_count = atomic_load(&ctx_lua->A_LAYER_COUNT);
            ctx_wr->color_cursor = full_white_hex;
            ctx_wr->color_cursor_transparent = white_hex;
            ctx_wr->color_note_outline_default = white_hex;
            atomic_store(&atomics->layer, (1ULL << layer_count) - 1);
            ctx_wr->layers_active_count = 9;
            for (int i = 0; i < ctx_wr->layers_active_count; i++) {
                ctx_wr->layers_active[i] = (i + 1) + '0';
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_1: {
            call_terry_davis("cmd_normal_alt_shift_1");
            int idx = 0;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_2: {
            call_terry_davis("cmd_normal_alt_shift_2");
            int idx = 1;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_3: {
            call_terry_davis("cmd_normal_alt_shift_3");
            int idx = 2;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_4: {
            call_terry_davis("cmd_normal_alt_shift_4");
            int idx = 3;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_5: {
            call_terry_davis("cmd_normal_alt_shift_5");
            int idx = 4;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_6: {
            call_terry_davis("cmd_normal_alt_shift_6");
            int idx = 5;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_7: {
            call_terry_davis("cmd_normal_alt_shift_7");
            int idx = 6;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_8: {
            call_terry_davis("cmd_normal_alt_shift_8");
            int idx = 7;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_9: {
            call_terry_davis("cmd_normal_alt_shift_9");
            int idx = 8;
            atomic_fetch_xor(&atomics->layer, (1ULL << idx));
            uint64_t layer = atomic_load(&atomics->layer);
            ctx_wr->layers_active_count = __builtin_popcountll(layer);
            switch (ctx_wr->layers_active_count) {
            case 0: {
                ctx_wr->color_cursor = white_hex;
                ctx_wr->color_cursor_transparent = full_white_hex;
                break;
            }
            case 1: {
                int active = __builtin_ctzll(layer);
                ctx_wr->color_cursor = ctx_color->colors[active];
                ctx_wr->color_cursor_transparent = ctx_color->colors[active];
                ctx_wr->color_note_outline_default = white_hex;
                ctx_wr->layers_active[0] = (active + 1) + '0';
                break;
            }
            default: {
                int count = 0;
                while (layer) {
                    int active = __builtin_ctzll(layer);
                    ctx_wr->layers_active[count++] = (active + 1) + '0';
                    layer &= layer - 1;
                }
                ctx_wr->color_cursor = full_white_hex;
                ctx_wr->color_cursor_transparent = white_hex;
                ctx_wr->color_note_outline_default = white_hex;
                break;
            }
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_alt_shift_0: {
            call_terry_davis("cmd_normal_alt_shift_0");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_m: {
            call_terry_davis("cmd_midi_m");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_T: {
            call_terry_davis("cmd_midi_T");
            ctx_wr->midi_toggle ^= 1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_b: {
            call_terry_davis("cmd_midi_b");
            ctx_wr->midi_toggle ^= 1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_x: {
            call_terry_davis("cmd_midi_x");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_c: {
            call_terry_davis("cmd_midi_c");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_K: {
            call_terry_davis("cmd_midi_K");
            float gain =
                atomic_load(&atomics->play_gain) + ctx_wr->gain_increment;
            gain = fminf(gain, 1.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_J: {
            call_terry_davis("cmd_midi_J");
            float gain =
                atomic_load(&atomics->play_gain) - ctx_wr->gain_increment;
            gain = fmaxf(gain, 0.0f);
            atomic_store(&atomics->play_gain, gain);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_Q: {
            call_terry_davis("cmd_midi_Q");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_space: {
            call_terry_davis("cmd_midi_space");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_q: {
            call_terry_davis("cmd_midi_q");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_w: {
            call_terry_davis("cmd_midi_w");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_e: {
            call_terry_davis("cmd_midi_e");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_r: {
            call_terry_davis("cmd_midi_r");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_t: {
            call_terry_davis("cmd_midi_t");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_y: {
            call_terry_davis("cmd_midi_y");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_u: {
            call_terry_davis("cmd_midi_u");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_i: {
            call_terry_davis("cmd_midi_i");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_o: {
            call_terry_davis("cmd_midi_o");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_p: {
            call_terry_davis("cmd_midi_p");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_leftbracket: {
            call_terry_davis("cmd_midi_leftbracket");
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_rightbracket: {
            call_terry_davis("cmd_midi_rightbracket");
            int note = 11 + 12 * (ctx_wr->midi_octave + 1);
            if (note > 127) {
                ctx_wr->numeric_prefix = 0;
                goto cmd_done;
            }
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_l: {
            call_terry_davis("cmd_midi_l");
            atomic_fetch_xor(&atomics->loop, 1);
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_minus: {
            call_terry_davis("cmd_midi_minus");
            ctx_wr->midi_octave = -1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_esc: {
            call_terry_davis("cmd_midi_esc");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            goto cmd_done;
        }
        cmd_midi_0: {
            call_terry_davis("cmd_midi_0");
            ctx_wr->midi_octave = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_1: {
            call_terry_davis("cmd_midi_1");
            ctx_wr->midi_octave = 1;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_2: {
            call_terry_davis("cmd_midi_2");
            ctx_wr->midi_octave = 2;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_3: {
            call_terry_davis("cmd_midi_3");
            ctx_wr->midi_octave = 3;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_4: {
            call_terry_davis("cmd_midi_4");
            ctx_wr->midi_octave = 4;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_5: {
            call_terry_davis("cmd_midi_5");
            ctx_wr->midi_octave = 5;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_6: {
            call_terry_davis("cmd_midi_6");
            ctx_wr->midi_octave = 6;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_7: {
            call_terry_davis("cmd_midi_7");
            ctx_wr->midi_octave = 7;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_8: {
            call_terry_davis("cmd_midi_8");
            ctx_wr->midi_octave = 8;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_midi_9: {
            call_terry_davis("cmd_midi_9");
            ctx_wr->midi_octave = 9;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
            //-----------------------------------------------------------------
            // COMMAND MODE
            //-----------------------------------------------------------------
        cmd_command_esc: {
            call_terry_davis("cmd_command_esc");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_command_w: {
            call_terry_davis("cmd_command_w");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_command_space: {
            call_terry_davis("cmd_command_space");
            goto cmd_done;
        }
        //--------------------------------------------------------------------
        // WAV COMMANDS
        //--------------------------------------------------------------------
        cmd_wav_Q: {
            call_terry_davis("cmd_wav_Q");
            ctx_capture->capture = !ctx_capture->capture;
            if (ctx_capture->capture) { goto cmd_done; }
            if (ctx_capture->prompt) {
                ctx_capture->capture = 1;
                ctx_capture->prompt_step = 0;
                ctx_capture->state = CAPTURE_PROMPT;
                goto cmd_done;
            }
            if (ftruncate(capture_wav->fd, capture_wav->memfd_size) == -1) {
                call_terry_davis("save_file: fd ftruncate failed: %s",
                                 capture_wav->fname);
                goto cmd_done;
            }
            off_t offset = 0;
            call_terry_davis("saving file");
            ssize_t result = sendfile(capture_wav->fd,
                                      capture_wav->memfd,
                                      &offset,
                                      capture_wav->memfd_size);
            if (result == -1) {
                call_terry_davis("save_file: sendfile failed: %s",
                                 capture_wav->fname);
            }
            lseek(capture_wav->fd, 0, SEEK_SET);
            memset(capture_wav->wav + 44, 0, capture_wav->memfd_capacity - 44);
            capture_wav->memfd_size = 44;
            goto cmd_done;
        }
        cmd_wav_esc: {
            call_terry_davis("cmd_wav_esc");
            ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
            ctx_capture->state = CAPTURE_WAITING;
            ctx_capture->capture = 0;
            ctx_wr->numeric_prefix = 0;
            goto cmd_done;
        }
        cmd_void:
            goto cmd_done;
        cmd_done: {
            ctx_wr->cursor_blink_previous_us = ctx_wr->now;
            ctx_wr->cursor_blinking = false;
            ctx_wr->trinity = true;
            if (ctx_fsm->goto_cmd_repeat_done) {
                ctx_fsm->goto_cmd_repeat_done = false;
                goto cmd_repeat_done;
            }
            if (ctx_fsm->goto_cmd_timeout_done) {
                ctx_fsm->goto_cmd_timeout_done = false;
                goto cmd_timeout_done;
            }
            goto wayland_done;
        }
        wl_keyboard_modifiers:
            // dump_bytes("wl_keyboard_modifiers event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            xkb_state_update_mask(
                ctx_fsm->xkb_state,
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4 +
                              4),
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
            ctx_wr->cursor_x = (float)(int32_t)war_read_le32(
                                   msg_buffer + msg_buffer_offset + 12) /
                               256.0f * scale_factor;
            ctx_wr->cursor_y = (float)(int32_t)war_read_le32(
                                   msg_buffer + msg_buffer_offset + 16) /
                               256.0f * scale_factor;
            goto wayland_done;
        wl_pointer_button:
            // dump_bytes("wl_pointer_button event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            switch (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 12)) {
            case 1:
                if (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 8) ==
                    BTN_LEFT) {
                    if (((int)(ctx_wr->cursor_x / ctx_vk->cell_width) -
                         (int)ctx_wr->num_cols_for_line_numbers) < 0) {
                        ctx_wr->cursor_pos_x = ctx_wr->left_col;
                        break;
                    }
                    if ((((physical_height - ctx_wr->cursor_y) /
                          ctx_vk->cell_height) -
                         ctx_wr->num_rows_for_status_bars) < 0) {
                        ctx_wr->cursor_pos_y = ctx_wr->bottom_row;
                        break;
                    }
                    ctx_wr->cursor_pos_x =
                        (uint32_t)(ctx_wr->cursor_x / ctx_vk->cell_width) -
                        ctx_wr->num_cols_for_line_numbers + ctx_wr->left_col;
                    ctx_wr->cursor_pos_y =
                        (uint32_t)((physical_height - ctx_wr->cursor_y) /
                                   ctx_vk->cell_height) -
                        ctx_wr->num_rows_for_status_bars + ctx_wr->bottom_row;
                    ctx_wr->cursor_blink_previous_us = ctx_wr->now;
                    ctx_wr->cursor_blinking = false;
                    if (ctx_wr->cursor_pos_y > ctx_wr->max_row) {
                        ctx_wr->cursor_pos_y = ctx_wr->max_row;
                    }
                    if (ctx_wr->cursor_pos_y > ctx_wr->top_row) {
                        ctx_wr->cursor_pos_y = ctx_wr->top_row;
                    }
                    if (ctx_wr->cursor_pos_y < ctx_wr->bottom_row) {
                        ctx_wr->cursor_pos_y = ctx_wr->bottom_row;
                    }
                    if (ctx_wr->cursor_pos_x > ctx_wr->max_col) {
                        ctx_wr->cursor_pos_x = ctx_wr->max_col;
                    }
                    if (ctx_wr->cursor_pos_x > ctx_wr->right_col) {
                        ctx_wr->cursor_pos_x = ctx_wr->right_col;
                    }
                    if (ctx_wr->cursor_pos_x < ctx_wr->left_col) {
                        ctx_wr->cursor_pos_x = ctx_wr->left_col;
                    }
                    if (ctx_wr->layer_flux) {
                        war_layer_flux(ctx_wr, atomics, ctx_play, ctx_color);
                    }
                }
            }
            goto wayland_done;
        wl_pointer_axis:
            dump_bytes(
                "wl_pointer_axis event", msg_buffer + msg_buffer_offset, size);
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
            dump_bytes(
                "wl_touch_down event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_touch_up:
            dump_bytes(
                "wl_touch_up event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_touch_motion:
            dump_bytes(
                "wl_touch_motion event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_touch_frame:
            dump_bytes(
                "wl_touch_frame event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_touch_cancel:
            dump_bytes(
                "wl_touch_cancel event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_touch_shape:
            dump_bytes(
                "wl_touch_shape event", msg_buffer + msg_buffer_offset, size);
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
            dump_bytes(
                "wl_output_mode event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_output_done:
            dump_bytes(
                "wl_output_done event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_output_scale:
            dump_bytes(
                "wl_output_scale event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_output_name:
            dump_bytes(
                "wl_output_name event", msg_buffer + msg_buffer_offset, size);
            goto wayland_done;
        wl_output_description:
            dump_bytes("wl_output_description event",
                       msg_buffer + msg_buffer_offset,
                       size);
            goto wayland_done;
        wayland_default:
            dump_bytes("default event", msg_buffer + msg_buffer_offset, size);
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
    goto wr;
}
end_wr:
    close(ctx_vk->dmabuf_fd);
    ctx_vk->dmabuf_fd = -1;
    xkb_state_unref(ctx_fsm->xkb_state);
    xkb_context_unref(ctx_fsm->xkb_context);
    end("war_window_render");
    return 0;
}
static void war_play(void* userdata) {
    void** data = (void**)userdata;
    war_pipewire_context* ctx_pw = data[0];
    war_producer_consumer* pc_play = data[1];
    war_lua_context* ctx_lua = data[2];
    uint64_t* play_last_read_time = data[3];
    uint64_t* play_read_count = data[4];
    war_atomics* atomics = data[5];
    uint64_t now = war_get_monotonic_time_us();
    if (now - *play_last_read_time >= 1000000) {
        atomic_store(&atomics->play_reader_rate, (double)*play_read_count);
        *play_read_count = 0;
        *play_last_read_time = now;
    }
    (*play_read_count)++;
    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_pw->play_stream);
    if (!b) { return; }
    float* dst = (float*)b->buffer->datas[0].data;
    uint64_t bytes_needed = atomic_load(&ctx_lua->A_BYTES_NEEDED);
    uint64_t write_pos = pc_play->i_to_a;
    uint64_t read_pos = pc_play->i_from_a;
    int64_t available_bytes;
    if (write_pos >= read_pos) {
        available_bytes = write_pos - read_pos;
    } else {
        available_bytes = pc_play->size + write_pos - read_pos;
    }
    if (available_bytes >= bytes_needed) {
        uint64_t read_idx = read_pos;
        uint64_t samples_needed = bytes_needed / sizeof(float);
        for (uint64_t i = 0; i < samples_needed; i++) {
            dst[i] = ((float*)pc_play->to_a)[read_idx / 4];
            read_idx = (read_idx + 4) & (pc_play->size - 1);
        }
        pc_play->i_from_a = read_idx;
    } else {
        uint64_t samples_available = available_bytes / sizeof(float);
        uint64_t read_idx = read_pos;
        for (uint64_t i = 0; i < samples_available; i++) {
            dst[i] = ((float*)pc_play->to_a)[read_idx / 4];
            read_idx = (read_idx + 4) & (pc_play->size - 1);
        }
        pc_play->i_from_a = read_idx;
        memset(dst + samples_available, 0, bytes_needed - available_bytes);
    }
    b->buffer->datas[0].chunk->size = bytes_needed;
    pw_stream_queue_buffer(ctx_pw->play_stream, b);
}
static void war_capture(void* userdata) {
    void** data = (void**)userdata;
    war_pipewire_context* ctx_pw = data[0];
    war_producer_consumer* pc_capture = data[1];
    war_lua_context* ctx_lua = data[2];
    uint64_t* capture_last_write_time = data[3];
    uint64_t* capture_write_count = data[4];
    war_atomics* atomics = data[5];
    uint64_t now = war_get_monotonic_time_us();
    if (now - *capture_last_write_time >= 1000000) {
        atomic_store(&atomics->capture_writer_rate,
                     (double)*capture_write_count);
        // call_terry_davis("capture_writer_rate: %.2f Hz",
        //              atomic_load(&atomics->capture_writer_rate));
        *capture_write_count = 0;
        *capture_last_write_time = now;
    }
    (*capture_write_count)++;
    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_pw->capture_stream);
    if (!b) { return; }
    float* src = (float*)b->buffer->datas[0].data;
    uint64_t available_bytes = b->buffer->datas[0].chunk->size;
    uint64_t write_pos = pc_capture->i_to_a;
    uint64_t read_pos = pc_capture->i_from_a;
    int64_t space_available;
    if (read_pos > write_pos) {
        space_available = read_pos - write_pos;
    } else {
        space_available = pc_capture->size + read_pos - write_pos;
    }
    uint64_t bytes_to_write = available_bytes;
    if (bytes_to_write > space_available - 4) {
        bytes_to_write = space_available - 4;
    }
    if (bytes_to_write > 0) {
        uint64_t write_idx = write_pos;
        uint64_t samples_to_write = bytes_to_write / sizeof(float);
        for (uint64_t i = 0; i < samples_to_write; i++) {
            ((float*)pc_capture->to_a)[write_idx / 4] = src[i];
            write_idx = (write_idx + 4) & (pc_capture->size - 1);
        }
        pc_capture->i_to_a = write_idx;
    }
    pw_stream_queue_buffer(ctx_pw->capture_stream, b);
}
//-----------------------------------------------------------------------------
// THREAD AUDIO
//-----------------------------------------------------------------------------
void* war_audio(void* args) {
    header("war_audio");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc_control = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];
    war_pool* pool_a = args_ptrs[2];
    war_lua_context* ctx_lua = args_ptrs[3];
    war_producer_consumer* pc_play = args_ptrs[4];
    war_producer_consumer* pc_capture = args_ptrs[5];
    pool_a->pool_alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    pool_a->pool_size =
        war_get_pool_a_size(pool_a, ctx_lua, "src/lua/war_main.lua");
    pool_a->pool_size = ALIGN_UP(pool_a->pool_size, pool_a->pool_alignment);
    pool_a->pool_size += pool_a->pool_alignment * 10;
    call_terry_davis("pool_a hack size: %zu", pool_a->pool_size);
    int pool_result = posix_memalign(
        &pool_a->pool, pool_a->pool_alignment, pool_a->pool_size);
    memset(pool_a->pool, 0, pool_a->pool_size);
    assert(pool_result == 0 && pool_a->pool);
    pool_a->pool_ptr = (uint8_t*)pool_a->pool;
    struct sched_param param = {
        .sched_priority = atomic_load(&ctx_lua->A_SCHED_FIFO_PRIORITY)};
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        call_terry_davis("AUDIO THREAD ERROR WITH SCHEDULING FIFO");
        perror("pthread_setschedparam");
    }
    //-------------------------------------------------------------------------
    //  PIPEWIRE
    //-------------------------------------------------------------------------
    war_pipewire_context* ctx_pw =
        war_pool_alloc(pool_a, sizeof(war_pipewire_context));
    ctx_pw->play_data = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->A_PLAY_DATA_SIZE));
    // play_data
    ctx_pw->play_data[0] = ctx_pw;
    ctx_pw->play_data[1] = pc_play;
    ctx_pw->play_data[2] = ctx_lua;
    uint64_t* play_last_read_time = war_pool_alloc(pool_a, sizeof(uint64_t));
    uint64_t* play_read_count = war_pool_alloc(pool_a, sizeof(uint64_t));
    *play_last_read_time = 0;
    *play_read_count = 0;
    ctx_pw->play_data[3] = play_last_read_time;
    ctx_pw->play_data[4] = play_read_count;
    ctx_pw->play_data[5] = atomics;
    ctx_pw->capture_data = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->A_CAPTURE_DATA_SIZE));
    ctx_pw->capture_data[0] = ctx_pw;
    ctx_pw->capture_data[1] = pc_capture;
    ctx_pw->capture_data[2] = ctx_lua;
    uint64_t* capture_last_write_time =
        war_pool_alloc(pool_a, sizeof(uint64_t));
    uint64_t* capture_write_count = war_pool_alloc(pool_a, sizeof(uint64_t));
    *capture_last_write_time = 0;
    *capture_write_count = 0;
    ctx_pw->capture_data[3] = capture_last_write_time;
    ctx_pw->capture_data[4] = capture_write_count;
    ctx_pw->capture_data[5] = atomics;
    pw_init(NULL, NULL);
    ctx_pw->loop = pw_loop_new(NULL);
    ctx_pw->play_info = (struct spa_audio_info_raw){
        .format = SPA_AUDIO_FORMAT_F32_LE,
        .rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    ctx_pw->play_builder_data = war_pool_alloc(
        pool_a, sizeof(uint8_t) * atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    spa_pod_builder_init(&ctx_pw->play_builder,
                         ctx_pw->play_builder_data,
                         sizeof(uint8_t) *
                             atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    ctx_pw->play_params = spa_format_audio_raw_build(
        &ctx_pw->play_builder, SPA_PARAM_EnumFormat, &ctx_pw->play_info);
    ctx_pw->play_events = (struct pw_stream_events){
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_play,
    };
    // ctx_pw->play_properties = pw_properties_new(...
    ctx_pw->play_stream = pw_stream_new_simple(ctx_pw->loop,
                                               "WAR_play",
                                               NULL,
                                               &ctx_pw->play_events,
                                               ctx_pw->play_data);
    if (!ctx_pw->play_stream) { call_terry_davis("play_stream init issue"); }
    int pw_stream_result = pw_stream_connect(ctx_pw->play_stream,
                                             PW_DIRECTION_OUTPUT,
                                             PW_ID_ANY,
                                             PW_STREAM_FLAG_AUTOCONNECT |
                                                 PW_STREAM_FLAG_MAP_BUFFERS |
                                                 PW_STREAM_FLAG_RT_PROCESS,
                                             &ctx_pw->play_params,
                                             1);
    if (pw_stream_result < 0) {
        call_terry_davis("play stream connection error");
    }
    ctx_pw->capture_info = (struct spa_audio_info_raw){
        .format = SPA_AUDIO_FORMAT_F32_LE,
        .rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    ctx_pw->capture_builder_data = war_pool_alloc(
        pool_a, sizeof(uint8_t) * atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    spa_pod_builder_init(&ctx_pw->capture_builder,
                         ctx_pw->capture_builder_data,
                         sizeof(uint8_t) *
                             atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    ctx_pw->capture_params = spa_format_audio_raw_build(
        &ctx_pw->capture_builder, SPA_PARAM_EnumFormat, &ctx_pw->capture_info);
    ctx_pw->capture_events = (struct pw_stream_events){
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_capture,
    };
    ctx_pw->capture_stream = pw_stream_new_simple(ctx_pw->loop,
                                                  "WAR_capture",
                                                  NULL,
                                                  &ctx_pw->capture_events,
                                                  ctx_pw->capture_data);
    if (!ctx_pw->capture_stream) {
        call_terry_davis("capture_stream init issue");
    }
    pw_stream_result = pw_stream_connect(ctx_pw->capture_stream,
                                         PW_DIRECTION_INPUT,
                                         PW_ID_ANY,
                                         PW_STREAM_FLAG_AUTOCONNECT |
                                             PW_STREAM_FLAG_MAP_BUFFERS |
                                             PW_STREAM_FLAG_RT_PROCESS,
                                         &ctx_pw->capture_params,
                                         1);
    if (pw_stream_result < 0) {
        call_terry_davis("capture stream connection error");
    }
    while (pw_stream_get_state(ctx_pw->capture_stream, NULL) !=
           PW_STREAM_STATE_PAUSED) {
        pw_loop_iterate(ctx_pw->loop, 1);
        struct timespec ts = {0, 500000};
        nanosleep(&ts, NULL);
    }
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    uint32_t header;
    uint32_t size;
    uint8_t* control_payload =
        war_pool_alloc(pool_a, sizeof(uint8_t) * pc_control->size);
    uint8_t* tmp_control_payload =
        war_pool_alloc(pool_a, sizeof(uint8_t) * pc_control->size);
    void** pc_control_cmd = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->CMD_COUNT));
    pc_control_cmd[CONTROL_END_WAR] = &&end_a;
    atomic_store(&atomics->start_war, 1);
    struct timespec ts = {0, 500000}; // 0.5 ms
    //-------------------------------------------------------------------------
    // AUDIO LOOP
    //-------------------------------------------------------------------------
pc_a: {
    if (war_pc_from_wr(pc_control, &header, &size, control_payload)) {
        goto* pc_control_cmd[header];
    }
    goto pc_a_done;
}
pc_a_done: {
    pw_loop_iterate(ctx_pw->loop, 0);
    goto pc_a;
}
end_a: {
    war_pc_to_wr(pc_control, CONTROL_END_WAR, 0, NULL);
    pw_stream_destroy(ctx_pw->play_stream);
    pw_stream_destroy(ctx_pw->capture_stream);
    pw_loop_destroy(ctx_pw->loop);
    pw_deinit();
    end("war_audio");
    return 0;
}
}
