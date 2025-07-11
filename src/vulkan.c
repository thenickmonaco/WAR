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
// src/vulkan.c
//=============================================================================

#include "vulkan.h"
#include "data.h"
#include "debug_macros.h"
#include "macros.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdrm/drm_fourcc.h> // DRM_FORMAT_ARGB8888
#include <libdrm/drm_mode.h>   // DRM_FORMAT_MOD_LINEAR
#include <linux/socket.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

VulkanContext vulkan_make_dmabuf_fd(uint32_t width, uint32_t height) {
    header("vulkan_make_dmabuf_fd");

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = NULL,
    };

    VkInstance instance;
    vkCreateInstance(&instance_info, NULL, &instance);

    enum {
        max_gpu_count = 10,
    };
    uint32_t gpu_count = 0;
    vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
    VkPhysicalDevice physical_devices[max_gpu_count];
    vkEnumeratePhysicalDevices(instance, &gpu_count, physical_devices);
    assert(gpu_count != 0);
    VkPhysicalDevice physical_device = physical_devices[0];

    const char* device_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    };

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &extension_count, NULL);

    VkExtensionProperties* available_extensions =
        malloc(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &extension_count, available_extensions);

    for (uint32_t i = 0; i < extension_count; i++) {
        call_carmack("  %s", available_extensions[i].extensionName);
    }

    uint8_t has_external_memory = 0;
    uint8_t has_external_memory_fd = 0;

    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(available_extensions[i].extensionName,
                   VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0) {
            has_external_memory = 1;
        }
        if (strcmp(available_extensions[i].extensionName,
                   VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) == 0) {
            has_external_memory_fd = 1;
        }
    }

    assert(has_external_memory && has_external_memory_fd);

    uint32_t queue_family_index = 0;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = device_extensions,
    };
    VkDevice device;
    vkCreateDevice(physical_device, &device_info, NULL, &device);

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    VkCommandPool cmd_pool;
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    VkResult res = vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool);
    assert(res == VK_SUCCESS);

    VkCommandBuffer cmd_buffer;
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    res = vkAllocateCommandBuffers(device, &cmd_buf_info, &cmd_buffer);
    assert(res == VK_SUCCESS);

    VkExternalMemoryImageCreateInfo ext_mem_image_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &ext_mem_image_info,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    res = vkCreateImage(device, &image_create_info, NULL, &image);
    assert(res == VK_SUCCESS);

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image, &mem_reqs);
    VkExportMemoryAllocateInfo export_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_alloc_info,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex =
            vulkan_find_memory_type(mem_reqs.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    physical_device),
    };

    VkDeviceMemory memory;
    res = vkAllocateMemory(device, &mem_alloc_info, NULL, &memory);
    assert(res == VK_SUCCESS);
    vkBindImageMemory(device, image, memory, 0);

    VkMemoryGetFdInfoKHR get_fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    int dmabuf_fd;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    res = vkGetMemoryFdKHR(device, &get_fd_info, &dmabuf_fd);
    assert(res == VK_SUCCESS);
    assert(dmabuf_fd > 0);

    VkAttachmentDescription color_attachment = {
        .flags = 0,
        .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32, // Same format as your image
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout =
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // or
                                             // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                             // depending on usage
    };
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    VkRenderPass render_pass;
    res = vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass);
    assert(res == VK_SUCCESS);

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkImageView image_view;
    res = vkCreateImageView(device, &image_view_info, NULL, &image_view);
    assert(res == VK_SUCCESS);
    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = width,
        .height = height,
        .layers = 1,
    };
    VkFramebuffer framebuffer;
    res = vkCreateFramebuffer(device, &framebuffer_info, NULL, &framebuffer);
    assert(res == VK_SUCCESS);

    // COMMENT TODO
    //VkShaderModuleCreateInfo shader_info = {
    //    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    //    .codeSize = ..., // size of SPIR-V file
    //    .pCode = ...,    // pointer to SPIR-V data
    //};
    //VkShaderModule vert_shader, frag_shader;
    //vkCreateShaderModule(device, &shader_info, NULL, &vert_shader);
    //// repeat for frag_shader

    //VkPipelineLayoutCreateInfo layout_info = {
    //    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //};
    //VkPipelineLayout pipeline_layout;
    //vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout);

    //VkVertexInputBindingDescription binding = {
    //    .binding = 0,
    //    .stride = sizeof(float) * 4, // pos + uv
    //    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    //};
    //VkVertexInputAttributeDescription attrs[] = {
    //    {.location = 0,
    //     .binding = 0,
    //     .format = VK_FORMAT_R32G32_SFLOAT,
    //     .offset = 0},
    //    {.location = 1,
    //     .binding = 0,
    //     .format = VK_FORMAT_R32G32_SFLOAT,
    //     .offset = sizeof(float) * 2},
    //};
    //VkPipelineVertexInputStateCreateInfo vertex_input = {
    //    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    //    .vertexBindingDescriptionCount = 1,
    //    .pVertexBindingDescriptions = &binding,
    //    .vertexAttributeDescriptionCount = 2,
    //    .pVertexAttributeDescriptions = attrs,
    //};

    //VkGraphicsPipelineCreateInfo pipeline_info = {
    //    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    //    .stageCount = 2,
    //    .pStages = (VkPipelineShaderStageCreateInfo[]){...},
    //    .pVertexInputState = &vertex_input,
    //    .pInputAssemblyState =
    //        &(VkPipelineInputAssemblyStateCreateInfo){
    //            .sType =
    //                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    //            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    //        },
    //    .pViewportState =
    //        &(VkPipelineViewportStateCreateInfo){
    //            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    //            .viewportCount = 1,
    //            .scissorCount = 1,
    //        },
    //    .pRasterizationState =
    //        &(VkPipelineRasterizationStateCreateInfo){
    //            .sType =
    //                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    //            .polygonMode = VK_POLYGON_MODE_FILL,
    //            .cullMode = VK_CULL_MODE_NONE,
    //            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    //            .lineWidth = 1.0f,
    //        },
    //    .pMultisampleState =
    //        &(VkPipelineMultisampleStateCreateInfo){
    //            .sType =
    //                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    //            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    //        },
    //    .pColorBlendState =
    //        &(VkPipelineColorBlendStateCreateInfo){
    //            .sType =
    //                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    //            .attachmentCount = 1,
    //            .pAttachments =
    //                (VkPipelineColorBlendAttachmentState[]){
    //                    {
    //                        .blendEnable = VK_FALSE,
    //                        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
    //                                          VK_COLOR_COMPONENT_G_BIT |
    //                                          VK_COLOR_COMPONENT_B_BIT |
    //                                          VK_COLOR_COMPONENT_A_BIT,
    //                    },
    //                },
    //        },
    //    .layout = pipeline_layout,
    //    .renderPass = render_pass,
    //    .subpass = 0,
    //};
    //VkPipeline pipeline;
    //vkCreateGraphicsPipelines(
    //    device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);

    free(available_extensions);

    end("vulkan_make_dmabuf_fd");
    return (VulkanContext){.dmabuf_fd = dmabuf_fd,
                           .instance = instance,
                           .physical_device = physical_device,
                           .device = device,
                           .queue = queue,
                           .queue_family_index = queue_family_index,
                           .image = image,
                           .memory = memory,
                           .cmd_pool = cmd_pool,
                           .cmd_buffer = cmd_buffer,
                           .pipeline = NULL, // COMMENT: unnullify
                           .pipeline_layout = NULL};
}

uint32_t vulkan_find_memory_type(uint32_t type_filter,
                                 VkMemoryPropertyFlags properties,
                                 VkPhysicalDevice physical_device) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    call_carmack("Looking for memory type with properties: 0x%x", properties);
    call_carmack("Available memory types:");

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags =
            mem_properties.memoryTypes[i].propertyFlags;
        call_carmack("  Type %u: flags=0x%x", i, flags);

        if ((type_filter & (1 << i)) && (flags & properties) == properties) {
            call_carmack("-> Selected memory type %u", i);
            return i;
        }
    }

    call_carmack("Failed to find suitable memory type!");
    exit(EXIT_FAILURE);
}
