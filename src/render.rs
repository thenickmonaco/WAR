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
// src/render.rs
//=============================================================================

use crate::message::RenderCommand;
use crate::state::{GlfwContext, State, Subsystem};
use glfw::{Context, WindowEvent, WindowHint, WindowMode};
use glow::HasContext;
use std::sync::Arc;

pub struct RenderSubsystem {
    pub glow: glow::Context,
    pub glfw: glfw::Glfw,
    pub window: glfw::Window,
    pub events: std::sync::mpsc::Receiver<(f64, WindowEvent)>,
    pub state: Arc<State>,
}

impl<'a> Subsystem<'a> for RenderSubsystem {
    type Command = RenderCommand;
    type InitContext = ();
    type RunContext = ();

    fn init(
        state: Arc<State>,
        context: Self::InitContext,
    ) -> Result<Self, String> {
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
            state,
        })
    }

    fn handle_message(&mut self, cmd: Self::Command) {}

    fn run(&mut self, context: Self::RunContext) {
        unsafe {
            self.glow.clear_color(0.0, 0.0, 0.0, 0.0);
            self.glow.clear(glow::COLOR_BUFFER_BIT);
        }

        self.window.swap_buffers();
    }

    fn shutdown(&mut self) {}
}

impl RenderSubsystem{}
