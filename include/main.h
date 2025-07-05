#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#ifndef VIMDAW_MAIN_H
#define VIMDAW_MAIN_H
/* build/pre/main.i */
int wayland_init(void);
int wayland_make_fd(void);
int main(void);
#endif /* VIMDAW_MAIN_H */
