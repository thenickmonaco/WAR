#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "data.h"
#ifndef VIMDAW_WAYLAND_H
#define VIMDAW_WAYLAND_H
/* build/pre/wayland.i */
void wayland_init(void);
void wayland_registry_bind(int fd, uint8_t *buffer, size_t offset, uint16_t size, uint16_t new_id);
int wayland_make_fd(void);
#endif /* VIMDAW_WAYLAND_H */
