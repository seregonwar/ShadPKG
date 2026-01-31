/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PS4 SYSTEM STUBS - IMPLEMENTATION                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Complete mock implementations based on shadPS4 emulator.                 ║
 * ║  Reference: https://github.com/shadps4-emu/shadPS4                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include "ps4_stubs.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <atomic>

// ═══════════════════════════════════════════════════════════════════════════
// LOGGING CONTROL
// ═══════════════════════════════════════════════════════════════════════════

#ifndef PS4_STUB_LOG_LEVEL
#define PS4_STUB_LOG_LEVEL 1  // 0=none, 1=errors, 2=info, 3=debug
#endif

#if PS4_STUB_LOG_LEVEL >= 1
#define LOG_ERROR(name) std::cerr << "[PS4Stub:ERROR] " << name << std::endl
#else
#define LOG_ERROR(name)
#endif

#if PS4_STUB_LOG_LEVEL >= 2
#define LOG_INFO(name) std::cout << "[PS4Stub:INFO] " << name << std::endl
#else
#define LOG_INFO(name)
#endif

#if PS4_STUB_LOG_LEVEL >= 3
#define LOG_DEBUG(name) std::cout << "[PS4Stub:DEBUG] " << name << std::endl
#else
#define LOG_DEBUG(name)
#endif

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

namespace {
    // PAD state
    bool g_pad_initialized = false;
    bool g_pad_opened = false;
    std::atomic<u64> g_pad_timestamp{0};
    
    // Audio state
    bool g_audio_initialized = false;
    std::atomic<int> g_audio_next_handle{1};
    
    // Video state
    bool g_video_initialized = false;
    std::atomic<int> g_video_next_handle{1};
    std::atomic<u64> g_flip_count{0};
    std::atomic<u64> g_vblank_count{0};
    
    // User state
    bool g_user_initialized = false;
    constexpr s32 DEFAULT_USER_ID = 1;
    
    // Timing
    auto g_start_time = std::chrono::steady_clock::now();
    
    u64 GetCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now - g_start_time).count();
    }
}

extern "C" {

// ═══════════════════════════════════════════════════════════════════════════
// PAD (CONTROLLER) - Based on shadPS4 pad.cpp
// ═══════════════════════════════════════════════════════════════════════════

s32 scePadInit() {
    LOG_INFO("scePadInit()");
    g_pad_initialized = true;
    return ORBIS_OK;
}

s32 scePadOpen(OrbisUserServiceUserId userId, s32 type, s32 index, const OrbisPadOpenParam* pParam) {
    LOG_INFO("scePadOpen(userId=" << userId << ", type=" << type << ", index=" << index << ")");
    
    if (!g_pad_initialized) {
        return -2137653247; // ORBIS_PAD_ERROR_NOT_INITIALIZED
    }
    if (userId == -1) {
        return -2137653244; // ORBIS_PAD_ERROR_DEVICE_NO_HANDLE
    }
    if (type != ORBIS_PAD_PORT_TYPE_STANDARD && 
        type != ORBIS_PAD_PORT_TYPE_SPECIAL &&
        type != ORBIS_PAD_PORT_TYPE_REMOTE_CONTROL) {
        return -2137653242; // ORBIS_PAD_ERROR_DEVICE_NOT_CONNECTED
    }
    
    g_pad_opened = true;
    return 1; // Return handle 1
}

s32 scePadClose(s32 handle) {
    LOG_DEBUG("scePadClose(handle=" << handle << ")");
    g_pad_opened = false;
    return ORBIS_OK;
}

s32 scePadRead(s32 handle, OrbisPadData* pData, s32 num) {
    if (!pData || num <= 0) {
        return -2137653246; // ORBIS_PAD_ERROR_INVALID_ARG
    }
    
    u64 timestamp = GetCurrentTimestamp();
    
    for (s32 i = 0; i < num; i++) {
        std::memset(&pData[i], 0, sizeof(OrbisPadData));
        
        // Neutral controller state (sticks centered at 128)
        pData[i].buttons = 0;
        pData[i].leftStick.x = 128;
        pData[i].leftStick.y = 128;
        pData[i].rightStick.x = 128;
        pData[i].rightStick.y = 128;
        pData[i].analogButtons.l2 = 0;
        pData[i].analogButtons.r2 = 0;
        
        // Orientation (identity quaternion)
        pData[i].orientation.x = 0.0f;
        pData[i].orientation.y = 0.0f;
        pData[i].orientation.z = 0.0f;
        pData[i].orientation.w = 1.0f;
        
        // Acceleration (gravity on Y axis)
        pData[i].acceleration.x = 0.0f;
        pData[i].acceleration.y = -1.0f;
        pData[i].acceleration.z = 0.0f;
        
        // Angular velocity (stationary)
        pData[i].angularVelocity.x = 0.0f;
        pData[i].angularVelocity.y = 0.0f;
        pData[i].angularVelocity.z = 0.0f;
        
        // Touch data
        pData[i].touchData.touchNum = 0;
        
        // Connection status
        pData[i].connected = true;
        pData[i].connectedCount = 1;
        pData[i].timestamp = timestamp;
        pData[i].deviceUniqueDataLen = 0;
    }
    
    return num;
}

s32 scePadReadState(s32 handle, OrbisPadData* pData) {
    return scePadRead(handle, pData, 1);
}

s32 scePadGetControllerInformation(s32 handle, OrbisPadControllerInformation* pInfo) {
    LOG_DEBUG("scePadGetControllerInformation(handle=" << handle << ")");
    
    if (!pInfo) {
        return -2137653246; // ORBIS_PAD_ERROR_INVALID_ARG
    }
    
    std::memset(pInfo, 0, sizeof(OrbisPadControllerInformation));
    
    pInfo->touchPadInfo.pixelDensity = 1.0f;
    pInfo->touchPadInfo.resolution.x = 1920;
    pInfo->touchPadInfo.resolution.y = 950;
    pInfo->stickInfo.deadZoneLeft = 1;
    pInfo->stickInfo.deadZoneRight = 1;
    pInfo->connectionType = ORBIS_PAD_PORT_TYPE_STANDARD;
    pInfo->connectedCount = 1;
    pInfo->connected = (handle >= 0);
    pInfo->deviceClass = OrbisPadDeviceClass::Standard;
    
    return ORBIS_OK;
}

s32 scePadSetVibration(s32 handle, const OrbisPadVibrationParam* pParam) {
    LOG_DEBUG("scePadSetVibration(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 scePadSetLightBar(s32 handle, const OrbisPadLightBarParam* pParam) {
    LOG_DEBUG("scePadSetLightBar(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 scePadResetLightBar(s32 handle) {
    LOG_DEBUG("scePadResetLightBar(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 scePadResetOrientation(s32 handle) {
    LOG_DEBUG("scePadResetOrientation(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 scePadSetMotionSensorState(s32 handle, bool bEnable) {
    LOG_DEBUG("scePadSetMotionSensorState(handle=" << handle << ", enable=" << bEnable << ")");
    return ORBIS_OK;
}

s32 scePadGetHandle(OrbisUserServiceUserId userId, s32 type, s32 index) {
    if (!g_pad_initialized) {
        return -2137653247;
    }
    if (userId == -1 || !g_pad_opened) {
        return -2137653244;
    }
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO OUT - Based on shadPS4 audioout.cpp
// ═══════════════════════════════════════════════════════════════════════════

s32 sceAudioOutInit() {
    LOG_INFO("sceAudioOutInit()");
    if (g_audio_initialized) {
        return -2144993279; // ORBIS_AUDIO_OUT_ERROR_ALREADY_INIT
    }
    g_audio_initialized = true;
    return ORBIS_OK;
}

s32 sceAudioOutOpen(OrbisUserServiceUserId userId, s32 portType, s32 index,
                    u32 length, u32 sampleRate, u32 paramType) {
    LOG_INFO("sceAudioOutOpen(userId=" << userId << ", portType=" << portType << 
             ", length=" << length << ", sampleRate=" << sampleRate << ")");
    
    if (!g_audio_initialized) {
        return -2144993280; // ORBIS_AUDIO_OUT_ERROR_NOT_INIT
    }
    if (sampleRate != 48000) {
        return -2144993274; // ORBIS_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ
    }
    
    return g_audio_next_handle++;
}

s32 sceAudioOutClose(s32 handle) {
    LOG_DEBUG("sceAudioOutClose(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 sceAudioOutOutput(s32 handle, void* ptr) {
    // Pretend we output the audio immediately
    // This is called every frame, so we don't log it
    return 256; // Return samples sent
}

s32 sceAudioOutOutputs(void* param, u32 num) {
    // Process multiple audio outputs
    return ORBIS_OK;
}

s32 sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol) {
    LOG_DEBUG("sceAudioOutSetVolume(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 sceAudioOutGetPortState(s32 handle, OrbisAudioOutPortState* state) {
    if (!state) {
        return -2144993277; // ORBIS_AUDIO_OUT_ERROR_INVALID_POINTER
    }
    
    std::memset(state, 0, sizeof(OrbisAudioOutPortState));
    state->output = 1;
    state->channel = 2;
    state->volume = static_cast<s16>(SCE_AUDIO_OUT_VOLUME_0DB);
    
    return ORBIS_OK;
}

s32 sceAudioOutGetLastOutputTime(s32 handle, u64* outputTime) {
    if (!outputTime) {
        return -2144993277;
    }
    *outputTime = GetCurrentTimestamp();
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// VIDEO OUT - Based on shadPS4 video_out.cpp
// ═══════════════════════════════════════════════════════════════════════════

s32 sceVideoOutOpen(OrbisUserServiceUserId userId, s32 busType, s32 index, const void* param) {
    LOG_INFO("sceVideoOutOpen(userId=" << userId << ", busType=" << busType << ")");
    
    if (busType != SCE_VIDEO_OUT_BUS_TYPE_MAIN) {
        return -2144796671; // ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE
    }
    if (index != 0) {
        return -2144796671;
    }
    
    g_video_initialized = true;
    return g_video_next_handle++;
}

s32 sceVideoOutClose(s32 handle) {
    LOG_DEBUG("sceVideoOutClose(handle=" << handle << ")");
    return ORBIS_OK;
}

void sceVideoOutSetBufferAttribute(BufferAttribute* attribute, u32 pixelFormat,
                                   u32 tilingMode, u32 aspectRatio, u32 width,
                                   u32 height, u32 pitchInPixel) {
    LOG_INFO("sceVideoOutSetBufferAttribute(width=" << width << ", height=" << height << ")");
    
    if (attribute) {
        std::memset(attribute, 0, sizeof(BufferAttribute));
        attribute->pixel_format = static_cast<PixelFormat>(pixelFormat);
        attribute->tiling_mode = static_cast<TilingMode>(tilingMode);
        attribute->aspect_ratio = aspectRatio;
        attribute->width = width;
        attribute->height = height;
        attribute->pitch_in_pixel = pitchInPixel;
    }
}

s32 sceVideoOutRegisterBuffers(s32 handle, s32 startIndex, void* const* addresses,
                               s32 bufferNum, const BufferAttribute* attribute) {
    LOG_INFO("sceVideoOutRegisterBuffers(handle=" << handle << ", bufferNum=" << bufferNum << ")");
    
    if (!addresses || !attribute) {
        return -2144796666; // ORBIS_VIDEO_OUT_ERROR_INVALID_ADDRESS
    }
    
    return ORBIS_OK;
}

s32 sceVideoOutUnregisterBuffers(s32 handle, s32 attributeIndex) {
    LOG_DEBUG("sceVideoOutUnregisterBuffers(handle=" << handle << ")");
    return ORBIS_OK;
}

s32 sceVideoOutSetFlipRate(s32 handle, s32 rate) {
    LOG_DEBUG("sceVideoOutSetFlipRate(handle=" << handle << ", rate=" << rate << ")");
    return ORBIS_OK;
}

s32 sceVideoOutSubmitFlip(s32 handle, s32 bufferIndex, s32 flipMode, s64 flipArg) {
    // This is called every frame - increment flip count
    g_flip_count++;
    return ORBIS_OK;
}

s32 sceVideoOutGetFlipStatus(s32 handle, FlipStatus* status) {
    if (!status) {
        return -2144796666;
    }
    
    std::memset(status, 0, sizeof(FlipStatus));
    status->count = g_flip_count.load();
    status->process_time = GetCurrentTimestamp();
    status->tsc = GetCurrentTimestamp();
    status->flip_arg = 0;
    status->submit_tsc = GetCurrentTimestamp();
    status->gc_queue_num = 0;
    status->flip_pending_num = 0;
    status->current_buffer = 0;
    
    return ORBIS_OK;
}

s32 sceVideoOutGetVblankStatus(s32 handle, SceVideoOutVblankStatus* status) {
    if (!status) {
        return -2144796666;
    }
    
    g_vblank_count++;
    
    std::memset(status, 0, sizeof(SceVideoOutVblankStatus));
    status->count = g_vblank_count.load();
    status->process_time = GetCurrentTimestamp();
    status->tsc = GetCurrentTimestamp();
    
    return ORBIS_OK;
}

s32 sceVideoOutGetResolutionStatus(s32 handle, SceVideoOutResolutionStatus* status) {
    LOG_DEBUG("sceVideoOutGetResolutionStatus(handle=" << handle << ")");
    
    if (!status) {
        return -2144796666;
    }
    
    std::memset(status, 0, sizeof(SceVideoOutResolutionStatus));
    status->width = 1920;
    status->height = 1080;
    status->paneWidth = 1920;
    status->paneHeight = 1080;
    status->refreshRate = 60000000; // 60 Hz in microhertz
    status->screenSize = 0.0f;
    
    return ORBIS_OK;
}

s32 sceVideoOutIsFlipPending(s32 handle) {
    return 0; // No flips pending
}

s32 sceVideoOutWaitVblank(s32 handle) {
    // Simulate waiting for vblank (~16.67ms at 60Hz)
    // In a real implementation, this would sync with the display
    return ORBIS_OK;
}

s32 sceVideoOutGetDeviceCapabilityInfo(s32 handle, SceVideoOutDeviceCapabilityInfo* info) {
    if (info) {
        info->capability = 0;
    }
    return ORBIS_OK;
}

s32 sceVideoOutColorSettingsSetGamma(SceVideoOutColorSettings* settings, float gamma) {
    if (gamma < 0.1f || gamma > 2.0f) {
        return -2144796671;
    }
    if (settings) {
        settings->gamma = gamma;
    }
    return ORBIS_OK;
}

s32 sceVideoOutAdjustColor(s32 handle, const SceVideoOutColorSettings* settings) {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// GNM (GRAPHICS) - GPU command submission stubs
// ═══════════════════════════════════════════════════════════════════════════

s32 sceGnmSubmitCommandBuffers(u32 count, void** dcbGpuAddrs, u32* dcbSizesInBytes,
                               void** ccbGpuAddrs, u32* ccbSizesInBytes) {
    return ORBIS_OK;
}

s32 sceGnmSubmitAndFlipCommandBuffers(u32 count, void** dcbGpuAddrs, u32* dcbSizesInBytes,
                                      void** ccbGpuAddrs, u32* ccbSizesInBytes,
                                      s32 videoHandle, s32 bufferIndex,
                                      s32 flipMode, s64 flipArg) {
    g_flip_count++;
    return ORBIS_OK;
}

u32 sceGnmGetGpuCoreClockFrequency() {
    return 800000000; // 800 MHz
}

s32 sceGnmFlushGarlic() {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// USER SERVICE - Based on shadPS4 userservice.cpp
// ═══════════════════════════════════════════════════════════════════════════

s32 sceUserServiceInitialize(void* params) {
    LOG_INFO("sceUserServiceInitialize()");
    g_user_initialized = true;
    return ORBIS_OK;
}

s32 sceUserServiceTerminate() {
    g_user_initialized = false;
    return ORBIS_OK;
}

s32 sceUserServiceGetInitialUser(OrbisUserServiceUserId* userId) {
    if (userId) {
        *userId = DEFAULT_USER_ID;
    }
    return ORBIS_OK;
}

s32 sceUserServiceGetLoginUserIdList(OrbisUserServiceUserId* userIdList) {
    if (userIdList) {
        userIdList[0] = DEFAULT_USER_ID;
        userIdList[1] = -1;
        userIdList[2] = -1;
        userIdList[3] = -1;
    }
    return ORBIS_OK;
}

s32 sceUserServiceGetUserName(OrbisUserServiceUserId userId, char* userName, size_t size) {
    if (userName && size > 0) {
        strncpy(userName, "Player", size - 1);
        userName[size - 1] = '\0';
    }
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// SYSTEM SERVICE
// ═══════════════════════════════════════════════════════════════════════════

s32 sceSystemServiceHideSplashScreen() {
    LOG_INFO("sceSystemServiceHideSplashScreen()");
    return ORBIS_OK;
}

s32 sceSystemServiceParamGetInt(s32 paramId, s32* value) {
    if (value) {
        *value = 0;
    }
    return ORBIS_OK;
}

s32 sceSystemServiceParamGetString(s32 paramId, char* buf, size_t bufSize) {
    if (buf && bufSize > 0) {
        buf[0] = '\0';
    }
    return ORBIS_OK;
}

s32 sceSystemServiceGetStatus(void* status) {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// SAVE DATA
// ═══════════════════════════════════════════════════════════════════════════

s32 sceSaveDataInitialize3(void* params) {
    LOG_INFO("sceSaveDataInitialize3()");
    return ORBIS_OK;
}

s32 sceSaveDataMount(void* mount) {
    return ORBIS_OK;
}

s32 sceSaveDataUmount(void* umount) {
    return ORBIS_OK;
}

s32 sceSaveDataDirNameSearch(void* search) {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// COMMON DIALOG
// ═══════════════════════════════════════════════════════════════════════════

s32 sceCommonDialogInitialize() {
    return ORBIS_OK;
}

s32 sceMsgDialogInitialize() {
    return ORBIS_OK;
}

s32 sceMsgDialogOpen(void* param) {
    return ORBIS_OK;
}

s32 sceMsgDialogUpdateStatus() {
    return 4; // FINISHED
}

s32 sceMsgDialogTerminate() {
    return ORBIS_OK;
}

s32 sceMsgDialogGetResult(void* result) {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// NETWORK (NP)
// ═══════════════════════════════════════════════════════════════════════════

s32 sceNpSetNpTitleId(void* titleId, void* titleSecret) {
    return ORBIS_OK;
}

s32 sceNpCheckCallback() {
    return ORBIS_OK;
}

s32 sceNetInit() {
    return ORBIS_OK;
}

s32 sceNetCtlInit() {
    return ORBIS_OK;
}

s32 sceNetCtlTerm() {
    return ORBIS_OK;
}

s32 sceNetCtlGetInfo(s32 code, void* info) {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// IME (INPUT METHOD)
// ═══════════════════════════════════════════════════════════════════════════

s32 sceImeDialogInit(void* param, void* extended) {
    return ORBIS_OK;
}

s32 sceImeDialogGetStatus() {
    return 0; // Not active
}

s32 sceImeDialogTerm() {
    return ORBIS_OK;
}

// ═══════════════════════════════════════════════════════════════════════════
// SYSMODULE
// ═══════════════════════════════════════════════════════════════════════════

s32 sceSysmoduleLoadModule(s32 id) {
    LOG_INFO("sceSysmoduleLoadModule(id=" << id << ")");
    return ORBIS_OK;
}

s32 sceSysmoduleUnloadModule(s32 id) {
    return ORBIS_OK;
}

s32 sceSysmoduleIsLoaded(s32 id) {
    return ORBIS_OK; // Module is loaded
}

} // extern "C"
