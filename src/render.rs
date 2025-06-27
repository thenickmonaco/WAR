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

use crate::data::Vertex;
use glfw::Context;
use glow::HasContext;

pub fn init() -> (
    glfw::Glfw,
    glfw::Window,
    std::sync::mpsc::Receiver<(f64, glfw::WindowEvent)>,
    glow::Context,
    glow::NativeBuffer,
    glow::NativeBuffer,
    glow::NativeVertexArray,
    glow::NativeBuffer,
    glow::NativeBuffer,
    glow::NativeVertexArray,
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

    //=========================================================================
    // quad
    //=========================================================================
    let (quad_vbo, quad_ebo, quad_vao) = unsafe {
        (
            glow.create_buffer().unwrap(),
            glow.create_buffer().unwrap(),
            glow.create_vertex_array().unwrap(),
        )
    };

    unsafe {
        glow.bind_vertex_array(Some(quad_vao));
        glow.bind_buffer(glow::ARRAY_BUFFER, Some(quad_vbo));
        glow.bind_buffer(glow::ELEMENT_ARRAY_BUFFER, Some(quad_ebo));

        // Set up vertex attribute pointers
        // assuming Vertex { position: [f32; 2], color: u32 }
        // position at location 0, color at location 1
        glow.enable_vertex_attrib_array(0);
        glow.vertex_attrib_pointer_f32(
            0,
            2,
            glow::FLOAT,
            false,
            std::mem::size_of::<Vertex>() as i32,
            0,
        );

        glow.enable_vertex_attrib_array(1);
        glow.vertex_attrib_pointer_f32(
            1,
            4,
            glow::UNSIGNED_BYTE,
            true,
            std::mem::size_of::<Vertex>() as i32,
            8,
        ); // color is u32 (4 bytes), normalized

        glow.bind_vertex_array(None);
        glow.bind_buffer(glow::ARRAY_BUFFER, None);
        glow.bind_buffer(glow::ELEMENT_ARRAY_BUFFER, None);
    }

    //=========================================================================
    // text
    //=========================================================================
    let (text_vbo, text_ebo, text_vao) = unsafe {
        (
            glow.create_buffer().unwrap(),
            glow.create_buffer().unwrap(),
            glow.create_vertex_array().unwrap(),
        )
    };

    unsafe {
        glow.bind_vertex_array(Some(text_vao));
        glow.bind_buffer(glow::ARRAY_BUFFER, Some(text_vbo));
        glow.bind_buffer(glow::ELEMENT_ARRAY_BUFFER, Some(text_ebo));
        // do this later...

        // Set up vertex attribute pointers
        // assuming Vertex { position: [f32; 2], color: u32 }
        // position at location 0, color at location 1
        // glow.enable_vertex_attrib_array(0);
        // glow.vertex_attrib_pointer_f32(
        //     0,
        //     2,
        //     glow::FLOAT,
        //     false,
        //     std::mem::size_of::<Vertex>() as i32,
        //     0,
        // );

        // glow.enable_vertex_attrib_array(1);
        // glow.vertex_attrib_pointer_f32(
        //     1,
        //     4,
        //     glow::UNSIGNED_BYTE,
        //     true,
        //     std::mem::size_of::<Vertex>() as i32,
        //     8,
        // ); // color is u32 (4 bytes), normalized

        glow.bind_vertex_array(None);
        glow.bind_buffer(glow::ARRAY_BUFFER, None);
        glow.bind_buffer(glow::ELEMENT_ARRAY_BUFFER, None);
    }

    (
        glfw, window, events, glow, quad_vbo, quad_ebo, quad_vao, text_vbo,
        text_ebo, text_vao,
    )
}

pub fn tick(
    glow: &mut glow::Context,
    window: &mut glfw::Window,
    cursor_rects_vertices: &Vec<Vertex>,
    cursor_rects_indices: &Vec<u16>,
    quad_vbo: glow::NativeBuffer,
    quad_ebo: glow::NativeBuffer,
    quad_vao: glow::NativeVertexArray,
    text_vbo: glow::NativeBuffer,
    text_ebo: glow::NativeBuffer,
    text_vao: glow::NativeVertexArray,
) {
    unsafe {
        glow.clear_color(0.0, 0.0, 0.0, 0.0);
        glow.clear(glow::COLOR_BUFFER_BIT);

        draw_quad_batch(glow, &cursor_rects_vertices, &cursor_rects_indices, quad_vbo, quad_ebo, quad_vao);
    }

    window.swap_buffers();
}

pub fn draw_quad_batch(
    glow: &mut glow::Context,
    vertices: &[Vertex],
    indices: &[u16],
    quad_vbo: glow::NativeBuffer,
    quad_ebo: glow::NativeBuffer,
    quad_vao: glow::NativeVertexArray,
) {
}
