// Simple display driver
#if defined(_WIN32)

#include "display.h"
#include "devices.h"
#include "util.h"
#include <stdlib.h>

#include <windows.h>
#include <windowsx.h>

#include "util.h"

// #define DISPLAY_WIN32_USE_ANSI

static HINSTANCE hInst;
static HWND hWnd;
#ifdef DISPLAY_WIN32_USE_ANSI
static WNDCLASSEXA wc;
#else
static WNDCLASSEXW wc;
#endif
static HDC dc_dest, dc_src; // Drawing contexts
static void* pixels;
static int cheight, cwidth, mouse_enabled;
static HBITMAP hBmp;
// Position of the cursor, relative to the whole screen
static int screenx, screeny;
// Position of the cursor, relative to the window
static int windowx, windowy;
static uint8_t display_inited;

enum {
    MENU_EXIT,
    MENU_SAVE_STATE,
    MENU_SEND_CTRL_ALT_DELETE,
    MENU_SEND_SHIFT_F10,
    MENU_SEND_ALT_F4,
    MENU_SEND_ALT_TAB
};

static void display_set_title(void)
{
#ifdef DISPLAY_WIN32_USE_ANSI
    char buffer[1000];
    sprintf(buffer, "Halfix x86 Emulator - [%dx%d] - %s", cwidth, cheight,
        mouse_enabled ? "Press ESC to release mouse" : "Click to capture mouse");
    SetWindowTextA(hWnd, buffer);
#else
    wchar_t buffer[1000];
    swprintf(buffer, 900, L"Halfix x86 Emulator - [%dx%d] - %s", cwidth, cheight,
        mouse_enabled ? L"Press ESC to release mouse" : L"Click to capture mouse");
    SetWindowTextW(hWnd, buffer);
#endif
}

static void display_capture_mouse(int yes)
{
    if (yes) {
        RECT rect;
        // Get window size and adjust it
        // GetWindowRect(hWnd, &rect);
        rect.left = screenx - windowx;
        rect.top = screeny - windowy;
        rect.right = rect.left + cwidth;
        rect.bottom = rect.top + cheight;
        ClipCursor(&rect);
        SetCapture(hWnd);
        ShowCursor(FALSE);
        SetCursorPos(screenx, screeny);
    } else {
        ClipCursor(NULL);
        SetCapture(NULL);
        ShowCursor(TRUE);
    }
    mouse_enabled = yes;
    display_set_title();
}

// Converts a Win32 virtual key code to a PS/2 scan code.
// See "sdl_keysym_to_scancode" in display.c for a similar implementation
// https://stanislavs.org/helppc/make_codes.html
static int win32_to_scancode(WPARAM w)
{
    // NOTE: this code might be replaced by calling MapVirtualKeyA instead
    switch (w) {
    case VK_BACK:
        return 0x0E;
    case VK_CAPITAL:
        return 0x3A;
    case VK_RETURN:
        return 0x1C;
    case VK_ESCAPE:
        return 0x01;
    case VK_MENU: // ALT
        return 0x38; // using left alt
    case VK_CONTROL:
        return 0x1D; // using left ctrl
    case VK_LSHIFT:
    case 0x10: // WTF ???
        return 0x2A;
    case VK_NUMLOCK:
        return 0x45;
    case VK_RSHIFT:
        return 0x36;
    case VK_SCROLL:
        return 0x46;
    case VK_SPACE:
        return 0x39;
    case VK_TAB:
        return 0x0F;
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
        return (int)w - VK_F1 + 0x3B;
    case VK_NUMPAD0:
        return 0x52;
    case VK_NUMPAD1:
        return 0x4F;
    case VK_NUMPAD2:
        return 0x50;
    case VK_NUMPAD3:
        return 0x51;
    case VK_NUMPAD4:
        return 0x4B;
    case VK_NUMPAD5:
        return 0x4C;
    case VK_NUMPAD6:
        return 0x4D;
    case VK_NUMPAD7:
        return 0x47;
    case VK_NUMPAD8:
        return 0x48;
    case VK_NUMPAD9:
        return 0x49;
    case VK_DECIMAL: // keypad period/del
        return 0x53;
    case VK_MULTIPLY: // keypad asterix/prtsc
        return 0x37;
    case VK_SUBTRACT: // keypad dash
        return 0x4A;
    case VK_ADD:
        return 0x4E;

    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39: // 1-9
        return (int)w + 2 - 0x31;
    case 0x30: // 0
        return 0x0B;
// lazy - copy-pasted from display.c with a few hackish macros
#define toUpper(b) b - ('a' - 'A')

    case toUpper('a'):
        return 0x1E;
    case toUpper('b'):
        return 0x30;
    case toUpper('c'):
        return 0x2E;
    case toUpper('d'):
        return 0x20;
    case toUpper('e'):
        return 0x12;
    case toUpper('f'):
        return 0x21;
    case toUpper('g'):
        return 0x22;
    case toUpper('h'):
        return 0x23;
    case toUpper('i'):
        return 0x17;
    case toUpper('j'):
        return 0x24;
    case toUpper('k'):
        return 0x25;
    case toUpper('l'):
        return 0x26;
    case toUpper('m'):
        return 0x32;
    case toUpper('n'):
        return 0x31;
    case toUpper('o'):
        return 0x18;
    case toUpper('p'):
        return 0x19;
    case toUpper('q'):
        return 0x10;
    case toUpper('r'):
        return 0x13;
    case toUpper('s'):
        return 0x1F;
    case toUpper('t'):
        return 0x14;
    case toUpper('u'):
        return 0x16;
    case toUpper('v'):
        return 0x2F;
    case toUpper('w'):
        return 0x11;
    case toUpper('x'):
        return 0x2D;
    case toUpper('y'):
        return 0x15;
    case toUpper('z'):
        return 0x2C;
    case VK_OEM_MINUS: // -/_
        return 0x0C;
    case VK_OEM_PLUS: // +/=
        return 0x0D;
    case VK_OEM_4: // {/[
        return 0x1A;
    case VK_OEM_6: // }/]
        return 0x1B;
    case VK_OEM_5: // \/|
        return 0x2B;
    case VK_OEM_1: // ;/:
        return 0x27;
    case VK_OEM_7: // '/"
        return 0x28;
    case VK_OEM_3: // `/~
        return 0x2C;
    case VK_OEM_COMMA: // ,/<
        return 0x33;
    case VK_OEM_PERIOD: // ./>
        return 0x34;
    case VK_OEM_2: // / or ?
        return 0x35;

    case VK_DELETE:
        return 0xE053;
    case VK_DOWN:
        return 0xE050;
    case VK_END:
        return 0xE04F;
    case VK_HOME:
        return 0xE047;
    case VK_INSERT:
        return 0xE052;
    case VK_LEFT:
        return 0xE048;
    case VK_PRIOR: // pgdn
        return 0xE049;
    case VK_NEXT: // pgup
        return 0xE051;
    case VK_RIGHT:
        return 0xE04D;
    case VK_UP:
        return 0xE048;
    /* default:
        printf("Unexpected Win32 virtual key code received -- aborting\n");
        abort(); */
    }
    return 0;
}

static inline void display_kbd_send_key(int k)
{
    if (!k)
        return;
    if (k & 0xFF00)
        kbd_add_key(k >> 8);
    kbd_add_key(k & 0xFF);
}

static void display_send_shortcut(WORD param, int down) {
    down = down ? 0 : 0x80;
    switch (param) {
        case MENU_SEND_CTRL_ALT_DELETE: {
            display_kbd_send_key(0x1D | down);
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0xE053 | down);
            break;
        }
        case MENU_SEND_SHIFT_F10: {
            display_kbd_send_key(0x2A | down);
            display_kbd_send_key(0x44 | down);
            break;
        }
        case MENU_SEND_ALT_F4: {
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0x3E | down);
            break;
        }
        case MENU_SEND_ALT_TAB: {
            display_kbd_send_key(0xE038 | down);
            display_kbd_send_key(0x0F | down);
            break;
        }
    }
}

static LRESULT CALLBACK display_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    char filename[4096];
    switch (msg) {
    case WM_CREATE:
        break;
    case WM_MOVE:
        // Recalculate the center of our window, if mouse capture is set
        windowx = (cwidth >> 1);
        windowy = (cheight >> 1);
        screenx = windowx + LOWORD(lparam);
        screeny = windowy + HIWORD(lparam);
        break;
    case WM_DESTROY:
        display_quit();
        exit(0);
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && mouse_enabled) {
            // display_capture_mouse(0);
        }
        else 
            display_kbd_send_key(win32_to_scancode(wparam));
        break;
    case WM_KEYUP:
        if (wparam == VK_ESCAPE && mouse_enabled) {
            display_capture_mouse(0);
        }
        else 
            display_kbd_send_key(win32_to_scancode(wparam) | 0x80);
        break;
    case WM_MOUSEMOVE:
        if (mouse_enabled) {
            // Windows gives us absolute coordinates, so we have to do the calculations ourselves
            int dx = (int)GET_X_LPARAM(lparam) - windowx, dy = (int)GET_Y_LPARAM(lparam) - windowy;
            // printf("x/y: %d, %d wx/wy: %d, %d, dx/dy: %d, %d\n", x, y, windowx, windowy, dx, dy);
            if (dx || dy) {
                kbd_send_mouse_move(dx, dy, 0, 0);
                SetCursorPos(screenx, screeny);
            }
        }
        break;
    case WM_RBUTTONDOWN:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_PRESSED);
        break;
    case WM_RBUTTONUP:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_RELEASED);
        break;
    case WM_LBUTTONDOWN:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_PRESSED, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE);
        break;
    case WM_LBUTTONUP:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_RELEASED, MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_NOCHANGE);
        else
            display_capture_mouse(1);
        break;
    case WM_MBUTTONDOWN:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_PRESSED, MOUSE_STATUS_NOCHANGE);
        break;
    case WM_MBUTTONUP:
        if (mouse_enabled)
            kbd_mouse_down(MOUSE_STATUS_NOCHANGE, MOUSE_STATUS_RELEASED, MOUSE_STATUS_NOCHANGE);
        break;
    case WM_MOUSEWHEEL:
        if (mouse_enabled) {
            int wdy = (int)GET_WHEEL_DELTA_WPARAM(wparam);
            kbd_send_mouse_move(0, 0, 0, -wdy / 120);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case MENU_EXIT:
            display_quit();
            exit(0);
        case MENU_SAVE_STATE: {
            // this currently doesn't support unicode
            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(OPENFILENAMEA));

            ofn.lStructSize = sizeof(OPENFILENAMEA);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "All files (*.)\0*.*\0\0";
            filename[0] = 0;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = 4095;
            ofn.Flags = OFN_EXPLORER;
            ofn.lpstrDefExt = "";
            ofn.lpstrTitle = "Save state to...";
            ofn.lpstrInitialDir = ".";

            if (GetSaveFileNameA(&ofn)) {
                state_store_to_file(filename);
                // printf("SELECTED\n");
            } else {
                // printf("NOT SELECTED\n");
            }
            break;
        }
        case MENU_SEND_CTRL_ALT_DELETE:
        case MENU_SEND_SHIFT_F10:
        case MENU_SEND_ALT_F4:
        case MENU_SEND_ALT_TAB: {
            display_send_shortcut(LOWORD(wparam), 1);
            display_send_shortcut(LOWORD(wparam), 0);
            break;
        }
        }
    }
#ifdef DISPLAY_WIN32_USE_ANSI
    return DefWindowProcA(hwnd, msg, wparam, lparam);
#else
    return DefWindowProcW(hwnd, msg, wparam, lparam);
#endif
}

void display_init(void)
{
#ifdef DISPLAY_WIN32_USE_ANSI
    ZeroMemory(&wc, sizeof(WNDCLASSEXA));
    hInst = GetModuleHandleA(NULL);
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpszClassName = "Halfix";
    wc.lpszMenuName = "HalfixMenu";
    wc.hCursor = LoadCursorA(0, MAKEINTRESOURCEA(32512));
#else
    ZeroMemory(&wc, sizeof(WNDCLASSEXW));
    hInst = GetModuleHandleW(NULL);
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpszClassName = L"Halfix";
    wc.lpszMenuName = L"HalfixMenu";
    wc.hCursor = LoadCursorW(0, MAKEINTRESOURCEW(32512));
#endif
    wc.hInstance = hInst;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpfnWndProc = display_callback;

#ifdef DISPLAY_WIN32_USE_ANSI
    RegisterClassExA(&wc);
#else
    RegisterClassExW(&wc);
#endif

    HMENU bar = CreateMenu(),
          file = CreateMenu(),
          hotkeys = CreateMenu();

#ifdef DISPLAY_WIN32_USE_ANSI
    AppendMenuA(file, MF_STRING, MENU_EXIT, "&Exit");
    AppendMenuA(file, MF_STRING, MENU_SAVE_STATE, "&Save State");

    AppendMenuA(hotkeys, MF_STRING, MENU_SEND_CTRL_ALT_DELETE, "&Ctrl + Alt + Delete");
    AppendMenuA(hotkeys, MF_STRING, MENU_SEND_SHIFT_F10, "&Shift + F10");
    AppendMenuA(hotkeys, MF_STRING, MENU_SEND_ALT_F4, "&Alt + F4");
    AppendMenuA(hotkeys, MF_STRING, MENU_SEND_ALT_TAB, "Alt + &Tab");

    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, "&File");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)hotkeys, "&Hotkeys");
#else
    AppendMenuW(file, MF_STRING, MENU_EXIT, L"&Exit");
    AppendMenuW(file, MF_STRING, MENU_SAVE_STATE, L"&Save State");

    AppendMenuW(hotkeys, MF_STRING, MENU_SEND_CTRL_ALT_DELETE, L"&Ctrl + Alt + Delete");
    AppendMenuW(hotkeys, MF_STRING, MENU_SEND_SHIFT_F10, L"&Shift + F10");
    AppendMenuW(hotkeys, MF_STRING, MENU_SEND_ALT_F4, L"&Alt + F4");
    AppendMenuW(hotkeys, MF_STRING, MENU_SEND_ALT_TAB, L"Alt + &Tab");

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)hotkeys, L"&Hotkeys");
#endif

#ifdef DISPLAY_WIN32_USE_ANSI
    hWnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Halfix",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        // Create it at some random spot
        100,
        100,
        // Make it 640x480
        640,
        480,
        // No parent window
        NULL,
        // Menu
        bar,
        // HINSTANCE
        hInst,
        // void
        NULL
    );
#else
    hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Halfix",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        // Create it at some random spot
        100,
        100,
        // Make it 640x480
        640,
        480,
        // No parent window
        NULL,
        // Menu
        bar,
        // HINSTANCE
        hInst,
        // void
        NULL
    );
#endif

    dc_dest = GetDC(hWnd);
    display_inited = 1;

    display_set_resolution(640, 400);
    // SetForegroundWindow(hWnd);

    // Now let it run through a few events.
    display_handle_events();
}

void display_quit(void) {
    if (!display_inited)
        return;
    display_inited = 0;
    if (dc_src)
        DeleteObject(dc_src);
    if (dc_dest)
        DeleteObject(dc_dest);
    if (hBmp)
        DeleteObject(hBmp);
    if (hWnd && 0)
        DestroyWindow(hWnd);
}

void display_update(int scanline_start, int scanlines)
{
    HDC hdc, mdc;
    hdc = GetDC(hWnd);
    mdc = CreateCompatibleDC(hdc);
    SelectObject(mdc, hBmp);
    BitBlt(
        // Destination context
        hdc,
        // Destination is at (0, 0)
        0, scanline_start,
        // Copy the entire rectangle
        cwidth, scanlines,
        // Our device context source
        dc_src,
        // Copy from top corner of rectangle
        0, scanline_start,
        // Just copy -- don't do anything fancy.
        SRCCOPY
    );
    ReleaseDC(hWnd, hdc);
    UNUSED(scanline_start | scanlines);
}
void display_set_resolution(int width, int height)
{
    // CreateDIB section doesn't like it when our width/height are zero.
    if (width == 0 || height == 0)
        return;
    if (dc_src)
        DeleteObject(dc_src);
    dc_src = CreateCompatibleDC(dc_dest);

    BITMAPINFO i;
    ZeroMemory(&i.bmiHeader, sizeof(BITMAPINFOHEADER));
    i.bmiHeader.biWidth = width;
    // Force the DIB to follow VGA/VESA rules:
    //  "If biHeight is negative, the bitmap is a top-down DIB with the origin at the upper left corner."
    i.bmiHeader.biHeight = -height;
    i.bmiHeader.biPlanes = 1;
    i.bmiHeader.biBitCount = 32;
    i.bmiHeader.biCompression = BI_RGB;
    i.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    void* pvBits;
    HDC hdc = GetDC(hWnd);
    if (hBmp)
        DeleteObject(hBmp);
    hBmp = CreateDIBSection(hdc, &i, DIB_RGB_COLORS, &pvBits, NULL, 0);
    ReleaseDC(hWnd, hdc);
    if (!hBmp) {
        // printf("Failed to create DIB section: %p [%dx%d]\n", dc_dest, width, height);
        abort();
    }
    pixels = pvBits;

    SelectObject(dc_src, hBmp);

    cheight = height;
    cwidth = width;
    display_set_title();

    // Update window size
    RECT rect;
    // x-coordinate of the upper left corner
    rect.left = 0;
    // y-coordinate
    rect.top = 0;
    // x-coordinate of the lower left corner
    rect.right = cwidth;
    // y-coordinate
    rect.bottom = cheight;
    if (!AdjustWindowRectEx(&rect, GetWindowLong(hWnd, GWL_STYLE), TRUE, 0)) {
        // printf("Failed to AdjustWindowRect\n");
        exit(0);
    }
    SetWindowPos(hWnd, (HWND)0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOOWNERZORDER);
    LONG offsetx = -rect.left;
    LONG offsety = -rect.top;
    GetWindowRect(hWnd, &rect);
    windowx = (cwidth >> 1);
    windowy = (cheight >> 1);
    screenx = rect.left + offsetx + windowx;
    screeny = rect.top + offsety + windowy;
}
void* display_get_pixels(void)
{
    return pixels;
}
void display_handle_events(void)
{
    MSG blah;
#ifdef DISPLAY_WIN32_USE_ANSI
    while (PeekMessageA(&blah, hWnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&blah);
        DispatchMessageA(&blah);
    }
#else
    while (PeekMessageW(&blah, hWnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&blah);
        DispatchMessageW(&blah);
    }
#endif
}
void display_release_mouse(void)
{
}
void display_sleep(int ms)
{
    UNUSED(ms);
}

#if 0
void display_init(void);
void display_update(int scanline_start, int scanlines);
void display_set_resolution(int width, int height);
void* display_get_pixels(void);
void display_handle_events(void);
void display_update_cycles(int cycles_elapsed, int us);
void display_sleep(int ms);

void display_release_mouse(void);
#endif

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