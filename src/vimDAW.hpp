#ifndef VIMDAW_H
#define VIMDAW_H

/*
 * Shared between render_and_input and audio_playback
 */
typedef struct Note {
    int start;
    int end;
    int note;
    int duration() const { return end - start; }

    // ascending order
    bool operator<(const Note& other) const {
        return start < other.start;
    }
} Note;

/*
 * Render and handle input
 * SDL3
 */
void render_and_input();

/*
 * Handle audio playback
 * PortAudio + RtMidi
 */
void audio_playback();

#endif
