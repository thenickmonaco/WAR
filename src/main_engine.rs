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
    Engine, EngineChannels, InputSubsystem, LuaSubsystem, RenderSubsystem,
    State, Subsystem,
};
use glfw::{Context, WindowEvent, WindowHint, WindowMode};
use glow::HasContext;
use std::sync::Arc;

pub struct MainEngine {
    pub state: Arc<State>,

    pub channels: EngineChannels,

    pub render: Box<dyn RenderSubsystem>,
    pub input: Box<dyn InputSubsystem>,
    pub lua: Box<dyn LuaSubsystem>,
}

impl Engine for MainEngine {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let render = Box::new(Render::init()?);
        let input = Box::new(Input::init()?);
        let lua = Box::new(Lua::init()?);

        Ok(Self {
            render,
            input,
            lua,
            channels,
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
    pub window: glfw::Window,
    pub events: std::sync::mpsc::Receiver<(f64, WindowEvent)>,
}

impl Subsystem for Render {
    fn init() -> Result<Self, String> {
        let mut glfw =
            glfw::init(glfw::FAIL_ON_ERRORS).expect("glfw::init failure");

        glfw.window_hint(WindowHint::ContextVersionMajor(3));
        glfw.window_hint(WindowHint::ContextVersionMinor(3));
        glfw.window_hint(WindowHint::OpenGlProfile(
            glfw::OpenGlProfileHint::Core,
        ));

        let (mut window, events) = glfw
            .create_window(1920, 1080, "vimDAW", WindowMode::Windowed)
            .expect("create_window failure");

        window.make_current();
        glfw.set_swap_interval(glfw::SwapInterval::Sync(1));
        window.set_key_polling(true);

        let glow = unsafe {
            glow::Context::from_loader_function(|s| {
                window.get_proc_address(s) as *const _
            })
        };

        Ok(Self {
            glow,
            glfw,
            window,
            events,
        })
    }

    fn handle_message(message: Message) {}

    fn run(&mut self) {
        unsafe {
            self.glow.clear_color(0.0, 0.0, 0.0, 0.0);
            self.glow.clear(glow::COLOR_BUFFER_BIT);
        }

        self.window.swap_buffers();
        self.glfw.poll_events();
    }

    fn shutdown() {}
}

pub struct Input {}

impl Subsystem for Input {
    fn init() -> Result<Self, String> {}

    fn handle_message(message: Message) {}

    fn run(&mut self) {}

    fn shutdown() {}
}

impl Input {}

pub struct Lua {}

impl Subsystem for Lua {
    fn init() -> Result<Self, String> {}

    fn handle_message(message: Message) {}

    fn run(&mut self) {}

    fn shutdown() {}
}

impl Lua {}
