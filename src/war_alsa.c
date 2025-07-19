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
// src/war_alsa.c
//-----------------------------------------------------------------------------

#include <alsa/asoundlib.h>
#include <alsa/timer.h>

void war_alsa_init() {
    // Open the default PCM device for playback (blocking mode)
    snd_pcm_t* pcm_handle;
    int result =
        snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    assert(result >= 0);

    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(
        pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);

    // uint?
    unsigned int sample_rate = 48000;
    snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0);

    snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 2); // stereo

    // uint?
    snd_pcm_uframes_t period_size = 512;
    snd_pcm_hw_params_set_period_size_near(
        pcm_handle, hw_params, &period_size, 0);

    // uint?
    snd_pcm_uframes_t buffer_size = 2048;
    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &buffer_size);

    result = snd_pcm_hw_params(pcm_handle, hw_params);
    assert(result >= 0);

    // Optional: prepare the device (not strictly necessary unless writing
    // audio)
    // result = snd_pcm_prepare(pcm_handle);
    // assert(result >= 0);

    // timing
    snd_pcm_sframes_t frames = snd_pcm_avail_update(pcm_handle);
    // snd_pcm_avail_update(pcm_handle);

    // timestamp
    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);
    result = snd_pcm_status(pcm_handle, status);
    assert(result >= 0);
    snd_timestamp_t ts;
    // snd_pcm_status_get_tstamp(status, &ts);

    enum {
        START_PLAYBACK = 0,
        STOP_PLAYBACK = 1,
        GET_TIMESTAMP = 2,
    };

    while (1) {}
}
