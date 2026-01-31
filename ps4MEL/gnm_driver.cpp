/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   GNM DRIVER - PS4 Graphics API Layer                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Translates PS4 GNM calls to Vulkan/SDL rendering                         ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "gnm_driver.h"
#include "sdl_backend.h"
#include <iostream>
#include <cstring>

namespace PS4Emu {
namespace GNM {

// Statistics
static uint64_t g_drawCallCount = 0;
static uint64_t g_frameCount = 0;
static uint64_t g_submitCount = 0;
static bool g_initialized = false;

// Forward declarations
void ParseCommandBuffer(const void* dcb, uint32_t sizeInBytes);

// Current render state
static void* g_currentVsShader = nullptr;
static void* g_currentPsShader = nullptr;
static void* g_currentRenderTarget = nullptr;
static int32_t g_videoOutHandle = -1;

bool Initialize() {
    if (g_initialized) return true;
    
    std::cout << "[GNM] Initializing GNM Driver..." << std::endl;
    g_drawCallCount = 0;
    g_frameCount = 0;
    g_submitCount = 0;
    g_initialized = true;
    
    std::cout << "[GNM] GNM Driver initialized (SDL backend)" << std::endl;
    return true;
}

void Shutdown() {
    if (!g_initialized) return;
    
    std::cout << "[GNM] Shutting down GNM Driver..." << std::endl;
    PrintStats();
    g_initialized = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// COMMAND BUFFER SUBMISSION
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmSubmitCommandBuffers(uint32_t count, void** dcbGpuAddrs, 
                                    uint32_t* dcbSizesInBytes,
                                    void** ccbGpuAddrs, uint32_t* ccbSizesInBytes) {
    g_submitCount++;
    
    // Parse PM4 command buffers
    for (uint32_t i = 0; i < count; i++) {
        if (dcbGpuAddrs && dcbGpuAddrs[i] && dcbSizesInBytes) {
            ParseCommandBuffer(dcbGpuAddrs[i], dcbSizesInBytes[i]);
        }
    }
    
    if (g_submitCount % 60 == 0) {
        std::cout << "[GNM] SubmitCommandBuffers: count=" << count 
                  << " (total submits: " << g_submitCount << ", draws: " << g_drawCallCount << ")" << std::endl;
    }
    
    return 0; // SCE_OK
}

int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void** dcbGpuAddrs,
                                           uint32_t* dcbSizesInBytes,
                                           void** ccbGpuAddrs, uint32_t* ccbSizesInBytes,
                                           int32_t videoOutHandle, int32_t flipArg,
                                           void* flipMode, int64_t flipArg2) {
    // Submit command buffers
    sceGnmSubmitCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes, 
                                ccbGpuAddrs, ccbSizesInBytes);
    
    // Flip (present frame)
    g_frameCount++;
    SDL::Present();
    
    if (g_frameCount % 60 == 0) {
        std::cout << "[GNM] Frame " << g_frameCount << " (draws: " << g_drawCallCount << ")" << std::endl;
    }
    
    return 0;
}

int32_t sceGnmSubmitDone() {
    // Signal that all submissions are complete
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// HARDWARE STATE INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmDrawInitDefaultHardwareState(void* dcb, uint32_t numDwords) {
    std::cout << "[GNM] DrawInitDefaultHardwareState" << std::endl;
    return 0;
}

int32_t sceGnmDrawInitDefaultHardwareState350(void* dcb, uint32_t numDwords) {
    std::cout << "[GNM] DrawInitDefaultHardwareState350" << std::endl;
    return 0;
}

int32_t sceGnmDispatchInitDefaultHardwareState(void* dcb, uint32_t numDwords) {
    std::cout << "[GNM] DispatchInitDefaultHardwareState" << std::endl;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// DRAW COMMANDS
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmDrawIndex(void* dcb, uint32_t indexCount, void* indexAddr, 
                        uint32_t predAndMod, uint32_t inlineMode) {
    g_drawCallCount++;
    
    // In a real implementation, we would:
    // 1. Bind the current shaders
    // 2. Set up vertex/index buffers
    // 3. Issue a Vulkan draw call
    
    return 0;
}

int32_t sceGnmDrawIndexAuto(void* dcb, uint32_t indexCount, uint32_t predAndMod) {
    g_drawCallCount++;
    return 0;
}

int32_t sceGnmDrawIndexOffset(void* dcb, uint32_t indexOffset, uint32_t indexCount,
                               uint32_t predAndMod) {
    g_drawCallCount++;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// SHADER SETUP
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmSetVsShader(void* dcb, void* shader, uint32_t shaderModifier) {
    g_currentVsShader = shader;
    return 0;
}

int32_t sceGnmSetPsShader(void* dcb, void* shader) {
    g_currentPsShader = shader;
    return 0;
}

int32_t sceGnmSetCsShader(void* dcb, void* shader) {
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// RESOURCE BINDING
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmSetVSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* buffer) {
    return 0;
}

int32_t sceGnmSetTSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* texture) {
    return 0;
}

int32_t sceGnmSetSSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* sampler) {
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// RENDER TARGET
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceGnmSetRenderTarget(void* dcb, uint32_t rtSlot, void* target) {
    g_currentRenderTarget = target;
    return 0;
}

int32_t sceGnmSetDepthRenderTarget(void* dcb, void* depthTarget) {
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// VIDEO OUTPUT
// ═══════════════════════════════════════════════════════════════════════════

int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, void* param) {
    std::cout << "[GNM] sceVideoOutOpen: userId=" << userId << std::endl;
    g_videoOutHandle = 1; // Return a valid handle
    return g_videoOutHandle;
}

int32_t sceVideoOutClose(int32_t handle) {
    std::cout << "[GNM] sceVideoOutClose: handle=" << handle << std::endl;
    g_videoOutHandle = -1;
    return 0;
}

int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate) {
    std::cout << "[GNM] sceVideoOutSetFlipRate: rate=" << rate << std::endl;
    return 0;
}

int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufferIndex, 
                               uint32_t flipMode, int64_t flipArg) {
    g_frameCount++;
    SDL::Present();
    return 0;
}

int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex,
                                    void** addresses, int32_t bufferNum,
                                    void* attribute) {
    std::cout << "[GNM] sceVideoOutRegisterBuffers: bufferNum=" << bufferNum << std::endl;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// FRAMEBUFFER MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

static uint32_t* g_framebuffer = nullptr;
static uint32_t g_fbWidth = 1920;
static uint32_t g_fbHeight = 1080;
static uint32_t g_fbPitch = 1920;

void SetFramebuffer(uint32_t* pixels, uint32_t width, uint32_t height, uint32_t pitch) {
    g_framebuffer = pixels;
    g_fbWidth = width;
    g_fbHeight = height;
    g_fbPitch = pitch;
}

uint32_t* GetFramebuffer() { return g_framebuffer; }
uint32_t GetFramebufferWidth() { return g_fbWidth; }
uint32_t GetFramebufferHeight() { return g_fbHeight; }

// ═══════════════════════════════════════════════════════════════════════════
// SOFTWARE RENDERING PRIMITIVES
// ═══════════════════════════════════════════════════════════════════════════

void ClearRenderTarget(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!g_framebuffer) return;
    uint32_t color = (a << 24) | (r << 16) | (g << 8) | b;
    for (uint32_t y = 0; y < g_fbHeight; y++) {
        for (uint32_t x = 0; x < g_fbWidth; x++) {
            g_framebuffer[y * g_fbPitch + x] = color;
        }
    }
}

void DrawRect(int x, int y, int w, int h, uint32_t color) {
    if (!g_framebuffer) return;
    for (int py = y; py < y + h && py < (int)g_fbHeight; py++) {
        if (py < 0) continue;
        for (int px = x; px < x + w && px < (int)g_fbWidth; px++) {
            if (px < 0) continue;
            g_framebuffer[py * g_fbPitch + px] = color;
        }
    }
}

void DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color) {
    if (!g_framebuffer) return;
    // Simple bounding box rasterization
    int minX = (int)std::min({x0, x1, x2});
    int maxX = (int)std::max({x0, x1, x2});
    int minY = (int)std::min({y0, y1, y2});
    int maxY = (int)std::max({y0, y1, y2});
    
    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min((int)g_fbWidth - 1, maxX);
    maxY = std::min((int)g_fbHeight - 1, maxY);
    
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            // Barycentric coordinates
            float w0 = (x1 - x0) * (py - y0) - (y1 - y0) * (px - x0);
            float w1 = (x2 - x1) * (py - y1) - (y2 - y1) * (px - x1);
            float w2 = (x0 - x2) * (py - y2) - (y0 - y2) * (px - x2);
            
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                g_framebuffer[py * g_fbPitch + px] = color;
            }
        }
    }
    g_drawCallCount++;
}

// ═══════════════════════════════════════════════════════════════════════════
// PM4 COMMAND BUFFER PARSING
// ═══════════════════════════════════════════════════════════════════════════

// PM4 Packet Types
enum PM4PacketType {
    PM4_TYPE0 = 0,
    PM4_TYPE2 = 2,
    PM4_TYPE3 = 3
};

// PM4 Type 3 Opcodes (subset)
enum PM4Opcode {
    PM4_NOP = 0x10,
    PM4_SET_CONTEXT_REG = 0x69,
    PM4_SET_SH_REG = 0x76,
    PM4_DRAW_INDEX_2 = 0x27,
    PM4_DRAW_INDEX_AUTO = 0x2D,
    PM4_DISPATCH_DIRECT = 0x15,
    PM4_EVENT_WRITE = 0x46,
    PM4_EVENT_WRITE_EOP = 0x47,
    PM4_RELEASE_MEM = 0x49,
    PM4_ACQUIRE_MEM = 0x58,
};

void ParseCommandBuffer(const void* dcb, uint32_t sizeInBytes) {
    if (!dcb || sizeInBytes < 4) return;
    
    const uint32_t* cmd = static_cast<const uint32_t*>(dcb);
    uint32_t offset = 0;
    uint32_t numDwords = sizeInBytes / 4;
    
    while (offset < numDwords) {
        uint32_t header = cmd[offset];
        uint32_t type = (header >> 30) & 0x3;
        
        if (type == PM4_TYPE3) {
            uint32_t opcode = (header >> 8) & 0xFF;
            uint32_t count = (header & 0x3FFF) + 1;
            
            switch (opcode) {
                case PM4_DRAW_INDEX_2:
                case PM4_DRAW_INDEX_AUTO:
                    g_drawCallCount++;
                    break;
                case PM4_NOP:
                    break;
                default:
                    break;
            }
            
            offset += count + 1;
        } else if (type == PM4_TYPE2) {
            offset++;
        } else {
            offset++;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATISTICS
// ═══════════════════════════════════════════════════════════════════════════

void PrintStats() {
    std::cout << "[GNM] === GNM Driver Statistics ===" << std::endl;
    std::cout << "[GNM] Total frames: " << g_frameCount << std::endl;
    std::cout << "[GNM] Total draw calls: " << g_drawCallCount << std::endl;
    std::cout << "[GNM] Total submits: " << g_submitCount << std::endl;
}

uint64_t GetDrawCallCount() {
    return g_drawCallCount;
}

uint64_t GetFrameCount() {
    return g_frameCount;
}

} // namespace GNM
} // namespace PS4Emu

// ═══════════════════════════════════════════════════════════════════════════
// C-COMPATIBLE EXPORTS for decompiled code
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {

int32_t sceGnmSubmitCommandBuffers(uint32_t count, void** dcbGpuAddrs, 
                                    uint32_t* dcbSizesInBytes,
                                    void** ccbGpuAddrs, uint32_t* ccbSizesInBytes) {
    return PS4Emu::GNM::sceGnmSubmitCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes,
                                                    ccbGpuAddrs, ccbSizesInBytes);
}

int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void** dcbGpuAddrs,
                                           uint32_t* dcbSizesInBytes,
                                           void** ccbGpuAddrs, uint32_t* ccbSizesInBytes,
                                           int32_t videoOutHandle, int32_t flipArg,
                                           void* flipMode, int64_t flipArg2) {
    return PS4Emu::GNM::sceGnmSubmitAndFlipCommandBuffers(count, dcbGpuAddrs, dcbSizesInBytes,
                                                           ccbGpuAddrs, ccbSizesInBytes,
                                                           videoOutHandle, flipArg, flipMode, flipArg2);
}

int32_t sceGnmSubmitDone() {
    return PS4Emu::GNM::sceGnmSubmitDone();
}

int32_t sceGnmDrawInitDefaultHardwareState(void* dcb, uint32_t numDwords) {
    return PS4Emu::GNM::sceGnmDrawInitDefaultHardwareState(dcb, numDwords);
}

int32_t sceGnmDrawIndex(void* dcb, uint32_t indexCount, void* indexAddr, 
                        uint32_t predAndMod, uint32_t inlineMode) {
    return PS4Emu::GNM::sceGnmDrawIndex(dcb, indexCount, indexAddr, predAndMod, inlineMode);
}

int32_t sceGnmDrawIndexAuto(void* dcb, uint32_t indexCount, uint32_t predAndMod) {
    return PS4Emu::GNM::sceGnmDrawIndexAuto(dcb, indexCount, predAndMod);
}

int32_t sceGnmSetVsShader(void* dcb, void* shader, uint32_t shaderModifier) {
    return PS4Emu::GNM::sceGnmSetVsShader(dcb, shader, shaderModifier);
}

int32_t sceGnmSetPsShader(void* dcb, void* shader) {
    return PS4Emu::GNM::sceGnmSetPsShader(dcb, shader);
}

int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, void* param) {
    return PS4Emu::GNM::sceVideoOutOpen(userId, busType, index, param);
}

int32_t sceVideoOutClose(int32_t handle) {
    return PS4Emu::GNM::sceVideoOutClose(handle);
}

int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate) {
    return PS4Emu::GNM::sceVideoOutSetFlipRate(handle, rate);
}

int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufferIndex, 
                               uint32_t flipMode, int64_t flipArg) {
    return PS4Emu::GNM::sceVideoOutSubmitFlip(handle, bufferIndex, flipMode, flipArg);
}

int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex,
                                    void** addresses, int32_t bufferNum,
                                    void* attribute) {
    return PS4Emu::GNM::sceVideoOutRegisterBuffers(handle, startIndex, addresses, bufferNum, attribute);
}

} // extern "C"
