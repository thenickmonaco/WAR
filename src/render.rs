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
// src/render.rslfjdks
//=============================================================================

use glfw::{Context, WindowHint, WindowMode};
use glow::HasContext;

pub fn render() {
    let mut glfw =
        glfw::init(glfw::FAIL_ON_ERRORS).expect("glfw::init failure");

    glfw.window_hint(WindowHint::ContextVersionMajor(3));
    glfw.window_hint(WindowHint::ContextVersionMinor(3));
    glfw.window_hint(WindowHint::OpenGlProfile(glfw::OpenGlProfileHint::Core));

    let (mut window, _events) = glfw
        .create_window(1920, 1080, "vimDAW", WindowMode::Windowed)
        .expect("create_window failure");

    window.make_current();
    window.set_key_polling(true);

    let gl = unsafe {
        glow::Context::from_loader_function(|s| {
            window.get_proc_address(s) as *const _
        })
    };

    while !window.should_close() {
        unsafe {
            gl.clear_color(0.0, 0.0, 0.0, 0.0);
            gl.clear(glow::COLOR_BUFFER_BIT);
        }

        window.swap_buffers();
        glfw.poll_events();
    }
}
