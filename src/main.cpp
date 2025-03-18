#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <SDL3_ttf/SDL_ttf.h>

int *gFrameBuffer;
SDL_Window *gSDLWindow;
SDL_Renderer *gSDLRenderer;
SDL_Texture *gSDLTexture;
TTF_Font *font;
static int gDone;
const int WINDOW_WIDTH = 2560;
const int WINDOW_HEIGHT = 1600;

const int COLS = 34;
const int ROWS = 20;
const int NOTE_WIDTH = WINDOW_WIDTH / COLS;
const int NOTE_HEIGHT = WINDOW_HEIGHT / ROWS;

std::vector<SDL_FRect> dirtyRects;

int state_cols = COLS;
int state_rows = ROWS;

float x, y;
int cursor_row = 0;
int cursor_col = 2;
                                               
int render_cols = COLS;
int render_rows = ROWS;

// blinking cursor
bool cursor_visible = true;
Uint64 lastToggleTime = 0;
Uint64 lastMovedTime = 0;
const Uint64 BLINK_INTERVAL = 500;
const Uint64 MOVE_RESET_TIME = 500;

// normal mode
bool normal_mode = true;

// visual mode
bool visual_mode = false;
int visual_col = 0;
int visual_row = 0;

// command mode
bool command_mode = false;

void markDirty(float x, float y, float w, float h) {
    SDL_FRect dirtyRect = {x, y, w, h};
    dirtyRects.push_back(dirtyRect);
}

struct MidiNote {
    float start;
    float duration;
    int pitch;
    int velocity;

    float end() const {
        return start + duration;
    }
};

struct AudioFile {
    std::vector<float> waveform;
    int sampleRate;
};

struct MidiSlice {
    float start;
    float duration;
    float end() const {
        return start + duration;
    }

    std::vector<MidiNote> notes;
    AudioFile *sample;
};

void initial_render() {
    /*
     * Draw static grid lines and status bars and piano
     */

    // Draw piano
    SDL_SetRenderDrawColor(gSDLRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_FRect piano;
    piano = {0, 0, NOTE_WIDTH * 2 - 1, NOTE_HEIGHT * (ROWS - 3) - 1};
    SDL_RenderFillRect(gSDLRenderer, &piano);

    SDL_Color text_color = {0x00, 0x00, 0x00};

    // horizontal lines
    std::string notes[12] = {"G#X", "GX", "F#X", "FX", "EX", "D#X", "DX", "C#X", "CX", "BX", "A#X", "AX"};
    int index = 0;
    for (int row = 0; row <= ROWS - 3; row++) {
        int y_pos = row * NOTE_HEIGHT;
        SDL_SetRenderDrawColor(gSDLRenderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderLine(gSDLRenderer, 0, y_pos, WINDOW_WIDTH, y_pos);
        if (row % 12 == 0) {
            index = 0;
        }
        if (notes[index][1] == '#') {
            SDL_FRect black;
            piano = {0, (float)y_pos, ((float)NOTE_WIDTH * 2 * 3/4) - 1, NOTE_HEIGHT};
            SDL_RenderFillRect(gSDLRenderer, &piano);
        }
        else if (notes[index][1] != '#') {
            SDL_Surface *text_surface = TTF_RenderText_Blended(font, notes[index].c_str(), 2, text_color);
            if (!text_surface) {
                SDL_Log("TTF_RenderText_Blended Error: %s", SDL_GetError());
                continue;
            }
            SDL_Texture *text_texture = SDL_CreateTextureFromSurface(gSDLRenderer, text_surface);
            if (!text_texture) {
                SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
                continue;
            }

            SDL_FRect text = {(float)NOTE_WIDTH * 3/4, (float)y_pos, (float)text_surface->w, (float)text_surface->h};
            SDL_RenderTexture(gSDLRenderer, text_texture, NULL, &text);
            SDL_DestroySurface(text_surface);
            SDL_DestroyTexture(text_texture);
        }
        index++;
    }
    // vertical lines
    for (int col = 2; col <= COLS; col++) {
        int x_pos = col * NOTE_WIDTH;
        if ((col - 2) % 4 == 0 && (col - 2) > 0) {
            SDL_SetRenderDrawColor(gSDLRenderer, 0x54, 0x54, 0x54, 0xFF);
            SDL_RenderLine(gSDLRenderer, x_pos, 0, x_pos, WINDOW_HEIGHT - NOTE_HEIGHT * 3);
            SDL_SetRenderDrawColor(gSDLRenderer, 0, 0, 0, 0xFF);
            continue;
        }
        SDL_RenderLine(gSDLRenderer, x_pos, 0, x_pos, WINDOW_HEIGHT - NOTE_HEIGHT * 3);
    }

    /*
     * Draw status bars -------------------------------------------------------
     */

    // bar1
    SDL_SetRenderDrawColor(gSDLRenderer, 0x2A, 0x27, 0x3F, 0xFF);
    SDL_FRect status_bar;
    status_bar = {0, float((render_rows - 3) * NOTE_HEIGHT), NOTE_WIDTH * COLS, NOTE_HEIGHT};
    SDL_RenderFillRect(gSDLRenderer, &status_bar);

    //text1
    text_color = {0x6E, 0x6C, 0x7E};
    SDL_Surface *text_surface = TTF_RenderText_Blended(font, ("roll1                                        " 
                + std::to_string(cursor_row + 1) + "," + std::to_string(cursor_col - 1)).c_str(), 0, text_color);
    if (!text_surface) {
        SDL_Log("TTF_RenderText_Blended Error: %s", SDL_GetError());
    }
    SDL_Texture *text_texture = SDL_CreateTextureFromSurface(gSDLRenderer, text_surface);
    if (!text_texture) {
        SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
    }
    SDL_FRect text = {0, float((render_rows - 3) * NOTE_HEIGHT), (float)text_surface->w, (float)text_surface->h};
    SDL_RenderTexture(gSDLRenderer, text_texture, NULL, &text);
    SDL_DestroySurface(text_surface);
    SDL_DestroyTexture(text_texture);

    //bar2
    SDL_SetRenderDrawColor(gSDLRenderer, 0x23, 0x21, 0x36, 0xFF);
    SDL_FRect command_bar;
    command_bar = {0, float((render_rows - 2) * NOTE_HEIGHT), NOTE_WIDTH * COLS, NOTE_HEIGHT};
    SDL_RenderFillRect(gSDLRenderer, &command_bar);
    
    //text2
    text_color = {0xFF, 0xFF, 0xFF};
    std::string mode = " ";
    if (visual_mode) {
        mode = "-- VISUAL --";
    }
    SDL_Surface *text_surface2 = TTF_RenderText_Blended(font, mode.c_str(), 0, text_color);
    if (!text_surface2) {
        SDL_Log("TTF_RenderText_Blended Error: %s", SDL_GetError());
    }
    SDL_Texture *text_texture2 = SDL_CreateTextureFromSurface(gSDLRenderer, text_surface2);
    if (!text_texture2) {
        SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
    }
    SDL_FRect text2 = {0, float((render_rows - 2) * NOTE_HEIGHT), (float)text_surface2->w, (float)text_surface2->h};
    SDL_RenderTexture(gSDLRenderer, text_texture2, NULL, &text2);
    SDL_DestroySurface(text_surface2);
    SDL_DestroyTexture(text_texture2);

    //bar3
    SDL_SetRenderDrawColor(gSDLRenderer, 0x33, 0xAF, 0xF4, 0xFF);
    SDL_FRect tmux_bar;
    tmux_bar = {0, float((render_rows - 1) * NOTE_HEIGHT), NOTE_WIDTH * COLS, NOTE_HEIGHT};
    SDL_RenderFillRect(gSDLRenderer, &tmux_bar);

    //text3
    text_color = {0x00, 0x00 ,0x00};
    SDL_Surface *text_surface3 = TTF_RenderText_Blended(font, "[project1] 1:roll1*", 0, text_color);
    if (!text_surface3) {
        SDL_Log("TTF_RenderText_Blended Error: %s", SDL_GetError());
    }
    SDL_Texture *text_texture3 = SDL_CreateTextureFromSurface(gSDLRenderer, text_surface3);
    if (!text_texture3) {
        SDL_Log("SDL_CreateTextureFromSurface Error: %s", SDL_GetError());
    }
    SDL_FRect text3 = {0, float((render_rows - 1) * NOTE_HEIGHT), (float)text_surface3->w, (float)text_surface3->h};
    SDL_RenderTexture(gSDLRenderer, text_texture3, NULL, &text3);
    SDL_DestroySurface(text_surface3);
    SDL_DestroyTexture(text_texture3);
}

bool containsRect(float x, float y) {
    return std::any_of(
        dirtyRects.begin(), dirtyRects.end(),
        [x, y](const SDL_FRect &rect) { return rect.x == x && rect.y == y; });
}

void removeRect(float x, float y) {
    dirtyRects.erase(std::remove_if(dirtyRects.begin(), dirtyRects.end(),[x, y](const SDL_FRect &rect) {
                return rect.x == x && rect.y == y;
    }), dirtyRects.end());
}

void visual_draw_note() {
    int left_col;
    int top_row;
    int notes_width;
    int notes_height;

    int right_col;
    int bottom_row;

    int col1 = cursor_col;
    int row1 = cursor_row;

    int col2 = visual_col;
    int row2 = visual_row;

    top_row = row1;
    bottom_row = row2;
    if (row2 < row1) {
        top_row = row2;
        bottom_row = row1;
    }

    left_col = col1;
    right_col = col2;
    if (col2 < col1) {
        left_col = col2;
        right_col = col1;
    }

    notes_width = (right_col - left_col + 1) * NOTE_WIDTH;
    notes_height = (bottom_row - top_row + 1) * NOTE_HEIGHT;

    left_col *= NOTE_WIDTH;
    top_row *= NOTE_HEIGHT;

    if (containsRect(left_col, top_row)) {
        removeRect(left_col, top_row);
        return;
    }
    markDirty(left_col, top_row, notes_width - 1, notes_height - 1);

    visual_mode = false;
    normal_mode = true;
}

void draw_note() {
    int col = x / (NOTE_WIDTH);
    int row = y / (NOTE_HEIGHT);
    if (row >= render_rows - 3 || col < 2) {
        return;
    }
    col *= NOTE_WIDTH;
    row *= NOTE_HEIGHT;

    if (containsRect(col, row)) {
        removeRect(col, row);
        return;
    }
    markDirty(col, row, NOTE_WIDTH - 1, NOTE_HEIGHT - 1);
}

void visual_motion(SDL_Event e, bool *moved, int *number) {
    const bool *key_states = SDL_GetKeyboardState(NULL);
    if (key_states[SDL_SCANCODE_LCTRL] || key_states[SDL_SCANCODE_RCTRL]) {
        if (key_states[SDL_SCANCODE_J] || key_states[SDL_SCANCODE_D]) {
            *moved = true;
            cursor_row += 3;
            if (cursor_row >= render_rows - 3) {
                cursor_row = render_rows - 4;
            }
        }
        if (key_states[SDL_SCANCODE_K] || key_states[SDL_SCANCODE_U]) {
            *moved = true;
            cursor_row -= 3;
            if (cursor_row < 0) {
                cursor_row = 0;
            }
        }
        if (key_states[SDL_SCANCODE_L]) {
            *moved = true;
            cursor_col += 3;
            if (cursor_col >= render_cols - 1) {
                cursor_col = render_cols - 1;
            }
        }
        if (key_states[SDL_SCANCODE_H]) {
            *moved = true;
            cursor_col -= 3;
            if (cursor_col < 2) {
                cursor_col = 2;
            }
        }
    }
    if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) {
        if (key_states[SDL_SCANCODE_4]) {
            *moved = true;
            cursor_col = render_cols - 1;
        }
        if (key_states[SDL_SCANCODE_G]) {
            *moved = true;
            cursor_row = render_rows - 4;
        }
    }
    switch (e.type) {
        case SDL_EVENT_QUIT:
            gDone = 1;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (e.key.key) {
                case SDLK_ESCAPE:
                    visual_mode = false;
                    normal_mode = true;
                    break;
                case SDLK_SPACE:
                    // Play/Pause playback bar update logic
                    break;
                case SDLK_H:
                    *moved = true;
                    cursor_col -= *number;
                    if (cursor_col < 2) {
                        cursor_col = 2;
                    }
                    break;
                case SDLK_J:
                    *moved = true;
                    cursor_row += *number;
                    if (cursor_row >= render_rows - 3) {
                        cursor_row = render_rows - 4;
                    }
                    break;
                case SDLK_K:
                    *moved = true;
                    cursor_row -= *number;
                    if (cursor_row < 0) {
                        cursor_row = 0; 
                    }
                    break;
                case SDLK_L:
                    *moved = true;
                    cursor_col += *number;
                    if (cursor_col >= render_cols) {
                        cursor_col = render_cols - 1;
                    }
                    break;
                case SDLK_X:
                case SDLK_RETURN:
                    *moved = true;
                    x = static_cast<float>(cursor_col) * WINDOW_WIDTH / COLS;
                    y = static_cast<float>(cursor_row) * WINDOW_HEIGHT / ROWS;
                    visual_draw_note();
                    break;
                case SDLK_0:
                    *moved = true;
                    cursor_col = 2;
                    break;
                case SDLK_G:
                    if (!key_states[SDL_SCANCODE_LSHIFT] && !key_states[SDL_SCANCODE_RSHIFT]) {
                        *moved = true;
                        cursor_row = 0;
                    }
                    break;
                case SDLK_V:
                    *moved = true;
                    visual_mode = false;
                    normal_mode = true;
                    break;
                default:
                    break;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                SDL_GetMouseState(&x, &y);
                draw_note();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
}

void normal_motion(SDL_Event e, bool *moved, int *number) {
    const bool *key_states = SDL_GetKeyboardState(NULL);
    if (key_states[SDL_SCANCODE_LCTRL] || key_states[SDL_SCANCODE_RCTRL]) {
        if (key_states[SDL_SCANCODE_J] || key_states[SDL_SCANCODE_D]) {
            *moved = true;
            cursor_row += 3;
            if (cursor_row >= render_rows - 3) {
                cursor_row = render_rows - 4;
            }
        }
        if (key_states[SDL_SCANCODE_K] || key_states[SDL_SCANCODE_U]) {
            *moved = true;
            cursor_row -= 3;
            if (cursor_row < 0) {
                cursor_row = 0;
            }
        }
        if (key_states[SDL_SCANCODE_L]) {
            *moved = true;
            cursor_col += 3;
            if (cursor_col >= render_cols - 1) {
                cursor_col = render_cols - 1;
            }
        }
        if (key_states[SDL_SCANCODE_H]) {
            *moved = true;
            cursor_col -= 3;
            if (cursor_col < 2) {
                cursor_col = 2;
            }
        }
    }
    if (key_states[SDL_SCANCODE_LSHIFT] || key_states[SDL_SCANCODE_RSHIFT]) {
        if (key_states[SDL_SCANCODE_4]) {
            *moved = true;
            cursor_col = render_cols - 1;
        }
        if (key_states[SDL_SCANCODE_G]) {
            *moved = true;
            cursor_row = render_rows - 4;
        }
    }
    switch (e.type) {
        case SDL_EVENT_QUIT:
            gDone = 1;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch (e.key.key) {
                case SDLK_ESCAPE:
                    gDone = 1;
                    break;
                case SDLK_SPACE:
                    // Play/Pause playback bar update logic
                    break;
                case SDLK_H:
                    *moved = true;
                    cursor_col -= *number;
                    if (cursor_col < 2) {
                        cursor_col = 2;
                    }
                    break;
                case SDLK_J:
                    *moved = true;
                    cursor_row += *number;
                    if (cursor_row >= render_rows - 3) {
                        cursor_row = render_rows - 4;
                    }
                    break;
                case SDLK_K:
                    *moved = true;
                    cursor_row -= *number;
                    if (cursor_row < 0) {
                        cursor_row = 0; 
                    }
                    break;
                case SDLK_L:
                    *moved = true;
                    cursor_col += *number;
                    if (cursor_col >= render_cols) {
                        cursor_col = render_cols - 1;
                    }
                    break;
                case SDLK_X:
                case SDLK_RETURN:
                    *moved = true;
                    x = static_cast<float>(cursor_col) * WINDOW_WIDTH / COLS;
                    y = static_cast<float>(cursor_row) * WINDOW_HEIGHT / ROWS;
                    draw_note();
                    break;
                case SDLK_0:
                    *moved = true;
                    cursor_col = 2;
                    break;
                case SDLK_G:
                    if (!key_states[SDL_SCANCODE_LSHIFT] && !key_states[SDL_SCANCODE_RSHIFT]) {
                        *moved = true;
                        cursor_row = 0;
                    }
                    break;
                case SDLK_V:
                    *moved = true;
                    visual_mode = true;
                    normal_mode = false;
                    break;
                default:
                    break;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:
                SDL_GetMouseState(&x, &y);
                draw_note();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
}

bool update() {
    SDL_Event e;
    bool moved = false;
    if (visual_mode) {
        moved = true;
    }
    int number = 1;
    while (SDL_PollEvent(&e)) {
        if (normal_mode) {
            normal_motion(e, &moved, &number);
            if (visual_mode) {
                visual_col = cursor_col;
                visual_row = cursor_row;
            }
        }
        else if (visual_mode) {
            visual_motion(e, &moved, &number);
        }
    }

    if (moved) {
        cursor_visible = true;
        lastMovedTime = SDL_GetTicks();
    }

    // Set background color
    SDL_SetRenderDrawColor(gSDLRenderer, 0x23, 0x21, 0x36, 0xFF);
    SDL_RenderClear(gSDLRenderer);

    // Render the static grid only once
    initial_render();

    // Update dirty rectangles
    for (const SDL_FRect &rect : dirtyRects) {
        SDL_SetRenderDrawColor(gSDLRenderer, 0x33, 0xAF, 0xF4, 0xFF);
        SDL_RenderFillRect(gSDLRenderer, &rect);
    }

    // Render cursor as a transparent, blinking block
    Uint64 time = SDL_GetTicks();
    if (time - lastMovedTime >= MOVE_RESET_TIME) {
        if (time - lastToggleTime >= BLINK_INTERVAL) {
            cursor_visible = !cursor_visible;
            lastToggleTime = time;
        }
    } else {
        cursor_visible = true;
    }

    if (cursor_visible) {
        SDL_FRect cursorRect;
        SDL_SetRenderDrawBlendMode(gSDLRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gSDLRenderer, 0x39, 0x35, 0x52, 0x80);  // gray cursor 50% opacity
        cursorRect = {float(cursor_col * NOTE_WIDTH), float(cursor_row * NOTE_HEIGHT), NOTE_WIDTH, NOTE_HEIGHT};
        if (visual_mode) {
            int left_col;
            int top_row;
            int notes_width;
            int notes_height;

            int right_col;
            int bottom_row;

            int col1 = cursor_col;
            int row1 = cursor_row;

            int col2 = visual_col;
            int row2 = visual_row;

            top_row = row1;
            bottom_row = row2;
            if (row2 < row1) {
                top_row = row2;
                bottom_row = row1;
            }

            left_col = col1;
            right_col = col2;
            if (col2 < col1) {
                left_col = col2;
                right_col = col1;
            }

            notes_width = (right_col - left_col + 1) * NOTE_WIDTH;
            notes_height = (bottom_row - top_row + 1) * NOTE_HEIGHT;

            left_col *= NOTE_WIDTH;
            top_row *= NOTE_HEIGHT;
            cursorRect = {float(left_col), float(top_row), float(notes_width), float(notes_height)};
        }
        SDL_RenderFillRect(gSDLRenderer, &cursorRect);
        SDL_SetRenderDrawBlendMode(gSDLRenderer, SDL_BLENDMODE_NONE);
    }

    SDL_RenderPresent(gSDLRenderer);

    // Clear dirty rectangles after rendering
    return true;
}

void loop() {
    if (!update()) {
        gDone = 1;
    }
}

int main(int argc, char **argv) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (!TTF_Init()) {
        SDL_Log("SDL_ttf Error: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    } 

    font = TTF_OpenFont("../assets/fonts/NotoMono.ttf", (float)WINDOW_WIDTH / (COLS + 6));
    if (!font) {
        SDL_Log("TTF_OpenFont Error: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
    }

    gFrameBuffer = new int[WINDOW_WIDTH * WINDOW_HEIGHT];
    gSDLWindow =
        SDL_CreateWindow("SDL3 window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);

    gSDLRenderer = SDL_CreateRenderer(gSDLWindow, NULL);

    gSDLTexture = SDL_CreateTexture(gSDLRenderer, SDL_PIXELFORMAT_RGBA8888,
                                    SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH,
                                    WINDOW_HEIGHT);

    if (!gFrameBuffer || !gSDLWindow || !gSDLRenderer || !gSDLTexture) {
        std::cerr << "SDL_Create* Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    gDone = 0;

    while (!gDone) {
        loop();
    }

    SDL_DestroyTexture(gSDLTexture);
    SDL_DestroyRenderer(gSDLRenderer);
    SDL_DestroyWindow(gSDLWindow);
    delete[] gFrameBuffer;
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

