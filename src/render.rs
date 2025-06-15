//=============================================================================
//  src/render.rs
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
