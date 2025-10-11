-------------------------------------------------------------------------------
--
-- WAR - make music with vim motions
-- Copyright (C) 2025 Nick Monaco
--
-- This file is part of WAR 1.0 software.
-- WAR 1.0 software is licensed under the GNU Affero General Public License
-- version 3, with the following modification: attribution to the original
-- author is waived.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
--
-- For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
--
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
-- src/lua/monaco/set.lua
-------------------------------------------------------------------------------

ctx_lua = {
    AUDIO_SAMPLE_RATE = 44100,
    AUDIO_SAMPLE_DURATION = 30,
    AUDIO_CHANNEL_COUNT = 2,
    AUDIO_NOTE_COUNT = 128,
    AUDIO_SAMPLES_PER_NOTE = 128,
    POOL_ALIGNMENT = 256,
}

pool_a = {
    atomics = {
        notes_on = { type = "uint8_t", count = ctx_lua.AUDIO_NOTE_COUNT },
        notes_on_previous = { type = "uint8_t", count = ctx_lua.AUDIO_NOTE_COUNT },
    },
    audio_context = {
        record_buffer = {
            type = "int16_t",
            count = ctx_lua.AUDIO_SAMPLE_RATE *
                ctx_lua.AUDIO_SAMPLE_DURATION *
                ctx_lua.AUDIO_CHANNEL_COUNT
        },
        resample_buffer = {
            type = "int16_t",
            count = ctx_lua.AUDIO_SAMPLE_RATE *
                ctx_lua.AUDIO_SAMPLE_DURATION *
                ctx_lua.AUDIO_CHANNEL_COUNT
        },
        sample_pool = {
            type = "int16_t",
            count = ctx_lua.AUDIO_NOTE_COUNT *
                ctx_lua.AUDIO_SAMPLE_RATE *
                ctx_lua.AUDIO_SAMPLE_DURATION *
                ctx_lua.AUDIO_CHANNEL_COUNT
        },
    },
    samples = {
        samples = {
            type = "int16*",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_frames_start = {
            type = "uint64_t",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_frames_duration = {
            type = "uint64_t",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_attack = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_sustain = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_release = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_gain = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
    },
    record_samples = {
        samples = {
            type = "int16*",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_frames_start = {
            type = "uint64_t",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_attack = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_sustain = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_release = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
        samples_gain = {
            type = "float",
            count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE
        },
    },
}
