#include "vimDAW.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <iostream>
#include <portaudio.h>
#include <random>
#include <vector>

typedef enum Waveform {
    WAVE_SINE,
    WAVE_SAW,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_NOISE
} Waveform;

typedef enum Voice {
    PIANO,
    PIANO2,
    PIANO3,
} Voice;

typedef struct Digital_Note {
    float frequency; // Frequency of the note (in Hz)
    float amplitude; // Amplitude (loudness) based on velocity
    float start;     // Start time (in beats or seconds)
    float end;
    bool is_active; // Flag if the note is currently active or not

    Waveform waveform;

    // Synthesis Parameters for Piano:
    float attack;                // Attack time (ADSR)
    float decay;                 // Decay time (ADSR)
    float sustain;               // Sustain level (ADSR)
    float release;               // Release time (ADSR)
    float filter_cutoff;         // Low-pass filter cutoff (optional)
    float reverb_amount;         // Amount of reverb
    float sympathetic_resonance; // Amount of resonance in sympathetic strings

    // Pedal Simulation
    bool sustain_pedal; // Sustain pedal state (MIDI CC64)

    // Extra Timbre Parameters
    float detune;        // Slight detuning for harmonic content
    float inharmonicity; // Inharmonicity factor (particularly for higher
                         // registers)
    float phase;

    // More...
    Voice voice;

    // Equality check
    bool operator==(const Digital_Note &other) const {
        return (frequency == other.frequency) &&
               (amplitude == other.amplitude) && (start == other.start) &&
               (end == other.end) && (waveform == other.waveform) &&
               (attack == other.attack) && (decay == other.decay) &&
               (sustain == other.sustain) && (release == other.release) &&
               (filter_cutoff == other.filter_cutoff) &&
               (reverb_amount == other.reverb_amount) &&
               (sympathetic_resonance == other.sympathetic_resonance) &&
               (sustain_pedal == other.sustain_pedal) &&
               (detune == other.detune) &&
               (inharmonicity == other.inharmonicity) &&
               (phase == other.phase) && (voice == other.voice);
    }
} Digital_Note;

// quiet ALSA
extern "C" void quiet_alsa_error_handler(const char *file, int line,
                                         const char *function, int err,
                                         const char *fmt, ...) {}

const double SAMPLE_RATE = 44100;
const unsigned long FRAMES_PER_BUFFER = 512;
const float TWO_PI = 2 * M_PI;
const float BPM_100 = 0.15;
const float VELOCITY = 3;

// rng
std::mt19937 rng(std::random_device{}());
std::uniform_real_distribution<float> noise_dist(-1.0f, 1.0f);

double start_time = -1.0;
PaStream *stream;

extern std::vector<Note> notes;
std::vector<Digital_Note> digital_notes;

// threading
extern std::atomic<bool> program_on;
extern std::mutex notes_mtx;
std::atomic<bool> play_audio{false};
std::mutex play_audio_mtx;
std::condition_variable play_audio_cv;
std::atomic<double> gui_playback_time{0.0};

float calculate_frequency(float pitch) {
    float frequency = 440.0 * pow(2.0, (pitch - 69) / 12.0);

    return frequency;
}

float calculate_amplitude(float velocity) {
    float amplitude = velocity / 127.0;

    return amplitude;
}

void populate_digital_notes() {
    digital_notes.clear();

    // thread safety
    std::lock_guard<std::mutex> lock(notes_mtx);

    for (auto &note : notes) {
        digital_notes.push_back(Digital_Note{
            .frequency =
                calculate_frequency(note.note), // Frequency of the note (in Hz)
            .amplitude = calculate_amplitude(
                VELOCITY), // Amplitude (loudness) based on velocity

            // time
            .start = note.start * BPM_100, // Start time (in beats or seconds)
            .end = note.end * BPM_100,
            .is_active = false, // Flag if the note is currently active or not

            .waveform = WAVE_SAW,

            // add functions for these later
            .attack = 0,
            .decay = 0,
            .sustain = 0,
            .release = 0,
            .filter_cutoff = 0,
            .reverb_amount = 0,
            .sympathetic_resonance = 0,

            .sustain_pedal = false,

            .detune = 0,
            .inharmonicity = 0,
        });
    }
}

// Simple waveform generator for Digital_Note
float generate_sample(Digital_Note &note) {
    note.phase += TWO_PI * note.frequency / SAMPLE_RATE;

    if (note.phase > TWO_PI) {
        note.phase -= TWO_PI; // wrap around to keep phase bounded
    }

    switch (note.waveform) {
    case WAVE_SINE:
        return note.amplitude * sin(note.phase);
    case WAVE_SQUARE:
        return note.amplitude * (sin(note.phase) > 0.0f ? 1.0f : -1.0f);
    case WAVE_SAW:
        return note.amplitude * (2.0f * (note.phase / TWO_PI -
                                         floor(note.phase / TWO_PI + 0.5f)));
    case WAVE_TRIANGLE:
        return note.amplitude *
               (fabs(fmod(note.phase / TWO_PI + 0.25f, 1.0f) - 0.5f) * 4.0f -
                1.0f);
    case WAVE_NOISE:
        return note.amplitude * noise_dist(rng);
    default:
        return 0.0f;
    }
}

// Audio callback function
int audio_callback(const void *input_buffer, void *output_buffer,
                   unsigned long frames_per_buffer,
                   const PaStreamCallbackTimeInfo *time_info,
                   PaStreamCallbackFlags status_flags, void *user_data) {

    float *out = (float *)output_buffer;

    if (start_time < 0.0) {
        start_time = time_info->currentTime;
    }
    float local_time = time_info->currentTime - start_time;

    // shared time
    gui_playback_time.store(local_time, std::memory_order_relaxed);

    for (unsigned long i = 0; i < frames_per_buffer; ++i) {

        float sample = 0.0f;

        for (auto &note : digital_notes) {
            if (local_time >= note.start && local_time <= note.end) {
                sample += generate_sample(note);
            }
        }

        *out++ = sample; // Left
        *out++ = sample; // Right
    }

    return paContinue;
}

// Initialize PortAudio
void initialize_portaudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio err: " << Pa_GetErrorText(err) << std::endl;
        exit(1);
    }

    // Open the default stream
    err =
        Pa_OpenDefaultStream(&stream,   // Use the global stream variable
                             0,         // No input channels
                             2,         // Stereo output (2 channels)
                             paFloat32, // Use 32-bit floating point for samples
                             SAMPLE_RATE,       // Sample rate (44100 Hz)
                             FRAMES_PER_BUFFER, // Frames per buffer
                             audio_callback,    // Callback function
                             nullptr            // No user data for now
        );

    if (err != paNoError) {
        std::cerr << "PortAudio err: " << Pa_GetErrorText(err) << std::endl;
        exit(1);
    }

    // Start the stream
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio err: " << Pa_GetErrorText(err) << std::endl;
        exit(1);
    }
}

// Example function to toggle playback
void trigger_playback() {
    for (auto &note : digital_notes) {
        note.is_active = true; // Activate all notes for playback
    }
}

long get_duration() {
    long duration = 0.0f;
    for (auto &note : digital_notes) {
        if (note.end > duration) {
            duration = note.end;
        }
    }
    return duration * 1000 + 1000; // milliseconds
}

void audio_playback() {
    // quiet ALSA
    snd_lib_error_set_handler(quiet_alsa_error_handler);

    initialize_portaudio();

    while (program_on.load()) {
        std::unique_lock<std::mutex> lock(play_audio_mtx);

        // wait until play_audio is true
        play_audio_cv.wait(
            lock, [&]() { return play_audio.load() || !program_on.load(); });

        if (!program_on.load())
            break;

        lock.unlock();

        populate_digital_notes();
        start_time = -1.0;
        trigger_playback();

        long duration = get_duration();
        if (play_audio.load()) {
            for (long part = 0; part < duration; part += duration / 100) {
                Pa_Sleep(duration / 100);
                if (!play_audio.load()) {
                    digital_notes.clear();
                    start_time = -1.0;
                    break;
                }
            }
        }
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
