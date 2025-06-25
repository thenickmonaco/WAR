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
// src/input.rs
//=============================================================================

use crate::message::InputCommand;
use crate::state::{GlfwContext, State, Subsystem};
use glfw::WindowEvent;
use std::sync::Arc;

pub struct InputSubsystem {
    pub state: Arc<State>,
}

impl<'a> Subsystem<'a> for InputSubsystem {
    type Command = InputCommand;
    type InitContext = ();
    type RunContext = GlfwContext<'a>;

    fn init(
        state: Arc<State>,
        context: Self::InitContext,
    ) -> Result<Self, String> {
        Ok(Self { state })
    }

    fn handle_message(&mut self, cmd: Self::Command) {}

    fn run(&mut self, context: Self::RunContext) {
        self.poll_and_flush(context);
    }

    fn shutdown(&mut self) {}
}

impl InputSubsystem {
    fn poll_and_flush(&mut self, context: GlfwContext) {
        context.glfw.poll_events();

        for (_, event) in glfw::flush_messages(context.events) {
            match event {
                WindowEvent::Key(key, scancode, action, modifiers) => {
                    println!("Key: {:?}, Action: {:?}", key, action);
                },

                WindowEvent::Close => {
                    println!("Window close requested");
                },
                WindowEvent::Size(width, height) => {
                    println!("Window resized: {}x{}", width, height);
                },
                WindowEvent::Pos(x, y) => {
                    println!("Window moved to: ({}, {})", x, y);
                },

                _ => {},
            }
        }
    }
}
