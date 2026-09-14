#define UNICODE
#include "../include/window.h"
#include <windows.h>

static HWND    g_hwnd;
static int     g_client_width;
static int     g_client_height;
static uint32_t* g_screen_buffer = NULL;
static size_t g_buffer_width = 0;
static size_t g_buffer_height = 0;

window_key_callback_t window_on_key_event = NULL;

void window_set_on_key(window_key_callback_t callback) {
    window_on_key_event = callback;
}

void win32_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    if (g_screen_buffer && g_buffer_width > 0 && g_buffer_height > 0) {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = (LONG)g_buffer_width;
        bmi.bmiHeader.biHeight      = -(LONG)g_buffer_height; // Négatif = Haut en bas
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;                     // Toujours 32-bit ici !
        bmi.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(
            hdc,
            0, 0, g_client_width, g_client_height, // Taille de la fenêtre
            0, 0, g_buffer_width, g_buffer_height, // Taille de la texture d'origine
            g_screen_buffer,
            &bmi, DIB_RGB_COLORS, SRCCOPY
        );
    }

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg) {
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP: {
            uint8_t is_pressed = (uMsg == WM_KEYDOWN);
            uint8_t scancode = (HIWORD(lParam) & 0xFF);
            window_on_key_event(scancode, is_pressed);
            return 0;
        }
        case WM_PAINT: {
            win32_paint(hwnd);
            return 0;
        }
        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            g_client_width  = rect.right  - rect.left;
            g_client_height = rect.bottom - rect.top;

            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void window_create(int x, int y, int width, int height, const char* title) {

    wchar_t wtitle[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 256);

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    int client_width = rect.right - rect.left;
    int client_height = rect.bottom - rect.top;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = u"Custom";
    RegisterClass(&wc);

    g_hwnd = CreateWindow(
        u"Custom",
        wtitle,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, client_width, client_height,
        NULL, NULL, wc.hInstance, NULL
    );

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
}

uint8_t window_process_events(void) {
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if(msg.message == WM_QUIT) { return 0; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 1;
}

void window_render_from_texture(const void *pixels, size_t width, size_t height, int bpp) {
    if (!pixels) return;

    if(g_buffer_width != width || g_buffer_height != height || !g_screen_buffer) {
        g_screen_buffer = realloc(g_screen_buffer, width * height * sizeof(uint32_t));
        g_buffer_width = width;
        g_buffer_height = height;
    }

    size_t total_pixels = width * height;

    if (bpp == 32) {
        memcpy(g_screen_buffer, pixels, total_pixels * sizeof(uint32_t));
    }

    RedrawWindow(g_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void window_destroy(void) {
    if(g_screen_buffer) {
        free(g_screen_buffer);
        g_screen_buffer = NULL;
        g_buffer_width = 0;
        g_buffer_height = 0;
    }

    if(g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }

    UnregisterClass(u"Custom", GetModuleHandle(NULL));
}
