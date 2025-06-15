//=============================================================================
//  src/config.rs
//=============================================================================

use mlua::{Lua, Result};

pub fn init_lua() -> Result<()> {
    //=========================================================================
    //  init luaJIT
    //=========================================================================

    let lua = unsafe { Lua::unsafe_new() };

    lua.load("jit.on()").exec()?;

    //=========================================================================
    //  init.lua
    //=========================================================================

    lua.load(&std::fs::read_to_string("config/init.lua")?)
        .exec()?;

    let theme: Theme = load_theme(&lua).expect("load_theme failure");

    println!(
        "theme bg: {}{}{}{}",
        theme.bg.r, theme.bg.g, theme.bg.b, theme.bg.a
    );
    println!(
        "theme fg: {}{}{}{}",
        theme.fg.r, theme.fg.g, theme.fg.b, theme.fg.a
    );

    Ok(())
}

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
        _ => return Err(mlua::Error::external("Hex length invalid")),
    })
}

pub fn load_theme(lua: &Lua) -> Result<Theme> {
    let theme = lua.globals().get::<_, mlua::Table>("theme")?;

    let default = theme.get::<_, mlua::Table>("default")?;

    let fg_lua: mlua::String = default.get("fg")?;
    let bg_lua: mlua::String = default.get("bg")?;

    Ok(Theme {
        fg: hex_to_rgba(fg_lua.to_str()?).expect("hex_to_rgba failure"),
        bg: hex_to_rgba(bg_lua.to_str()?).expect("hex_to_rgba failure"),
    })
}
