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
// src/main_engine.rs
//=============================================================================

use crate::message::{send_shutdown, Message};
use crate::state::{
    Engine, InputSubsystem, LuaSubsystem, Producers, RenderSubsystem, State,
};
use glfw::{Context, Receiver, Window, WindowEvent};
use glow::HasContext;
use ringbuf::{Consumer, Producer};
use std::sync::Arc;

pub struct MainEngine {
    pub state: Arc<State>,

    pub to_audio: Producer<Message>,
    pub from_audio: Consumer<Message>,

    pub to_background: Producer<Message>,
    pub from_background: Consumer<Message>,

    pub render: Box<dyn RenderSubsystem>,
    pub input: Box<dyn InputSubsystem>,
    pub lua: Box<dyn LuaSubsystem>,
}

impl Engine for MainEngine {
    fn init(
         
        state: Arc<State>,
    ) -> Result<Self, String> {
        Ok(Self {
            state,
        })
    }

    fn poll_message(&mut self) {
        while let Some(message) = self.consumer.pop() {
            self.dispatch_message(message);
        }
    }

    fn dispatch_message(&mut self, message: Message) {
        match message {
            Message::Render(cmd) => {
                println!("Render received: {:?}", cmd);
                // Handle render command
            }
            Message::Shutdown => {
                println!("Render shutting down...");
                self.shutdown();
            }
            other => {
                println!("Render got unrelated message: {:?}", other);
            }
        }
    }

    fn shutdown(&mut self) {}

    fn run(&mut self) {
        while !self.window.should_close() {}
        send_shutdown(&self.producers);
    }
}

impl MainEngine {}

pub struct Render {
    pub glow: glow::Context,
    pub glfw: glfw::Glfw,
    pub window: glfw::PWindow,
    pub events: glfw::Receiver<(f64, glfw::WindowEvent)>,
}

impl RenderSubsystem for Render {
    fn run(&mut self) {
        unsafe {
            self.glow.clear_color(0.0, 0.0, 0.0, 0.0);
            self.glow.clear(glow::COLOR_BUFFER_BIT);
        }

        self.window.swap_buffers();
        self.glfw.poll_events();
    }
}

pub struct Input {
    // fields
}

impl InputSubsystem for Input {
    // fields
}
