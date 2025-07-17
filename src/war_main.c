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
// src/war_main.c
//-----------------------------------------------------------------------------

#include "war_alsa.c"
#include "war_drm.c"
#include "war_vulkan.c"
#include "war_wayland.c"

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_macros.h"
#include "h/war_main.h"

#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

int main() {
    CALL_CARMACK("main");

    uint8_t war_window_render_to_audio_ring_buffer[ring_buffer_size];
    uint8_t war_audio_to_window_render_ring_buffer[ring_buffer_size];
    uint8_t write_to_audio_index = 0;
    uint8_t read_from_audio_index = 0;
    uint8_t write_to_window_render_index = 0;
    uint8_t read_from_window_render_index = 0;
    war_thread_args war_ring_buffer_thread_args = {
        .window_render_to_audio_ring_buffer =
            war_window_render_to_audio_ring_buffer,
        .audio_to_window_render_ring_buffer =
            war_audio_to_window_render_ring_buffer,
        .write_to_audio_index = &write_to_audio_index,
        .read_from_audio_index = &read_from_audio_index,
        .write_to_window_render_index = &write_to_window_render_index,
        .read_from_window_render_index = &read_from_window_render_index,
    };
    pthread_t war_window_render_thread;
    pthread_t war_audio_thread;

    pthread_create(&war_window_render_thread,
                   NULL,
                   war_window_render,
                   &war_ring_buffer_thread_args);
    pthread_create(
        &war_audio_thread, NULL, war_audio, &war_ring_buffer_thread_args);

    pthread_join(war_window_render_thread, NULL);
    pthread_join(war_audio_thread, NULL);

    END("main");
    return 0;
}

void* war_window_render(void* args) {
    war_thread_args* a = (war_thread_args*)args;

    war_wayland_init();

    // uint8_t end_window_render = 0;
    // while (!end_window_render) {
    // }

    return 0;
}

void* war_audio(void* args) {
    war_thread_args* a = (war_thread_args*)args;

    // uint8_t end_audio = 0;
    // while (!end_audio) {
    // }

    return 0;
}
