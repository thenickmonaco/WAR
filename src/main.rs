//=============================================================================
//  src/main.rs
//=============================================================================

mod render;
mod input;
mod font;
mod audio;
mod colors;
mod config;
mod shared;

pub fn main() {
    render::render();
    config::init_lua().expect("lua init failed");
}
