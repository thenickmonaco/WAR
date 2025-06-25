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
use parking_lot::RwLock;
use ringbuf::{Consumer, Producer};
use std::sync::Arc;

pub struct EngineChannels {
    pub to_heart: Option<Producer<Message>>,
    pub from_heart: Option<Consumer<Message>>,
    pub to_audio: Option<Producer<Message>>,
    pub from_audio: Option<Consumer<Message>>,
    pub to_worker: Option<Producer<Message>>,
    pub from_worker: Option<Consumer<Message>>,
}

pub struct SubsystemChannels {
    pub to_heart: Producer<Message>,
    pub to_audio: Producer<Message>,
    pub to_worker: Producer<Message>,
}

pub trait Engine {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String>
    where
        Self: Sized;
    fn poll_message(&mut self);
    fn dispatch_message(&mut self, message: Message);
    fn send_message(&mut self, engine_type: EngineType, message: Message);
    fn run(&mut self);
    fn shutdown(&mut self);
}

pub trait Subsystem<'a> {
    type Command;
    type InitContext;
    type RunContext;

    fn init(
        state: Arc<State>,
        context: Self::InitContext,
    ) -> Result<Self, String>
    where
        Self: Sized;
    fn handle_message(&mut self, cmd: Self::Command);
    fn run(&mut self, context: Self::RunContext);
    fn shutdown(&mut self);
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum EngineType {
    Heart,
    Audio,
    Worker,
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

pub struct GlfwContext<'a> {
    pub glfw: &'a mut glfw::Glfw,
    pub window: &'a mut glfw::Window,
    pub events: &'a mut std::sync::mpsc::Receiver<(f64, glfw::WindowEvent)>,
}

#[derive(Default)]
pub struct HeartState {}

#[derive(Default)]
pub struct AudioState {}

#[derive(Default)]
pub struct WorkerState {}

#[derive(Default)]
pub struct State {
    pub heart: RwLock<HeartState>,
    pub audio: RwLock<AudioState>,
    pub worker: RwLock<WorkerState>,
}
