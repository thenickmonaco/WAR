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

use crate::message::Message;
use crate::state::{Engine, EngineChannels, EngineType, State, Subsystem};
use crate::render::RenderSubsystem;
use crate::input::InputSubsystem;
use crate::lua::LuaSubsystem;
use std::sync::Arc;

pub struct Heart {
    pub state: Arc<State>,
    pub channels: EngineChannels,
    pub render: RenderSubsystem,
    pub input: InputSubsystem,
    pub lua: LuaSubsystem,
}

impl Engine for Heart {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let render = RenderSubsystem::init(state.clone())?;
        let input = InputSubsystem::init(state.clone())?;
        let lua = LuaSubsystem::init(state.clone())?;

        Ok(Self {
            render,
            input,
            lua,
            channels,
            state,
        })
    }

    fn poll_message(&mut self) {
        while let Some(message) = self.channels.from_audio.pop() {
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

    fn shutdown(&mut self) {}

    fn run(&mut self) {}
}

impl Heart {}

