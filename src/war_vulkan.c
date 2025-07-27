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
// src/war_vulkan.c
//-----------------------------------------------------------------------------

#include "h/war_vulkan.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdrm/drm_fourcc.h>
#include <libdrm/drm_mode.h>
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

war_vulkan_context war_vulkan_init(uint32_t width, uint32_t height) {
    header("war_vulkan_init");

    // ???
    uint32_t instance_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, NULL);
    VkExtensionProperties* instance_extensions_properties =
        malloc(sizeof(VkExtensionProperties) * instance_extension_count);
    vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, instance_extensions_properties);

    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const char* instance_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo =
            &(VkApplicationInfo){
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "WAR",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "war-engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_2,
            },
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = validation_layers,
        .enabledExtensionCount =
            sizeof(instance_extensions) / sizeof(instance_extensions[0]),
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkInstance instance;
    VkResult result = vkCreateInstance(&instance_info, NULL, &instance);
    assert(result == VK_SUCCESS);

    enum {
        max_gpu_count = 10,
    };
    uint32_t gpu_count = 0;
    VkPhysicalDevice physical_devices[max_gpu_count];
    vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
    assert(gpu_count != 0 && gpu_count <= max_gpu_count);

    vkEnumeratePhysicalDevices(instance, &gpu_count, physical_devices);

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties device_props;

    for (uint32_t i = 0; i < gpu_count; i++) {
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);

        call_carmack("Found GPU %u: %s (vendorID=0x%x, deviceID=0x%x)",
                     i,
                     device_props.deviceName,
                     device_props.vendorID,
                     device_props.deviceID);

        if (device_props.vendorID == 0x8086) {
            physical_device = physical_devices[i];
            call_carmack("Selected Intel GPU: %s", device_props.deviceName);
            break;
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        physical_device = physical_devices[0];
        vkGetPhysicalDeviceProperties(physical_device, &device_props);
        call_carmack("Fallback GPU selected: %s (vendorID=0x%x)",
                     device_props.deviceName,
                     device_props.vendorID);
    }

    assert(physical_device != VK_NULL_HANDLE);

    uint32_t device_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &device_extension_count, NULL);
    VkExtensionProperties* device_extensions_properties =
        malloc(sizeof(VkExtensionProperties) * device_extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device,
                                         NULL,
                                         &device_extension_count,
                                         device_extensions_properties);

    const char* device_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    };

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &extension_count, NULL);

    VkExtensionProperties* available_extensions =
        malloc(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(
        physical_device, NULL, &extension_count, available_extensions);

#if DEBUG
    for (uint32_t i = 0; i < extension_count; i++) {
        call_carmack("%s", available_extensions[i].extensionName);
    }
#endif

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

    free(available_extensions);

    assert(has_external_memory && has_external_memory_fd);

    enum {
        max_family_count = 16,
    };
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, NULL);
    if (queue_family_count > max_family_count) {
        queue_family_count = max_family_count;
    }
    VkQueueFamilyProperties queue_families[max_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families);

    uint32_t graphics_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_index = i;
            break;
        }
    }
    assert(graphics_family_index != UINT32_MAX);
    uint32_t queue_family_index = graphics_family_index;

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
        .enabledExtensionCount = 4,
        .ppEnabledExtensionNames = device_extensions,
    };
    VkDevice device;
    result = vkCreateDevice(physical_device, &device_info, NULL, &device);
    assert(result == VK_SUCCESS);

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    // ???
    VkCommandPool cmd_pool;
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool);
    assert(result == VK_SUCCESS);

    // ???
    VkCommandBuffer cmd_buffer;
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = vkAllocateCommandBuffers(device, &cmd_buf_info, &cmd_buffer);
    assert(result == VK_SUCCESS);

    // ???
    VkExternalMemoryImageCreateInfo ext_mem_image_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &ext_mem_image_info,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    result = vkCreateImage(device, &image_create_info, NULL, &image);
    assert(result == VK_SUCCESS);

    // ???
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image, &mem_reqs);
    VkExportMemoryAllocateInfo export_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    uint32_t memory_type = 0;
    uint8_t found_memory_type = 0;
    call_carmack("Looking for memory type with properties: 0x%x", properties);
    call_carmack("Available memory types:");
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags =
            mem_properties.memoryTypes[i].propertyFlags;
        call_carmack("Type %u: flags=0x%x", i, flags);

        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (flags & properties) == properties) {
            call_carmack("-> Selected memory type %u", i);
            memory_type = i;
            found_memory_type = 1;
            break;
        }
    }
    assert(found_memory_type);
    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_alloc_info,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = memory_type,
    };
    VkDeviceMemory memory;
    result = vkAllocateMemory(device, &mem_alloc_info, NULL, &memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(device, image, memory, 0);
    assert(result == VK_SUCCESS);
    VkMemoryGetFdInfoKHR get_fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    int dmabuf_fd;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    result = vkGetMemoryFdKHR(device, &get_fd_info, &dmabuf_fd);
    assert(result == VK_SUCCESS);
    assert(dmabuf_fd > 0);
    int flags = fcntl(dmabuf_fd, F_GETFD);
    assert(flags != -1);

    // ???
    VkAttachmentDescription color_attachment = {
        .flags = 0,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
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
    result = vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass);
    assert(result == VK_SUCCESS);

    // ???
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
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
    result = vkCreateImageView(device, &image_view_info, NULL, &image_view);
    assert(result == VK_SUCCESS);
    VkFramebufferCreateInfo frame_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = width,
        .height = height,
        .layers = 1,
    };
    VkFramebuffer frame_buffer;
    result =
        vkCreateFramebuffer(device, &frame_buffer_info, NULL, &frame_buffer);
    assert(result == VK_SUCCESS);

    // vertex shader
    uint32_t* vertex_code;
    const char* vertex_path = "build/shaders/war_vertex.spv";
    FILE* vertex_spv = fopen(vertex_path, "rb");
    assert(vertex_spv);
    fseek(vertex_spv, 0, SEEK_END);
    long vertex_size = ftell(vertex_spv);
    fseek(vertex_spv, 0, SEEK_SET);
    assert(vertex_size > 0 &&
           (vertex_size % 4 == 0)); // SPIR-V is 4-byte aligned
    vertex_code = malloc(vertex_size);
    assert(vertex_code);
    size_t vertex_spv_read = fread(vertex_code, 1, vertex_size, vertex_spv);
    assert(vertex_spv_read == (size_t)vertex_size);
    fclose(vertex_spv);
    VkShaderModuleCreateInfo vertex_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_size,
        .pCode = vertex_code};
    VkShaderModule vertex_shader;
    result =
        vkCreateShaderModule(device, &vertex_shader_info, NULL, &vertex_shader);
    assert(result == VK_SUCCESS);
    free(vertex_code);

    // fragment shader
    uint32_t* fragment_code;
    const char* fragment_path = "build/shaders/war_fragment.spv";
    FILE* fragment_spv = fopen(fragment_path, "rb");
    assert(fragment_spv);
    fseek(fragment_spv, 0, SEEK_END);
    long fragment_size = ftell(fragment_spv);
    fseek(fragment_spv, 0, SEEK_SET);
    assert(fragment_size > 0 &&
           (fragment_size % 4 == 0)); // SPIR-V is 4-byte aligned
    fragment_code = malloc(fragment_size);
    assert(fragment_code);
    size_t fragment_spv_read =
        fread(fragment_code, 1, fragment_size, fragment_spv);
    assert(fragment_spv_read == (size_t)fragment_size);
    fclose(fragment_spv);
    VkShaderModuleCreateInfo fragment_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragment_size,
        .pCode = fragment_code};
    VkShaderModule fragment_shader;
    result = vkCreateShaderModule(
        device, &fragment_shader_info, NULL, &fragment_shader);
    assert(result == VK_SUCCESS);
    free(fragment_code);

    // ???
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };
    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };
    VkDescriptorSetLayout descriptor_set_layout;
    vkCreateDescriptorSetLayout(
        device, &descriptor_layout_info, NULL, &descriptor_set_layout);
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName = "main",
        }};
    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 16, // 4 bytes for zoom + 8 bytes for pan + padding
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VkPipelineLayout pipeline_layout;
    result =
        vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout);
    assert(result == VK_SUCCESS);

    // ???
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = 12,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attrs[] = {
        {.location = 0,
         .binding = 0,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = 0},
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .offset = sizeof(float) * 2,
        },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attrs,
    };

    // ???
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState =
            &(VkPipelineInputAssemblyStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            },
        .pViewportState =
            &(VkPipelineViewportStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports = NULL,
                .scissorCount = 1,
                .pScissors = NULL,
            },
        .pDynamicState =
            &(VkPipelineDynamicStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates =
                    (VkDynamicState[]){
                        VK_DYNAMIC_STATE_VIEWPORT,
                        VK_DYNAMIC_STATE_SCISSOR,
                    }},
        .pRasterizationState =
            &(VkPipelineRasterizationStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
            },
        .pMultisampleState =
            &(VkPipelineMultisampleStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            },
        .pColorBlendState =
            &(VkPipelineColorBlendStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments =
                    (VkPipelineColorBlendAttachmentState[]){
                        {
                            .blendEnable = VK_FALSE,
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                        },
                    },
            },
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };
    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
    assert(result == VK_SUCCESS);

    // initial image transition for render target
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
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
    vkBeginCommandBuffer(
        cmd_buffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        });
    vkCmdPipelineBarrier(cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier);
    vkEndCommandBuffer(cmd_buffer);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // double/triple buffering
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    vkCreateSemaphore(
        device, &semaphore_info, NULL, &image_available_semaphore);
    vkCreateSemaphore(
        device, &semaphore_info, NULL, &render_finished_semaphore);

    // fps
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence in_flight_fence;
    vkCreateFence(device, &fence_info, NULL, &in_flight_fence);

    // quads vertex buffer
    VkBufferCreateInfo quads_vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = max_quads * 4 * (sizeof(float) * 4 + sizeof(uint32_t)),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // quads index buffer
    VkBuffer quads_vertex_buffer;
    result = vkCreateBuffer(
        device, &quads_vertex_buffer_info, NULL, &quads_vertex_buffer);
    assert(result == VK_SUCCESS);
    VkBufferCreateInfo quads_index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = max_quads * 6 * sizeof(uint16_t),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer quads_index_buffer;
    result = vkCreateBuffer(
        device, &quads_index_buffer_info, NULL, &quads_index_buffer);
    assert(result == VK_SUCCESS);

    // COMMENT OPTIMIZE: make these 2 vertex+index allocations into one single
    // allocate quads vertex memory
    VkMemoryRequirements quads_vertex_mem_reqs;
    vkGetBufferMemoryRequirements(
        device, quads_vertex_buffer, &quads_vertex_mem_reqs);
    uint32_t quads_vertex_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_vertex_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &quads_vertex_mem_properties);
    for (uint32_t i = 0; i < quads_vertex_mem_properties.memoryTypeCount; i++) {
        if ((quads_vertex_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_vertex_mem_properties.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            quads_vertex_memory_type_index = i;
            break;
        }
    }
    assert(quads_vertex_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_vertex_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_vertex_mem_reqs.size,
        .memoryTypeIndex = quads_vertex_memory_type_index,
    };
    VkDeviceMemory quads_vertex_buffer_memory;
    result = vkAllocateMemory(
        device, &quads_vertex_alloc_info, NULL, &quads_vertex_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, quads_vertex_buffer, quads_vertex_buffer_memory, 0);
    assert(result == VK_SUCCESS);

    // allocate quads index memory
    VkMemoryRequirements quads_index_mem_reqs;
    vkGetBufferMemoryRequirements(
        device, quads_index_buffer, &quads_index_mem_reqs);
    uint32_t quads_index_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_index_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &quads_index_mem_properties);
    for (uint32_t i = 0; i < quads_index_mem_properties.memoryTypeCount; i++) {
        if ((quads_index_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_index_mem_properties.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            quads_index_memory_type_index = i;
            break;
        }
    }
    assert(quads_index_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_index_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_index_mem_reqs.size,
        .memoryTypeIndex = quads_index_memory_type_index,
    };
    VkDeviceMemory quads_index_buffer_memory;
    result = vkAllocateMemory(
        device, &quads_index_alloc_info, NULL, &quads_index_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, quads_index_buffer, quads_index_buffer_memory, 0);
    assert(result == VK_SUCCESS);

    // make texture_image
    VkImageCreateInfo texture_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage texture_image;
    vkCreateImage(device, &texture_image_info, NULL, &texture_image);

    // query memory requirements for the image
    VkMemoryRequirements texture_image_mem_reqs;
    vkGetImageMemoryRequirements(
        device, texture_image, &texture_image_mem_reqs);
    VkPhysicalDeviceMemoryProperties texture_image_mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &texture_image_mem_props);
    uint32_t texture_image_memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < texture_image_mem_props.memoryTypeCount; i++) {
        if ((texture_image_mem_reqs.memoryTypeBits & (1 << i)) &&
            (texture_image_mem_props.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            texture_image_memory_type_index = i;
            break;
        }
    }
    assert(texture_image_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo texture_image_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = texture_image_mem_reqs.size,
        .memoryTypeIndex = texture_image_memory_type_index,
    };
    VkDeviceMemory texture_memory;
    result = vkAllocateMemory(
        device, &texture_image_alloc_info, NULL, &texture_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(device, texture_image, texture_memory, 0);
    assert(result == VK_SUCCESS);

    // texture sampler
    VkSamplerCreateInfo sampler_info = {0};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    VkSampler texture_sampler;
    result = vkCreateSampler(device, &sampler_info, NULL, &texture_sampler);
    assert(result == VK_SUCCESS);

    // 2d texture image view
    VkImageViewCreateInfo view_info = {0};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = texture_image; // Your VkImage handle
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    VkImageView texture_image_view;
    result = vkCreateImageView(device, &view_info, NULL, &texture_image_view);
    assert(result == VK_SUCCESS);

    // descriptor pool and allocation
    VkDescriptorPoolSize descriptor_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size,
        .maxSets = 1,
    };
    VkDescriptorPool descriptor_pool;
    vkCreateDescriptorPool(
        device, &descriptor_pool_info, NULL, &descriptor_pool);
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };
    VkDescriptorSet descriptor_set;
    vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set);
    VkDescriptorImageInfo descriptor_image_info = {
        .sampler = texture_sampler,
        .imageView = texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptor_image_info,
    };
    vkUpdateDescriptorSets(device, 1, &descriptor_write, 0, NULL);

    end("war_vulkan_init");
    // no tuple?!
    return (war_vulkan_context){
        .dmabuf_fd = dmabuf_fd,
        .instance = instance,
        .physical_device = physical_device,
        .device = device,
        .queue = queue,
        .queue_family_index = queue_family_index,
        .image = image,
        .memory = memory,
        .cmd_pool = cmd_pool,
        .cmd_buffer = cmd_buffer,
        .render_pass = render_pass,
        .frame_buffer = frame_buffer,
        .pipeline = pipeline,
        .pipeline_layout = pipeline_layout,
        .image_view = image_view,
        .image_available_semaphore = image_available_semaphore,
        .render_finished_semaphore = render_finished_semaphore,
        .in_flight_fence = in_flight_fence,
        .quads_index_buffer = quads_index_buffer,
        .quads_index_buffer_memory = quads_index_buffer_memory,
        .quads_vertex_buffer = quads_vertex_buffer,
        .quads_vertex_buffer_memory = quads_vertex_buffer_memory,
        .texture_image = texture_image,
        .texture_memory = texture_memory,
        .texture_image_view = texture_image_view,
        .texture_sampler = texture_sampler,
        .texture_descriptor_set = descriptor_set,
        .texture_descriptor_pool = descriptor_pool,
    };
}
