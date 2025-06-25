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
use crate::state::{Engine, EngineChannels, EngineType, State, Subsystem};
use std::sync::Arc;

pub struct Worker {
    pub state: Arc<State>,
    pub channels: EngineChannels,
    pub worker: WorkerSubsystem,
    pub should_run: bool,
}

impl Engine for Worker {
    fn init(
        channels: EngineChannels,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let worker = WorkerSubsystem::init(state.clone(), ())?;

        Ok(Self {
            worker,
            channels,
            state,
            should_run: true,
        })
    }

    fn poll_message(&mut self) {
        // Collect all messages from from_audio first
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

        // Collect all messages from from_worker first
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

        // Now process messages without holding any mutable borrows on channels
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

    fn dispatch_message(&mut self, message: Message) {
        match message {
            Message::Render(cmd) => {
                println!("heart received: {:?}", cmd);
            }
            Message::Shutdown => {
                println!("heart shutting down...");
                self.shutdown();
            }
            other => {
                println!("heart got unrelated message: {:?}", other);
            }
        }
    }

    fn send_message(&mut self, engine_type: EngineType, message: Message) {}

    fn shutdown(&mut self) {
        self.should_run = false;

        self.worker.shutdown();
    }

    fn run(&mut self) {
        while self.should_run {
            self.worker.run(());

            self.poll_message();
        }
    }
}

impl Worker {}

pub struct WorkerSubsystem {
    state: Arc<State>,
}

impl<'a> Subsystem<'a> for WorkerSubsystem {
    type Command = WorkerCommand;
    type InitContext = ();
    type RunContext = ();

    fn init(
        state: Arc<State>,
        context: Self::InitContext,
    ) -> Result<Self, String> {
        Ok(Self { state })
    }

    fn handle_message(&mut self, cmd: Self::Command) {}

    fn run(&mut self, context: Self::RunContext) {}

    fn shutdown(&mut self) {}
}

impl WorkerSubsystem {}
