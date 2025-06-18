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

use crate::message::{Message, EngineType};
use crate::state::{Engine, State};
use std::collections::HashMap;
use crossbeam::channel::{Receiver, Sender};
use std::sync::Arc;

pub struct InputEngine {
    pub state: Arc<State>,
    pub receiver: Receiver<Message>,
    pub senders: Arc<HashMap<EngineType, Sender<Message>>>,
}

impl Engine for InputEngine {
    fn init(
        receiver: Receiver<Message>,
        senders: Arc<HashMap<EngineType, Sender<Message>>>,
        state: Arc<State>,
    ) -> Result<Self, String> {
        Ok(Self {
            receiver,
            senders,
            state,
        })
    }

    fn handle_message(&mut self) {
        while let Ok(message) = self.receiver.recv() {
            match message {
                Message::Input(cmd) => {
                    println!("Input received: {:?}", cmd);
                    // handle command variants
                }
                Message::Shutdown => {
                    println!("Input shutting down...");
                    break;
                }
                _ => {
                    println!("Input got unrelated message: {:?}", message);
                }
            }
        }
    }

    fn run(&mut self) {}

    fn shutdown(&mut self) {}
}

impl InputEngine {}

pub fn hello_world() {
    println!("hello world")
}

pub fn up(count: u32) {
    for _ in 0..count {
        // move up one line
    }
}

pub fn down(count: u32) {
    for _ in 0..count {
        // move up one line
    }
}

pub fn left(count: u32) {
    for _ in 0..count {
        // move up one line
    }
}

pub fn right(count: u32) {
    for _ in 0..count {
        // move up one line
    }
}
