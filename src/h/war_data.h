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

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

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
    NUM_SEQUENCES = 8,
};

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
} war_vulkan_context;

typedef struct war_drm_context {
    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
} war_drm_context;

#endif // WAR_DATA_H
