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

//=============================================================================
// include/data.h
//=============================================================================

#ifndef WAR_DATA_H
#define WAR_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

typedef struct {
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
    VkViewport viewport;
    VkRect2D scissor;
    VkImageView image_view;

} VulkanContext;

#endif // WAR_DATA_H
