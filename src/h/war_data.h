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
    MOD_SHIFT = (1 << 0),
    MOD_CTRL = (1 << 1),
    MOD_ALT = (1 << 2),
    MOD_LOGO = (1 << 3),
    MOD_CAPS = (1 << 4),
    MOD_NUM = (1 << 5),
    MOD_FN = (1 << 6),
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
    max_quads = 800,
    max_fds = 50,
    ring_buffer_size = 256,
    OLED_MODE = 0,
    MAX_MIDI_NOTES = 128,
};

typedef struct war_key_event {
    uint32_t keysym;
    uint8_t mod;
    uint8_t state;
    uint64_t timestamp_us;
} war_key_event;

typedef struct war_key_trie_node {
    uint32_t keysym;
    uint8_t mod;
    uint8_t is_terminal;
    void* command;
    struct war_key_trie_node* children[32];
    size_t child_count;
} war_key_trie_node;

enum war_key_trie {
    MAX_NODES = 1024,
    MAX_SEQUENCE_LENGTH = 4,
    MAX_CHILDREN = 32,
    NUM_SEQUENCES = 26,
};

typedef struct war_input_cmd_context {
    uint32_t col;
    uint32_t row;
    uint32_t col_increment;
    uint32_t row_increment;
    uint32_t col_leap_increment;
    uint32_t row_leap_increment;
    uint32_t numeric_prefix;
    uint32_t max_cols;
    uint32_t max_rows;
    float cursor_x;
    float cursor_y;
    float zoom_scale;
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

typedef struct war_vulkan_context {
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
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkImageView image_view;
    VkFence in_flight_fence;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkBuffer quads_vertex_buffer;
    VkDeviceMemory quads_vertex_buffer_memory;
    VkBuffer quads_index_buffer;
    VkDeviceMemory quads_index_buffer_memory;
    VkImage texture_image;
    VkDeviceMemory texture_memory;
    VkImageView texture_image_view;
    VkSampler texture_sampler;
    VkDescriptorSet texture_descriptor_set;
    VkDescriptorPool texture_descriptor_pool;

    //-------------------------------------------------------------------------
    // font
    //-------------------------------------------------------------------------
    FT_Library ft_library;
    FT_Face ft_regular;
    VkImage sdf_image;
    VkImageView sdf_image_view;
    VkDeviceMemory sdf_image_mem;
    VkSampler sdf_sampler;
    struct {
        float advance_x;
        float advance_y;
        float bearing_x;
        float bearing_y;
        float width;
        float height;
        float uv_x0, uv_y0, uv_x1, uv_y1; // uvs inside SDF atlas
    } glyphs[128];
    VkDescriptorSet font_descriptor_set;
    VkDescriptorSetLayout font_descriptor_set_layout;
    VkDescriptorPool font_descriptor_pool;
    VkPipeline sdf_pipeline;
    VkPipelineLayout sdf_pipeline_layout;
    VkShaderModule sdf_vert_shader;
    VkShaderModule sdf_frag_shader;
    VkPushConstantRange sdf_push_constant_range;
    uint32_t pixel_height;
} war_vulkan_context;

typedef struct war_drm_context {
    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
} war_drm_context;

#endif // WAR_DATA_H
