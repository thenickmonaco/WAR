#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#ifndef VIMDAW_WAYLAND_H
#define VIMDAW_WAYLAND_H
/* build/pre/wayland.i */
int wayland_init(void);
int wayland_make_dmabuf_fd(uint32_t width, uint32_t height);
uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties, VkPhysicalDevice physical_device);
void wayland_registry_bind(int fd, uint8_t *buffer, size_t offset, uint16_t size, uint16_t new_id);
int wayland_make_fd(void);
#endif /* VIMDAW_WAYLAND_H */
