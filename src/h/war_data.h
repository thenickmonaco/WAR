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
    KEYSYM_DEFAULT = 511,
    MAX_KEYSYM = 512,
    MAX_MOD = 16,
};

enum war_audio {
    SAMPLE_RATE = 48000,
    PERIOD_SIZE = 512,
    NUM_CHANNELS = 2,
    BPM = 100,
};

enum war_audio_functions {
    AUDIO_PLAY = 1,
    AUDIO_PAUSE = 2,
    AUDIO_GET_TIMESTAMP = 3,
    AUDIO_ADD_NOTE = 4,
    AUDIO_END_WAR = 5,
};

enum war_misc {
    max_objects = 1000,
    max_opcodes = 20,
    max_quads = 8000,
    max_text_quads = 8000,
    max_note_quads = 8000,
    max_frames = 1,
    max_instances_per_quad = 1,
    max_instances_per_sdf_quad = 1,
    max_fds = 50,
    ring_buffer_size = 256,
    OLED_MODE = 0,
    MAX_MIDI_NOTES = 128,
    UNSET = 0,
    MAX_DIGITS = 10,
    NUM_STATUS_BARS = 3,
    MAX_GRIDLINE_SPLITS = 4,
    MAX_VIEWS_SAVED = 8,
};

enum war_layers {
    LAYER_COUNT = 8,
    LAYER_BACKGROUND = 0,
    LAYER_GRIDLINES = 1,
    LAYER_PLAYBACK_BAR = 2,
    LAYER_NOTES = 3,
    LAYER_NOTE_TEXT = 4,
    LAYER_HUD = 5,
    LAYER_HUD_TEXT = 6,
    LAYER_CURSOR = 7,
};

enum war_hud {
    SHOW_PIANO = 0,
    SHOW_LINE_NUMBERS = 1,
    SHOW_PIANO_AND_LINE_NUMBERS = 2,
};

enum war_modes {
    MODE_COUNT = 9,
    MODE_NORMAL = 0,
    MODE_VISUAL = 1,
    MODE_VISUAL_LINE = 2,
    MODE_VISUAL_BLOCK = 3,
    MODE_INSERT = 4,
    MODE_COMMAND = 5,
    MODE_M = 6,
    MODE_O = 7,
    MODE_VIEWS_SAVED = 8,
};

enum war_fsm {
    MAX_NODES = 1024,
    MAX_SEQUENCE_LENGTH = 7,
    MAX_CHILDREN = 32,
    SEQUENCE_COUNT = 90,
    MAX_STATES = 256,
    MAX_COMMAND_BUFFER_LENGTH = 128,
};

enum war_pipelines {
    PIPELINE_NONE = 0,
    PIPELINE_QUAD = 1,
    PIPELINE_SDF = 2,
};

typedef struct war_fsm_state {
    bool is_terminal;
    void* command[MODE_COUNT];
    uint16_t next_state[512][16];
} war_fsm_state;

typedef struct war_key_event {
    uint32_t keysym;
    uint8_t mod;
} war_key_event;

typedef struct war_key_trie_node {
    uint32_t keysym;
    uint8_t mod;
    bool pressed;
    bool is_terminal;
    void* command[MODE_COUNT];
    struct war_key_trie_node* children[32];
    size_t child_count;
    uint64_t last_event_us;
} war_key_trie_node;

typedef struct war_rgba_t {
    float r;
    float g;
    float b;
    float a;
} war_rgba_t;

enum war_voice {
    VOICE_GRAND_PIANO = 0,
};

typedef struct war_note {
    uint32_t id;
    uint32_t col;
    uint32_t row;
    uint32_t sub_col;
    uint32_t sub_row;
    uint32_t sub_cells_col;
    uint32_t cursor_width_whole_number;
    uint32_t cursor_width_sub_col;
    uint32_t cursor_width_sub_cells;
    float float_offset;
    uint32_t color;
    float strength;
    uint32_t voice;
    uint32_t hidden;
    uint32_t mute;
} war_note;

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
    float* strength;
    uint32_t* voice;
    uint32_t* hidden;
    uint32_t* mute;
} war_note_quads;

typedef struct war_views {
    uint32_t col[MAX_VIEWS_SAVED];
    uint32_t row[MAX_VIEWS_SAVED];
    uint32_t left_col[MAX_VIEWS_SAVED];
    uint32_t right_col[MAX_VIEWS_SAVED];
    uint32_t bottom_row[MAX_VIEWS_SAVED];
    uint32_t top_row[MAX_VIEWS_SAVED];
} war_views;

typedef struct war_warpoon {
    uint32_t col;
} war_warpoon;

typedef struct war_input_cmd_context {
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
    war_views views_saved;
    uint32_t views_saved_count;
    war_note_quads note_quads;
    uint32_t note_quads_count;
    float layers[LAYER_COUNT];
    float layer_count;
} war_input_cmd_context;

typedef struct war_key_trie_pool {
    war_key_trie_node nodes[MAX_NODES];
    size_t node_count;
} war_key_trie_pool;

typedef struct war_thread_args {
    uint8_t* window_render_to_audio_ring_buffer;
    uint8_t* audio_to_window_render_ring_buffer;
    uint8_t* write_to_audio_index;
    uint8_t* read_from_audio_index;
    uint8_t* write_to_window_render_index;
    uint8_t* read_from_window_render_index;
} war_thread_args;

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
    float cell_offsets[2];
    float scroll_margin[2];
    float anchor_cell[2];
    float top_right[2];
    float ascent;
    float descent;
    float line_gap;
    float baseline;
    float font_height;
    uint32_t _pad[2];
} war_text_push_constants;

typedef enum war_quad_flags {
    QUAD_LINE = 1 << 0,
    QUAD_OUTLINE = 1 << 1,
    QUAD_GRID = 1 << 2,
} war_quad_flags;

typedef struct war_quad_vertex {
    float corner[2];
    float pos[3];
    uint32_t color;
    float outline_thickness;
    uint32_t outline_color;
    float line_thickness[2];
    uint32_t flags;
    uint32_t _pad[1];
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
    // SDF PIPELINE
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
