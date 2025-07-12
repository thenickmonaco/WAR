#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "data.h"
#ifndef VIMDAW_MAIN_H
#define VIMDAW_MAIN_H
/* build/pre/main.i */
VulkanContext vulkan_make_dmabuf_fd(uint32_t width, uint32_t height);
uint32_t vulkan_find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties, VkPhysicalDevice physical_device);
void wayland_init(void);
void wayland_registry_bind(int fd, uint8_t *buffer, size_t offset, uint16_t size, uint16_t new_id);
int wayland_make_fd(void);
int main(void);
#endif /* VIMDAW_MAIN_H */
