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

use crate::data::{CursorRect, InputState, RenderState};
use glfw::Context;
use glow::HasContext;

pub fn init() -> RenderState {
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

    RenderState { glfw, window, events, glow }
}

pub fn tick(render_state: &mut RenderState, input_state: &mut InputState) {
    let glow = &mut render_state.glow;
    let window = &mut render_state.window;
    let cursor_rects = &input_state.cursor_rects;

    unsafe {
        glow.clear_color(0.0, 0.0, 0.0, 0.0);
        glow.clear(glow::COLOR_BUFFER_BIT);

        draw_cursor(glow, cursor_rects);
    }

    window.swap_buffers();
}

pub fn draw_cursor(glow: &mut glow::Context, cursor_rects: &Vec<CursorRect>) {}
