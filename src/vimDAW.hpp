#ifndef VIMDAW_H
#define VIMDAW_H

#include <GL/eglew.h>
#include <GL/gl.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <alsa/asoundlib.h>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <ft2build.h>
#include <harfbuzz/hb.h>
#include <hb-ft.h>
#include <iostream>
#include <map>
#include <portaudio.h>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include FT_FREETYPE_H
#include <GL/glew.h>
#include <algorithm>
#include <hb-ft.h>
#include <hb.h>

/*
 * Shared between render_and_input and audio_playback
 */
typedef struct Note {
    int start;
    int end;
    int note;
    int duration() const { return end - start; }

    // ascending order
    bool operator<(const Note &other) const { return start < other.start; }
} Note;

/*****************************************************************************/
/* render_and_input.cpp                                                      */
/*****************************************************************************/
/*****************************************************************************/
/* window                                                                    */
/*****************************************************************************/

extern GLFWwindow *window;
extern const float TARGET_ASPECT;

GLFWmonitor *find_best_monitor_for_16_9();

/*****************************************************************************/
/* colors                                                                    */
/*****************************************************************************/

typedef struct Color {
    GLclampf r;
    GLclampf g;
    GLclampf b;
    GLclampf a;
} Color;

Color color_from_hex(const std::string &hex);

typedef struct Palette {
    const Color note = color_from_hex("#DD1F01FF"); // red: #DD1F01FF
    const Color bg = color_from_hex("#262626FF");   // dark-gray: #262626FF
    const Color vim_status_fg = color_from_hex("#EEEEEEFF"); // white: #EEEEEEFF
    const Color vim_status_bg =
        color_from_hex("#4E4E4EFF"); // light-gray: #4E4E4EFF
    const Color mode_status =
        color_from_hex("#FFAF00FF"); // gold-yellow: #FFAF00FF
    const Color tmux_status_fg =
        color_from_hex("#EEEEEEFF"); // white: #EEEEEEFF
    const Color tmux_status_bg = color_from_hex("#DD1F01FF"); // red: #DD1F01FF
    const Color gutter_bar =
        color_from_hex("#3A3A3AFF"); // dark-gray-2: #3A3A3AFF
    const Color gutter_number =
        color_from_hex("#6A6C6BFF"); // light-gray-2: #6A6C6BFF
    const Color gutter_piano_white = color_from_hex("#EEEEEEFF");
    const Color gutter_piano_black = color_from_hex("#000000FF");
    const Color cursor = color_from_hex("#FFD7AFFF"); // white2: #5F5F5FFF
    const Color cursor_bg =
        color_from_hex("#5F5F5FFF"); // light-gray-3: #5F5F5FFF
} Palette;

extern Palette palette;

/*****************************************************************************/
/* font                                                                      */
/*****************************************************************************/

extern FT_Library ft;
extern FT_Face face;
extern const char *font_path;
extern int font_size_pt;
extern FT_Library bold_ft;
extern FT_Face bold_face;
extern const char *bold_font_path;
extern hb_buffer_t *hb_buffer;
extern hb_font_t *hb_font;

void init_font();

extern const int ATLAS_WIDTH;
extern const int ATLAS_HEIGHT;

extern GLuint atlas_texture;
extern int atlas_x;
extern int atlas_y;
extern int row_height;

struct Glyph_Info {
    GLuint texture_id;
    float u1;
    float v1;
    float u2;
    float v2;
    int width;
    int height;
    int bearing_x;
    int bearing_y;
    int advance;
};

extern std::map<hb_codepoint_t, Glyph_Info> glyph_cache;

void init_texture_atlas();

void load_glyph_to_atlas(hb_codepoint_t glyph_index);

void draw_textured_quad(float x, float y, float w, float h, float u1, float v1,
                        float u2, float v2);

void shape_and_render_text(const char *&text, int x, int y);

/*****************************************************************************/
/* render                                                                    */
/*****************************************************************************/

extern int width;
extern int height;
extern int cell_width;
extern int cell_height;
extern int usable_width;
extern int usable_height;
extern float dpi;
extern float scale_factor;
extern float zoom_step;
extern const int gutter_cols;
extern const int status_rows;
extern const int MIDI_NOTES;
extern const int MAX_INT;
extern bool relative_line_numbers;
extern bool redraw;

void render();

/*****************************************************************************/
/* input                                                                     */
/*****************************************************************************/

extern int col;
extern int row;
extern int relative_col;
extern int relative_row;
extern std::string command_buffer;
extern std::string numeric_prefix;
extern bool leader;
extern bool slave;
extern const uint32_t LEADER_TIMEOUT;
extern uint32_t leader_press_time;

extern int up_limit;
extern int down_limit;
extern int left_limit;
extern int right_limit;

extern int col_step;
extern int row_step;

/*
 * lookahead scheduling (process only milliseconds ahead)
 * for realtime updates (when they add note after playback it actually plays it)
 */
extern std::map<int, std::vector<Note>> buffer_map;

typedef enum Mode {
    NORMAL,
    INSERT,
    VISUAL,
    VISUAL_BLOCK,
    VISUAL_LINE,
    COMMAND,
    SLAVE
} Mode;

extern Mode mode;

typedef enum Movement { UP, DOWN, LEFT, RIGHT } Movement;

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mod);

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/

void loop();

/*****************************************************************************/
/* threads                                                                   */
/*****************************************************************************/

/*
 * Render and handle input
 * SDL3
 */
void render_and_input();

/*
 * Handle audio playback
 * PortAudio + RtMidi
 */
void audio_playback();

#endif
