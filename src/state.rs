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
// src/state.rs
//=============================================================================

use crate::message::Message;
use parking_lot::{Mutex, RwLock};
use ringbuf::{Consumer, Producer};
use std::collections::HashMap;
use std::sync::Arc;

//=============================================================================
// engine
//=============================================================================

pub struct EngineChannels {
    pub to_main: Producer<Message>,
    pub from_main: Consumer<Message>,
    pub to_audio: Producer<Message>,
    pub from_audio: Consumer<Message>,
    pub to_background: Producer<Message>,
    pub from_background: Consumer<Message>,
}

pub trait Engine: Send + 'static {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String>
    where
        Self: Sized;
    fn poll_message(&mut self);
    fn dispatch_message(&mut self, message: Message);
    fn run(&mut self);
    fn shutdown(&mut self);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum EngineType {
    Main,
    Audio,
    Background,
}

pub trait Subsystem: Send {
    fn init() -> Result<Self, String>
    where
        Self: Sized;
    fn handle_message(&mut self, message: Message);
    fn run(&mut self);
    fn shutdown(&mut self);
}

//=============================================================================
// main engine
//=============================================================================

pub trait Window {
    fn id(&self) -> WindowType;
    fn render(&mut self);
    fn handle_input(&mut self, message: &Message);
    fn update(&mut self, dt: f32);
    fn is_active(&self) -> bool;
}

#[derive(Debug, PartialEq, Eq, Hash)]
pub enum WindowType {
    PianoRoll,
    SamplingInterface,
    Mixer,
}

#[derive(Default)]
pub struct MainState {}

//=============================================================================
// audio engine
//=============================================================================

#[derive(Default)]
pub struct AudioState {}

//=============================================================================
// background engine
//=============================================================================

#[derive(Default)]
pub struct BackgroundState {}

//=============================================================================
// state struct
//=============================================================================

#[derive(Default)]
pub struct State {
    pub main: RwLock<MainState>,
    pub audio: RwLock<AudioState>,
    pub background: RwLock<BackgroundState>,
}
