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

use crate::data::Vertex;

pub fn init() -> (i32, i32, Vec<Vertex>, Vec<u16>) {
    (
        0,          // col
        0,          // row
        Vec::new(), // cursor_rects_vertices
        Vec::new(), // cursor_rects_indices
    )
}

pub fn tick(
    col: &mut i32,
    row: &mut i32,
    cursor_rects_vertices: &mut Vec<Vertex>,
    cursor_rects_indices: &mut Vec<u16>,
) {
}
