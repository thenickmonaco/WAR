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
// src/heart.rs
//=============================================================================

//=============================================================================
// notes
//
// All shared is located under the Engine, but HeartContext is used to pass
// as a parameter into run functions or init functions
//
// HeartState (local): Owned by the engine, lives as a field in the Heart
// struct (e.g., Option<HeartState>). Used for single-threaded, persistent
// shared.
//
// Arc<State> (shared): Contains RwLock<HeartSharedState> for cross-thread
// communication or shared data.
//
// HeartContext: A reference-based struct that gives subsystems access to the
// local HeartState (and optionally, shared shared if needed).
//
// Make subsystems stateless
//=============================================================================
