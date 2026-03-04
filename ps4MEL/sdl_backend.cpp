/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   SDL BACKEND IMPLEMENTATION                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Window, rendering, and input using SDL2.                                 ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "sdl_backend.h"

#ifndef PS4_NO_SDL
#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>
#include <thread>

namespace PS4Emu {
namespace SDL {

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static bool g_initialized = false;
static bool g_shouldQuit = false;
static bool g_vsyncEnabled = true;
static PadState g_padState = {};
static uint64_t g_initTime = 0;

// Keyboard to PAD mapping
struct KeyMapping {
    SDL_Scancode key;
    uint32_t button;
};

static const KeyMapping g_keyMappings[] = {
    { SDL_SCANCODE_RETURN,  PAD_OPTIONS },
    { SDL_SCANCODE_UP,      PAD_UP },
    { SDL_SCANCODE_DOWN,    PAD_DOWN },
    { SDL_SCANCODE_LEFT,    PAD_LEFT },
    { SDL_SCANCODE_RIGHT,   PAD_RIGHT },
    { SDL_SCANCODE_Z,       PAD_CROSS },      // X button
    { SDL_SCANCODE_X,       PAD_CIRCLE },     // O button
    { SDL_SCANCODE_A,       PAD_SQUARE },     // Square
    { SDL_SCANCODE_S,       PAD_TRIANGLE },   // Triangle
    { SDL_SCANCODE_Q,       PAD_L1 },
    { SDL_SCANCODE_W,       PAD_R1 },
    { SDL_SCANCODE_E,       PAD_L2 },
    { SDL_SCANCODE_R,       PAD_R2 },
    { SDL_SCANCODE_1,       PAD_L3 },
    { SDL_SCANCODE_2,       PAD_R3 },
    { SDL_SCANCODE_SPACE,   PAD_TOUCHPAD },
};

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

bool Initialize(const std::string& title, int width, int height) {
    if (g_initialized) {
        return true;
    }

    std::cout << "[SDL] Initializing SDL2..." << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0) {
        std::cerr << "[SDL] Failed to initialize: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    g_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!g_window) {
        std::cerr << "[SDL] Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    // Create renderer with VSync
    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (g_vsyncEnabled) {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, rendererFlags);
    if (!g_renderer) {
        std::cerr << "[SDL] Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return false;
    }

    // Initialize pad state
    g_padState.connected = true;
    g_padState.leftStickX = 128;
    g_padState.leftStickY = 128;
    g_padState.rightStickX = 128;
    g_padState.rightStickY = 128;
    g_padState.buttons = 0;
    g_padState.l2 = 0;
    g_padState.r2 = 0;

    g_initTime = SDL_GetTicks64();
    g_initialized = true;
    g_shouldQuit = false;

    std::cout << "[SDL] Window created: " << width << "x" << height << std::endl;
    std::cout << "[SDL] Controls: Arrow keys=D-Pad, Z=X, X=O, A=Square, S=Triangle" << std::endl;
    std::cout << "[SDL] Q/W=L1/R1, E/R=L2/R2, Enter=Options, Space=Touchpad" << std::endl;

    return true;
}

void Shutdown() {
    if (!g_initialized) return;

    std::cout << "[SDL] Shutting down..." << std::endl;

    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }

    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }

    SDL_Quit();
    g_initialized = false;
}

bool IsInitialized() {
    return g_initialized;
}

// ═══════════════════════════════════════════════════════════════════════════
// WINDOW & RENDERING
// ═══════════════════════════════════════════════════════════════════════════

void* GetWindowHandle() {
    return g_window;
}

void* GetRendererHandle() {
    return g_renderer;
}

void* CreateFrameBuffer(int width, int height) {
    if (!g_renderer) return nullptr;
    
    SDL_Texture* texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    
    return texture;
}

void DestroyFrameBuffer(void* buffer) {
    if (buffer) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(buffer));
    }
}

void UpdateFrameBuffer(void* buffer, const void* pixels, int pitch) {
    if (!buffer || !pixels) return;
    
    SDL_UpdateTexture(
        static_cast<SDL_Texture*>(buffer),
        nullptr,
        pixels,
        pitch
    );
}

void PresentFrameBuffer(void* buffer) {
    if (!g_renderer || !buffer) return;
    
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, static_cast<SDL_Texture*>(buffer), nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

void ClearScreen(uint8_t r, uint8_t g, uint8_t b) {
    if (!g_renderer) return;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderClear(g_renderer);
}

void Present() {
    if (!g_renderer) return;
    SDL_RenderPresent(g_renderer);
}

void SetVSync(bool enabled) {
    g_vsyncEnabled = enabled;
    // Note: VSync can only be changed by recreating the renderer
}

// ═══════════════════════════════════════════════════════════════════════════
// INPUT
// ═══════════════════════════════════════════════════════════════════════════

void PollEvents() {
    SDL_Event event;
    
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                g_shouldQuit = true;
                break;
                
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool pressed = (event.type == SDL_KEYDOWN);
                SDL_Scancode scancode = event.key.keysym.scancode;
                
                // Check key mappings
                for (const auto& mapping : g_keyMappings) {
                    if (mapping.key == scancode) {
                        if (pressed) {
                            g_padState.buttons |= mapping.button;
                        } else {
                            g_padState.buttons &= ~mapping.button;
                        }
                        break;
                    }
                }
                
                // WASD for left stick
                const uint8_t* keys = SDL_GetKeyboardState(nullptr);
                
                // Left stick (IJKL or WASD alternative)
                g_padState.leftStickX = 128;
                g_padState.leftStickY = 128;
                if (keys[SDL_SCANCODE_I]) g_padState.leftStickY = 0;    // Up
                if (keys[SDL_SCANCODE_K]) g_padState.leftStickY = 255;  // Down
                if (keys[SDL_SCANCODE_J]) g_padState.leftStickX = 0;    // Left
                if (keys[SDL_SCANCODE_L]) g_padState.leftStickX = 255;  // Right
                
                // Right stick (numpad or TFGH)
                g_padState.rightStickX = 128;
                g_padState.rightStickY = 128;
                if (keys[SDL_SCANCODE_T]) g_padState.rightStickY = 0;
                if (keys[SDL_SCANCODE_G]) g_padState.rightStickY = 255;
                if (keys[SDL_SCANCODE_F]) g_padState.rightStickX = 0;
                if (keys[SDL_SCANCODE_H]) g_padState.rightStickX = 255;
                
                // L2/R2 analog (E/R keys give full press)
                g_padState.l2 = (g_padState.buttons & PAD_L2) ? 255 : 0;
                g_padState.r2 = (g_padState.buttons & PAD_R2) ? 255 : 0;
                
                // ESC to quit
                if (scancode == SDL_SCANCODE_ESCAPE && pressed) {
                    g_shouldQuit = true;
                }
                break;
            }
            
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    g_shouldQuit = true;
                }
                break;
        }
    }
}

bool ShouldQuit() {
    return g_shouldQuit;
}

PadState GetPadState() {
    return g_padState;
}

// ═══════════════════════════════════════════════════════════════════════════
// TIMING
// ═══════════════════════════════════════════════════════════════════════════

uint64_t GetTicks() {
    return SDL_GetTicks64() - g_initTime;
}

void Delay(uint32_t ms) {
    SDL_Delay(ms);
}

void WaitVBlank() {
    // If VSync is enabled, Present() already waits
    // Otherwise, manually wait for ~16.67ms
    if (!g_vsyncEnabled) {
        static auto lastVBlank = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastVBlank);
        
        const auto targetFrame = std::chrono::microseconds(16667); // 60 Hz
        if (elapsed < targetFrame) {
            std::this_thread::sleep_for(targetFrame - elapsed);
        }
        
        lastVBlank = std::chrono::steady_clock::now();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// TEXT AND DIALOG RENDERING
// ═══════════════════════════════════════════════════════════════════════════

static std::string g_dialogMessage = "";
static std::string g_dialogTitle = "";

// Simple 8x8 bitmap font (subset of ASCII printable characters)
static const uint8_t g_font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // DEL
};

void DrawText(const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!g_renderer || !text) return;
    
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    
    int curX = x;
    int curY = y;
    int scale = 2; // 2x scale for visibility
    
    for (const char* p = text; *p; p++) {
        if (*p == '\n') {
            curX = x;
            curY += 10 * scale;
            continue;
        }
        
        int charIndex = *p - 32;
        if (charIndex < 0 || charIndex >= 96) charIndex = 0;
        
        const uint8_t* glyph = g_font8x8[charIndex];
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                // Fixed: was (7 - col), now just col for correct orientation
                if (glyph[row] & (1 << col)) {
                    SDL_Rect pixel = {curX + col * scale, curY + row * scale, scale, scale};
                    SDL_RenderFillRect(g_renderer, &pixel);
                }
            }
        }
        curX += 8 * scale;
    }
}

void DrawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool filled) {
    if (!g_renderer) return;
    
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_Rect rect = {x, y, w, h};
    
    if (filled) {
        SDL_RenderFillRect(g_renderer, &rect);
    } else {
        SDL_RenderDrawRect(g_renderer, &rect);
    }
}

void SetDialogMessage(const char* message) {
    if (message) {
        g_dialogMessage = message;
    }
}

const char* GetCurrentDialogMessage() {
    return g_dialogMessage.c_str();
}

void ShowDialog(const char* title, const char* message) {
    if (!g_renderer) return;
    
    g_dialogTitle = title ? title : "";
    g_dialogMessage = message ? message : "";
    
    // Draw dialog box
    int winW = 800, winH = 600;
    int boxW = 500, boxH = 200;
    int boxX = (winW - boxW) / 2;
    int boxY = (winH - boxH) / 2;
    
    // Background
    DrawRect(boxX, boxY, boxW, boxH, 40, 40, 80, true);
    // Border
    DrawRect(boxX, boxY, boxW, boxH, 100, 100, 200, false);
    DrawRect(boxX+2, boxY+2, boxW-4, boxH-4, 80, 80, 160, false);
    
    // Title bar
    DrawRect(boxX, boxY, boxW, 30, 60, 60, 120, true);
    DrawText(title, boxX + 10, boxY + 8, 255, 255, 255);
    
    // Message
    DrawText(message, boxX + 20, boxY + 50, 220, 220, 255);
    
    // OK button hint
    DrawText("Press X to continue", boxX + 150, boxY + boxH - 35, 150, 150, 200);
}

} // namespace SDL
} // namespace PS4Emu

#else // PS4_NO_SDL - provide no-op stubs

#include <iostream>

namespace PS4Emu {
namespace SDL {

bool Initialize(const std::string& title, int width, int height) {
    std::cout << "[SDL] No-SDL stub: Initialize(\"" << title << "\")" << std::endl;
    return true;
}
void Shutdown() {}
bool IsInitialized() { return false; }
void* GetWindowHandle() { return nullptr; }
void* GetRendererHandle() { return nullptr; }
void* CreateFrameBuffer(int, int) { return nullptr; }
void DestroyFrameBuffer(void*) {}
void UpdateFrameBuffer(void*, const void*, int) {}
void PresentFrameBuffer(void*) {}
void ClearScreen(uint8_t, uint8_t, uint8_t) {}
void Present() {}
void SetVSync(bool) {}
void DrawText(const char*, int, int, uint8_t, uint8_t, uint8_t) {}
void DrawRect(int, int, int, int, uint8_t, uint8_t, uint8_t, bool) {}
void ShowDialog(const char* title, const char* message) {
    std::cout << "[DIALOG] " << (title ? title : "") << ": " << (message ? message : "") << std::endl;
}
void SetDialogMessage(const char*) {}
const char* GetCurrentDialogMessage() { return ""; }
void PollEvents() {}
bool ShouldQuit() { return false; }
PadState GetPadState() { return PadState{}; }
uint64_t GetTicks() { return 0; }
void Delay(uint32_t) {}
void WaitVBlank() {}

} // namespace SDL
} // namespace PS4Emu

#endif // PS4_NO_SDL
