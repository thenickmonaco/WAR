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
// src/message.rs
//=============================================================================

use crossbeam::channel::Sender;
use std::collections::HashMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum EngineType {
    Audio,
    Input,
    Render,
    Lua
}

#[derive(Debug)]
pub enum Message {
    Input(InputCommand),
    Audio(AudioCommand),
    Render(RenderCommand),
    Lua(LuaCommand),
    State(StateUpdate),
    Shutdown,
}

#[derive(Debug)]
pub enum RenderCommand {}

#[derive(Debug)]
pub enum InputCommand {}

#[derive(Debug)]
pub enum AudioCommand {}

#[derive(Debug)]
pub enum LuaCommand {}

#[derive(Debug)]
pub enum StateUpdate {}

pub fn send_shutdown(senders: &HashMap<EngineType, Sender<Message>>) {
    for sender in senders.values() {
        let _ = sender.send(Message::Shutdown);
    }
}
