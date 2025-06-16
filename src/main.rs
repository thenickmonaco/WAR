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
mod input;
mod lua;
mod message;
mod render;
mod state;

use crate::state::State;
use audio::AudioEngine;
use input::InputEngine;
use lua::LuaEngine;
use render::RenderEngine;
use std::collections::HashMap;
use std::sync::mpsc;
use std::sync::{Arc, RwLock};
use std::thread;

pub fn main() {
    let state = Arc::new(RwLock::new(State::default()));

    let (lua_tx, lua_rx) = mpsc::channel();
    let (input_tx, input_rx) = mpsc::channel();
    let (render_tx, render_rx) = mpsc::channel();
    let (audio_tx, audio_rx) = mpsc::channel();

    let mut senders_map = HashMap::new();
    senders_map.insert("render", render_tx.clone());
    senders_map.insert("input", input_tx.clone());
    senders_map.insert("audio", audio_tx.clone());
    senders_map.insert("lua", lua_tx.clone());

    let senders = Arc::new(senders_map);

    let render_senders = Arc::clone(&senders);
    let state_for_render = Arc::clone(&state);
    let render_handle = thread::spawn(move || {
        let mut render_engine = RenderEngine::init_render(
            render_rx,
            render_senders,
            state_for_render,
        )
        .unwrap();
        render_engine.run();
    });

    let input_senders = Arc::clone(&senders);
    let state_for_input = Arc::clone(&state);
    let input_handle = thread::spawn(move || {
        let input_engine =
            InputEngine::init_input(input_rx, input_senders, state_for_input)
                .unwrap();
        input_engine.run();
    });

    let audio_senders = Arc::clone(&senders);
    let state_for_audio = Arc::clone(&state);
    let audio_handle = thread::spawn(move || {
        let audio_engine =
            AudioEngine::init_audio(audio_rx, audio_senders, state_for_audio)
                .unwrap();
        audio_engine.run();
    });

    // lua is not send
    let lua_senders = Arc::clone(&senders);
    let state_for_lua = Arc::clone(&state);
    let lua_handle = thread::spawn(move || {
        let lua_engine =
            LuaEngine::init_lua(lua_rx, lua_senders, state_for_lua).unwrap();
        lua_engine.run();
    });

    render_handle.join().expect("Render thread panicked");
    input_handle.join().expect("Input thread panicked");
    audio_handle.join().expect("Audio thread panicked");
    lua_handle.join().expect("Lua thread panicked");
}
