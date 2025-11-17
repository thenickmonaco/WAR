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
    KEYSYM_PLUS = 268,
    KEYSYM_EQUAL = 269,
    KEYSYM_BACKSPACE = 270,
    KEYSYM_APOSTROPHE = 271,
    KEYSYM_COMMA = 272,
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
    MAX_STATUS_BAR_COLS = 200,
    PROMPT_LAYER = 1,
    PROMPT_NOTE = 2,
    PROMPT_NAME = 3,
    ALL_NOTE_LAYERS = -13,
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
    MODE_CAPTURE = 3,
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

enum war_undo_commands_enum {
    CMD_ADD_NOTE = 0,
    CMD_DELETE_NOTE = 1,
    CMD_ADD_NOTES = 2,
    CMD_DELETE_NOTES = 3,
    CMD_SWAP_ADD_NOTES = 4,
    CMD_SWAP_DELETE_NOTES = 5,
    CMD_ADD_NOTES_SAME = 6,
    CMD_DELETE_NOTES_SAME = 7,
};

typedef struct __attribute__((packed)) war_riff_header {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
} war_riff_header;

typedef struct __attribute__((packed)) war_fmt_chunk {
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} war_fmt_chunk;

typedef struct __attribute__((packed)) war_data_chunk {
    char subchunk2_id[4];
    uint32_t subchunk2_size;
} war_data_chunk;

typedef struct war_notes {
    uint8_t* alive;
    uint64_t* id;
    uint64_t* notes_start_frames;
    uint64_t* notes_duration_frames;
    int16_t* note;
    uint64_t* layer;
    float* notes_phase_increment;
    float* notes_gain;
    float* notes_attack;
    float* notes_sustain;
    float* notes_release;
    uint32_t notes_count;
} war_notes;

typedef struct war_note {
    uint8_t alive;
    uint64_t id;
    uint64_t note_start_frames;
    uint64_t note_duration_frames;
    int16_t note;
    uint64_t layer;
    float note_phase_increment;
    float note_gain;
    float note_attack;
    float note_sustain;
    float note_release;
} war_note;

typedef struct war_note_quads {
    uint8_t* alive;
    uint64_t* id;
    double* pos_x;
    double* pos_y;
    uint64_t* layer;
    double* size_x;
    double* navigation_x;
    uint32_t* navigation_x_numerator;
    uint32_t* navigation_x_denominator;
    uint32_t* size_x_numerator;
    uint32_t* size_x_denominator;
    uint32_t* color;
    uint32_t* outline_color;
    float* gain;
    uint32_t* voice;
    uint32_t* hidden;
    uint32_t* mute;
    uint32_t count;
} war_note_quads;

typedef struct war_note_quad {
    uint8_t alive;
    uint64_t id;
    double pos_x;
    double pos_y;
    uint64_t layer;
    double size_x;
    double navigation_x;
    uint32_t navigation_x_numerator;
    uint32_t navigation_x_denominator;
    uint32_t size_x_numerator;
    uint32_t size_x_denominator;
    uint32_t color;
    uint32_t outline_color;
    float gain;
    uint32_t voice;
    uint32_t hidden;
    uint32_t mute;
} war_note_quad;

typedef struct war_payload_add_note {
    war_note note;
    war_note_quad note_quad;
} war_payload_add_note;

typedef struct war_payload_delete_note {
    war_note note;
    war_note_quad note_quad;
} war_payload_delete_note;

typedef struct war_payload_add_notes {
    war_note* note;
    war_note_quad* note_quad;
    uint32_t count;
} war_payload_add_notes;

typedef struct war_payload_delete_notes {
    war_note* note;
    war_note_quad* note_quad;
    uint32_t count;
} war_payload_delete_notes;

typedef struct war_payload_add_notes_same {
    war_note note;
    war_note_quad note_quad;
    uint64_t* ids;
    uint32_t count;
} war_payload_add_notes_same;

typedef struct war_payload_delete_notes_same {
    war_note note;
    war_note_quad note_quad;
    uint64_t* ids;
    uint32_t count;
} war_payload_delete_notes_same;

typedef struct war_payload_swap_add_notes {
    char* fname;
    uint32_t count;
} war_payload_swap_add_notes;

typedef struct war_payload_swap_delete_notes {
    char* fname;
    uint32_t count;
} war_payload_swap_delete_notes;

typedef union war_payload_union {
    war_payload_add_note add_note;
    war_payload_delete_note delete_note;
    war_payload_add_notes add_notes;
    war_payload_delete_notes delete_notes;
    war_payload_add_notes_same add_notes_same;
    war_payload_delete_notes_same delete_notes_same;
    war_payload_swap_add_notes swap_add_notes;
    war_payload_swap_delete_notes swap_delete_notes;
} war_payload_union;

typedef struct war_undo_node {
    uint64_t id;
    uint64_t seq_num;
    uint32_t branch_id;
    uint32_t command;
    war_payload_union payload;
    double cursor_pos_x;
    double cursor_pos_y;
    uint32_t left_col;
    uint32_t right_col;
    uint32_t top_row;
    uint32_t bottom_row;
    char* timestamp;
    struct war_undo_node* parent;
    struct war_undo_node* next;
    struct war_undo_node* prev;
    struct war_undo_node* alt_next;
    struct war_undo_node* alt_prev;
} war_undo_node;

typedef struct war_undo_tree {
    war_undo_node* root;
    war_undo_node* current;
    uint64_t next_id;
    uint64_t next_seq_num;
    uint32_t next_branch_id;
} war_undo_tree;

typedef struct war_lua_context {
    // audio
    _Atomic int A_SAMPLE_RATE;
    _Atomic double A_SAMPLE_DURATION;
    _Atomic int A_CHANNEL_COUNT;
    _Atomic int A_NOTE_COUNT;
    _Atomic int A_LAYER_COUNT;
    _Atomic int A_LAYERS_IN_RAM;
    _Atomic double A_BPM;
    _Atomic int A_BASE_FREQUENCY;
    _Atomic int A_BASE_NOTE;
    _Atomic int A_EDO;
    _Atomic int A_NOTES_MAX;
    _Atomic float A_DEFAULT_ATTACK;
    _Atomic float A_DEFAULT_SUSTAIN;
    _Atomic float A_DEFAULT_RELEASE;
    _Atomic float A_DEFAULT_GAIN;
    _Atomic double A_DEFAULT_COLUMNS_PER_BEAT;
    _Atomic int A_USERDATA;
    _Atomic int A_CACHE_SIZE;
    _Atomic int A_PATH_LIMIT;
    _Atomic int A_WARMUP_FRAMES_FACTOR;
    // window render
    _Atomic int WR_VIEWS_SAVED;
    _Atomic float WR_COLOR_STEP;
    _Atomic int WR_WARPOON_TEXT_COLS;
    _Atomic int WR_STATES;
    _Atomic int WR_SEQUENCE_COUNT;
    _Atomic int WR_SEQUENCE_LENGTH_MAX;
    _Atomic int WR_MODE_COUNT;
    _Atomic int WR_KEYSYM_COUNT;
    _Atomic int WR_MOD_COUNT;
    _Atomic int WR_NOTE_QUADS_MAX;
    _Atomic int WR_STATUS_BAR_COLS_MAX;
    _Atomic int WR_TEXT_QUADS_MAX;
    _Atomic int WR_QUADS_MAX;
    _Atomic char* WR_LEADER;
    _Atomic int WR_WAYLAND_MSG_BUFFER_SIZE;
    _Atomic int WR_WAYLAND_MAX_OBJECTS;
    _Atomic int WR_WAYLAND_MAX_OP_CODES;
    _Atomic int WR_UNDO_NODES_MAX;
    _Atomic int WR_UNDO_NODES_CHILDREN_MAX;
    _Atomic int WR_TIMESTAMP_LENGTH_MAX;
    _Atomic int WR_REPEAT_DELAY_US;
    _Atomic int WR_REPEAT_RATE_US;
    _Atomic int WR_CURSOR_BLINK_DURATION_US;
    _Atomic double WR_FPS;
    _Atomic int WR_UNDO_NOTES_BATCH_MAX;
    _Atomic int WR_INPUT_SEQUENCE_LENGTH_MAX;
    // pool
    _Atomic int POOL_ALIGNMENT;
    // cmd
    _Atomic int CMD_COUNT;
    // pc
    _Atomic int PC_BUFFER_SIZE;
    // vk
    _Atomic int VK_ATLAS_WIDTH;
    _Atomic int VK_ATLAS_HEIGHT;
    _Atomic float VK_FONT_PIXEL_HEIGHT;
    // misc
    _Atomic float DEFAULT_ALPHA_SCALE;
    _Atomic float DEFAULT_CURSOR_ALPHA_SCALE;
    _Atomic float DEFAULT_PLAYBACK_BAR_THICKNESS;
    _Atomic float DEFAULT_TEXT_FEATHER;
    _Atomic float DEFAULT_TEXT_THICKNESS;
    _Atomic float WINDOWED_TEXT_FEATHER;
    _Atomic float WINDOWED_TEXT_THICKNESS;
    _Atomic float DEFAULT_WINDOWED_ALPHA_SCALE;
    _Atomic float DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE;
    _Atomic(char*) CWD;
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
    // cmds
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
    AUDIO_CMD_DELETE_NOTE = 20,
    AUDIO_CMD_DELETE_ALL_NOTES = 21,
    AUDIO_CMD_REPLACE_NOTE = 22,
    AUDIO_CMD_REPLACE_NOTE_DURATION = 23,
    AUDIO_CMD_REPLACE_NOTE_START = 24,
    AUDIO_CMD_REPEAT_SECTION = 25,
    AUDIO_CMD_INSERT_NOTE = 26,
    AUDIO_CMD_COMPACT = 27,
    AUDIO_CMD_REVIVE_NOTE = 28,
    AUDIO_CMD_ADD_NOTES = 29,
    AUDIO_CMD_DELETE_NOTES = 30,
    AUDIO_CMD_ADD_NOTES_SAME = 31,
    AUDIO_CMD_DELETE_NOTES_SAME = 32,
    AUDIO_CMD_REVIVE_NOTES = 33,
    AUDIO_CMD_WRITE = 34,
    //
    AUDIO_VOICE_GRAND_PIANO = 0,
    AUDIO_VOICE_COUNT = 128,
    AUDIO_SINE_TABLE_SIZE = 1024,
};

typedef struct war_atomics {
    _Atomic uint64_t play_clock;
    _Atomic uint64_t play_frames;
    _Atomic uint64_t capture_frames;
    _Atomic uint8_t state;
    _Atomic float capture_threshold;
    _Atomic uint8_t capture;
    _Atomic uint8_t play;
    _Atomic float bpm;
    _Atomic int16_t map_note;
    _Atomic uint64_t layer;
    _Atomic uint64_t map_layer;
    _Atomic uint8_t map;
    _Atomic uint8_t capture_monitor;
    _Atomic float play_gain;
    _Atomic float capture_gain;
    _Atomic uint8_t* notes_on;
    _Atomic uint8_t* notes_on_previous;
    _Atomic uint8_t loop;
    _Atomic uint8_t repeat_section;
    _Atomic uint64_t repeat_start_frames;
    _Atomic uint64_t repeat_end_frames;
    _Atomic uint8_t start_war;
    _Atomic uint8_t resample;
    _Atomic uint64_t note_next_id;
    _Atomic uint64_t cache_next_id;
    _Atomic uint64_t cache_next_timestamp;
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

typedef struct war_cache_audio {
    uint64_t* id;
    uint64_t* timestamp;
    void* wav;
    ssize_t* size;
    char* fname;
    int16_t* note;
    uint64_t* layer;
    int* fd;
    uint32_t count;
} war_cache_audio;

typedef struct war_midi_context {
    uint64_t* start_frames;
} war_midi_context;

typedef struct war_cache_window_render {
    uint64_t* id;
    int* fd;
    void** map;
    size_t* size;
    war_riff_header* riff;
    war_fmt_chunk* fmt;
    war_data_chunk* data_chunk;
    int16_t** sample;
    char** fname;
    int16_t* note;
    uint64_t* layer;
    size_t count;
} war_cache_window_render;

typedef struct war_sequencer {
    uint64_t* id;
    char* fname;
} war_sequencer;

typedef struct war_audio_context {
    double BPM;
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
    struct pw_stream* capture_stream;
    int16_t* capture_buffer;
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

typedef struct war_color_context {
    uint32_t white_hex;
    uint32_t full_white_hex;
    uint32_t* colors;
} war_color_context;

typedef struct war_window_render_context {
    uint64_t now;
    char* layers_active;
    int layers_active_count;
    double cursor_pos_x;
    double cursor_pos_y;
    double cursor_size_x;
    double cursor_size_y;
    double cursor_navigation_x;
    double cursor_navigation_y;
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
    char* input_sequence;
    uint8_t num_chars_in_sequence;
    war_note_quads note_quads;
    float layers[LAYER_COUNT];
    float layer_count;
    float playback_bar_pos_x;
    float playback_bar_pos_x_increment;
    double FPS;
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
    bool midi_toggle;
    bool skip_release;
    uint8_t prompt;
    uint32_t num_chars_in_prompt;
    uint32_t cursor_pos_x_command_mode;
    uint8_t layer_flux;
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
