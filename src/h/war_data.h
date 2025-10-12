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
// src/h/war_data.h
//-----------------------------------------------------------------------------

#ifndef WAR_DATA_H
#define WAR_DATA_H

#include <ft2build.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include FT_FREETYPE_H

enum war_mods {
    MOD_NONE = 0,
    MOD_SHIFT = (1 << 0),
    MOD_CTRL = (1 << 1),
    MOD_ALT = (1 << 2),
    MOD_LOGO = (1 << 3),
    MOD_CAPS = (1 << 4),
    MOD_NUM = (1 << 5),
    MOD_FN = (1 << 6),
};

enum war_keysyms {
    KEYSYM_ESCAPE = 256,
    KEYSYM_LEFT = 257,
    KEYSYM_UP = 258,
    KEYSYM_RIGHT = 259,
    KEYSYM_DOWN = 260,
    KEYSYM_RETURN = 261,
    KEYSYM_SPACE = 262,
    KEYSYM_TAB = 263,
    KEYSYM_MINUS = 264,
    KEYSYM_LEFTBRACKET = 265,
    KEYSYM_RIGHTBRACKET = 266,
    KEYSYM_SEMICOLON = 267,
    KEYSYM_DEFAULT = 511,
    MAX_KEYSYM = 512,
    MAX_MOD = 16,
};

enum war_misc {
    max_objects = 1000,
    max_opcodes = 20,
    max_quads = 20000,
    max_text_quads = 20000,
    max_note_quads = 20000,
    max_frames = 1,
    max_instances_per_quad = 1,
    max_instances_per_sdf_quad = 1,
    max_fds = 50,
    OLED_MODE = 0,
    MAX_MIDI_NOTES = 128,
    MAX_SAMPLES_PER_NOTE = 128,
    UNSET = 0,
    MAX_DIGITS = 10,
    NUM_STATUS_BARS = 3,
    MAX_GRIDLINE_SPLITS = 4,
    MAX_VIEWS_SAVED = 13,
    MAX_WARPOON_TEXT_COLS = 25,
    atlas_width = 8192,
    atlas_height = 8192,
    MAX_STATUS_BAR_COLS = 200,
};

enum war_layers {
    LAYER_COUNT = 15,
    LAYER_OPAQUE_REGION = 0,
    LAYER_BACKGROUND = 1,
    LAYER_GRIDLINES = 2,
    LAYER_PLAYBACK_BAR = 3,
    LAYER_NOTES = 4,
    LAYER_NOTE_TEXT = 5,
    LAYER_HUD = 6,
    LAYER_HUD_TEXT = 7,
    LAYER_CURSOR = 8,
    LAYER_POPUP_BACKGROUND = 9,
    LAYER_POPUP_OUTLINE = 10,
    LAYER_POPUP_TEXT = 11,
    LAYER_POPUP_HUD = 12,
    LAYER_POPUP_HUD_TEXT = 13,
    LAYER_POPUP_CURSOR = 14,
};

enum war_hud {
    HUD_PIANO = 0,
    HUD_LINE_NUMBERS = 1,
    HUD_PIANO_AND_LINE_NUMBERS = 2,
};

enum war_modes {
    MODE_COUNT = 10,
    MODE_NORMAL = 0,
    MODE_VIEWS = 1,
    MODE_VISUAL_LINE = 2,
    MODE_RECORD = 3,
    MODE_MIDI = 4,
    MODE_COMMAND = 5,
    MODE_VISUAL_BLOCK = 6,
    MODE_INSERT = 7,
    MODE_O = 8,
    MODE_VISUAL = 9,
};

enum war_fsm {
    MAX_NODES = 1024,
    MAX_SEQUENCE_LENGTH = 7,
    MAX_CHILDREN = 32,
    SEQUENCE_COUNT = 140,
    MAX_STATES = 256,
    MAX_COMMAND_BUFFER_LENGTH = 128,
};

enum war_pipelines {
    PIPELINE_NONE = 0,
    PIPELINE_QUAD = 1,
    PIPELINE_SDF = 2,
};

enum war_cursor {
    CURSOR_BLINK_BPM = 1,
    CURSOR_BLINK = 2,
    DEFAULT_CURSOR_BLINK_DURATION = 700000,
};

typedef struct war_lua_context {
    _Atomic int AUDIO_SAMPLE_RATE;
    _Atomic int AUDIO_SAMPLE_DURATION;
    _Atomic int AUDIO_CHANNEL_COUNT;
    _Atomic int AUDIO_NOTE_COUNT;
    _Atomic int AUDIO_SAMPLES_PER_NOTE;
    _Atomic int POOL_ALIGNMENT;
    _Atomic int AUDIO_BPM;
    _Atomic int AUDIO_BASE_FREQUENCY;
    _Atomic int AUDIO_BASE_NOTE;
    _Atomic int AUDIO_EDO;
} war_lua_context;

typedef struct war_fsm_state {
    uint8_t* is_terminal;
    uint8_t* handle_release;
    uint8_t* handle_timeout;
    uint8_t* handle_repeat;
    uint8_t* is_prefix;
    void** command;
    uint16_t* next_state;
} war_fsm_state;

typedef struct war_label {
    void* command;
    uint8_t handle_release;
    uint8_t handle_timeout;
    uint8_t handle_repeat;
} war_label;

typedef struct war_key_event {
    uint32_t keysym;
    uint8_t mod;
} war_key_event;

typedef struct war_rgba_t {
    float r;
    float g;
    float b;
    float a;
} war_rgba_t;

typedef struct war_notes {
    uint64_t* start_frames;
    uint64_t* duration_frames;
    float* phase_increment;
    float* velocity; // can be constant if ASR-10 style
    uint32_t* sample_id;
    uint32_t* voice_id;
    // Envelope
    float* attack;
    float* decay;
    float* sustain;
    float* release;
    // Optional for multisample instruments
    uint8_t* key_low;
    uint8_t* key_high;
    uint32_t count;
} war_notes;

typedef struct war_note_quads {
    uint64_t* timestamp;
    uint32_t* col;
    uint32_t* row;
    uint32_t* sub_col;
    uint32_t* sub_row;
    uint32_t* sub_cells_col;
    uint32_t* cursor_width_whole_number;
    uint32_t* cursor_width_sub_col;
    uint32_t* cursor_width_sub_cells;
    uint32_t* color;
    uint32_t* outline_color;
    float* gain;
    uint32_t* voice;
    uint32_t* hidden;
    uint32_t* mute;
} war_note_quads;

typedef struct war_views {
    uint32_t* col;
    uint32_t* row;
    uint32_t* left_col;
    uint32_t* right_col;
    uint32_t* bottom_row;
    uint32_t* top_row;
    uint32_t views_count;
    // warpoon
    char** warpoon_text;
    uint32_t warpoon_mode;
    uint32_t warpoon_visual_line_row;
    uint32_t warpoon_col;
    uint32_t warpoon_row;
    uint32_t warpoon_left_col;
    uint32_t warpoon_right_col;
    uint32_t warpoon_bottom_row;
    uint32_t warpoon_top_row;
    uint32_t warpoon_hud_cols;
    uint32_t warpoon_hud_rows;
    uint32_t warpoon_color_bg;
    uint32_t warpoon_color_outline;
    uint32_t warpoon_color_text;
    uint32_t warpoon_color_hud;
    uint32_t warpoon_color_cursor;
    uint32_t warpoon_color_hud_text;
    uint32_t warpoon_viewport_cols;
    uint32_t warpoon_viewport_rows;
    uint32_t warpoon_max_col;
    uint32_t warpoon_min_col;
    uint32_t warpoon_max_row;
    uint32_t warpoon_min_row;
} war_views;

enum war_audio {
    // defaults
    AUDIO_DEFAULT_SAMPLE_RATE = 44100,
    AUDIO_DEFAULT_PERIOD_SIZE = 512,
    AUDIO_DEFAULT_SUB_PERIOD_FACTOR = 20,
    AUDIO_DEFAULT_CHANNEL_COUNT = 2,
    AUDIO_DEFAULT_BPM = 100,
    AUDIO_DEFAULT_PERIOD_COUNT = 4,
    AUDIO_DEFAULT_SAMPLE_DURATION = 30,
    AUDIO_DEFAULT_WARMUP_FRAMES_FACTOR = 800,
    // cmds
    AUDIO_CMD_COUNT = 20,
    AUDIO_CMD_STOP = 1,
    AUDIO_CMD_PLAY = 2,
    AUDIO_CMD_PAUSE = 3,
    AUDIO_CMD_GET_FRAMES = 4,
    AUDIO_CMD_ADD_NOTE = 5,
    AUDIO_CMD_END_WAR = 6,
    AUDIO_CMD_SEEK = 7,
    AUDIO_CMD_RECORD_WAIT = 8,
    AUDIO_CMD_RECORD = 9,
    AUDIO_CMD_RECORD_MAP = 10,
    AUDIO_CMD_SET_THRESHOLD = 11,
    AUDIO_CMD_NOTE_ON = 12,
    AUDIO_CMD_NOTE_OFF = 13,
    AUDIO_CMD_NOTE_OFF_ALL = 14,
    AUDIO_CMD_RESET_MAPPINGS = 15,
    AUDIO_CMD_MIDI_RECORD_WAIT = 16,
    AUDIO_CMD_MIDI_RECORD = 17,
    AUDIO_CMD_MIDI_RECORD_MAP = 18,
    AUDIO_CMD_SAVE = 19,
    // cmd sizes (not including header)
    // voices
    AUDIO_VOICE_GRAND_PIANO = 0,
    AUDIO_VOICE_COUNT = 128,
    AUDIO_SINE_TABLE_SIZE = 1024,
};

typedef struct war_atomics {
    _Atomic uint64_t play_clock;
    _Atomic uint64_t play_frames;
    _Atomic uint64_t record_frames;
    _Atomic uint8_t state;
    _Atomic float record_threshold;
    _Atomic uint8_t record;
    _Atomic uint8_t midi_record;
    _Atomic uint64_t midi_record_frames;
    _Atomic uint8_t play;
    _Atomic float bpm;
    _Atomic int16_t map_note;
    _Atomic uint8_t map;
    _Atomic uint8_t record_monitor;
    _Atomic float play_gain;
    _Atomic float record_gain;
    _Atomic uint8_t* notes_on;
    _Atomic uint8_t* notes_on_previous;
    _Atomic uint8_t loop;
    _Atomic uint8_t start_war;
    _Atomic uint8_t resample;
} war_atomics;

typedef struct war_producer_consumer {
    uint8_t* to_a;
    uint8_t* to_wr;
    uint32_t i_to_a;
    uint32_t i_to_wr;
    uint32_t i_from_a;
    uint32_t i_from_wr;
} war_producer_consumer;

typedef struct war_pool {
    void* pool;
    uint8_t* pool_ptr;
    size_t pool_size;
    size_t pool_alignment;
} war_pool;

typedef struct war_samples {
    int16_t** samples;
    uint64_t* samples_frames_start;
    uint64_t* samples_frames_duration;
    uint64_t* samples_frames;
    uint64_t* samples_frames_trim_start;
    uint64_t* samples_frames_trim_end;
    uint8_t* samples_active;
    float* samples_attack;
    float* samples_sustain;
    float* samples_release;
    float* samples_gain;
    float* notes_attack;
    float* notes_sustain;
    float* notes_release;
    float* notes_gain;
    uint64_t* notes_frames_start;
    uint64_t* notes_frames_duration;
    uint64_t* notes_frames_trim_start;
    uint64_t* notes_frames_trim_end;
    uint32_t* samples_count;
} war_samples;

typedef struct war_audio_context {
    float BPM;
    uint32_t sample_rate;
    uint32_t period_size;
    uint32_t sub_period_size;
    uint32_t channel_count;
    uint32_t sample_duration_seconds;
    float default_attack;
    float default_sustain;
    float default_release;
    float default_gain;
    // PipeWire
    struct pw_loop* pw_loop;
    struct pw_stream* play_stream;
    struct pw_stream* record_stream;
    int16_t* record_buffer;
    int16_t* resample_buffer;
    float phase;
    uint8_t over_threshold;
    uint64_t* sample_frames;
    uint64_t* sample_frames_duration;
    uint64_t warmup_frames;
    float* sample_phase;
    uint8_t* previous_note_states;
    uint64_t* note_play_start;
} war_audio_context;

typedef struct war_window_render_context {
    uint64_t now;
    uint32_t col;
    uint32_t row;
    uint32_t sub_col;
    uint32_t sub_row;
    uint32_t navigation_whole_number_col;
    uint32_t navigation_whole_number_row;
    uint32_t navigation_sub_cells_col;
    uint32_t navigation_sub_cells_row;
    uint32_t previous_navigation_whole_number_col;
    uint32_t previous_navigation_whole_number_row;
    uint32_t previous_navigation_sub_cells_col;
    uint32_t previous_navigation_sub_cells_row;
    uint8_t hud_state;
    uint32_t f_navigation_whole_number;
    uint32_t t_navigation_sub_cells;
    uint32_t t_navigation_whole_number;
    uint32_t f_navigation_sub_cells;
    uint32_t cursor_width_whole_number;
    uint32_t cursor_width_sub_col;
    uint32_t cursor_width_sub_cells;
    uint32_t f_cursor_width_whole_number;
    uint32_t f_cursor_width_sub_cells;
    uint32_t f_cursor_width_sub_col;
    uint32_t t_cursor_width_whole_number;
    uint32_t t_cursor_width_sub_cells;
    uint32_t t_cursor_width_sub_col;
    uint32_t gridline_splits[MAX_GRIDLINE_SPLITS];
    uint32_t left_col;
    uint32_t bottom_row;
    uint32_t right_col;
    uint32_t top_row;
    uint32_t col_increment;
    uint32_t row_increment;
    uint32_t col_leap_increment;
    uint32_t row_leap_increment;
    uint32_t numeric_prefix;
    uint32_t max_col;
    uint32_t max_row;
    uint32_t min_col;
    uint32_t min_row;
    float cursor_x;
    float cursor_y;
    float zoom_scale;
    float max_zoom_scale;
    float min_zoom_scale;
    float panning_x;
    float panning_y;
    float zoom_increment;
    float zoom_leap_increment;
    float anchor_x;
    float anchor_y;
    float anchor_ndc_x;
    float anchor_ndc_y;
    uint32_t scroll_margin_cols;
    uint32_t scroll_margin_rows;
    uint32_t viewport_cols;
    uint32_t viewport_rows;
    float cell_width;  // from vulkan_context
    float cell_height; // from vulkan_context
    float physical_width;
    float physical_height;
    float logical_width;
    float logical_height;
    uint32_t num_rows_for_status_bars;
    uint32_t num_cols_for_line_numbers;
    uint32_t mode;
    char input_sequence[MAX_SEQUENCE_LENGTH];
    uint8_t num_chars_in_sequence;
    war_note_quads note_quads;
    uint32_t note_quads_count;
    float layers[LAYER_COUNT];
    float layer_count;
    float playback_bar_pos_x;
    float playback_bar_pos_x_increment;
    float FPS;
    uint64_t frame_duration_us;
    bool sleep;
    uint64_t sleep_duration_us;
    bool end_window_render;
    bool trinity;
    bool fullscreen;
    uint32_t light_gray_hex;
    uint32_t darker_light_gray_hex;
    uint32_t dark_gray_hex;
    uint32_t red_hex;
    uint32_t white_hex;
    uint32_t black_hex;
    uint32_t full_white_hex;
    float horizontal_line_thickness;
    float vertical_line_thickness;
    float outline_thickness;
    float alpha_scale;
    float alpha_scale_cursor;
    float playback_bar_thickness;
    float text_feather;
    float text_thickness;
    float text_feather_bold;
    float text_thickness_bold;
    char* text_top_status_bar;
    uint32_t text_top_status_bar_count;
    char* text_middle_status_bar;
    uint32_t text_middle_status_bar_count;
    char* text_bottom_status_bar;
    uint32_t text_bottom_status_bar_count;
    uint32_t text_status_bar_start_index;
    uint32_t text_status_bar_middle_index;
    uint32_t text_status_bar_end_index;
    uint8_t cursor_blink_state;
    uint64_t cursor_blink_duration_us;
    uint64_t cursor_blink_previous_us;
    bool cursor_blinking;
    uint32_t color_note_default;
    uint32_t color_note_outline_default;
    uint32_t color_cursor;
    uint32_t color_cursor_transparent;
    float record_octave;
    float gain_increment;
    float midi_octave;
    float midi_note;
    bool trigger;
    bool skip_release;
} war_window_render_context;

enum war_producer_consumer_enum {
    PC_BUFFER_SIZE = 4096,
};

typedef struct war_glyph_info {
    float advance_x;
    float advance_y;
    float bearing_x;
    float bearing_y;
    float width;
    float height;
    float uv_x0, uv_y0, uv_x1, uv_y1;
    float ascent;
    float descent;
} war_glyph_info;

typedef struct war_text_vertex {
    float corner[2];
    float pos[3];
    uint32_t color;
    float uv[2];
    float glyph_bearing[2];
    float glyph_size[2];
    float ascent;
    float descent;
    float thickness;
    float feather;
    uint32_t flags;
} war_text_vertex;

typedef struct war_text_instance {
    uint32_t x;
    uint32_t y;
    uint32_t color;
    float uv_x;
    float uv_y;
    float thickness;
    float feather;
    uint32_t flags;
} war_text_instance;

typedef struct war_text_push_constants {
    float bottom_left[2];
    float physical_size[2];
    float cell_size[2];
    float zoom;
    uint32_t _pad;
    float cell_offsets[2];
    float scroll_margin[2];
    float anchor_cell[2];
    float top_right[2];
    float ascent;
    float descent;
    float line_gap;
    float baseline;
    float font_height;
} war_text_push_constants;

typedef enum war_quad_flags {
    QUAD_LINE = 1 << 0,
    QUAD_OUTLINE = 1 << 1,
    QUAD_GRID = 1 << 2,
} war_quad_flags;

typedef struct war_quad_vertex {
    float corner[2];
    float pos[3];
    float span[2];
    uint32_t color;
    float outline_thickness;
    uint32_t outline_color;
    float line_thickness[2];
    uint32_t flags;
    uint32_t _pad;
} war_quad_vertex;

typedef struct war_quad_instance {
    uint32_t x;
    uint32_t y;
    uint32_t color;
    uint32_t flags;
} war_quad_instance;

typedef struct war_quad_push_constants {
    float bottom_left[2];
    float physical_size[2];
    float cell_size[2];
    float zoom;
    uint _pad1;
    float cell_offsets[2];
    float scroll_margin[2];
    float anchor_cell[2];
    float top_right[2];
    uint32_t _pad2[2];
} war_quad_push_constants;

typedef struct war_vulkan_context {
    //-------------------------------------------------------------------------
    // QUAD PIPELINE
    //-------------------------------------------------------------------------
    int dmabuf_fd;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family_index;
    VkImage image;
    VkDeviceMemory memory;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
    VkRenderPass render_pass;
    VkFramebuffer frame_buffer;
    VkPipeline quad_pipeline;
    VkPipeline transparent_quad_pipeline;
    VkPipelineLayout pipeline_layout;
    VkImageView image_view;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkBuffer quads_vertex_buffer;
    VkDeviceMemory quads_vertex_buffer_memory;
    VkBuffer quads_index_buffer;
    VkDeviceMemory quads_index_buffer_memory;
    VkBuffer quads_instance_buffer;
    VkDeviceMemory quads_instance_buffer_memory;
    VkImage texture_image;
    VkDeviceMemory texture_memory;
    VkImageView texture_image_view;
    VkSampler texture_sampler;
    VkDescriptorSet texture_descriptor_set;
    VkDescriptorPool texture_descriptor_pool;
    VkFence* in_flight_fences;
    void* quads_vertex_buffer_mapped;
    void* quads_index_buffer_mapped;
    void* quads_instance_buffer_mapped;
    uint32_t current_frame;

    //-------------------------------------------------------------------------
    // TEXT PIPELINE
    //-------------------------------------------------------------------------
    FT_Library ft_library;
    FT_Face ft_regular;
    VkImage text_image;
    VkImageView text_image_view;
    VkDeviceMemory text_image_memory;
    VkSampler text_sampler;
    war_glyph_info* glyphs;
    VkDescriptorSet font_descriptor_set;
    VkDescriptorSetLayout font_descriptor_set_layout;
    VkDescriptorPool font_descriptor_pool;
    VkPipeline text_pipeline;
    VkPipelineLayout text_pipeline_layout;
    VkShaderModule text_vertex_shader;
    VkShaderModule text_fragment_shader;
    VkPushConstantRange text_push_constant_range;
    VkBuffer text_vertex_buffer;
    VkDeviceMemory text_vertex_buffer_memory;
    VkBuffer text_instance_buffer;
    VkDeviceMemory text_instance_buffer_memory;
    VkBuffer text_index_buffer;
    VkDeviceMemory text_index_buffer_memory;
    VkRenderPass text_render_pass;
    float ascent;
    float descent;
    float line_gap;
    float baseline;
    float font_height;
    float cell_height;
    float cell_width;
    float scale_x;
    float scale_y;
    void* text_vertex_buffer_mapped;
    void* text_instance_buffer_mapped;
    void* text_index_buffer_mapped;
} war_vulkan_context;

typedef struct war_drm_context {
    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
} war_drm_context;

#endif // WAR_DATA_H
