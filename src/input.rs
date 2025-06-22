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

use crate::state::{State, Subsystem};
use crate::message::Message;
use std::sync::Arc;
use glfw::{Context, WindowEvent, WindowHint, WindowMode};
use glow::HasContext;
use mlua::Lua as MLua;

pub struct InputSubsystem {
    pub state: Arc<State>,
}

impl Subsystem for InputSubsystem {
    fn init(state: Arc<State>) -> Result<Self, String> {
        Ok(Self { state })
    }

    fn handle_message(&mut self, message: Message) {}

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl InputSubsystem {}

