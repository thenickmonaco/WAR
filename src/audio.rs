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
// src/audio.rs
//=============================================================================

use crate::message::Message;
use crate::state::{Engine, EngineChannels, EngineType, State, Subsystem};
use std::sync::Arc;

pub struct Audio {
    pub state: Arc<State>,
    pub channels: EngineChannels,
    pub audio: AudioSubsystem,
    pub should_run: bool,
}

impl Engine for Audio {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let audio = AudioSubsystem::init(state.clone())?;

        Ok(Self {
            audio,
            channels,
            state,
            should_run: true,
        })
    }

    fn poll_message(&mut self) {
        while let Some(message) = self.channels.from_heart.pop() {
            self.dispatch_message(message);
        }
        while let Some(message) = self.channels.from_worker.pop() {
            self.dispatch_message(message);
        }
    }

    fn dispatch_message(&mut self, message: Message) {
        match message {
            Message::Render(cmd) => {
                println!("heart received: {:?}", cmd);
            }
            Message::Shutdown => {
                println!("heart shutting down...");
                self.shutdown();
            }
            other => {
                println!("heart got unrelated message: {:?}", other);
            }
        }
    }

    fn send_message(&mut self, engine_type: EngineType, message: Message) {}

    fn run(&mut self) {
        while self.should_run {
            self.audio.run();

            self.poll_message();
        }
    }

    fn shutdown(&mut self) {
        self.should_run = false;

        self.audio.shutdown();
    }
}

impl Audio {}

pub struct AudioSubsystem {
    state: Arc<State>,
}

impl Subsystem for AudioSubsystem {
    fn init(state: Arc<State>) -> Result<Self, String> {
        Ok(Self { state })
    }

    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl AudioSubsystem {}
