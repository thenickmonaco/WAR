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
mod colors;
mod font;
mod heart;
mod message;
mod state;
mod worker;
mod render;
mod input;
mod lua;

use crate::audio::Audio;
use crate::heart::Heart;
use crate::message::Message;
use crate::state::{Engine, EngineChannels, State};
use crate::worker::Worker;
use ringbuf::RingBuffer;
use std::sync::Arc;
use std::thread;

fn make_ring() -> (ringbuf::Producer<Message>, ringbuf::Consumer<Message>) {
    // Adjust buffer size as needed
    RingBuffer::<Message>::new(1024).split()
}

pub fn main() {
    let state = Arc::new(State::default());

    let heart_to_audio = make_ring();
    let audio_to_heart = make_ring();

    let heart_to_worker = make_ring();
    let worker_to_heart = make_ring();

    let audio_to_worker = make_ring();
    let worker_to_audio = make_ring();

    let heart_channels = EngineChannels {
        to_heart: panic!("heart can't send to its self"),
        from_heart: panic!("heart can't receive from itself"),

        to_audio: heart_to_audio.0,
        from_audio: audio_to_heart.1,

        to_worker: heart_to_worker.0,
        from_worker: worker_to_heart.1,
    };

    let audio_channels = EngineChannels {
        to_heart: audio_to_heart.0,
        from_heart: heart_to_audio.1,

        to_audio: panic!("audio can't send to its self"),
        from_audio: panic!("audio can't receive from itself"),

        to_worker: audio_to_worker.0,
        from_worker: worker_to_audio.1,
    };

    let worker_channels = EngineChannels {
        to_heart: worker_to_heart.0,
        from_heart: heart_to_worker.1,

        to_audio: worker_to_audio.0,
        from_audio: audio_to_worker.1,

        to_worker: panic!("worker can't send to its self"),
        from_worker: panic!("worker can't receive from itself"),
    };

    // HeartEngine runs on the heart thread
    {
        let heart_state = Arc::clone(&state);
        let mut heart_engine =
            Heart::init(heart_channels, heart_state).unwrap();
        heart_engine.run();
    }

    // AudioEngine thread
    let audio_state = Arc::clone(&state);
    let audio_thread = thread::spawn(move || {
        let mut audio_engine =
            Audio::init(audio_channels, audio_state).unwrap();
        audio_engine.run();
    });

    // WorkerEngine thread
    let worker_state = Arc::clone(&state);
    let worker_thread = thread::spawn(move || {
        let mut worker_engine =
            Worker::init(worker_channels, worker_state).unwrap();
        worker_engine.run();
    });

    // Wait for threads
    audio_thread.join().expect("Audio thread panicked");
    worker_thread.join().expect("Worker thread panicked");
}
