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
// src/render.rs
//=============================================================================

use crate::data::SolidRects;
use glfw::Context;
use glow::HasContext;

pub fn init() -> (
    glfw::Glfw,
    glfw::Window,
    std::sync::mpsc::Receiver<(f64, glfw::WindowEvent)>,
    glow::Context,
) {
    let mut glfw =
        glfw::init(glfw::FAIL_ON_ERRORS).expect("glfw::init failure");

    glfw.window_hint(glfw::WindowHint::ContextVersionMajor(3));
    glfw.window_hint(glfw::WindowHint::ContextVersionMinor(3));
    glfw.window_hint(glfw::WindowHint::OpenGlProfile(
        glfw::OpenGlProfileHint::Core,
    ));

    let (mut window, events) = glfw
        .create_window(1920, 1080, "vimDAW", glfw::WindowMode::Windowed)
        .expect("create_window failure");

    window.make_current();
    glfw.set_swap_interval(glfw::SwapInterval::Sync(1));
    window.set_key_polling(true);

    window.maximize();

    let glow = unsafe {
        glow::Context::from_loader_function(|s| {
            window.get_proc_address(s) as *const _
        })
    };

    (glfw, window, events, glow)
}

pub fn tick(
    glow: &mut glow::Context,
    window: &mut glfw::Window,
    cursor_rects: &SolidRects,
) {
    unsafe {
        glow.clear_color(0.0, 0.0, 0.0, 0.0);
        glow.clear(glow::COLOR_BUFFER_BIT);

        // draw solid, same color cursor boxes
        draw_solid_rects(glow, cursor_rects);
    }

    window.swap_buffers();
}

pub fn draw_solid_rects(glow: &mut glow::Context, solid_rects: &SolidRects) {
    let mut positions: Vec<[f32; 2]> = Vec::new(); // x, y
    let mut indices: Vec<u16> = Vec::new();
    let mut index_offset = 0;

    for i in 0..solid_rects.x.len() {
        let x = solid_rects.x[i];
        let y = solid_rects.y[i];
        let w = solid_rects.width[i];
        let h = solid_rects.height[i];

        // Add 4 positions per quad
        positions.extend_from_slice(&[
            [x, y],         // v0: top-left
            [x + w, y],     // v1: top-right
            [x, y + h],     // v2: bottom-left
            [x + w, y + h], // v3: bottom-right
        ]);

        // Add 6 indices to make 2 triangles
        indices.extend_from_slice(&[
            index_offset,
            index_offset + 1,
            index_offset + 2, // first triangle
            index_offset + 1,
            index_offset + 3,
            index_offset + 2, // second triangle
        ]);

        index_offset += 4;
    }

    // Now pass `positions` and `indices` to a function that uploads to GPU
    draw_quad_batch(glow, &positions, &indices, &solid_rects.color);
}

pub fn draw_quad_batch(
    glow: &mut glow::Context,
    positions: &Vec<[f32; 2]>,
    indices: &Vec<u16>,
    color: &Vec<u32>,
) {
}
