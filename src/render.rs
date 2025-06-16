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

use glfw::{Context, WindowHint, WindowMode};
use glow::HasContext;

use crate::message::Message;
use crate::state::State;
use std::collections::HashMap;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, RwLock};

pub struct RenderEngine {
    pub _events: Receiver<(f64, glfw::WindowEvent)>,
    pub glow: glow::Context,
    pub glfw: glfw::Glfw,
    pub window: glfw::Window,
    pub state: Arc<RwLock<State>>,
    pub receiver: Receiver<Message>,
    pub senders: Arc<HashMap<&'static str, Sender<Message>>>,
}

impl RenderEngine {
    pub fn init_render(
        receiver: Receiver<Message>,
        senders: Arc<HashMap<&'static str, Sender<Message>>>,
        state: Arc<RwLock<State>>,
    ) -> Result<Self, String> {
        let mut glfw =
            glfw::init(glfw::FAIL_ON_ERRORS).expect("glfw::init failure");

        glfw.window_hint(WindowHint::ContextVersionMajor(3));
        glfw.window_hint(WindowHint::ContextVersionMinor(3));
        glfw.window_hint(WindowHint::OpenGlProfile(
            glfw::OpenGlProfileHint::Core,
        ));

        let (mut window, _events) = glfw
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
            _events,
            glow,
            glfw,
            window,
            receiver,

            senders,
            state,
        })
    }

    pub fn send_shutdown(&mut self) {
        for sender in self.senders.values() {
            let _ = sender.send(Message::Shutdown);
        }
    }

    pub fn handle_message(&mut self) -> bool {
        while let Ok(message) = self.receiver.try_recv() {
            match message {
                Message::Input(cmd) => {
                    println!("Render received: {:?}", cmd);
                    // handle command variants
                }
                Message::Shutdown => {
                    println!("Render shutting down...");
                    return true;
                }
                _ => {
                    println!("Render got unrelated message: {:?}", message);
                }
            }
        }
        false
    }

    pub fn render(&mut self) {
        unsafe {
            self.glow.clear_color(0.0, 0.0, 0.0, 0.0);
            self.glow.clear(glow::COLOR_BUFFER_BIT);
        }

        self.window.swap_buffers();
        self.glfw.poll_events();
    }

    pub fn run(&mut self) {
        while !self.window.should_close() {
            if self.handle_message() {
                break;
            }

            self.render();
        }

        self.send_shutdown();
    }
}
