/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      PS4 GRAPHICS STUBS - HEADER                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Emulates sceVideoOut* and sceGnm* graphics functions.                    ║
 * ║  These are stubs - no actual rendering occurs.                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#pragma once

#include <cstdint>

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    VIDEO OUTPUT CONSTANTS                       │
 * └─────────────────────────────────────────────────────────────────┘
 */

#define SCE_VIDEO_OUT_BUS_TYPE_MAIN 0
#define SCE_VIDEO_OUT_BUS_TYPE_SUB 1

// Pixel formats
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB 0x80000000
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB 0x80002200
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A2R10G10B10 0x80002100

// Flip modes
#define SCE_VIDEO_OUT_FLIP_MODE_VSYNC 1
#define SCE_VIDEO_OUT_FLIP_MODE_HSYNC 2

// Resolution
#define SCE_VIDEO_OUT_RESOLUTION_1920x1080 1
#define SCE_VIDEO_OUT_RESOLUTION_3840x2160 2

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    GNM DRIVER CONSTANTS                         │
 * └─────────────────────────────────────────────────────────────────┘
 */

// Submit flags
#define SCE_GNM_SUBMIT_DEFAULT 0
#define SCE_GNM_SUBMIT_FLIP 1

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    STRUCTURES                                   │
 * └─────────────────────────────────────────────────────────────────┘
 */

struct SceVideoOutBufferAttribute {
  int32_t pixelFormat;
  int32_t tilingMode;
  int32_t aspectRatio;
  uint32_t width;
  uint32_t height;
  uint32_t pitchInPixel;
};

struct SceVideoOutFlipStatus {
  uint64_t count;
  uint64_t processTime;
  uint64_t tsc;
  int64_t flipArg;
  uint64_t submitTsc;
  uint64_t gcQueueNum;
  uint64_t flipPendingNum;
  uint64_t currentBuffer;
};

struct SceVideoOutResolutionStatus {
  uint32_t width;
  uint32_t height;
  uint32_t paneWidth;
  uint32_t paneHeight;
  uint64_t refreshRate;
  float aspectRatio;
  uint8_t padding[8];
};

/*
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    VIDEO OUTPUT FUNCTIONS                       │
 * └─────────────────────────────────────────────────────────────────┘
 */

extern "C" {

// ═══════════════════════════════════════════════════════════════════
// VIDEO OUT
// ═══════════════════════════════════════════════════════════════════
int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index,
                        const void *param);
int32_t sceVideoOutClose(int32_t handle);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex,
                                   void *const *addresses, int32_t bufferNum,
                                   const SceVideoOutBufferAttribute *attr);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufferIndex,
                              int32_t flipMode, int64_t flipArg);
int32_t sceVideoOutGetFlipStatus(int32_t handle, SceVideoOutFlipStatus *status);
int32_t sceVideoOutGetResolutionStatus(int32_t handle,
                                       SceVideoOutResolutionStatus *status);
void sceVideoOutSetBufferAttribute(SceVideoOutBufferAttribute *attr,
                                   int32_t pixelFormat, int32_t tilingMode,
                                   int32_t aspectRatio, uint32_t width,
                                   uint32_t height, uint32_t pitchInPixel);
int32_t sceVideoOutWaitVblank(int32_t handle);

// ═══════════════════════════════════════════════════════════════════
// GNM DRIVER (GPU COMMAND SUBMISSION)
// ═══════════════════════════════════════════════════════════════════
int32_t sceGnmSubmitCommandBuffers(uint32_t count, void *const *dcbGpuAddrs,
                                   const uint32_t *dcbSizesInBytes,
                                   void *const *ccbGpuAddrs,
                                   const uint32_t *ccbSizesInBytes);
int32_t sceGnmSubmitAndFlipCommandBuffers(
    uint32_t count, void *const *dcbGpuAddrs, const uint32_t *dcbSizesInBytes,
    void *const *ccbGpuAddrs, const uint32_t *ccbSizesInBytes,
    int32_t videoHandle, int32_t bufferIndex, int32_t flipMode,
    int64_t flipArg);
void sceGnmFlushGarlic();
uint32_t sceGnmGetGpuCoreClockFrequency();
int32_t sceGnmIsUserPaEnabled();
void *sceGnmGetTheTessellationFactorRingBufferBaseAddress();

// ═══════════════════════════════════════════════════════════════════
// PAD (CONTROLLER INPUT)
// ═══════════════════════════════════════════════════════════════════
int32_t scePadOpen(int32_t userId, int32_t type, int32_t index,
                   const void *param);
int32_t scePadClose(int32_t handle);
int32_t scePadRead(int32_t handle, void *data, int32_t num);
int32_t scePadReadState(int32_t handle, void *data);
int32_t scePadSetVibration(int32_t handle, const void *param);
int32_t scePadResetLightBar(int32_t handle);
int32_t scePadSetLightBar(int32_t handle, const void *param);

// ═══════════════════════════════════════════════════════════════════
// AUDIO OUTPUT
// ═══════════════════════════════════════════════════════════════════
int32_t sceAudioOutOpen(int32_t userId, int32_t type, int32_t index,
                        uint32_t len, uint32_t freq, uint32_t param);
int32_t sceAudioOutClose(int32_t handle);
int32_t sceAudioOutOutput(int32_t handle, const void *ptr);
int32_t sceAudioOutSetVolume(int32_t handle, int32_t flag, int32_t *vol);

} // extern "C"
