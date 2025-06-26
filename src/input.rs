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

use crate::data::{CursorRect, InputState, RenderState};

pub fn init() -> InputState {
    let col = 0;
    let row = 0;

    let initial_cursor_rect =
        CursorRect { x: 0.0, y: 0.0, width: 0.0, height: 0.0 };

    let cursor_rects = vec![initial_cursor_rect];

    InputState { col, row, cursor_rects }
}

pub fn tick(render_state: &mut RenderState) {
    let glfw = &mut render_state.glfw;

    glfw.poll_events();
}
