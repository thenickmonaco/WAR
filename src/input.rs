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

use crate::data::SolidRects;

pub fn init() -> (i32, i32, SolidRects) {
    (
        0, // col
        0, // row
        SolidRects {
            // cursor_rects
            positions: Vec::new(),
            colors: Vec::new(),
            indices: Vec::new(),
        },
    )
}

pub fn tick(
    glfw: &mut glfw::Glfw,
    events: &mut std::sync::mpsc::Receiver<(f64, glfw::WindowEvent)>,
    cursor_rects: &mut SolidRects,
    col: &mut i32,
    row: &mut i32,
) {
    glfw.poll_events();
}
