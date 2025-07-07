#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#ifndef VIMDAW_MAIN_H
#define VIMDAW_MAIN_H
/* build/pre/main.i */
int wayland_init(void);
void wayland_registry_bind_request(int fd, uint8_t *buffer, size_t offset, uint16_t size, uint16_t new_id);
int wayland_make_fd(void);
int main(void);
#endif /* VIMDAW_MAIN_H */
