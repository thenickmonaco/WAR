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
use parking_lot::{RwLock, Mutex};
use ringbuf::{Consumer, Producer};
use std::collections::HashMap;
use std::sync::Arc;

//=============================================================================
// engine
//=============================================================================

pub type Producers = Arc<HashMap<EngineType, Mutex<Producer<Message>>>>;

pub trait Engine: Send + 'static {
    fn init(
        consumer: Consumer<Message>,
        producers: Producers,
        state: Arc<State>,
    ) -> Result<Self, String>
    where
        Self: Sized;
    fn handle_message(&mut self);
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
// main thread
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

pub trait RenderSubsystem {}

pub trait InputSubsystem {}

pub trait LuaSubsystem {
    fn execute_script(&mut self, script: &str);
    fn update(&mut self);
}

#[derive(Default)]
pub struct MainState {}

//=============================================================================
// audio thread
//=============================================================================

pub trait AudioProcessor {
    fn process_audio(&mut self, buffer: &mut [f32]);
}

pub trait ClockSubsystem {}

#[derive(Default)]
pub struct AudioState {}

//=============================================================================
// background thread
//=============================================================================

pub trait IOSubsystem {}

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
