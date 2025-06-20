// vimDAW - make music with vim motions
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

//=============================================================================
// src/main.rs
//=============================================================================

mod audio;
mod bak;
mod clock;
mod colors;
mod font;
mod input;
mod lua;
mod main_engine;
mod message;
mod state;

use crate::message::Message;
use crate::state::{
    AudioEngine, BackgroundEngine, Channels, Engine, EngineChannels,
    EngineType, MainEngine, Producers, State,
};
use parking_lot::Mutex;
use ringbuf::RingBuffer;
use std::collections::HashMap;
use std::sync::Arc;
use std::thread;

fn make_ring() -> (ringbuf::Producer<Message>, ringbuf::Consumer<Message>) {
    // Adjust buffer size as needed
    RingBuffer::<Message>::new(1024).split()
}

pub fn main() {
    let state = Arc::new(State::default());

    let main_to_audio = make_ring();
    let audio_to_main = make_ring();

    let main_to_background = make_ring();
    let background_to_main = make_ring();

    let audio_to_background = make_ring();
    let background_to_audio = make_ring();

    let main_channels = EngineChannels {
        to_main: panic!("main can't send to its self"),
        from_main: panic!("main can't receive from itself"),

        to_audio: main_to_audio.0,
        from_audio: audio_to_main.1,

        to_background: main_to_background.0,
        from_background: background_to_main.1,
    };

    let audio_channels = EngineChannels {
        to_main: audio_to_main.0,
        from_main: main_to_audio.1,

        to_audio: panic!("audio can't send to its self"),
        from_audio: panic!("audio can't receive from itself"),

        to_background: audio_to_background.0,
        from_background: background_to_audio.1,
    };

    let background_channels = EngineChannels {
        to_main: background_to_main.0,
        from_main: main_to_background.1,

        to_audio: background_to_audio.0,
        from_audio: audio_to_background.1,

        to_background: panic!("background can't send to its self"),
        from_background: panic!("background can't receive from itself"),
    };

    // MainEngine runs on the main thread
    {
        let main_state = Arc::clone(&state);
        let mut main_engine =
            MainEngine::init(main_channels, main_state).unwrap();
        main_engine.run();
    }

    // AudioEngine thread
    let audio_state = Arc::clone(&state);
    let audio_thread = thread::spawn(move || {
        let mut audio_engine =
            AudioEngine::init(audio_channels, audio_state).unwrap();
        audio_engine.run();
    });

    // BackgroundEngine thread
    let background_state = Arc::clone(&state);
    let background_thread = thread::spawn(move || {
        let mut background_engine =
            BackgroundEngine::init(background_channels, background_state)
                .unwrap();
        background_engine.run();
    });

    // Wait for threads
    audio_thread.join().expect("Audio thread panicked");
    background_thread
        .join()
        .expect("Background thread panicked");
}
