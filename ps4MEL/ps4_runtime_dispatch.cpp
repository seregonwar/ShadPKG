#include "ps4_runtime_dispatch.h"
#include "sdl_backend.h"
#include <iostream>
#include <unistd.h>

namespace PS4Runtime {

// ═══════════════════════════════════════════════════════════════════════════
// Backend implementations for known PS4 functions
// ═══════════════════════════════════════════════════════════════════════════

// GNM: Submit command buffers (placeholder - logs and returns success)
static int64_t Backend_GNM_SubmitCommandBuffers(void* obj, uint64_t* args, int argCount) {
    static uint64_t callCount = 0;
    callCount++;
    if (callCount % 60 == 1) {
        std::cout << "[GNM] SubmitCommandBuffers #" << callCount << std::endl;
    }
    return 0; // ORBIS_OK
}

// GNM: Submit and flip
static int64_t Backend_GNM_SubmitAndFlipCommandBuffers(void* obj, uint64_t* args, int argCount) {
    static uint64_t callCount = 0;
    callCount++;
    
    // Trigger SDL present
    PS4Emu::SDL::PollEvents();
    PS4Emu::SDL::Present();
    
    if (callCount % 60 == 1) {
        std::cout << "[GNM] SubmitAndFlipCommandBuffers #" << callCount << std::endl;
    }
    return 0;
}

// GNM: Clear render target
static int64_t Backend_GNM_ClearRenderTarget(void* obj, uint64_t* args, int argCount) {
    // Use SDL to clear with a color
    static uint8_t hue = 0;
    hue += 2;
    
    // Simple color cycling to show something is happening
    uint8_t r = (hue < 85) ? (255 - hue * 3) : ((hue < 170) ? 0 : ((hue - 170) * 3));
    uint8_t g = (hue < 85) ? (hue * 3) : ((hue < 170) ? (255 - (hue - 85) * 3) : 0);
    uint8_t b = (hue < 85) ? 0 : ((hue < 170) ? ((hue - 85) * 3) : (255 - (hue - 170) * 3));
    
    PS4Emu::SDL::ClearScreen(r, g, b);
    return 0;
}

// VideoOut: Submit flip
static int64_t Backend_VideoOut_SubmitFlip(void* obj, uint64_t* args, int argCount) {
    PS4Emu::SDL::PollEvents();
    PS4Emu::SDL::Present();
    return 0;
}

// VideoOut: Open
static int64_t Backend_VideoOut_Open(void* obj, uint64_t* args, int argCount) {
    // Already initialized via runtime_init
    return 1; // Return handle
}

// VideoOut: Close
static int64_t Backend_VideoOut_Close(void* obj, uint64_t* args, int argCount) {
    return 0;
}

// VideoOut: Register buffers
static int64_t Backend_VideoOut_RegisterBuffers(void* obj, uint64_t* args, int argCount) {
    std::cout << "[VideoOut] RegisterBuffers called" << std::endl;
    return 0;
}

// Pad: Read
static int64_t Backend_Pad_Read(void* obj, uint64_t* args, int argCount) {
    // Get current pad state from SDL
    auto state = PS4Emu::SDL::GetPadState();
    // The actual data would be written to args[1] (pData)
    return 1; // Return 1 = success with 1 data packet
}

// Kernel: Usleep
static int64_t Backend_Kernel_Usleep(void* obj, uint64_t* args, int argCount) {
    uint32_t usec = static_cast<uint32_t>(args[0]);
    if (usec > 0 && usec < 1000000) {
        usleep(usec);
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Initialize dispatch system with known functions
// ═══════════════════════════════════════════════════════════════════════════
void InitializeDispatch() {
    auto& registry = RuntimeRegistry::Instance();
    
    // Register semantic names
    registry.RegisterBackend(SemanticID::GNM_SubmitCommandBuffers, Backend_GNM_SubmitCommandBuffers);
    registry.RegisterBackend(SemanticID::GNM_SubmitAndFlipCommandBuffers, Backend_GNM_SubmitAndFlipCommandBuffers);
    registry.RegisterBackend(SemanticID::GNM_ClearRenderTarget, Backend_GNM_ClearRenderTarget);
    registry.RegisterBackend(SemanticID::VideoOut_SubmitFlip, Backend_VideoOut_SubmitFlip);
    registry.RegisterBackend(SemanticID::VideoOut_Open, Backend_VideoOut_Open);
    registry.RegisterBackend(SemanticID::VideoOut_Close, Backend_VideoOut_Close);
    registry.RegisterBackend(SemanticID::VideoOut_RegisterBuffers, Backend_VideoOut_RegisterBuffers);
    registry.RegisterBackend(SemanticID::Pad_Read, Backend_Pad_Read);
    registry.RegisterBackend(SemanticID::Kernel_Usleep, Backend_Kernel_Usleep);
    
    std::cout << "[DISPATCH] Runtime dispatch system initialized" << std::endl;
}

} // namespace PS4Runtime

// ═══════════════════════════════════════════════════════════════════════════
// C-compatible wrappers (used by decompiled code)
// ═══════════════════════════════════════════════════════════════════════════
extern "C" {

int64_t ps4_vtable_dispatch(void* obj, uint64_t offset,
                             uint64_t a1, uint64_t a2, uint64_t a3,
                             uint64_t a4, uint64_t a5, uint64_t a6) {
    return PS4Runtime::ps4_vtable_dispatch(obj, offset, a1, a2, a3, a4, a5, a6);
}

int64_t ps4_indirect_dispatch(void* fnPtr,
                               uint64_t a1, uint64_t a2, uint64_t a3,
                               uint64_t a4, uint64_t a5, uint64_t a6) {
    return PS4Runtime::ps4_indirect_dispatch(fnPtr, a1, a2, a3, a4, a5, a6);
}

}
