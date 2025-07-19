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

enum war_wayland_interface_hash_value {
    WL_SHM_HASH_VALUE = 1,
    WL_COMPOSITOR_HASH_VALUE = 2,
    WL_OUTPUT_HASH_VALUE = 3,
    WL_SEAT_HASH_VALUE = 4,
    ZWP_LINUX_DMABUF_V1_HASH_VALUE = 5,
    XDG_WM_BASE_HASH_VALUE = 6,
    WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_HASH_VALUE = 7,
    ZWP_IDLE_INHIBIT_MANAGER_V1_HASH_VALUE = 8,
    ZXDG_DECORATION_MANAGER_V1_HASH_VALUE = 9,
    ZWP_RELATIVE_POINTER_MANAGER_V1_HASH_VALUE = 10,
    ZWP_POINTER_CONSTRAINTS_V1_HASH_VALUE = 11,
    ZWLR_OUTPUT_MANAGER_V1_HASH_VALUE = 12,
    ZWLR_DATA_CONTROL_MANAGER_V1_HASH_VALUE = 13,
    ZWP_VIRTUAL_KEYBOARD_MANAGER_V1_HASH_VALUE = 14,
    WP_VIEWPORTER_HASH_VALUE = 15,
    WP_FRACTIONAL_SCALE_MANAGER_V1_HASH_VALUE = 16,
    ZWP_POINTER_GESTURES_V1_HASH_VALUE = 17,
    XDG_ACTIVATION_V1_HASH_VALUE = 18,
    WP_PRESENTATION_HASH_VALUE = 19,
    ZWLR_LAYER_SHELL_V1_HASH_VALUE = 20,
    EXT_FOREIGN_TOPLEVEL_LIST_V1_HASH_VALUE = 21,
    WP_CONTENT_TYPE_MANAGER_V1_HASH_VALUE = 22,
};

enum war_misc {
    max_objects = 1000,
    max_opcodes = 20,
    max_quads = 800,
    max_fds = 50,
    ring_buffer_size = 256,
};

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
