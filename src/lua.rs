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
// src/lua.rs
//=============================================================================

pub fn tick() {}

pub fn hex_to_rgba_u32(hex: &str) -> u32 {
    let hex = hex.trim_start_matches('#');
    let color = u32::from_str_radix(hex, 16).expect("Invalid hex string");

    match hex.len() {
        6 => {
            let r = (color >> 16) & 0xFF;
            let g = (color >> 8) & 0xFF;
            let b = color & 0xFF;
            (r << 24) | (g << 16) | (b << 8) | 0xFF
        },
        8 => {
            let r = (color >> 24) & 0xFF;
            let g = (color >> 16) & 0xFF;
            let b = (color >> 8) & 0xFF;
            let a = color & 0xFF;
            (r << 24) | (g << 16) | (b << 8) | a
        },
        _ => panic!("Invalid hex length, must be 6 or 8"),
    }
}
