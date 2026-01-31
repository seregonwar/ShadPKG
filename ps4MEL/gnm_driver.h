/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   GNM DRIVER - PS4 Graphics API Layer                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Translates PS4 GNM calls to Vulkan/SDL rendering                         ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef GNM_DRIVER_H
#define GNM_DRIVER_H

#include <cstdint>

namespace PS4Emu {
namespace GNM {

// Initialize GNM driver
bool Initialize();
void Shutdown();

// Command Buffer Submission
int32_t sceGnmSubmitCommandBuffers(uint32_t count, void** dcbGpuAddrs, 
                                    uint32_t* dcbSizesInBytes,
                                    void** ccbGpuAddrs, uint32_t* ccbSizesInBytes);

int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, void** dcbGpuAddrs,
                                           uint32_t* dcbSizesInBytes,
                                           void** ccbGpuAddrs, uint32_t* ccbSizesInBytes,
                                           int32_t videoOutHandle, int32_t flipArg,
                                           void* flipMode, int64_t flipArg2);

int32_t sceGnmSubmitDone();

// Hardware State Initialization
int32_t sceGnmDrawInitDefaultHardwareState(void* dcb, uint32_t numDwords);
int32_t sceGnmDrawInitDefaultHardwareState350(void* dcb, uint32_t numDwords);
int32_t sceGnmDispatchInitDefaultHardwareState(void* dcb, uint32_t numDwords);

// Draw Commands
int32_t sceGnmDrawIndex(void* dcb, uint32_t indexCount, void* indexAddr, 
                        uint32_t predAndMod, uint32_t inlineMode);
int32_t sceGnmDrawIndexAuto(void* dcb, uint32_t indexCount, uint32_t predAndMod);
int32_t sceGnmDrawIndexOffset(void* dcb, uint32_t indexOffset, uint32_t indexCount,
                               uint32_t predAndMod);

// Shader Setup
int32_t sceGnmSetVsShader(void* dcb, void* shader, uint32_t shaderModifier);
int32_t sceGnmSetPsShader(void* dcb, void* shader);
int32_t sceGnmSetCsShader(void* dcb, void* shader);

// Resource Binding
int32_t sceGnmSetVSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* buffer);
int32_t sceGnmSetTSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* texture);
int32_t sceGnmSetSSharpInUserData(void* dcb, uint32_t stage, uint32_t startSlot, void* sampler);

// Render Target
int32_t sceGnmSetRenderTarget(void* dcb, uint32_t rtSlot, void* target);
int32_t sceGnmSetDepthRenderTarget(void* dcb, void* depthTarget);

// Video Output
int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, void* param);
int32_t sceVideoOutClose(int32_t handle);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufferIndex, 
                               uint32_t flipMode, int64_t flipArg);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex,
                                    void** addresses, int32_t bufferNum,
                                    void* attribute);

// Framebuffer Management
void SetFramebuffer(uint32_t* pixels, uint32_t width, uint32_t height, uint32_t pitch);
uint32_t* GetFramebuffer();
uint32_t GetFramebufferWidth();
uint32_t GetFramebufferHeight();

// PM4 Command Buffer Parsing
void ParseCommandBuffer(const void* dcb, uint32_t sizeInBytes);

// Software Rendering Primitives
void ClearRenderTarget(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
void DrawRect(int x, int y, int w, int h, uint32_t color);

// Statistics
void PrintStats();
uint64_t GetDrawCallCount();
uint64_t GetFrameCount();

} // namespace GNM
} // namespace PS4Emu

#endif // GNM_DRIVER_H
