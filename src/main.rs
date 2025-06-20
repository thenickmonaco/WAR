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
use crate::state::{Channels, Engine, EngineType, Producers, State};
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
    let main_to_audio = make_ring();
    let audio_to_main = make_ring();

    let main_to_background = make_ring();
    let background_to_main = make_ring();

    let audio_to_background = make_ring();
    let background_to_audio = make_ring();

    let channels = Channels {
        main_to_audio,
        audio_to_main,
        main_to_background,
        background_to_main,
        audio_to_background,
        background_to_audio,
    };

    let state = Arc::new(State::default());

    // Create a ring buffer for each engine
    let (main_producer, main_consumer) = make_ring();
    let (audio_producer, audio_consumer) = make_ring();
    let (background_producer, background_consumer) = make_ring();

    // Shared map of producers
    let mut producers_map = HashMap::new();
    producers_map.insert(EngineType::Main, Mutex::new(main_producer));
    producers_map.insert(EngineType::Audio, Mutex::new(audio_producer));
    producers_map
        .insert(EngineType::Background, Mutex::new(background_producer));

    let producers: Producers = Arc::new(producers_map);

    // MainEngine runs on the main thread
    {
        let main_state = Arc::clone(&state);
        let main_producers_clone = Arc::clone(&producers);
        let mut main_engine =
            MainEngine::init(main_consumer, main_producers_clone, main_state)
                .unwrap();
        main_engine.run();
    }

    // AudioEngine thread
    let audio_state = Arc::clone(&state);
    let audio_producers_clone = Arc::clone(&producers);
    let audio_thread = thread::spawn(move || {
        let mut audio_engine = AudioEngine::init(
            audio_consumer,
            audio_producers_clone,
            audio_state,
        )
        .unwrap();
        audio_engine.run();
    });

    // BackgroundEngine thread
    let background_state = Arc::clone(&state);
    let background_producer_clone = Arc::clone(&producers);
    let background_thread = thread::spawn(move || {
        let mut background_engine = BackgroundEngine::init(
            background_consumer,
            background_producer_clone,
            background_state,
        )
        .unwrap();
        background_engine.run();
    });

    // Wait for threads
    audio_thread.join().expect("Audio thread panicked");
    background_thread
        .join()
        .expect("Background thread panicked");
}
