// Simple display driver

// For Emscripten, we use a faster method by simply slicing the buffers instead of copying them dword by dword
// We still need SDL for events and the window title, but the blitting can be done independently.
// See: https://github.com/emscripten-core/emscripten/blob/incoming/src/library_sdl.js

#if 1
#include "display.h"
#include "devices.h"
#include "util.h"
#ifdef SDL2_INC_DIR
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#include <windows.h>
#endif
#else
#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#include <windows.h>
#endif
#endif
#include <stdlib.h>
#include <stdbool.h>

#define SDL2_LOCK_IMPL

#define DISPLAY_LOG(x, ...) LOG("DISPLAY", x, ##__VA_ARGS__)
#define DISPLAY_FATAL(x, ...)          \
    do {                               \
        DISPLAY_LOG(x, ##__VA_ARGS__); \
        ABORT();                       \
    } while (0)

#ifdef SDL2_LOCK_IMPL
static int pitch;
#endif
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static void* surface_pixels = NULL;

void* display_get_pixels(void)
{
    return surface_pixels;
}

static float scale_x = 1.0f;
static float scale_y = 1.0f;
static SDL_bool resizable = SDL_FALSE;
static int fullscreen = 0;
static int display_inited = 0;
static int h, w, mouse_enabled = 0, mhz_rating = -1;

static void display_set_title(void)
{
    char buffer[1000];
    UNUSED(mhz_rating);
    sprintf(buffer, "Halfix x86 Emulator - [%dx%d] - %s", w, h,
        mouse_enabled ? "Press ESC to release mouse" : "Click to capture mouse");
    SDL_SetWindowTitle(window, buffer);
}

void display_update_cycles(int cycles_elapsed, int us)
{
    mhz_rating = (int)((double)cycles_elapsed / (double)us);
    display_set_title();
}

// Nasty hack: don't update until screen has been resized (screen is resized during VGABIOS init)
static int resized = 0;
void display_set_resolution(int width, int height)
{
    resized = 1;
    if (!width || !height) {
        return display_set_resolution(640, 480);
    }
    DISPLAY_LOG("Changed resolution to w=%d h=%d\n", width, height);

#ifndef SDL2_LOCK_IMPL
    if (surface_pixels)
        free(surface_pixels);
    surface_pixels = malloc(width * height * 4);
#endif
    if (texture) {
#ifdef SDL2_LOCK_IMPL
        SDL_UnlockTexture(texture);
#endif
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, width, height
    );
#ifdef SDL2_LOCK_IMPL
    SDL_LockTexture(texture, NULL, &surface_pixels, &pitch);
#endif
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    w = width;
    h = height;
    if (scale_x == 1.0f && scale_y == 1.0f)
        SDL_SetWindowSize(window, width, height);
    else {
        int sw, sh;
        SDL_GetWindowSize(window, &sw, &sh);
        scale_x = (float)sw / (float)w;
        scale_y = (float)sh / (float)h;
    }
    display_set_title();
}

void display_update(int scanline_start, int scanlines)
{
    if (!resized)
        return;
    if ((w == 0) || (h == 0))
        return;
    if ((scanline_start + scanlines) > h) {
        printf("%d x %d [%d %d]\n", w, h, scanline_start, scanlines);
        ABORT();
    } else {
        //__asm__("int3");
#ifdef SDL2_LOCK_IMPL
        SDL_UnlockTexture(texture);
        SDL_RenderCopyF(renderer, texture, NULL, NULL);
        SDL_LockTexture(texture, NULL, &surface_pixels, &pitch);
#else
        SDL_UpdateTexture(texture, NULL, surface_pixels, 4 * w);
        SDL_Rect src_rect = {
            .x = 0, .y = scanline_start, .w = w, .h = scanlines
        };
        SDL_FRect dst_rect = {
            .x = 0.0f, .y = (float)scanline_start * scale_y, .w = (float)w * scale_x, .h = (float)scanlines * scale_y
        };
        SDL_RenderCopyF(renderer, texture, &src_rect, &dst_rect);
#endif
        SDL_RenderPresent(renderer);
    }
}

static void display_mouse_capture_update(int y)
{
    mouse_enabled = y;
    SDL_SetRelativeMouseMode(y ? SDL_TRUE : SDL_FALSE);
    SDL_SetWindowKeyboardGrab(window, y ? SDL_TRUE : SDL_FALSE);
    display_set_title();
}

void display_release_mouse(void)
{
    display_mouse_capture_update(0);
}

#define KEYMOD_INVALID -1

static int sdl_keysym_to_scancode(int sym)
{
    int n;
    switch (sym) {
    case SDLK_0 ... SDLK_9:
        n = sym - SDLK_0;
        if (!n)
            n = 10;
        return n + 1;
    case SDLK_ESCAPE:
        display_mouse_capture_update(0);
        return KEYMOD_INVALID;
    case SDLK_EQUALS:
        return 0x0D;
    case SDLK_RETURN:
        return 0x1C;
    case SDLK_a:
        return 0x1E;
    case SDLK_b:
        return 0x30;
    case SDLK_c:
        return 0x2E;
    case SDLK_d:
        return 0x20;
    case SDLK_e:
        return 0x12;
    case SDLK_f:
        return 0x21;
    case SDLK_g:
        return 0x22;
    case SDLK_h:
        return 0x23;
    case SDLK_i:
        return 0x17;
    case SDLK_j:
        return 0x24;
    case SDLK_k:
        return 0x25;
    case SDLK_l:
        return 0x26;
    case SDLK_m:
        return 0x32;
    case SDLK_n:
        return 0x31;
    case SDLK_o:
        return 0x18;
    case SDLK_p:
        return 0x19;
    case SDLK_q:
        return 0x10;
    case SDLK_r:
        return 0x13;
    case SDLK_s:
        return 0x1F;
    case SDLK_t:
        return 0x14;
    case SDLK_u:
        return 0x16;
    case SDLK_v:
        return 0x2F;
    case SDLK_w:
        return 0x11;
    case SDLK_x:
        return 0x2D;
    case SDLK_y:
        return 0x15;
    case SDLK_z:
        return 0x2C;
    case SDLK_BACKSPACE:
        return 0x0E;
    case SDLK_LEFT:
        return 0xE04B;
    case SDLK_DOWN:
        return 0xE050;
    case SDLK_RIGHT:
        return 0xE04D;
    case SDLK_UP:
        return 0xE048;
    case SDLK_SPACE:
        return 0x39;
    case SDLK_PAGEUP:
        return 0xE04F;
    case SDLK_PAGEDOWN:
        return 0xE051;
    case SDLK_DELETE:
        return 0xE053;
    case SDLK_F1 ... SDLK_F12:
        return 0x3B + (sym - SDLK_F1);
    case SDLK_SLASH:
        return 0x35;
    case SDLK_LALT:
        return 0x38;
    case SDLK_LCTRL:
        return 0x1D;
    case SDLK_LSHIFT:
        return 0x2A;
    case SDLK_RSHIFT:
        return 0x36;
    case SDLK_SEMICOLON:
        return 0x27;
    case SDLK_BACKSLASH:
        return 0x2B;
    case SDLK_COMMA:
        return 0x33;
    case SDLK_PERIOD:
        return 0x34;
    case SDLK_MINUS:
        return 0x0C;
    case SDLK_RIGHTBRACKET:
        return 0x1A;
    case SDLK_LEFTBRACKET:
        return 0x1B;
    case SDLK_QUOTE:
        return 0x28;
    case SDLK_BACKQUOTE:
        return 0x29;
    case SDLK_TAB:
        return 0x0F;
    case SDLK_LGUI:
    case SDLK_APPLICATION:
        return 0xE05B;
    case SDLK_RGUI:
        return 0xE05C;
    default:
        printf("Unknown keysym: %d\n", sym);
        return KEYMOD_INVALID;
        //DISPLAY_FATAL("Unknown keysym %d\n", sym);
    }
}
static inline void display_kbd_send_key(int k)
{
    if (k == KEYMOD_INVALID)
        return;
    if (k & 0xFF00)
        kbd_add_key(k >> 8);
    kbd_add_key(k & 0xFF);
}

void display_send_hotkey(int hotkey, int down)
{
    down = down ? 0 : 0x80;
    switch (hotkey) {
        case 0: { // Ctrl + Alt + Del
            display_kbd_send_key(0x1D | down);
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0xE053 | down);
            break;
        }
        case 1: { // Shift + F10
            display_kbd_send_key(0x2A | down);
            display_kbd_send_key(0x44 | down);
            break;
        }
        case 2: { // Alt + F4
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0x3E | down);
            break;
        }
        case 3: { // Alt + TAB
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0x0F | down);
            break;
        }
    }
}

void display_handle_events(void)
{
    SDL_Event event;
    int k;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            if (!mouse_enabled) {
                display_quit();
                exit(0);
            }
            break;
        case SDL_KEYDOWN: {
            display_set_title();
            if (mouse_enabled) {
                display_kbd_send_key(sdl_keysym_to_scancode(event.key.keysym.sym));
            }
            else {
                switch (event.key.keysym.sym) {
                    case SDLK_1...SDLK_4:
                        display_send_hotkey(event.key.keysym.sym - SDLK_1, 1);
                        break;
                    case SDLK_ESCAPE: {
                        display_kbd_send_key(1);
                        break;
                    }
                    case SDLK_c: {
                        SDL_DisplayMode mode;
                        int index = SDL_GetWindowDisplayIndex(window);
                        SDL_GetCurrentDisplayMode(index, &mode);
                        SDL_SetWindowPosition(
                            window,
                            (mode.w - w) >> 1, (mode.h - h) >> 1
                        );
                        break;
                    }
                    case SDLK_q: {
                        display_quit();
                        exit(0);
                        break;
                    }
                    case SDLK_r: {
                        scale_x = scale_y = 1.0f;
                        if (!fullscreen)
                            SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
                        break;
                    }
                    case SDLK_z: {
                        if (resizable) {
                            scale_x += 0.25f;
                            scale_y += 0.25f;
                            if (!fullscreen)
                                SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
                        }
                        break;
                    }
                    case SDLK_x: {
                        if (resizable && scale_x > 0.25f && scale_y > 0.25f) {
                            scale_x -= 0.25f;
                            scale_y -= 0.25f;
                            if (!fullscreen)
                                SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
                        }
                        break;
                    }
                    case SDLK_s: {
                        resizable = !resizable;
                        SDL_SetWindowResizable(window, resizable);
                        if (!resizable) {
                            scale_x = scale_y = 1.0f;
                        }
                        if (!fullscreen)
                            SDL_SetWindowSize(window, w, h);
                        break;
                    }
                    case SDLK_d:
                    case SDLK_f: {
                        fullscreen = fullscreen ? 0 : (
                            event.key.keysym.sym == SDLK_d ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN
                        );
                        SDL_SetWindowFullscreen(window, fullscreen);
                        if (fullscreen) {
                            resizable = 1;
                            int sw, sh;
                            SDL_GetWindowSize(window, &sw, &sh);
                            scale_x = (float)sw / (float)w;
                            scale_y = (float)sh / (float)h;
                        }
                        else {
                            SDL_SetWindowSize(window, w, h);
                            if (!resizable) {
                                scale_x = scale_y = 1.0f;
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            k = event.type == SDL_MOUSEBUTTONDOWN ? MOUSE_STATUS_PRESSED : MOUSE_STATUS_RELEASED;
            switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                if (k == MOUSE_STATUS_PRESSED && !mouse_enabled) // Don't send anything
                    display_mouse_capture_update(1);
                else if (mouse_enabled)
                    kbd_mouse_down(k, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE);
                break;
            case SDL_BUTTON_MIDDLE:
                if (mouse_enabled)
                    kbd_mouse_down(MOUSE_STATUS_NOCHANGE, k, MOUSE_STATUS_NOCHANGE);
                break;
            case SDL_BUTTON_RIGHT:
                if (mouse_enabled)
                    kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE, k);
                break;
            }
            break;
        }
        case SDL_MOUSEMOTION: {
            if (mouse_enabled)
                kbd_send_mouse_move(event.motion.xrel, event.motion.yrel, 0, 0);
            break;
        }
        case SDL_MOUSEWHEEL: {
            if (mouse_enabled)
                kbd_send_mouse_move(0, 0, event.wheel.x, event.wheel.y);
            break;
        }
        case SDL_KEYUP: {
            if (mouse_enabled) {
                int c = sdl_keysym_to_scancode(event.key.keysym.sym);
                display_kbd_send_key(c | 0x80);
            }
            else {
                switch (event.key.keysym.sym) {
                    case SDLK_1...SDLK_4:
                        display_send_hotkey(event.key.keysym.sym - SDLK_1, 0);
                        break;
                    case SDLK_ESCAPE: {
                        display_kbd_send_key(1 | 0x80);
                        break;
                    }
                }
            }
            break;
        }
        case SDL_WINDOWEVENT: {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                scale_x = (float)event.window.data1 / (float)w;
                scale_y = (float)event.window.data2 / (float)h;
            }
            break;
        }
        }
    }
}

void display_send_scancode(int key)
{
    display_kbd_send_key(key);
}

#ifdef _WIN32
void display_check_dark_mode(void)
{
    // TODO: make better
    HMODULE dwm = LoadLibraryExA("dwmapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dwm)
        return;
    HMODULE uxtheme = LoadLibraryExA("uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!uxtheme) {
        FreeLibrary(dwm);
        return;
    }
    typedef HRESULT (*DwmSetWindowAttributePTR)(HWND, DWORD, LPCVOID, DWORD);
    DwmSetWindowAttributePTR DwmSetWindowAttribute = (DwmSetWindowAttributePTR)((size_t)GetProcAddress(dwm, "DwmSetWindowAttribute"));
    typedef bool (WINAPI *ShouldAppsUseDarkModePTR)();
    ShouldAppsUseDarkModePTR ShouldAppsUseDarkMode = (ShouldAppsUseDarkModePTR)((size_t)GetProcAddress(uxtheme, MAKEINTRESOURCEA(132)));
    if (!DwmSetWindowAttribute || !ShouldAppsUseDarkMode || !ShouldAppsUseDarkMode()) {
        FreeLibrary(uxtheme);
        FreeLibrary(dwm);
        return;
    }
    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);
    SDL_GetWindowWMInfo(window, &wm_info);
    HWND hwnd = (HWND)wm_info.info.win.window;
    BOOL dark_mode = 1;
    if (!DwmSetWindowAttribute(hwnd, 20, &dark_mode, sizeof(BOOL))) {
        dark_mode = 1;
        DwmSetWindowAttribute(hwnd, 19, &dark_mode, sizeof(BOOL));
    }
    FreeLibrary(uxtheme);
    FreeLibrary(dwm);
}
#endif

void display_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
        DISPLAY_FATAL("Unable to initialize SDL");

    window = SDL_CreateWindow(
        "halfix",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN
    );
    if (window == NULL)
        DISPLAY_FATAL("Unable to create window");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
        DISPLAY_FATAL("Unable to create renderer");
#ifdef _WIN32
    display_check_dark_mode();
#endif
    SDL_ShowWindow(window);
    display_inited = 1;
    display_set_title();
    resized = 0;
}
void display_quit(void) {
    if (display_inited < 1)
        return;
    display_inited = 0;
    if (texture) {
#ifdef SDL2_LOCK_IMPL
        SDL_UnlockTexture(texture);
#else
        if (surface_pixels)
            free(surface_pixels);
#endif
        SDL_DestroyTexture(texture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}
void display_sleep(int ms)
{
    SDL_Delay(ms);
}

#else // Headless mode
#include "util.h"
static uint32_t pixels[800 * 500];
void display_init(void) {}
void display_update(int scanline_start, int scanlines)
{
    UNUSED(scanline_start | scanlines);
}
void display_set_resolution(int width, int height)
{
    UNUSED(width | height);
}
void* display_get_pixels(void)
{
    return pixels;
}
#endif
