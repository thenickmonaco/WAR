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
// src/lua.rs
//=============================================================================

use crate::state::State;
use crate::colors::{hex_to_rgba, Theme};
use crate::input::hello_world;
use crate::message::Message;
use mlua::{Lua, Result};
use std::collections::HashMap;
use std::sync::mpsc::{Receiver, Sender};
use std::sync::{Arc, RwLock};

pub struct LuaEngine {
    pub lua: Lua,
    pub state: Arc<RwLock<State>>,
    pub receiver: Receiver<Message>,
    pub senders: Arc<HashMap<&'static str, Sender<Message>>>,
}

impl LuaEngine {
    pub fn init_lua(
        receiver: Receiver<Message>,
        senders: Arc<HashMap<&'static str, Sender<Message>>>,
        state: Arc<RwLock<State>>,
    ) -> Result<Self> {
        let lua: mlua::Lua = unsafe { Lua::unsafe_new() };
        lua.load("jit.on()").exec()?;
        lua.load(&std::fs::read_to_string("src/lua/init.lua")?)
            .exec()?;
        Ok(Self {
            lua,
            receiver,
            senders,
            state,
        })
    }

    pub fn run(&self) {
        while let Ok(message) = self.receiver.recv() {
            match message {
                Message::Lua(cmd) => {
                    println!("Lua received: {:?}", cmd);
                    // handle command variants
                }
                Message::Shutdown => {
                    println!("Lua shutting down...");
                    break;
                }
                _ => {
                    println!("Lua got unrelated message: {:?}", message);
                }
            }
        }
    }

    pub fn load_theme(&self) -> Result<Theme> {
        let theme: mlua::Table = self.lua.globals().get("theme")?;
        let default: mlua::Table = theme.get("default")?;

        let fg: mlua::String = default.get("fg")?;
        let bg: mlua::String = default.get("bg")?;

        Ok(Theme {
            fg: hex_to_rgba(fg.to_str()?)?,
            bg: hex_to_rgba(bg.to_str()?)?,
        })
    }

    pub fn register_rust_functions(&self) -> Result<()> {
        let globals = self.lua.globals();

        let vimdaw_table = match globals.get::<_, mlua::Table>("vimdaw") {
            Ok(table) => table,
            Err(_) => self.lua.create_table()?,
        };

        let hello_world = self.lua.create_function(|_, ()| {
            hello_world();

            Ok(())
        })?;

        vimdaw_table.set("hello_world", hello_world)?;

        self.lua.globals().set("vimdaw", vimdaw_table)?;

        Ok(())
    }
}
