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
    AUDIO_BASE_FREQUENCY = 440,
    AUDIO_BASE_NOTE = 69, -- A4
    AUDIO_EDO = 12,
    AUDIO_SAMPLE_RATE = 22000,
    AUDIO_BPM = 100,
    AUDIO_SAMPLE_DURATION = 30,
    AUDIO_CHANNEL_COUNT = 2,
    AUDIO_NOTE_COUNT = 128,
    AUDIO_SAMPLES_PER_NOTE = 128,
    POOL_ALIGNMENT = 256,
}

pool_a = {
    -- Atoms
    { name = "atomics.notes_on",                         type = "uint8_t",           count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "atomics.notes_on_previous",                type = "uint8_t",           count = ctx_lua.AUDIO_NOTE_COUNT },

    -- Audio Context struct itself
    { name = "audio_context",                            type = "war_audio_context", count = 1 },
    { name = "audio_context.sample_frames",              type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "audio_context.sample_frames_duration",     type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "audio_context.sample_phase",               type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "audio_context.record_buffer",              type = "int16_t",           count = ctx_lua.AUDIO_SAMPLE_RATE * ctx_lua.AUDIO_SAMPLE_DURATION * ctx_lua.AUDIO_CHANNEL_COUNT },
    { name = "audio_context.resample_buffer",            type = "int16_t",           count = ctx_lua.AUDIO_SAMPLE_RATE * ctx_lua.AUDIO_SAMPLE_DURATION * ctx_lua.AUDIO_CHANNEL_COUNT },

    -- Sample Pool
    { name = "sample_pool",                              type = "int16_t",           count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLE_RATE * ctx_lua.AUDIO_SAMPLE_DURATION * ctx_lua.AUDIO_CHANNEL_COUNT },

    -- Samples struct
    { name = "samples",                                  type = "war_samples",       count = 1 },
    { name = "samples.samples",                          type = "int16_t*",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_start",             type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_duration",          type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames",                   type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_trim_start",        type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_frames_trim_end",          type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_attack",                   type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_sustain",                  type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_release",                  type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_gain",                     type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.samples_active",                   type = "uint8_t",           count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "samples.notes_attack",                     type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_sustain",                    type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_release",                    type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_gain",                       type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_frames_start",               type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_frames_duration",            type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_frames_trim_start",          type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.notes_frames_trim_end",            type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "samples.samples_count",                    type = "uint32_t",          count = ctx_lua.AUDIO_NOTE_COUNT },

    -- Record Samples struct
    { name = "record_samples",                           type = "war_samples",       count = 1 },
    { name = "record_samples.samples",                   type = "int16_t*",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_start",      type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_duration",   type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames",            type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_trim_start", type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_frames_trim_end",   type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_attack",            type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_sustain",           type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_release",           type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_gain",              type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.samples_active",            type = "uint8_t",           count = ctx_lua.AUDIO_NOTE_COUNT * ctx_lua.AUDIO_SAMPLES_PER_NOTE },
    { name = "record_samples.notes_attack",              type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_sustain",             type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_release",             type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_gain",                type = "float",             count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_frames_start",        type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_frames_duration",     type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_frames_trim_start",   type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.notes_frames_trim_end",     type = "uint64_t",          count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "record_samples.samples_count",             type = "uint32_t",          count = ctx_lua.AUDIO_NOTE_COUNT },

    -- Record indices and userdata
    { name = "record_samples_notes_indices",             type = "int32_t",           count = ctx_lua.AUDIO_NOTE_COUNT },
    { name = "userdata",                                 type = "void*",             count = 8 },
}
