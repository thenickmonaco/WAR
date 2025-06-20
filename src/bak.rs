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
// src/bak.rs
//=============================================================================

use crate::message::{send_shutdown, Message};
use crate::state::{
    Engine, EngineType, InputSubsystem, LuaSubsystem, Producers,
    RenderSubsystem, State,
};
use glfw::{Context, WindowHint, WindowMode};
use glow::HasContext;
use ringbuf::Consumer;
use std::collections::HashMap;
use std::sync::Arc;

pub struct MainEngine {
    pub render: Box<dyn RenderSubsystem>,
    pub input: Box<dyn InputSubsystem>,
    pub lua: Box<dyn LuaSubsystem>,
    pub state: Arc<State>,
    pub consumer: Consumer<Message>,
    pub producers: Producers,
}

impl Engine for MainEngine {
    fn init(
        consumer: Consumer<Message>,
        producers: Producers,
        state: Arc<State>,
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
            glow,
            glfw,
            window,
            receiver,
            senders,
            state,
        })
    }

    fn handle_message(&mut self) {
        //while let Ok(message) = self.consumer.pop() {
        //    match message {
        //        Message::Render(cmd) => {
        //            println!("Render received: {:?}", cmd);
        //            // handle command variants
        //        }
        //        Message::Shutdown => {
        //            println!("Render shutting down...");
        //            break;
        //        }
        //        _ => {
        //            println!("Render got unrelated message: {:?}", message);
        //        }
        //    }
        //}
    }

    fn run(&mut self) {
        while !self.window.should_close() {
            self.handle_message();

            self.render();
        }

        send_shutdown(&self.senders);
    }

    fn shutdown(&mut self) {}
}

impl MainEngine {
}

pub struct Render {
    pub glow: glow::Context,
    pub glfw: glfw::Glfw,
    pub window: glfw::Window,
}
