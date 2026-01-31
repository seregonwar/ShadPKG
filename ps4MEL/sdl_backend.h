/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   SDL BACKEND FOR PS4 EMULATION                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Provides window, rendering, and input for decompiled PS4 games.          ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef SDL_BACKEND_H
#define SDL_BACKEND_H

#include <cstdint>
#include <string>

namespace PS4Emu {
namespace SDL {

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

bool Initialize(const std::string& title = "PS4 Game", int width = 1920, int height = 1080);
void Shutdown();
bool IsInitialized();

// ═══════════════════════════════════════════════════════════════════════════
// WINDOW & RENDERING
// ═══════════════════════════════════════════════════════════════════════════

void* GetWindowHandle();
void* GetRendererHandle();

// Frame buffer operations
void* CreateFrameBuffer(int width, int height);
void DestroyFrameBuffer(void* buffer);
void UpdateFrameBuffer(void* buffer, const void* pixels, int pitch);
void PresentFrameBuffer(void* buffer);

// Simple rendering
void ClearScreen(uint8_t r, uint8_t g, uint8_t b);
void Present();
void SetVSync(bool enabled);

// Text and dialog rendering
void DrawText(const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void DrawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool filled = true);
void ShowDialog(const char* title, const char* message);
void SetDialogMessage(const char* message);
const char* GetCurrentDialogMessage();

// ═══════════════════════════════════════════════════════════════════════════
// INPUT
// ═══════════════════════════════════════════════════════════════════════════

struct PadState {
    uint32_t buttons;      // Button bitmask
    uint8_t leftStickX;    // 0-255, 128 = center
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;
    uint8_t l2;            // 0-255
    uint8_t r2;
    bool connected;
};

// PS4 button masks (matching OrbisPadButtonDataOffset)
constexpr uint32_t PAD_L3       = 0x0002;
constexpr uint32_t PAD_R3       = 0x0004;
constexpr uint32_t PAD_OPTIONS  = 0x0008;
constexpr uint32_t PAD_UP       = 0x0010;
constexpr uint32_t PAD_RIGHT    = 0x0020;
constexpr uint32_t PAD_DOWN     = 0x0040;
constexpr uint32_t PAD_LEFT     = 0x0080;
constexpr uint32_t PAD_L2       = 0x0100;
constexpr uint32_t PAD_R2       = 0x0200;
constexpr uint32_t PAD_L1       = 0x0400;
constexpr uint32_t PAD_R1       = 0x0800;
constexpr uint32_t PAD_TRIANGLE = 0x1000;
constexpr uint32_t PAD_CIRCLE   = 0x2000;
constexpr uint32_t PAD_CROSS    = 0x4000;
constexpr uint32_t PAD_SQUARE   = 0x8000;
constexpr uint32_t PAD_TOUCHPAD = 0x100000;

void PollEvents();
bool ShouldQuit();
PadState GetPadState();

// ═══════════════════════════════════════════════════════════════════════════
// TIMING
// ═══════════════════════════════════════════════════════════════════════════

uint64_t GetTicks();        // Milliseconds since init
void Delay(uint32_t ms);
void WaitVBlank();          // Wait for ~16.67ms (60Hz)

} // namespace SDL
} // namespace PS4Emu

#endif // SDL_BACKEND_H
