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

use crate::state::{State, Subsystem};
use crate::message::Message;
use std::sync::Arc;
use mlua::Lua as MLua;

pub struct LuaSubsystem {
    pub lua: MLua,
    pub state: Arc<State>,
}

impl Subsystem for LuaSubsystem {
    fn init(state: Arc<State>) -> Result<Self, String> {
        let lua = unsafe { MLua::unsafe_new() };

        lua.load("jit.on()").exec().map_err(|e| e.to_string())?;

        let lua_code = std::fs::read_to_string("src/lua/init.lua")
            .map_err(|e| e.to_string())?;

        lua.load(&lua_code).exec().map_err(|e| e.to_string())?;

        Ok(Self { lua, state })
    }
    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl LuaSubsystem {
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
