#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#ifndef VIMDAW_WAYLAND_H
#define VIMDAW_WAYLAND_H
/* build/pre/wayland.i */
int wayland_init(void);
void wayland_registry_bind(int fd, uint8_t *buffer, size_t offset, uint16_t size, uint16_t new_id);
int wayland_make_fd(void);
#endif /* VIMDAW_WAYLAND_H */
