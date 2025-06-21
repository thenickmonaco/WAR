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
use crate::state::{
    Engine, EngineChannels, State, Subsystem, SubsystemChannels,
};
use glfw::{Context, WindowEvent, WindowHint, WindowMode};
use glow::HasContext;
use mlua::Lua as MLua;
use std::sync::Arc;

pub struct Heart<'a> {
    pub state: Arc<State>,
    pub channels: &'a EngineChannels<'a>,
    pub render: &'a RenderSubsystem<'a>,
    pub input: &'a InputSubsystem<'a>,
    pub lua: &'a LuaSubsystem<'a>,
}

impl<'a> Engine<'a> for Heart<'a> {
    fn init(
        channels: &'a EngineChannels<'a>,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let producers = SubsystemChannels {
            to_heart: &channels.to_heart,
            to_audio: &channels.to_audio,
            to_worker: &channels.to_worker,
        };

        let render = &RenderSubsystem::init(&producers, state.clone())?;
        let input = &InputSubsystem::init(&producers, state.clone())?;
        let lua = &LuaSubsystem::init(&producers, state.clone())?;

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

    fn shutdown(&mut self) {}

    fn run(&mut self) {}
}

impl<'a> Heart<'a> {}

//=============================================================================
// render subsystem
//=============================================================================

pub struct RenderSubsystem<'a> {
    pub glow: glow::Context,
    pub glfw: glfw::Glfw,
    pub window: glfw::Window,
    pub events: std::sync::mpsc::Receiver<(f64, WindowEvent)>,
    pub channels: &'a SubsystemChannels<'a>, 
    pub state: Arc<State>,
}

impl<'a> Subsystem<'a> for RenderSubsystem<'a> {
    fn init(
        channels: &'a SubsystemChannels,
        state: Arc<State>,
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
            channels,
            state,
        })
    }

    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {
        unsafe {
            self.glow.clear_color(0.0, 0.0, 0.0, 0.0);
            self.glow.clear(glow::COLOR_BUFFER_BIT);
        }

        self.window.swap_buffers();
        self.glfw.poll_events();
    }

    fn shutdown(&mut self) {}
}

//=============================================================================
// input subsystem
//=============================================================================

pub struct InputSubsystem<'a> {
    pub producers: &'a SubsystemChannels<'a>,
    pub state: Arc<State>,
}

impl<'a> Subsystem<'a> for InputSubsystem<'a> {
    fn init(
        producers: &'a SubsystemChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        Ok(Self { producers, state })
    }

    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl<'a> InputSubsystem<'a> {}

//=============================================================================
// lua subsystem
//=============================================================================

pub struct LuaSubsystem<'a> {
    pub lua: MLua,
    pub producers: &'a SubsystemChannels<'a>,
    pub state: Arc<State>,
}

impl<'a> Subsystem<'a> for LuaSubsystem<'a> {
    fn init(
        producers: &'a SubsystemChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let lua = unsafe { MLua::unsafe_new() };

        lua.load("jit.on()").exec().map_err(|e| e.to_string())?;

        let lua_code = std::fs::read_to_string("src/lua/init.lua")
            .map_err(|e| e.to_string())?;

        lua.load(&lua_code).exec().map_err(|e| e.to_string())?;

        Ok(Self {
            lua,
            producers,
            state,
        })
    }

    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl LuaSubsystem<'a> {
    //fn load_theme(&self) -> Result<Theme> {
    //    let theme: mlua::Table = self.lua.globals().get("theme")?;
    //    let default: mlua::Table = theme.get("default")?;

    //    let fg: mlua::String = default.get("fg")?;
    //    let bg: mlua::String = default.get("bg")?;

    //    Ok(Theme {
    //        fg: hex_to_rgba(fg.to_str()?)?,
    //        bg: hex_to_rgba(bg.to_str()?)?,
    //    })
    //}

    //fn register_rust_functions(&self) -> Result<()> {
    //    let globals = self.lua.globals();

    //    let vimdaw_table = match globals.get::<_, mlua::Table>("vimdaw") {
    //        Ok(table) => table,
    //        Err(_) => self.lua.create_table()?,
    //    };

    //    let hello_world = self.lua.create_function(|_, ()| {
    //        hello_world();

    //        Ok(())
    //    })?;

    //    vimdaw_table.set("hello_world", hello_world)?;

    //    self.lua.globals().set("vimdaw", vimdaw_table)?;

    //    Ok(())
    //}
}
