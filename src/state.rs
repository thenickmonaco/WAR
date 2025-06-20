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

pub struct Channels {
    pub main_to_audio: (Producer<Message>, Consumer<Message>),
    pub audio_to_main: (Producer<Message>, Consumer<Message>),

    pub main_to_background: (Producer<Message>, Consumer<Message>),
    pub background_to_main: (Producer<Message>, Consumer<Message>),

    pub audio_to_background: (Producer<Message>, Consumer<Message>),
    pub background_to_audio: (Producer<Message>, Consumer<Message>),
}

pub type Producers = Arc<HashMap<EngineType, Mutex<Producer<Message>>>>;

pub trait Engine: Send + 'static {
    fn init(
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

//=============================================================================
// main engine
//=============================================================================

pub trait Subsystem: Send {
    fn init() -> Result<Self, String>
    where
        Self: Sized;
    fn handle_message(&mut self, message: Message);
    fn run(&mut self);
    fn shutdown(&mut self);
}

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

pub trait Renderer: Subsystem {}

pub trait Input: Subsystem {}

pub trait Lua: Subsystem {}

#[derive(Default)]
pub struct MainState {}

//=============================================================================
// audio engine
//=============================================================================

pub trait Audio: Subsystem {}

pub trait Clock: Subsystem {}

#[derive(Default)]
pub struct AudioState {}

//=============================================================================
// background engine
//=============================================================================

pub trait IO: Subsystem {}

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
