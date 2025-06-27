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
// src/main.rs
//=============================================================================

mod input;
mod lua;
mod render;

mod data;

pub fn main() {
    let () = render::init();

    let (
        mut col,
        mut row,
        mut cursor_rects_vertices,
        mut cursor_rects_indices,
    ) = input::init();

    loop {
        input::tick(
            &mut col,
            &mut row,
            &mut cursor_rects_vertices,
            &mut cursor_rects_indices,
        );
        lua::tick();
        render::tick(&cursor_rects_vertices, &cursor_rects_indices);
    }
}
