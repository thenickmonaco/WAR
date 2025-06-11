#include "vimDAW.hpp"

/*****************************************************************************/
/* window                                                                    */
/*****************************************************************************/

GLFWwindow *window;
const float TARGET_ASPECT = 16.0f / 9.0f;

GLFWmonitor *find_best_monitor_for_16_9() {
    int count;
    GLFWmonitor **monitors = glfwGetMonitors(&count);
    if (!monitors || count == 0) {
        return nullptr;
    }

    GLFWmonitor *best = nullptr;
    float closest_diff = 1000.0f;

    for (int i = 0; i < count; i++) {
        const GLFWvidmode *mode = glfwGetVideoMode(monitors[i]);
        if (!mode) {
            continue;
        }

        float aspect = static_cast<float>(mode->width) / mode->height;
        float diff = std::fabs(aspect - TARGET_ASPECT);

        if (diff < closest_diff) {
            closest_diff = diff;
            best = monitors[i];
        }
    }
    return best;
}

/*****************************************************************************/
/* colors                                                                    */
/*****************************************************************************/

Color color_from_hex(const std::string &hex) {
    if (hex.empty() || hex[0] != '#' ||
        (hex.length() != 7 && hex.length() != 9)) {
        throw std::invalid_argument("Invalid hex color format");
    }

    unsigned int rgba = 0;
    std::istringstream iss(hex.substr(1)); // Parse hex digits after '#'
    iss >> std::hex >> rgba;

    unsigned int r, g, b, a;
    if (hex.length() == 7) { // #RRGGBB
        r = (rgba >> 16) & 0xFF;
        g = (rgba >> 8) & 0xFF;
        b = rgba & 0xFF;
        a = 255;
    } else { // #RRGGBBAA
        r = (rgba >> 24) & 0xFF;
        g = (rgba >> 16) & 0xFF;
        b = (rgba >> 8) & 0xFF;
        a = rgba & 0xFF;
    }

    return Color{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

Palette palette;

/*****************************************************************************/
/* font                                                                      */
/*****************************************************************************/

FT_Library ft;
FT_Face face;
const char *font_path = "assets/fonts/FreeMono.otf";
int font_size_pt = 25;
FT_Library bold_ft;
FT_Face bold_face;
const char *bold_font_path = "assets/fonts/FreeMonoBold.otf";

hb_buffer_t *hb_buffer;
hb_font_t *hb_font;

void init_font() {
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "Could not init FreeType Library\n";
        return;
    }

    if (FT_New_Face(ft, font_path, 0, &face)) {
        std::cerr << "Failed to load font\n";
        return;
    }

    FT_Set_Char_Size(face, 0, font_size_pt * 64, dpi, dpi);
    FT_Load_Char(face, '0', FT_LOAD_DEFAULT);

    cell_width = (face->glyph->advance.x / 64.0f) * scale_factor;
    cell_height = (face->size->metrics.height / 64.0f) * scale_factor;

    hb_buffer = hb_buffer_create();
    hb_font = hb_ft_font_create(face, nullptr);
}

const int ATLAS_WIDTH = 1024;
const int ATLAS_HEIGHT = 1024;

GLuint atlas_texture;
int atlas_x = 0;
int atlas_y = 0;
int row_height = 0;

std::map<hb_codepoint_t, Glyph_Info> glyph_cache;

void init_texture_atlas() {
    glGenTextures(1, &atlas_texture);
    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_RED,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void load_glyph_to_atlas(hb_codepoint_t glyph_index) {
    if (glyph_cache.count(glyph_index)) {
        return;
    }

    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
        return;
    }
    FT_Bitmap &bitmap = face->glyph->bitmap;

    // Line wrap
    if (atlas_x + bitmap.width >= ATLAS_WIDTH) {
        atlas_x = 0;
        atlas_y += row_height;
        row_height = 0;
    }

    // Upload glyph to atlas
    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, atlas_x, atlas_y, bitmap.width,
                    bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, bitmap.buffer);

    Glyph_Info info;
    info.u1 = (float)atlas_x / ATLAS_WIDTH;
    info.v1 = (float)atlas_y / ATLAS_HEIGHT;
    info.u2 = (float)(atlas_x + bitmap.width) / ATLAS_WIDTH;
    info.v2 = (float)(atlas_y + bitmap.rows) / ATLAS_HEIGHT;
    info.width = bitmap.width;
    info.height = bitmap.rows;
    info.bearing_x = face->glyph->bitmap_left;
    info.bearing_y = face->glyph->bitmap_top;
    info.advance = face->glyph->advance.x >> 6;

    glyph_cache[glyph_index] = info;

    atlas_x += bitmap.width + 1;
    row_height = std::max(row_height, (int)bitmap.rows);
}

void draw_textured_quad(float x, float y, float w, float h, float u1, float v1,
                        float u2, float v2) {
    static GLuint VAO = 0, VBO = 0;

    if (VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL,
                     GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              (void *)(2 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    float vertices[] = {
        x, y + h, u1, v2, x,     y, u1, v1, x + w, y,     u2, v1,
        x, y + h, u1, v2, x + w, y, u2, v1, x + w, y + h, u2, v2,
    };

    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void shape_and_render_text(const char *&text, int x, int y) {
    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(hb_font, buf, nullptr, 0);

    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos =
        hb_buffer_get_glyph_positions(buf, &glyph_count);

    glBindTexture(GL_TEXTURE_2D, atlas_texture);

    int pen_x = x;
    int pen_y = y;

    for (unsigned int i = 0; i < glyph_count; i++) {
        hb_codepoint_t gid = glyph_info[i].codepoint;

        load_glyph_to_atlas(gid);
        const Glyph_Info &g = glyph_cache[gid];

        float xpos = pen_x + (glyph_pos[i].x_offset >> 6) + g.bearing_x;
        float ypos = pen_y - (glyph_pos[i].y_offset >> 6) - g.bearing_y;

        float w = g.width;
        float h = g.height;

        glUniform1i(glGetUniformLocation(shaderProgram, "text"),
                    0); // texture unit
        glUniform4f(glGetUniformLocation(shaderProgram, "textColor"), 1.0, 1.0,
                    1.0, 1.0); // white text

        draw_textured_quad(xpos, ypos, w, h, g.u1, g.v1, g.u2, g.v2);

        pen_x += glyph_pos[i].x_advance >> 6;
        pen_y += glyph_pos[i].y_advance >> 6;
    }

    hb_buffer_destroy(buf);
}

/*****************************************************************************/
/* render                                                                    */
/*****************************************************************************/

int width;
int height;
int grid_width;
int grid_height;
int cell_width;
int cell_height;
int usable_width;
int usable_height;
float dpi = 100.0f; // default 100.0f
float scale_factor = 1;
float zoom_step = 0.1;
const int gutter_cols = 8;
const int status_rows = 4;
const int MIDI_NOTES = 128;
const int MAX_INT = std::numeric_limits<int>::max();
bool relative_line_numbers = false;
bool redraw = true;

void render() {
    Color bg = palette.bg;
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT);

    const char *hello = "Hello, World!";
    int start_x = 100;
    int start_y = 200;
    shape_and_render_text(hello, start_x, start_y);

    glfwSwapBuffers(window);
}

GLuint create_text_shader();

/*****************************************************************************/
/* input                                                                     */
/*****************************************************************************/

int col = 0;
int row = 60;
int relative_col;
int relative_row;
std::string command_buffer = "";
std::string numeric_prefix = "1";
bool leader = false;
bool slave = false;
const uint32_t LEADER_TIMEOUT = 500;
uint32_t leader_press_time;

int up_limit = MIDI_NOTES - 1;
int down_limit = 0;
int left_limit = 0;
int right_limit = MAX_INT - 1;

int col_step = 13;
int row_step = 13;

/*
 * lookahead scheduling (process only milliseconds ahead)
 * for realtime updates (when they add note after playback it actually plays it)
 */
std::map<int, std::vector<Note>> buffer_map;

Mode mode = NORMAL;

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mod) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
        case GLFW_KEY_K:
            if (mod & GLFW_MOD_CONTROL) {
                row = std::min(row + row_step, up_limit);
            } else {
                row = std::min(row + 1, up_limit);
            }
            break;
        case GLFW_KEY_J:
            if (mod & GLFW_MOD_CONTROL) {
                row = std::max(row - row_step, down_limit);
            } else {
                row = std::max(row - 1, down_limit);
            }
            break;
        case GLFW_KEY_H:
            if (mod & GLFW_MOD_CONTROL) {
                col = std::max(col - col_step, left_limit);
            } else {
                col = std::max(col - 1, left_limit);
            }
            break;
        case GLFW_KEY_L:
            if (mod & GLFW_MOD_CONTROL) {
                col = std::min(col + col_step, right_limit);
            } else {
                col = std::min(col + 1, right_limit);
            }
            break;
        }
    }
    redraw = true;
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/

void loop() {
    while (!glfwWindowShouldClose(window)) {
        glfwWaitEvents();
        if (redraw) {
            render();
            redraw = false;
        }
    }
}

void render_and_input() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return;
    }

    GLFWmonitor *monitor = find_best_monitor_for_16_9();
    if (!monitor) {
        std::cerr << "No monitors found\n";
        glfwTerminate();
        return;
    }

    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    width = mode->width;
    height = mode->height;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "vimDAW", monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE; // Required in core profile
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return;
    }

    GLuint shader_program = create_text_shader(); // define this function
    glUseProgram(shader_program);

    glfwSwapInterval(1); // vsync

    glfwSetKeyCallback(window, key_callback);
    glfwPostEmptyEvent(); // initial render

    init_font();
    init_texture_atlas();

    loop();

    glfwDestroyWindow(window);
    glfwTerminate();
    hb_buffer_destroy(hb_buffer);
    hb_font_destroy(hb_font);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}
