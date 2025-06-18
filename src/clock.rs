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
// src/clock.rs
//=============================================================================

use crate::message::{EngineType, Message};
use crate::state::{Engine, State};
use std::collections::HashMap;
use crossbeam::channel::{Receiver, Sender};
use std::sync::Arc;
use std::time::{Duration, Instant};

pub struct ClockEngine {
    receiver: Receiver<Message>,
    senders: Arc<HashMap<EngineType, Sender<Message>>>,
    state: Arc<State>,
    start_time: Instant,
    bpm: f32,
    tick_duration: Duration,
}

impl Engine for ClockEngine {
    fn init(
        receiver: Receiver<Message>,
        senders: Arc<HashMap<EngineType, Sender<Message>>>,
        state: Arc<State>,
    ) -> Result<Self, String> {
        let bpm = 120.0;
        let tick_duration = Duration::from_secs_f32(60.0 / bpm / 24.0); // 24 PPQN (pulses per quarter note)
        Ok(Self {
            receiver,
            senders,
            state,
            start_time: Instant::now(),
            bpm,
            tick_duration,
        })
    }

    fn handle_message(&mut self) {
        while let Ok(msg) = self.receiver.try_recv() {
            match msg {
                Message::Shutdown => {
                    println!("ClockEngine shutting down");
                    // Maybe set a flag to exit run loop
                }
                _ => {}
            }
        }
    }

    fn run(&mut self) {
        //let mut next_tick = Instant::now();
        //loop {
        //    self.handle_message();

        //    let now = Instant::now();
        //    if now >= next_tick {
        //        // broadcast a Tick message or similar
        //        //for sender in self.senders.values() {
        //        //    let _ = sender.send(Message::Tick);
        //        //}
        //        //next_tick += self.tick_duration;
        //    } else {
        //        std::thread::sleep(next_tick - now);
        //    }
        //}
    }

    fn shutdown(&mut self) {
        // cleanup if needed
    }
}
