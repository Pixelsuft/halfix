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
static float real_scale_x = 1.0f;
static float real_scale_y = 1.0f;
static float def_real_scale_x = 1.0f;
static float def_real_scale_y = 1.0f;
static SDL_bool resizable = SDL_FALSE;
static int fullscreen = 0;
static int display_inited = 0;
static int w, h, mouse_enabled = 0, mhz_rating = -1;
static int ren_w, ren_h;
static int scale_mode = 0;
static SDL_FRect dst_rect;

static void display_set_title(void)
{
    UNUSED(mhz_rating);
#ifndef MOBILE_BUILD
    char buffer[1000];
    h_sprintf(buffer, "Halfix x86 Emulator - [%dx%d] - %s", w, h,
        mouse_enabled ? "Press ESC to release mouse" : "Click to capture mouse");
    SDL_SetWindowTitle(window, buffer);
#endif
}

void display_update_cycles(int cycles_elapsed, int us)
{
    mhz_rating = (int)((double)cycles_elapsed / (double)us);
    display_set_title();
}

void display_update_scale_mode(void)
{
    if (scale_mode == 0) {
        dst_rect.w = (float)w * real_scale_x;
        dst_rect.h = (float)h * real_scale_y;
        dst_rect.x = (float)ren_w / 2.0f - (float)dst_rect.w / 2.0f;
        dst_rect.y = (float)ren_h / 2.0f - (float)dst_rect.h / 2.0f;
    }
    else if (scale_mode == 1) {
        float min_scale = (scale_x > scale_y) ? scale_y : scale_x;
        dst_rect.w = (float)w * min_scale * real_scale_x;
        dst_rect.h = (float)h * min_scale * real_scale_y;
        dst_rect.x = (float)ren_w / 2.0f - (float)dst_rect.w / 2.0f;
        dst_rect.y = (float)ren_h / 2.0f - (float)dst_rect.h / 2.0f;
    }
    else if (scale_mode == 2) {
        dst_rect.w = (float)w * scale_x * real_scale_x;
        dst_rect.h = (float)h * scale_y * real_scale_y;
        dst_rect.x = dst_rect.y = 0.0f;
    }
}

// Nasty hack: don't update until screen has been resized (screen is resized during VGABIOS init)
static int resized = 0;
void display_set_resolution(int width, int height)
{
    if (!width || !height) {
        return display_set_resolution(640, 480);
    }
    resized = 1;
    DISPLAY_LOG("Changed resolution to w=%d h=%d\n", width, height);

#ifndef SDL2_LOCK_IMPL
    if (surface_pixels)
        h_free(surface_pixels);
    surface_pixels = h_malloc(width * height * 4);
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
    // TODO: Should I use this?
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    int sw, sh;
    SDL_GetWindowSize(window, &sw, &sh);
    w = width;
    h = height;
    if (SDL_GetRendererOutputSize(renderer, &ren_w, &ren_h) < 0) {
        ren_w = (int)sw;
        ren_h = (int)sh;
    }
    if (scale_x == 1.0f && scale_y == 1.0f) {
#ifndef MOBILE_BUILD
        SDL_SetWindowSize(window, width, height);
        sw = w;
        sh = h;
#endif
    }
    else {
        scale_x = (float)ren_w / (float)w;
        scale_y = (float)ren_h / (float)h;
    }
    display_set_title();
    display_update_scale_mode();
}

void display_update(int scanline_start, int scanlines)
{
    if (!resized)
        return;
    if ((w == 0) || (h == 0))
        return;
    if ((scanline_start + scanlines) > h) {
        h_printf("%d x %d [%d %d]\n", w, h, scanline_start, scanlines);
        ABORT();
    } else {
        //__asm__("int3");
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
#ifdef SDL2_LOCK_IMPL
        SDL_UnlockTexture(texture);
        SDL_Rect real_dst_rect = {
            (int)dst_rect.x,
            (int)dst_rect.y,
            (int)dst_rect.w,
            (int)dst_rect.h
        };
        SDL_RenderCopy(renderer, texture, NULL, &real_dst_rect);
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
#ifdef MOBILE_BUILD
        if (!mouse_enabled) {
            SDL_FRect rect = { 0.0f, 0.0f, 0.1f * (float)ren_w, 0.1f * (float)ren_h };
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100);
            SDL_RenderFillRectF(renderer, &rect);
        }
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
        h_printf("Unknown keysym: %d\n", sym);
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
        case 4: { // F8
            display_kbd_send_key((0x3B + (SDLK_F8 - SDLK_F1)) | down);
            break;
        }
        case 5: { // Down
            display_kbd_send_key(0xE050 | down);
            break;
        }
    }
}

#if defined(MOBILE_BUILD) || 1
static int64_t finger_ids[5] = { -1337, -1337, -1337, -1337, -1337 };

void display_on_touch_event(int64_t finger_id, SDL_FPoint* pos, int event_id) {
    if (event_id == SDL_FINGERMOTION) {
        if (finger_id == finger_ids[1]) {
            kbd_send_mouse_move(pos->x * (float)ren_w, pos->y * (float)ren_h, 0, 0);
        }
    }
    else if (event_id == SDL_FINGERDOWN) {
        if (pos->x < 0.1f && pos->y < 0.1f) {
            finger_ids[0] = finger_id;
            return;
        }
        SDL_StopTextInput();
        if (!mouse_enabled)
            return;
        if (pos->x > 0.2f) {
            finger_ids[1] = finger_id;
            return;
        }
        if (pos->y > 0.4f) {
            finger_ids[2] = finger_id;
            kbd_mouse_down(MOUSE_STATUS_PRESSED, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE);
        }
        else {
            finger_ids[3] = finger_id;
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_PRESSED);
        }
    }
    else if (event_id == SDL_FINGERUP) {
        if (finger_ids[0] == finger_id) {
            if (pos->x < 0.1f && pos->y < 0.1) {
                SDL_StartTextInput();
            }
            finger_ids[0] = -1337;
            return;
        }
        if (!mouse_enabled) {
            mouse_enabled = 1;
            return;
        }
        if (finger_id == finger_ids[1]) {
            finger_ids[1] = -1337;
        }
        else if (finger_id == finger_ids[2]) {
            finger_ids[2] = -1337;
            kbd_mouse_down(MOUSE_STATUS_RELEASED, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE);
        }
        else if (finger_id == finger_ids[3]) {
            finger_ids[3] = -1337;
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_RELEASED);
        }
    }
}
#endif

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
                if (event.key.keysym.sym == SDLK_AC_BACK)
                    mouse_enabled = 0;
                display_kbd_send_key(sdl_keysym_to_scancode(event.key.keysym.sym));
            }
            else {
                switch (event.key.keysym.sym) {
                    case SDLK_1...SDLK_6:
                        display_send_hotkey(event.key.keysym.sym - SDLK_1, 1);
                        break;
                    case SDLK_ESCAPE: {
                        display_kbd_send_key(1);
                        break;
                    }
                    case SDLK_t: {
                        scale_mode = 0;
                        display_update_scale_mode();
                        break;
                    }
                    case SDLK_y: {
                        scale_mode = 1;
                        display_update_scale_mode();
                        break;
                    }
                    case SDLK_u: {
                        scale_mode = 2;
                        display_update_scale_mode();
                        break;
                    }
                    case SDLK_c: {
#ifndef MOBILE_BUILD
                        SDL_DisplayMode mode;
                        int index = SDL_GetWindowDisplayIndex(window);
                        SDL_GetCurrentDisplayMode(index, &mode);
                        SDL_SetWindowPosition(
                            window,
                            (mode.w - w) >> 1, (mode.h - h) >> 1
                        );
#endif
                        break;
                    }
                    case SDLK_q: {
                        display_quit();
                        exit(0);
                        break;
                    }
                    case SDLK_r: {
                        scale_x = scale_y = 1.0f;
#ifdef MOBILE_BUILD
                        real_scale_x = def_real_scale_x;
                        real_scale_y = def_real_scale_y;
#else
                        if (!fullscreen)
                            SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
#endif
                        display_update_scale_mode();
                        break;
                    }
                    case SDLK_z: {
                        if (resizable) {
                            scale_x += 0.25f;
                            scale_y += 0.25f;
#ifdef MOBILE_BUILD
                            real_scale_x += 0.25f;
                            real_scale_y += 0.25f;
#else
                            if (!fullscreen)
                                SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
#endif
                            display_update_scale_mode();
                        }
                        break;
                    }
                    case SDLK_x: {
                        if (resizable && scale_x > 0.25f && scale_y > 0.25f) {
                            scale_x -= 0.25f;
                            scale_y -= 0.25f;
#ifdef MOBILE_BUILD
                            real_scale_x -= 0.25f;
                            real_scale_y -= 0.25f;
#else
                            if (!fullscreen)
                                SDL_SetWindowSize(window, (int)((float)w * scale_x), (int)((float)h * scale_y));
#endif
                            display_update_scale_mode();
                        }
                        break;
                    }
                    case SDLK_s: {
                        resizable = !resizable;
#ifndef MOBILE_BUILD
                        SDL_SetWindowResizable(window, resizable);
#endif
                        if (!resizable) {
                            scale_x = scale_y = 1.0f;
                        }
#ifndef MOBILE_BUILD
                        if (!fullscreen)
                            SDL_SetWindowSize(window, w, h);
#endif
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
#ifndef MOBILE_BUILD
                        else {
                            SDL_SetWindowSize(window, w, h);
                            if (!resizable) {
                                scale_x = scale_y = 1.0f;
                            }
                        }
#endif
                        break;
                    }
                }
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (event.button.which == SDL_TOUCH_MOUSEID)
                break;
#ifdef MOBILE_WIP
            SDL_FPoint pos = { (float)event.button.x / (float)ren_w, (float)event.button.y / (float)ren_h };
            display_on_touch_event(228, &pos, (event.type == SDL_MOUSEBUTTONDOWN) ? SDL_FINGERDOWN : SDL_FINGERUP);
            break;
#endif
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
            if (event.motion.which == SDL_TOUCH_MOUSEID)
                break;
#ifdef MOBILE_BUILD
            SDL_FPoint pos = { (float)event.motion.xrel / (float)ren_w, (float)event.motion.yrel / (float)ren_h };
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK)
                display_on_touch_event(228, &pos, SDL_FINGERMOTION);
            break;
#endif
            if (mouse_enabled)
                kbd_send_mouse_move(event.motion.xrel, event.motion.yrel, 0, 0);
            break;
        }
        case SDL_FINGERMOTION: {
#ifdef MOBILE_BUILD
            SDL_FPoint pos = { event.tfinger.dx, event.tfinger.dy };
            display_on_touch_event(event.tfinger.fingerId, &pos, SDL_FINGERMOTION);
#endif
            break;
        }
        case SDL_FINGERDOWN:
        case SDL_FINGERUP: {
#ifdef MOBILE_BUILD
            SDL_FPoint pos = { event.tfinger.x, event.tfinger.y };
            display_on_touch_event(event.tfinger.fingerId, &pos, (int)event.type);
#endif
            break;
        }
        case SDL_MOUSEWHEEL: {
            if (event.wheel.which == SDL_TOUCH_MOUSEID)
                break;
            if (mouse_enabled)
                kbd_send_mouse_move(0, 0, event.wheel.x, -event.wheel.y);
            break;
        }
        case SDL_KEYUP: {
            if (mouse_enabled) {
                int c = sdl_keysym_to_scancode(event.key.keysym.sym);
                display_kbd_send_key(c | 0x80);
            }
            else {
                switch (event.key.keysym.sym) {
                    case SDLK_1...SDLK_6:
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
                if (SDL_GetRendererOutputSize(renderer, &ren_w, &ren_h) < 0) {
                    ren_w = event.window.data1;
                    ren_h = event.window.data2;
                }
                scale_x = (float)ren_w / (float)w;
                scale_y = (float)ren_h / (float)h;
                if (SDL_GetRendererOutputSize(renderer, &ren_w, &ren_h) == 0) {
                    int win_w, win_h;
                    SDL_GetWindowSize(window, &win_w, &win_h);
                    def_real_scale_x = (float)ren_w / (float)win_w;
                    def_real_scale_y = (float)ren_h / (float)win_h;
                }
                display_update_scale_mode();
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
    HMODULE ntdll = LoadLibraryExW(L"ntdll.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ntdll)
        return;
    HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dwm) {
        FreeLibrary(ntdll);
        return;
    }
    HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!uxtheme) {
        FreeLibrary(dwm);
        FreeLibrary(ntdll);
        return;
    }
    typedef void (*RtlGetVersionPTR)(void*);
    typedef HRESULT (*DwmSetWindowAttributePTR)(HWND, DWORD, LPCVOID, DWORD);
    typedef bool (WINAPI *ShouldAppsUseDarkModePTR)();
    RtlGetVersionPTR RtlGetVersion = (RtlGetVersionPTR)((size_t)GetProcAddress(ntdll, "RtlGetVersion"));
    DwmSetWindowAttributePTR DwmSetWindowAttribute = (DwmSetWindowAttributePTR)((size_t)GetProcAddress(dwm, "DwmSetWindowAttribute"));
    struct {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
        USHORT wServicePackMajor;
        USHORT wServicePackMinor;
        USHORT wSuiteMask;
        UCHAR wProductType;
        UCHAR wReserved;
    } ntdll_ver_struct;
    memset(&ntdll_ver_struct, 0, sizeof(ntdll_ver_struct));
    ntdll_ver_struct.dwOSVersionInfoSize = sizeof(ntdll_ver_struct);
    if (!RtlGetVersion || !DwmSetWindowAttribute) {
        FreeLibrary(uxtheme);
        FreeLibrary(dwm);
        FreeLibrary(ntdll);
        return;
    }
    RtlGetVersion(&ntdll_ver_struct);
    if (ntdll_ver_struct.dwBuildNumber < 17763) {
        FreeLibrary(uxtheme);
        FreeLibrary(dwm);
        FreeLibrary(ntdll);
        return;
    }
    ShouldAppsUseDarkModePTR ShouldAppsUseDarkMode = (ShouldAppsUseDarkModePTR)((size_t)GetProcAddress(uxtheme, MAKEINTRESOURCEA(132)));
    if (!ShouldAppsUseDarkMode || !ShouldAppsUseDarkMode()) {
        FreeLibrary(uxtheme);
        FreeLibrary(dwm);
        FreeLibrary(ntdll);
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
    FreeLibrary(ntdll);
}
#endif

void* display_get_handle(int handle_id) {
    if (handle_id == 0)
        return window;
    else if (handle_id == 1)
        return renderer;
    else
        return NULL;
}

void display_init(void)
{
    if (display_inited > 0)
        return;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
        DISPLAY_FATAL("Unable to initialize SDL");
    window = SDL_CreateWindow(
        "halfix",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
#ifdef MOBILE_BUILD
        800, 480,
#else
        640, 480,
#endif
        SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN |
#ifdef MOBILE_BUILD
        SDL_WINDOW_RESIZABLE |
#else
        0 |
#endif
#if defined(MOBILE_BUILD) && !defined(MOBILE_WIP)
        SDL_WINDOW_FULLSCREEN
#else
        0
#endif
    );
    if (window == NULL)
        DISPLAY_FATAL("Unable to create window");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
        DISPLAY_FATAL("Unable to create renderer");
    int rren_w, rren_h;
    if (SDL_GetRendererOutputSize(renderer, &rren_w, &rren_h) == 0) {
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        def_real_scale_x = real_scale_x = (float)rren_w / (float)win_w;
        def_real_scale_y = real_scale_y = (float)rren_h / (float)win_h;
    }
#ifdef _WIN32
    display_check_dark_mode();
#endif
#ifdef MOBILE_BUILD
    resizable = 1;
#else
    resizable = 0;
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
            h_free(surface_pixels);
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
