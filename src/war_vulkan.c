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

    VkFormat quad_depth_format = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo quad_depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = quad_depth_format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage quad_depth_image;
    VkResult r =
        vkCreateImage(device, &quad_depth_image_info, NULL, &quad_depth_image);
    assert(r == VK_SUCCESS);
    VkMemoryRequirements quad_depth_mem_reqs;
    vkGetImageMemoryRequirements(
        device, quad_depth_image, &quad_depth_mem_reqs);
    VkPhysicalDeviceMemoryProperties quad_depth_mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &quad_depth_mem_props);
    int quad_depth_memory_type_index = -1;
    for (uint32_t i = 0; i < quad_depth_mem_props.memoryTypeCount; i++) {
        if ((quad_depth_mem_reqs.memoryTypeBits & (1u << i)) &&
            (quad_depth_mem_props.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            quad_depth_memory_type_index = i;
            break;
        }
    }
    assert(quad_depth_memory_type_index != -1);
    VkMemoryAllocateInfo quad_depth_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quad_depth_mem_reqs.size,
        .memoryTypeIndex = quad_depth_memory_type_index,
    };
    VkDeviceMemory quad_depth_image_memory;
    r = vkAllocateMemory(
        device, &quad_depth_alloc_info, NULL, &quad_depth_image_memory);
    assert(r == VK_SUCCESS);
    r = vkBindImageMemory(device, quad_depth_image, quad_depth_image_memory, 0);
    assert(r == VK_SUCCESS);
    // --- create image view ---
    VkImageViewCreateInfo quad_depth_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = quad_depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = quad_depth_format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkImageView quad_depth_image_view;
    r = vkCreateImageView(
        device, &quad_depth_view_info, NULL, &quad_depth_image_view);
    assert(r == VK_SUCCESS);
    VkAttachmentDescription quad_depth_attachment = {
        .format = quad_depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);
    VkCommandPool cmd_pool;
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool);
    assert(result == VK_SUCCESS);
    VkCommandBuffer cmd_buffer;
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = vkAllocateCommandBuffers(device, &cmd_buf_info, &cmd_buffer);
    assert(result == VK_SUCCESS);
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
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, image, &mem_reqs);
    VkExportMemoryAllocateInfo export_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
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
    VkAttachmentDescription quad_attachments[2] = {color_attachment,
                                                   quad_depth_attachment};
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference quad_depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &quad_depth_ref,
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = quad_attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    VkRenderPass render_pass;
    result = vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass);
    assert(result == VK_SUCCESS);
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
    VkImageView quad_fb_attachments[2] = {image_view, quad_depth_image_view};
    VkFramebufferCreateInfo frame_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 2,
        .pAttachments = quad_fb_attachments,
        .width = width,
        .height = height,
        .layers = 1,
    };
    VkFramebuffer frame_buffer;
    result =
        vkCreateFramebuffer(device, &frame_buffer_info, NULL, &frame_buffer);
    assert(result == VK_SUCCESS);

    uint32_t* vertex_code;
    const char* vertex_path = "build/shaders/war_quad_vertex.spv";
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
    uint32_t* fragment_code;
    const char* fragment_path = "build/shaders/war_quad_fragment.spv";
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
        .size = sizeof(war_quad_push_constants),
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
    VkVertexInputBindingDescription quad_vertex_binding = {
        .binding = 0,
        .stride = sizeof(war_quad_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputBindingDescription quad_instance_binding = {
        .binding = 1,
        .stride = sizeof(war_quad_instance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    VkVertexInputAttributeDescription quad_vertex_attrs[] = {
        (VkVertexInputAttributeDescription){
            .location = 0,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, corner),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 1,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, pos),
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 2,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, color),
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        },
        (VkVertexInputAttributeDescription){
            .location = 3,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, outline_thickness),
            .format = VK_FORMAT_R32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 4,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, outline_color),
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        },
        (VkVertexInputAttributeDescription){
            .location = 5,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, line_thickness),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 6,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, flags),
            .format = VK_FORMAT_R32_UINT,
        },
        (VkVertexInputAttributeDescription){
            .location = 7,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, span),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
    };
    uint32_t num_quad_vertex_attrs = 8;
    VkVertexInputAttributeDescription quad_instance_attrs[] = {
        (VkVertexInputAttributeDescription){.location = num_quad_vertex_attrs,
                                            .binding = 1,
                                            .format = VK_FORMAT_R32_UINT,
                                            .offset =
                                                offsetof(war_quad_instance, x)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 1,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, y)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 2,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, color)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 3,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, flags)},
    };
    uint32_t num_quad_instance_attrs = 4;
    VkVertexInputAttributeDescription* all_attrs =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               (num_quad_vertex_attrs + num_quad_instance_attrs));
    memcpy(all_attrs,
           quad_vertex_attrs,
           sizeof(VkVertexInputAttributeDescription) * num_quad_vertex_attrs);
    memcpy(all_attrs + num_quad_vertex_attrs,
           quad_instance_attrs,
           sizeof(VkVertexInputAttributeDescription) * num_quad_instance_attrs);
    VkVertexInputBindingDescription all_bindings[] = {quad_vertex_binding,
                                                      quad_instance_binding};
    VkPipelineVertexInputStateCreateInfo quad_vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = all_bindings,
        .vertexAttributeDescriptionCount =
            num_quad_instance_attrs + num_quad_vertex_attrs,
        .pVertexAttributeDescriptions = all_attrs,
    };
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {width, height},
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,  // enable depth testing
        .depthWriteEnable = VK_TRUE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .stencilTestEnable = VK_FALSE,
    };
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &quad_vertex_input,
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
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pDepthStencilState = &depth_stencil,
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
                            .blendEnable = VK_TRUE, // enable blending
                            .srcColorBlendFactor =
                                VK_BLEND_FACTOR_SRC_ALPHA, // source color
                                                           // weight
                            .dstColorBlendFactor =
                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // dest
                                                                     // color
                                                                     // weight
                            .colorBlendOp = VK_BLEND_OP_ADD, // combine src+dst
                            .srcAlphaBlendFactor =
                                VK_BLEND_FACTOR_ONE, // source alpha
                            .dstAlphaBlendFactor =
                                VK_BLEND_FACTOR_ZERO,        // dest alpha
                            .alphaBlendOp = VK_BLEND_OP_ADD, // combine alpha
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
        .pDynamicState = NULL,
    };
    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
    assert(result == VK_SUCCESS);
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
    VkImageMemoryBarrier quad_depth_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = quad_depth_image, // <-- the depth VkImage you created
        .subresourceRange =
            {
                .aspectMask =
                    VK_IMAGE_ASPECT_DEPTH_BIT, // use DEPTH_BIT, add STENCIL_BIT
                                               // if format has stencil
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
    VkImageMemoryBarrier quad_barriers[2] = {barrier, quad_depth_barrier};
    vkBeginCommandBuffer(
        cmd_buffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        });
    vkCmdPipelineBarrier(cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         2,
                         quad_barriers);
    vkEndCommandBuffer(cmd_buffer);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
    };
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    // no need for semaphores becasue wayalnd
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
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence in_flight_fences[max_frames];
    for (size_t i = 0; i < max_frames; i++) {
        result = vkCreateFence(device, &fence_info, NULL, &in_flight_fences[i]);
        assert(result == VK_SUCCESS);
    }
    VkBufferCreateInfo quads_vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = max_quads * sizeof(war_quad_vertex) * 4 * max_frames,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer quads_vertex_buffer;
    result = vkCreateBuffer(
        device, &quads_vertex_buffer_info, NULL, &quads_vertex_buffer);
    assert(result == VK_SUCCESS);
    VkBufferCreateInfo quads_index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = max_quads * 6 * sizeof(uint16_t) * max_frames,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer quads_index_buffer;
    result = vkCreateBuffer(
        device, &quads_index_buffer_info, NULL, &quads_index_buffer);
    assert(result == VK_SUCCESS);
    VkBufferCreateInfo quads_instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = max_quads * max_instances_per_quad * sizeof(war_quad_instance) *
                max_frames,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer quads_instance_buffer;
    result = vkCreateBuffer(
        device, &quads_instance_buffer_info, NULL, &quads_instance_buffer);
    assert(result == VK_SUCCESS);
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
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
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
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
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
    VkMemoryRequirements quads_instance_mem_reqs;
    vkGetBufferMemoryRequirements(
        device, quads_instance_buffer, &quads_instance_mem_reqs);
    uint32_t quads_instance_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_instance_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &quads_instance_mem_properties);
    for (uint32_t i = 0; i < quads_instance_mem_properties.memoryTypeCount;
         i++) {
        if ((quads_instance_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_instance_mem_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            quads_instance_memory_type_index = i;
            break;
        }
    }
    assert(quads_instance_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_instance_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_instance_mem_reqs.size,
        .memoryTypeIndex = quads_instance_memory_type_index,
    };
    VkDeviceMemory quads_instance_buffer_memory;
    result = vkAllocateMemory(device,
                              &quads_instance_alloc_info,
                              NULL,
                              &quads_instance_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, quads_instance_buffer, quads_instance_buffer_memory, 0);
    assert(result == VK_SUCCESS);
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
    void* quads_vertex_buffer_mapped;
    vkMapMemory(device,
                quads_vertex_buffer_memory,
                0,
                sizeof(war_quad_vertex) * max_quads * 4 * max_frames,
                0,
                &quads_vertex_buffer_mapped);
    void* quads_index_buffer_mapped;
    vkMapMemory(device,
                quads_index_buffer_memory,
                0,
                sizeof(uint16_t) * max_quads * 6 * max_frames,
                0,
                &quads_index_buffer_mapped);
    void* quads_instance_buffer_mapped;
    vkMapMemory(device,
                quads_instance_buffer_memory,
                0,
                sizeof(war_quad_instance) * max_quads * max_instances_per_quad *
                    max_frames,
                0,
                &quads_instance_buffer_mapped);

    //-------------------------------------------------------------------------
    // SDF FONT RENDERING PIPELINE
    //-------------------------------------------------------------------------
    FT_Library ft_library;
    FT_Init_FreeType(&ft_library);
    FT_Face ft_regular;
    FT_New_Face(ft_library, "assets/fonts/FreeMono.otf", 0, &ft_regular);
    float font_pixel_height = 69.0f;
    FT_Set_Pixel_Sizes(ft_regular, 0, (int)font_pixel_height);
    float ascent = ft_regular->size->metrics.ascender / 64.0f;
    float descent = ft_regular->size->metrics.descender / 64.0f;
    float cell_height = ft_regular->size->metrics.height / 64.0f;
    float font_height = ascent - descent;
    float line_gap = cell_height - font_height;
    float baseline = ascent + line_gap / 2.0f;
    float cell_width = 0;
    uint8_t* atlas_pixels = malloc(atlas_width * atlas_height);
    assert(atlas_pixels);
    war_glyph_info glyphs[128];
    int pen_x = 0, pen_y = 0, row_height = 0;
    for (int c = 0; c < 128; c++) {
        FT_Load_Char(ft_regular, c, FT_LOAD_RENDER);
        if (c == 'M') {
            call_carmack("for monospaced fonts");
            cell_width = ft_regular->glyph->advance.x / 64.0f;
        }
        FT_Bitmap* bmp = &ft_regular->glyph->bitmap;
        int w = bmp->width;
        int h = bmp->rows;
        if (pen_x + w >= atlas_width) {
            pen_x = 0;
            pen_y += row_height + 1;
            row_height = 0;
        }
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                atlas_pixels[(pen_x + x) + (pen_y + y) * atlas_width] =
                    bmp->buffer[x + y * bmp->width];
            }
        }
        glyphs[c].advance_x = ft_regular->glyph->advance.x / 64.0f;
        glyphs[c].advance_y = ft_regular->glyph->advance.y / 64.0f;
        glyphs[c].bearing_x = ft_regular->glyph->bitmap_left;
        glyphs[c].bearing_y = ft_regular->glyph->bitmap_top;
        glyphs[c].width = w;
        glyphs[c].height = h;
        glyphs[c].uv_x0 = (float)pen_x / atlas_width;
        glyphs[c].uv_y0 = (float)pen_y / atlas_height;
        glyphs[c].uv_x1 = (float)(pen_x + w) / atlas_width;
        glyphs[c].uv_y1 = (float)(pen_y + h) / atlas_height;
        glyphs[c].ascent = ft_regular->glyph->metrics.horiBearingY / 64.0f;
        glyphs[c].descent =
            (ft_regular->glyph->metrics.height / 64.0f) - glyphs[c].ascent;
        pen_x += w + 1;
        if (h > row_height) { row_height = h; }
    }
    assert(cell_width != 0);
    VkImageCreateInfo sdf_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM, // single-channel grayscale
        .extent = {atlas_width, atlas_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage text_image;
    result = vkCreateImage(device, &sdf_image_info, NULL, &text_image);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_memory_requirements;
    vkGetImageMemoryRequirements(device, text_image, &sdf_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_image_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &sdf_image_memory_properties);
    uint32_t sdf_image_memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_image_memory_properties.memoryTypeCount; i++) {
        if ((sdf_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_image_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            sdf_image_memory_type_index = i;
            break;
        }
    }
    assert(sdf_image_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo sdf_image_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_memory_requirements.size,
        .memoryTypeIndex = sdf_image_memory_type_index,
    };
    VkDeviceMemory text_image_memory;
    result = vkAllocateMemory(
        device, &sdf_image_allocate_info, NULL, &text_image_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(device, text_image, text_image_memory, 0);
    assert(result == VK_SUCCESS);
    VkImageViewCreateInfo sdf_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = text_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkImageView text_image_view;
    result = vkCreateImageView(device, &sdf_view_info, NULL, &text_image_view);
    assert(result == VK_SUCCESS);
    VkSamplerCreateInfo sdf_sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    VkSampler text_sampler;
    vkCreateSampler(device, &sdf_sampler_info, NULL, &text_sampler);
    VkDeviceSize sdf_image_size = atlas_width * atlas_height;
    VkBuffer sdf_staging_buffer;
    VkDeviceMemory sdf_staging_buffer_memory;
    VkBufferCreateInfo sdf_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vkCreateBuffer(device, &sdf_buffer_info, NULL, &sdf_staging_buffer);
    VkMemoryRequirements sdf_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(
        device, sdf_staging_buffer, &sdf_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &sdf_buffer_memory_properties);
    uint32_t sdf_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_buffer_memory_type,
    };
    result = vkAllocateMemory(
        device, &sdf_buffer_allocate_info, NULL, &sdf_staging_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, sdf_staging_buffer, sdf_staging_buffer_memory, 0);
    assert(result == VK_SUCCESS);
    void* sdf_buffer_data;
    vkMapMemory(device,
                sdf_staging_buffer_memory,
                0,
                sdf_image_size,
                0,
                &sdf_buffer_data);
    memcpy(sdf_buffer_data, atlas_pixels, (size_t)sdf_image_size);
    vkUnmapMemory(device, sdf_staging_buffer_memory);
    VkCommandBufferAllocateInfo sdf_command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool, // reuse your existing command pool here
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer sdf_copy_command_buffer;
    vkAllocateCommandBuffers(
        device, &sdf_command_buffer_allocate_info, &sdf_copy_command_buffer);
    VkCommandBufferBeginInfo sdf_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(sdf_copy_command_buffer, &sdf_begin_info);
    VkImageMemoryBarrier barrier_to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = text_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(sdf_copy_command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier_to_transfer);
    VkBufferImageCopy sdf_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0, // tightly packed
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent =
            {
                .width = atlas_width,
                .height = atlas_height,
                .depth = 1,
            },
    };
    vkCmdCopyBufferToImage(sdf_copy_command_buffer,
                           sdf_staging_buffer,
                           text_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &sdf_region);
    VkImageMemoryBarrier barrier_to_shader_read = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = text_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(sdf_copy_command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier_to_shader_read);
    vkEndCommandBuffer(sdf_copy_command_buffer);
    VkSubmitInfo sdf_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &sdf_copy_command_buffer,
    };
    vkQueueSubmit(queue, 1, &sdf_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, cmd_pool, 1, &sdf_copy_command_buffer);
    vkDestroyBuffer(device, sdf_staging_buffer, NULL);
    vkFreeMemory(device, sdf_staging_buffer_memory, NULL);
    VkDescriptorSetLayoutBinding sdf_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo sdf_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sdf_binding,
    };
    VkDescriptorSetLayout font_descriptor_set_layout;
    vkCreateDescriptorSetLayout(
        device, &sdf_layout_info, NULL, &font_descriptor_set_layout);
    VkDescriptorPoolSize sdf_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo sdf_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &sdf_pool_size,
        .maxSets = 1,
    };
    VkDescriptorPool font_descriptor_pool;
    vkCreateDescriptorPool(device, &sdf_pool_info, NULL, &font_descriptor_pool);
    VkDescriptorSetAllocateInfo sdf_descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = font_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &font_descriptor_set_layout,
    };
    VkDescriptorSet font_descriptor_set;
    vkAllocateDescriptorSets(
        device, &sdf_descriptor_set_allocate_info, &font_descriptor_set);
    VkDescriptorImageInfo sdf_descriptor_info = {
        .sampler = text_sampler,
        .imageView = text_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write_descriptor_sets = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = font_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &sdf_descriptor_info,
    };
    vkUpdateDescriptorSets(device, 1, &write_descriptor_sets, 0, NULL);
    uint32_t* sdf_vertex_code;
    const char* sdf_vertex_path = "build/shaders/war_text_vertex.spv";
    FILE* sdf_vertex_spv = fopen(sdf_vertex_path, "rb");
    assert(sdf_vertex_spv);
    fseek(sdf_vertex_spv, 0, SEEK_END);
    long sdf_vertex_size = ftell(sdf_vertex_spv);
    fseek(sdf_vertex_spv, 0, SEEK_SET);
    assert(sdf_vertex_size > 0 &&
           (sdf_vertex_size % 4 == 0)); // SPIR-V is 4-byte aligned
    sdf_vertex_code = malloc(sdf_vertex_size);
    assert(sdf_vertex_code);
    size_t sdf_vertex_spv_read =
        fread(sdf_vertex_code, 1, sdf_vertex_size, sdf_vertex_spv);
    assert(sdf_vertex_spv_read == (size_t)sdf_vertex_size);
    fclose(sdf_vertex_spv);
    VkShaderModuleCreateInfo sdf_vertex_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sdf_vertex_size,
        .pCode = sdf_vertex_code};
    VkShaderModule text_vertex_shader;
    result = vkCreateShaderModule(
        device, &sdf_vertex_shader_info, NULL, &text_vertex_shader);
    assert(result == VK_SUCCESS);
    free(sdf_vertex_code);
    uint32_t* sdf_fragment_code;
    const char* sdf_fragment_path = "build/shaders/war_text_fragment.spv";
    FILE* sdf_fragment_spv = fopen(sdf_fragment_path, "rb");
    assert(sdf_fragment_spv);
    fseek(sdf_fragment_spv, 0, SEEK_END);
    long sdf_fragment_size = ftell(sdf_fragment_spv);
    fseek(sdf_fragment_spv, 0, SEEK_SET);
    assert(sdf_fragment_size > 0 &&
           (sdf_fragment_size % 4 == 0)); // SPIR-V is 4-byte aligned
    sdf_fragment_code = malloc(sdf_fragment_size);
    assert(sdf_fragment_code);
    size_t sdf_fragment_spv_read =
        fread(sdf_fragment_code, 1, sdf_fragment_size, sdf_fragment_spv);
    assert(sdf_fragment_spv_read == (size_t)sdf_fragment_size);
    fclose(sdf_fragment_spv);
    VkShaderModuleCreateInfo sdf_fragment_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sdf_fragment_size,
        .pCode = sdf_fragment_code};
    VkShaderModule text_fragment_shader;
    result = vkCreateShaderModule(
        device, &sdf_fragment_shader_info, NULL, &text_fragment_shader);
    assert(result == VK_SUCCESS);
    free(sdf_fragment_code);
    VkPushConstantRange text_push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(war_text_push_constants),
    };
    VkPipelineLayoutCreateInfo sdf_pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &font_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &text_push_constant_range,
    };
    VkPipelineLayout text_pipeline_layout;
    result = vkCreatePipelineLayout(
        device, &sdf_pipeline_layout_info, NULL, &text_pipeline_layout);
    assert(result == VK_SUCCESS);
    VkPipelineShaderStageCreateInfo sdf_shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = text_vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = text_fragment_shader,
            .pName = "main",
        },
    };
    VkDeviceSize sdf_vertex_buffer_size =
        sizeof(war_text_vertex) * max_text_quads * 4 * max_frames;
    VkBufferCreateInfo sdf_vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer text_vertex_buffer;
    result = vkCreateBuffer(
        device, &sdf_vertex_buffer_info, NULL, &text_vertex_buffer);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_vertex_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(
        device, text_vertex_buffer, &sdf_vertex_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_vertex_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &sdf_vertex_buffer_memory_properties);
    uint32_t sdf_vertex_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0;
         i < sdf_vertex_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_vertex_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_vertex_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_vertex_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_vertex_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_vertex_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_vertex_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_vertex_buffer_memory_type,
    };
    VkDeviceMemory text_vertex_buffer_memory;
    result = vkAllocateMemory(device,
                              &sdf_vertex_buffer_allocate_info,
                              NULL,
                              &text_vertex_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, text_vertex_buffer, text_vertex_buffer_memory, 0);
    assert(result == VK_SUCCESS);
    VkDeviceSize sdf_index_buffer_size =
        sizeof(uint16_t) * max_text_quads * 6 * max_frames;
    VkBufferCreateInfo sdf_index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_index_buffer_size,
        .usage =
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer text_index_buffer;
    result = vkCreateBuffer(
        device, &sdf_index_buffer_info, NULL, &text_index_buffer);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_index_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(
        device, text_index_buffer, &sdf_index_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_index_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &sdf_index_buffer_memory_properties);
    uint32_t sdf_index_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_index_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_index_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_index_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_index_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_index_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_index_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_index_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_index_buffer_memory_type,
    };
    VkDeviceMemory text_index_buffer_memory;
    result = vkAllocateMemory(device,
                              &sdf_index_buffer_allocate_info,
                              NULL,
                              &text_index_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, text_index_buffer, text_index_buffer_memory, 0);
    assert(result == VK_SUCCESS);
    VkDeviceSize sdf_instance_buffer_size =
        sizeof(war_text_instance) * max_text_quads *
        max_instances_per_sdf_quad * max_frames;
    VkBufferCreateInfo sdf_instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_instance_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer text_instance_buffer;
    result = vkCreateBuffer(
        device, &sdf_instance_buffer_info, NULL, &text_instance_buffer);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_instance_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(
        device, text_instance_buffer, &sdf_instance_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_instance_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &sdf_instance_buffer_memory_properties);
    uint32_t sdf_instance_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0;
         i < sdf_instance_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_instance_buffer_memory_requirements.memoryTypeBits &
             (1 << i)) &&
            (sdf_instance_buffer_memory_properties.memoryTypes[i]
                 .propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_instance_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_instance_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_instance_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_instance_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_instance_buffer_memory_type,
    };
    VkDeviceMemory text_instance_buffer_memory;
    result = vkAllocateMemory(device,
                              &sdf_instance_buffer_allocate_info,
                              NULL,
                              &text_instance_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        device, text_instance_buffer, text_instance_buffer_memory, 0);
    assert(result == VK_SUCCESS);
    VkVertexInputBindingDescription sdf_vertex_binding_desc = {
        .binding = 0,
        .stride = sizeof(war_text_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputBindingDescription sdf_instance_binding_desc = {
        .binding = 1,
        .stride = sizeof(war_text_instance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    VkVertexInputAttributeDescription sdf_vertex_attribute_descs[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, corner)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_text_vertex, pos)},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(war_text_vertex, color)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, uv)},
        {4,
         0,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_text_vertex, glyph_bearing)},
        {5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, glyph_size)},
        {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, ascent)},
        {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, descent)},
        {8, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, thickness)},
        {9, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, feather)},
        {10, 0, VK_FORMAT_R32_UINT, offsetof(war_text_vertex, flags)},
    };
    uint32_t num_sdf_vertex_attrs = 11;
    VkVertexInputAttributeDescription sdf_instance_attribute_descs[] = {
        {num_sdf_vertex_attrs,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, x)},
        {num_sdf_vertex_attrs + 1,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, y)},
        {num_sdf_vertex_attrs + 2,
         1,
         VK_FORMAT_R8G8B8A8_UINT,
         offsetof(war_text_instance, color)},
        {num_sdf_vertex_attrs + 3,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, uv_x)},
        {num_sdf_vertex_attrs + 4,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, uv_y)},
        {num_sdf_vertex_attrs + 5,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, thickness)},
        {num_sdf_vertex_attrs + 6,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, feather)},
        {num_sdf_vertex_attrs + 7,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, flags)},
    };
    uint32_t num_sdf_instance_attrs = 8;
    VkVertexInputAttributeDescription* sdf_all_attrs =
        malloc((num_sdf_vertex_attrs + num_sdf_instance_attrs) *
               sizeof(VkVertexInputAttributeDescription));
    memcpy(sdf_all_attrs,
           sdf_vertex_attribute_descs,
           sizeof(sdf_vertex_attribute_descs));
    memcpy(sdf_all_attrs + num_sdf_vertex_attrs,
           sdf_instance_attribute_descs,
           sizeof(sdf_instance_attribute_descs));
    VkVertexInputBindingDescription sdf_all_bindings[] = {
        sdf_vertex_binding_desc, sdf_instance_binding_desc};
    VkPipelineVertexInputStateCreateInfo sdf_vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = sdf_all_bindings,
        .vertexAttributeDescriptionCount =
            num_sdf_vertex_attrs + num_sdf_instance_attrs,
        .pVertexAttributeDescriptions = sdf_all_attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport sdf_viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D sdf_scissor = {
        .offset = {0, 0},
        .extent = {width, height},
    };
    VkPipelineDepthStencilStateCreateInfo sdf_depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,  // enable depth testing
        .depthWriteEnable = VK_TRUE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_GREATER,
        .stencilTestEnable = VK_FALSE,
    };
    VkPipelineViewportStateCreateInfo sdf_viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &sdf_viewport,
        .scissorCount = 1,
        .pScissors = &sdf_scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_TRUE,                           // enable blending
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, // source color weight
        .dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,     // dest color weight
        .colorBlendOp = VK_BLEND_OP_ADD,             // combine src+dst
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // source alpha
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // dest alpha
        .alphaBlendOp = VK_BLEND_OP_ADD,             // combine alpha
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    VkAttachmentDescription sdf_color_attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM, // e.g., VK_FORMAT_B8G8R8A8_UNORM
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // Keep existing content
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference sdf_color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription sdf_subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &sdf_color_attachment_ref,
        .pDepthStencilAttachment = &quad_depth_ref,
    };
    VkAttachmentDescription sdf_attachments[2] = {sdf_color_attachment,
                                                  quad_depth_attachment};
    VkRenderPassCreateInfo sdf_render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = sdf_attachments,
        .subpassCount = 1,
        .pSubpasses = &sdf_subpass,
    };
    VkRenderPass text_render_pass;
    result = vkCreateRenderPass(
        device, &sdf_render_pass_info, NULL, &text_render_pass);
    assert(result == VK_SUCCESS);
    VkGraphicsPipelineCreateInfo sdf_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = sdf_shader_stages,
        .pVertexInputState = &sdf_vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState =
            &(VkPipelineViewportStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = NULL,
        .layout = text_pipeline_layout,
        .renderPass = text_render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    VkPipeline text_pipeline;
    result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &sdf_pipeline_info, NULL, &text_pipeline);
    assert(result == VK_SUCCESS);
    war_glyph_info* heap_glyphs = malloc(sizeof(war_glyph_info) * 128);
    memcpy(heap_glyphs, glyphs, sizeof(war_glyph_info) * 128);
    void* text_vertex_buffer_mapped;
    vkMapMemory(device,
                text_vertex_buffer_memory,
                0,
                sizeof(war_text_vertex) * max_text_quads * 4 * max_frames,
                0,
                &text_vertex_buffer_mapped);
    void* text_index_buffer_mapped;
    vkMapMemory(device,
                text_index_buffer_memory,
                0,
                sizeof(uint16_t) * max_text_quads * 6 * max_frames,
                0,
                &text_index_buffer_mapped);
    void* text_instance_buffer_mapped;
    vkMapMemory(device,
                text_instance_buffer_memory,
                0,
                sizeof(war_text_instance) * max_text_quads *
                    max_instances_per_sdf_quad * max_frames,
                0,
                &text_instance_buffer_mapped);

    VkPipelineDepthStencilStateCreateInfo transparent_depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,   // enable depth testing
        .depthWriteEnable = VK_FALSE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .stencilTestEnable = VK_FALSE,
    };
    VkGraphicsPipelineCreateInfo transparent_quad_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &quad_vertex_input,
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
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pDepthStencilState = &transparent_depth_stencil,
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
                            .blendEnable = VK_TRUE, // enable blending
                            .srcColorBlendFactor =
                                VK_BLEND_FACTOR_SRC_ALPHA, // source color
                                                           // weight
                            .dstColorBlendFactor =
                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // dest
                                                                     // color
                                                                     // weight
                            .colorBlendOp = VK_BLEND_OP_ADD, // combine src+dst
                            .srcAlphaBlendFactor =
                                VK_BLEND_FACTOR_ONE, // source alpha
                            .dstAlphaBlendFactor =
                                VK_BLEND_FACTOR_ZERO,        // dest alpha
                            .alphaBlendOp = VK_BLEND_OP_ADD, // combine alpha
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
        .pDynamicState = NULL,
    };
    VkPipeline transparent_quad_pipeline;
    result = vkCreateGraphicsPipelines(device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &transparent_quad_pipeline_info,
                                       NULL,
                                       &transparent_quad_pipeline);
    assert(result == VK_SUCCESS);

    return (war_vulkan_context){
        //----------------------------------------------------------------------
        // QUAD PIPELINE
        //----------------------------------------------------------------------
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
        .quad_pipeline = pipeline,
        .transparent_quad_pipeline = transparent_quad_pipeline,
        .pipeline_layout = pipeline_layout,
        .image_view = image_view,
        .image_available_semaphore = image_available_semaphore,
        .render_finished_semaphore = render_finished_semaphore,
        .quads_index_buffer = quads_index_buffer,
        .quads_index_buffer_memory = quads_index_buffer_memory,
        .quads_vertex_buffer = quads_vertex_buffer,
        .quads_vertex_buffer_memory = quads_vertex_buffer_memory,
        .quads_instance_buffer = quads_instance_buffer,
        .quads_instance_buffer_memory = quads_instance_buffer_memory,
        .texture_image = texture_image,
        .texture_memory = texture_memory,
        .texture_image_view = texture_image_view,
        .texture_sampler = texture_sampler,
        .texture_descriptor_set = descriptor_set,
        .texture_descriptor_pool = descriptor_pool,
        .in_flight_fences = in_flight_fences,
        .quads_vertex_buffer_mapped = quads_vertex_buffer_mapped,
        .quads_index_buffer_mapped = quads_index_buffer_mapped,
        .quads_instance_buffer_mapped = quads_instance_buffer_mapped,
        .current_frame = 0,

        //---------------------------------------------------------------------
        // SDF TEXT PIPELINE
        //---------------------------------------------------------------------
        .ft_library = ft_library,
        .ft_regular = ft_regular,
        .text_image = text_image,
        .text_image_view = text_image_view,
        .text_image_memory = text_image_memory,
        .text_sampler = text_sampler,
        .glyphs = heap_glyphs,
        .font_descriptor_set = font_descriptor_set,
        .font_descriptor_set_layout = font_descriptor_set_layout,
        .font_descriptor_pool = font_descriptor_pool,
        .text_pipeline = text_pipeline,
        .text_pipeline_layout = text_pipeline_layout,
        .text_vertex_shader = text_vertex_shader,
        .text_vertex_buffer = text_vertex_buffer,
        .text_vertex_buffer_memory = text_vertex_buffer_memory,
        .text_index_buffer = text_index_buffer,
        .text_index_buffer_memory = text_index_buffer_memory,
        .text_instance_buffer = text_instance_buffer,
        .text_instance_buffer_memory = text_instance_buffer_memory,
        .text_fragment_shader = text_fragment_shader,
        .text_push_constant_range = text_push_constant_range,
        .text_render_pass = text_render_pass,
        .ascent = ascent,
        .descent = descent,
        .line_gap = line_gap,
        .baseline = baseline,
        .font_height = font_height,
        .cell_height = cell_height,
        .cell_width = cell_width,
        .text_vertex_buffer_mapped = text_vertex_buffer_mapped,
        .text_index_buffer_mapped = text_index_buffer_mapped,
        .text_instance_buffer_mapped = text_instance_buffer_mapped,
    };
}
