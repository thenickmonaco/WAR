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
// src/worker.rs
//=============================================================================

use crate::message::{Message, WorkerCommand};
use crate::state::{EngineChannels, EngineType, State, WorkerContext};
use std::sync::Arc;

pub struct Worker {
    pub state: Arc<State>,
    pub channels: EngineChannels,
    pub worker: WorkerSubsystem,
    pub should_run: bool,
}

impl Worker {
    pub fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let worker = WorkerSubsystem::init(state.clone(), ())?;

        Ok(Self { worker, channels, state, should_run: true })
    }

    pub fn poll_message(&mut self) {
        let audio_messages =
            if let Some(from_audio) = self.channels.from_audio.as_mut() {
                let mut messages = Vec::new();
                while let Some(message) = from_audio.pop() {
                    messages.push(message);
                }
                Some(messages)
            } else {
                None
            };

        let worker_messages =
            if let Some(from_worker) = self.channels.from_worker.as_mut() {
                let mut messages = Vec::new();
                while let Some(message) = from_worker.pop() {
                    messages.push(message);
                }
                Some(messages)
            } else {
                None
            };

        if let Some(messages) = audio_messages {
            for message in messages {
                self.dispatch_message(message);
            }
        }

        if let Some(messages) = worker_messages {
            for message in messages {
                self.dispatch_message(message);
            }
        }
    }

    pub fn dispatch_message(&mut self, message: Message) {
        match message {
            Message::Render(cmd) => {
                println!("heart received: {:?}", cmd);
            },
            Message::Shutdown => {
                println!("heart shutting down...");
                self.shutdown();
            },
            other => {
                println!("heart got unrelated message: {:?}", other);
            },
        }
    }

    pub fn send_message(&mut self, engine_type: EngineType, message: Message) {}

    pub fn shutdown(&mut self) {
        self.should_run = false;

        self.worker.shutdown();
    }

    pub fn run(&mut self) {
        while self.should_run {
            self.worker.run(());

            self.poll_message();
        }
    }
}

pub struct WorkerSubsystem {
    state: Arc<State>,
}

impl WorkerSubsystem {
    pub fn init(
        state: Arc<State>,
        context: WorkerContext,
    ) -> Result<Self, String> {
        Ok(Self { state })
    }

    pub fn handle_message(&mut self, cmd: WorkerCommand) {}

    pub fn run(&mut self, context: WorkerContext) {}

    pub fn shutdown(&mut self) {}
}
