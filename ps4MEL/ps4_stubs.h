/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PS4 SYSTEM STUBS - HEADER                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  Data structures and declarations based on shadPS4 emulator.              ║
 * ║  Reference: https://github.com/shadps4-emu/shadPS4                        ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef PS4_STUBS_H
#define PS4_STUBS_H

#pragma once

#include <cstdint>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════════
// COMMON TYPES
// ═══════════════════════════════════════════════════════════════════════════

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#define ORBIS_OK 0

// ═══════════════════════════════════════════════════════════════════════════
// PAD (CONTROLLER) STRUCTURES - Based on shadPS4
// ═══════════════════════════════════════════════════════════════════════════

constexpr int ORBIS_PAD_MAX_TOUCH_NUM = 2;
constexpr int ORBIS_PAD_MAX_DEVICE_UNIQUE_DATA_SIZE = 12;
constexpr int ORBIS_PAD_PORT_TYPE_STANDARD = 0;
constexpr int ORBIS_PAD_PORT_TYPE_SPECIAL = 2;
constexpr int ORBIS_PAD_PORT_TYPE_REMOTE_CONTROL = 16;

enum class OrbisPadDeviceClass : s32 {
    Invalid = -1,
    Standard = 0,
    Guitar = 1,
    Drum = 2,
    DjTurntable = 3,
    Dancemat = 4,
    Navigation = 5,
    SteeringWheel = 6,
    Stick = 7,
    FightStick = 8,
    Gun = 9,
};

struct OrbisPadAnalogButtons {
    u8 l2;
    u8 r2;
    u8 padding[2];
};

struct OrbisPadAnalogStick {
    u8 x;
    u8 y;
};

struct OrbisFQuaternion {
    float x, y, z, w;
};

struct OrbisFVector3 {
    float x, y, z;
};

struct OrbisPadTouch {
    u16 x;
    u16 y;
    u8 id;
    u8 reserve[3];
};

struct OrbisPadTouchData {
    u8 touchNum;
    u8 reserve[3];
    u32 reserve1;
    OrbisPadTouch touch[ORBIS_PAD_MAX_TOUCH_NUM];
};

struct OrbisPadExtensionUnitData {
    u32 extensionUnitId;
    u8 reserve[1];
    u8 dataLength;
    u8 data[10];
};

struct OrbisPadData {
    u32 buttons;
    OrbisPadAnalogStick leftStick;
    OrbisPadAnalogStick rightStick;
    OrbisPadAnalogButtons analogButtons;
    OrbisFQuaternion orientation;
    OrbisFVector3 acceleration;
    OrbisFVector3 angularVelocity;
    OrbisPadTouchData touchData;
    bool connected;
    u64 timestamp;
    OrbisPadExtensionUnitData extensionUnitData;
    u8 connectedCount;
    u8 reserve[2];
    u8 deviceUniqueDataLen;
    u8 deviceUniqueData[ORBIS_PAD_MAX_DEVICE_UNIQUE_DATA_SIZE];
};

struct OrbisPadTouchPadInformation {
    float pixelDensity;
    struct {
        u16 x;
        u16 y;
    } resolution;
};

struct OrbisPadStickInformation {
    u8 deadZoneLeft;
    u8 deadZoneRight;
};

struct OrbisPadControllerInformation {
    OrbisPadTouchPadInformation touchPadInfo;
    OrbisPadStickInformation stickInfo;
    u8 connectionType;
    u8 connectedCount;
    bool connected;
    OrbisPadDeviceClass deviceClass;
    u8 reserve[8];
};

struct OrbisPadOpenParam {
    u8 reserve[8];
};

struct OrbisPadLightBarParam {
    u8 r;
    u8 g;
    u8 b;
    u8 reserve[1];
};

struct OrbisPadVibrationParam {
    u8 largeMotor;
    u8 smallMotor;
};

// ═══════════════════════════════════════════════════════════════════════════
// AUDIO OUT STRUCTURES - Based on shadPS4
// ═══════════════════════════════════════════════════════════════════════════

constexpr int SCE_AUDIO_OUT_NUM_PORTS = 8;
constexpr int SCE_AUDIO_OUT_VOLUME_0DB = 32768;

enum class OrbisAudioOutPort : s32 {
    Main = 0,
    Bgm = 1,
    Voice = 2,
    Personal = 3,
    PadSpk = 4,
    Aux = 127,
    Audio3d = 128,
};

struct OrbisAudioOutPortState {
    u16 output;
    u8 channel;
    u8 reserved8_1[1];
    s16 volume;
    u16 rerouteCounter;
    u64 flag;
};

struct OrbisAudioOutOutputParam {
    s32 handle;
    void* ptr;
};

// ═══════════════════════════════════════════════════════════════════════════
// VIDEO OUT STRUCTURES - Based on shadPS4
// ═══════════════════════════════════════════════════════════════════════════

constexpr int SCE_VIDEO_OUT_BUS_TYPE_MAIN = 0;

enum class PixelFormat : u32 {
    A8R8G8B8Srgb = 0x80000000,
    A8B8G8R8Srgb = 0x80002200,
    A2R10G10B10 = 0x88060000,
    A2R10G10B10Srgb = 0x88000000,
    A2R10G10B10Bt2020Pq = 0x88740000,
    A16R16G16B16Float = 0xC1060000,
};

enum class TilingMode : u32 {
    Tile = 0,
    Linear = 1,
};

struct BufferAttribute {
    PixelFormat pixel_format;
    TilingMode tiling_mode;
    u32 aspect_ratio;
    u32 width;
    u32 height;
    u32 pitch_in_pixel;
    u32 option;
    u32 reserved0;
    u64 reserved1;
};

struct FlipStatus {
    u64 count;
    u64 process_time;
    u64 tsc;
    s64 flip_arg;
    u64 submit_tsc;
    u64 reserved0;
    s32 gc_queue_num;
    s32 flip_pending_num;
    s32 current_buffer;
    u32 reserved1;
};

struct SceVideoOutVblankStatus {
    u64 count;
    u64 process_time;
    u64 tsc;
    u64 reserved;
    u8 flags;
    u8 pad[7];
};

struct SceVideoOutResolutionStatus {
    u32 width;
    u32 height;
    u32 paneWidth;
    u32 paneHeight;
    u64 refreshRate;
    float screenSize;
    u16 flags;
    u16 reserved0;
    u32 reserved1[3];
};

struct SceVideoOutDeviceCapabilityInfo {
    u64 capability;
};

struct SceVideoOutColorSettings {
    float gamma;
};

// ═══════════════════════════════════════════════════════════════════════════
// USER SERVICE STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

using OrbisUserServiceUserId = s32;

// ═══════════════════════════════════════════════════════════════════════════
// FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════

extern "C" {

// PAD
s32 scePadInit();
s32 scePadOpen(OrbisUserServiceUserId userId, s32 type, s32 index, const OrbisPadOpenParam* pParam);
s32 scePadClose(s32 handle);
s32 scePadRead(s32 handle, OrbisPadData* pData, s32 num);
s32 scePadReadState(s32 handle, OrbisPadData* pData);
s32 scePadGetControllerInformation(s32 handle, OrbisPadControllerInformation* pInfo);
s32 scePadSetVibration(s32 handle, const OrbisPadVibrationParam* pParam);
s32 scePadSetLightBar(s32 handle, const OrbisPadLightBarParam* pParam);
s32 scePadResetLightBar(s32 handle);
s32 scePadResetOrientation(s32 handle);
s32 scePadSetMotionSensorState(s32 handle, bool bEnable);
s32 scePadGetHandle(OrbisUserServiceUserId userId, s32 type, s32 index);

// Audio
s32 sceAudioOutInit();
s32 sceAudioOutOpen(OrbisUserServiceUserId userId, s32 portType, s32 index, u32 length, u32 sampleRate, u32 paramType);
s32 sceAudioOutClose(s32 handle);
s32 sceAudioOutOutput(s32 handle, void* ptr);
s32 sceAudioOutOutputs(void* param, u32 num);
s32 sceAudioOutSetVolume(s32 handle, s32 flag, s32* vol);
s32 sceAudioOutGetPortState(s32 handle, OrbisAudioOutPortState* state);
s32 sceAudioOutGetLastOutputTime(s32 handle, u64* outputTime);

// Video
s32 sceVideoOutOpen(OrbisUserServiceUserId userId, s32 busType, s32 index, const void* param);
s32 sceVideoOutClose(s32 handle);
void sceVideoOutSetBufferAttribute(BufferAttribute* attribute, u32 pixelFormat, u32 tilingMode, u32 aspectRatio, u32 width, u32 height, u32 pitchInPixel);
s32 sceVideoOutRegisterBuffers(s32 handle, s32 startIndex, void* const* addresses, s32 bufferNum, const BufferAttribute* attribute);
s32 sceVideoOutUnregisterBuffers(s32 handle, s32 attributeIndex);
s32 sceVideoOutSetFlipRate(s32 handle, s32 rate);
s32 sceVideoOutSubmitFlip(s32 handle, s32 bufferIndex, s32 flipMode, s64 flipArg);
s32 sceVideoOutGetFlipStatus(s32 handle, FlipStatus* status);
s32 sceVideoOutGetVblankStatus(s32 handle, SceVideoOutVblankStatus* status);
s32 sceVideoOutGetResolutionStatus(s32 handle, SceVideoOutResolutionStatus* status);
s32 sceVideoOutIsFlipPending(s32 handle);
s32 sceVideoOutWaitVblank(s32 handle);
s32 sceVideoOutGetDeviceCapabilityInfo(s32 handle, SceVideoOutDeviceCapabilityInfo* info);
s32 sceVideoOutColorSettingsSetGamma(SceVideoOutColorSettings* settings, float gamma);
s32 sceVideoOutAdjustColor(s32 handle, const SceVideoOutColorSettings* settings);

// GNM
s32 sceGnmSubmitCommandBuffers(u32 count, void** dcbGpuAddrs, u32* dcbSizesInBytes, void** ccbGpuAddrs, u32* ccbSizesInBytes);
s32 sceGnmSubmitAndFlipCommandBuffers(u32 count, void** dcbGpuAddrs, u32* dcbSizesInBytes, void** ccbGpuAddrs, u32* ccbSizesInBytes, s32 videoHandle, s32 bufferIndex, s32 flipMode, s64 flipArg);
u32 sceGnmGetGpuCoreClockFrequency();
s32 sceGnmFlushGarlic();

// User Service
s32 sceUserServiceInitialize(void* params);
s32 sceUserServiceTerminate();
s32 sceUserServiceGetInitialUser(OrbisUserServiceUserId* userId);
s32 sceUserServiceGetLoginUserIdList(OrbisUserServiceUserId* userIdList);
s32 sceUserServiceGetUserName(OrbisUserServiceUserId userId, char* userName, size_t size);

// System Service
s32 sceSystemServiceHideSplashScreen();
s32 sceSystemServiceParamGetInt(s32 paramId, s32* value);
s32 sceSystemServiceParamGetString(s32 paramId, char* buf, size_t bufSize);
s32 sceSystemServiceGetStatus(void* status);

// Save Data
s32 sceSaveDataInitialize3(void* params);
s32 sceSaveDataMount(void* mount);
s32 sceSaveDataUmount(void* umount);
s32 sceSaveDataDirNameSearch(void* search);

// Common Dialog
s32 sceCommonDialogInitialize();
s32 sceMsgDialogInitialize();
s32 sceMsgDialogOpen(void* param);
s32 sceMsgDialogUpdateStatus();
s32 sceMsgDialogTerminate();
s32 sceMsgDialogGetResult(void* result);

// Network
s32 sceNpSetNpTitleId(void* titleId, void* titleSecret);
s32 sceNpCheckCallback();
s32 sceNetInit();
s32 sceNetCtlInit();
s32 sceNetCtlTerm();
s32 sceNetCtlGetInfo(s32 code, void* info);

// IME
s32 sceImeDialogInit(void* param, void* extended);
s32 sceImeDialogGetStatus();
s32 sceImeDialogTerm();

// Sysmodule
s32 sceSysmoduleLoadModule(s32 id);
s32 sceSysmoduleUnloadModule(s32 id);
s32 sceSysmoduleIsLoaded(s32 id);

} // extern "C"

#endif // PS4_STUBS_H
