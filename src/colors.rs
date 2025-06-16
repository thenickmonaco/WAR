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
// src/colors.rs
//=============================================================================

use mlua::{Result};

pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

pub struct Theme {
    pub fg: Color,
    pub bg: Color,
}

pub fn hex_to_rgba(hex: &str) -> Result<Color> {
    let hex = hex.trim_start_matches('#');
    let color = u32::from_str_radix(hex, 16).map_err(mlua::Error::external)?;

    Ok(match hex.len() {
        6 => Color {
            r: u8::try_from((color >> 16) & 0xFF)
                .expect("red channel failure"),
            g: u8::try_from((color >> 8) & 0xFF)
                .expect("green channel failure"),
            b: u8::try_from(color & 0xFF).expect("blue channel failure"),
            a: 255,
        },
        8 => Color {
            r: u8::try_from((color >> 24) & 0xFF)
                .expect("red channel failure"),
            g: u8::try_from((color >> 16) & 0xFF)
                .expect("green channel failure"),
            b: u8::try_from((color >> 8) & 0xFF)
                .expect("blue channel failure"),
            a: u8::try_from(color & 0xFF).expect("alpha channel failure"),
        },
        _ => return Err(mlua::Error::external("hex length invalid")),
    })
}
