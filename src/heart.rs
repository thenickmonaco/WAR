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
// src/heart.rs
//=============================================================================

//=============================================================================
// notes
//
// All shared_state is located under the Engine, but HeartContext is used to pass
// as a parameter into run functions or init functions
//
// HeartState (local): Owned by the engine, lives as a field in the Heart
// struct (e.g., Option<HeartState>). Used for single-threaded, persistent
// shared_state.
//
// Arc<State> (shared): Contains RwLock<HeartSharedState> for cross-thread
// communication or shared data.
//
// HeartContext: A reference-based struct that gives subsystems access to the
// local HeartState (and optionally, shared shared_state if needed).
//=============================================================================

use crate::input::InputSubsystem;
use crate::lua::LuaSubsystem;
use crate::message::Message;
use crate::render::RenderSubsystem;
use crate::state::{
    Engine, EngineChannels, EngineType, HeartContext, State, Subsystem, HeartState,
};
use std::sync::Arc;

pub struct Heart {
    pub render: RenderSubsystem,
    pub input: InputSubsystem,
    pub lua: LuaSubsystem,
    pub local_state: HeartState,

    pub shared_state: Arc<State>,
    pub should_run: bool,
    pub channels: EngineChannels,
}

impl Engine for Heart {
    fn init(
        channels: EngineChannels,
        shared_state: Arc<State>,
    ) -> Result<Self, String> {
        let render = RenderSubsystem::init(shared_state.clone(), ())?;

        let input = InputSubsystem::init(shared_state.clone(), ())?;

        let lua = LuaSubsystem::init(shared_state.clone(), ())?;

        Ok(Self { render, input, lua, channels, shared_state, should_run: true })
    }

    fn poll_message(&mut self) {
        let audio_messages =
            if let Some(from_audio) = self.channels.from_audio.as_mut() {
                let mut messages = Vec::new();
                while let Some(message) = from_audio.pop() {
                    messages.push(message);
                }
                Some(messages)
            } else {
                None
            };

        let worker_messages =
            if let Some(from_worker) = self.channels.from_worker.as_mut() {
                let mut messages = Vec::new();
                while let Some(message) = from_worker.pop() {
                    messages.push(message);
                }
                Some(messages)
            } else {
                None
            };

        if let Some(messages) = audio_messages {
            for message in messages {
                self.dispatch_message(message);
            }
        }

        if let Some(messages) = worker_messages {
            for message in messages {
                self.dispatch_message(message);
            }
        }
    }

    fn dispatch_message(&mut self, message: Message) {
        match message {
            Message::Render(cmd) => {
                println!("heart received: {:?}", cmd);
                self.render.handle_message(cmd);
            },
            Message::Shutdown => {
                println!("heart shutting down...");
                self.shutdown();
            },
            other => {
                println!("heart got unrelated message: {:?}", other);
            },
        }
    }

    fn send_message(&mut self, engine_type: EngineType, message: Message) {}

    fn run(&mut self) {
        while self.should_run {
            self.poll_message();

            let heart_context = HeartContext {
                glfw: &mut self.render.glfw,
                window: &mut self.render.window,
                events: &mut self.render.events,

            };

            self.input.run(HeartContext);
            self.lua.run(());
            self.render.run(());
        }
    }

    fn shutdown(&mut self) {
        self.should_run = false;

        self.render.shutdown();
        self.input.shutdown();
        self.lua.shutdown();
    }
}

impl Heart {}
