#include "vimDAW.hpp"
#include <thread>

/*
 * Runs vimDAW
 * Handles threading
 */
int main() {
    std::thread ui_thread(render_and_input);

    std::thread audio_thread(audio_playback);

    ui_thread.join();

    audio_thread.join();

    return 0;
}
