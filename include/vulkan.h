#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#ifndef VIMDAW_VULKAN_H
#define VIMDAW_VULKAN_H
/* build/pre/vulkan.i */
VulkanContext vulkan_make_dmabuf_fd(uint32_t width, uint32_t height);
uint32_t vulkan_find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties, VkPhysicalDevice physical_device);
#endif /* VIMDAW_VULKAN_H */
