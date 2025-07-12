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
// src/main.c
//=============================================================================

#include "main.h"
#include "data.h"
#include "debug_macros.h"
#include "macros.h"
#include "vulkan.c"
#include "wayland.c"

#include <stdint.h>

int main() {
    CALL_CARMACK("WAR");
    VulkanContext vulkan_context = vulkan_make_dmabuf_fd(1920, 1080);

    //record_and_submit_command_buffer(&vulkan_context, 1920, 1080);

    END("WAR");
    return 0;
}

void record_and_submit_command_buffer(VulkanContext* ctx,
                                      uint32_t width,
                                      uint32_t height) {
    header("record and submit command buffer");
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };

    vkBeginCommandBuffer(ctx->cmd_buffer, &begin_info);

    // Transition image layout: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx->image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    vkCmdPipelineBarrier(ctx->cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier1);

    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // black clear

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->render_pass,
        .framebuffer = ctx->frame_buffer,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = {width, height},
            },
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };

    vkCmdBeginRenderPass(
        ctx->cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(
        ctx->cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->pipeline);

    // Set viewport and scissor dynamically if your pipeline allows (if not
    // static, set here)
    vkCmdSetViewport(ctx->cmd_buffer, 0, 1, &ctx->viewport);
    vkCmdSetScissor(ctx->cmd_buffer, 0, 1, &ctx->scissor);

    // Draw a simple triangle (3 vertices) - replace this with smiley vertices
    // if you want
    vkCmdDraw(ctx->cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(ctx->cmd_buffer);

    // Transition image layout: COLOR_ATTACHMENT_OPTIMAL -> GENERAL (or whatever
    // is needed)
    VkImageMemoryBarrier barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx->image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    };
    vkCmdPipelineBarrier(ctx->cmd_buffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier2);

    vkEndCommandBuffer(ctx->cmd_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx->cmd_buffer,
    };

    vkQueueSubmit(ctx->queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->queue);
    end("record_and_submit_command_buffer");
}
