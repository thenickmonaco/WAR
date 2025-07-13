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
// src/war_main.c
//-----------------------------------------------------------------------------

#include "war_drm.c"
#include "war_vulkan.c"
#include "war_wayland.c"

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_main.h"

#include <stdint.h>
#include <unistd.h>

int main() {
    CALL_CARMACK("main");

    // wayland_init();
    WAR_VulkanContext vulkan_context = war_vulkan_init(1920, 1080);

    // render logic
    VkClearColorValue clear_color = {.float32 = {1.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange clear_range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(vulkan_context.cmd_buffer, &begin_info);
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkan_context.image,
        .subresourceRange = clear_range,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    vkCmdPipelineBarrier(vulkan_context.cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier);
    vkCmdClearColorImage(vulkan_context.cmd_buffer,
                         vulkan_context.image,
                         VK_IMAGE_LAYOUT_GENERAL,
                         &clear_color,
                         1,
                         &clear_range);
    vkEndCommandBuffer(vulkan_context.cmd_buffer);
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &vulkan_context.cmd_buffer,
    };
    vkQueueSubmit(vulkan_context.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkan_context.queue);

    // export vulkan image memory as DMABUF fd
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(vulkan_context.device,
                                                  "vkGetMemoryFdKHR");
    assert(vkGetMemoryFdKHR && "vkGetMemoryFdKHR not found");
    VkMemoryGetFdInfoKHR get_fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .pNext = NULL,
        .memory = vulkan_context.memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    int dmabuf_fd = -1;
    VkResult result =
        vkGetMemoryFdKHR(vulkan_context.device, &get_fd_info, &dmabuf_fd);
    assert(result == VK_SUCCESS);
    assert(dmabuf_fd >= 0);
    vulkan_context.dmabuf_fd = dmabuf_fd;

    WAR_DRMContext drm_context = war_drm_init();

    // WAR_DRMContext* drm_context
    // int dmabuf_fd
    // uint32_t width
    // uint32_t height
    // uint32_t format
    // uint32_t stride

    war_drm_present_dmabuf(&drm_context,
                           vulkan_context.dmabuf_fd,
                           1920,
                           1080,
                           DRM_FORMAT_ARGB8888,
                           1920 * 4);

    close(vulkan_context.dmabuf_fd);
    vulkan_context.dmabuf_fd = -1;

    END("main");
    return 0;
}
