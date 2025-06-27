// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>
#include <fstream>
#include <string>
#include <fmt/core.h>
#include <fmt/xchar.h> // for wstring support

#include "common/path_util.h"
#include "config.h"
#include "logging/formatter.h"
#include "version.h"

namespace Config {

static bool isNeo = false;
static bool isFullscreen = false;
static std::string fullscreenMode = "borderless";
static bool playBGM = false;
static bool isTrophyPopupDisabled = false;
static int BGMvolume = 50;
static bool enableDiscordRPC = false;
static u32 screenWidth = 1280;
static u32 screenHeight = 720;
static s32 gpuId = -1; // Vulkan physical device index. Set to negative for auto select
static std::string logFilter;
static std::string logType = "async";
static std::string userName = "shadPS4";
static std::string updateChannel;
static std::string chooseHomeTab;
static u16 deadZoneLeft = 2.0;
static u16 deadZoneRight = 2.0;
static std::string backButtonBehavior = "left";
static bool useSpecialPad = false;
static int specialPadClass = 1;
static bool isMotionControlsEnabled = true;
static bool isDebugDump = false;
static bool isShaderDebug = false;
static bool isShowSplash = false;
static bool isAutoUpdate = false;
static bool isNullGpu = false;
static bool shouldCopyGPUBuffers = false;
static bool shouldDumpShaders = false;
static bool shouldPatchShaders = true;
static u32 vblankDivider = 1;
static bool vkValidation = false;
static bool vkValidationSync = false;
static bool vkValidationGpu = false;
static bool vkCrashDiagnostic = false;
static bool vkHostMarkers = false;
static bool vkGuestMarkers = false;
static bool rdocEnable = false;
static s16 cursorState = HideCursorState::Idle;
static int cursorHideTimeout = 5; // 5 seconds (default)
static bool separateupdatefolder = false;
static bool compatibilityData = false;
static bool checkCompatibilityOnStartup = false;
static std::string trophyKey;

// Gui
static bool load_game_size = true;
std::vector<std::filesystem::path> settings_install_dirs = {};
std::filesystem::path settings_addon_install_dir = {};
std::filesystem::path save_data_path = {};
u32 main_window_geometry_x = 400;
u32 main_window_geometry_y = 400;
u32 main_window_geometry_w = 1280;
u32 main_window_geometry_h = 720;
u32 mw_themes = 0;
u32 m_icon_size = 36;
u32 m_icon_size_grid = 69;
u32 m_slider_pos = 0;
u32 m_slider_pos_grid = 0;
u32 m_table_mode = 0;
u32 m_window_size_W = 1280;
u32 m_window_size_H = 720;
std::vector<std::string> m_pkg_viewer;
std::vector<std::string> m_elf_viewer;
std::vector<std::string> m_recent_files;
std::string emulator_language = "en";

// Language
u32 m_language = 1; // english

std::string getTrophyKey() {
    return "";
}

void setTrophyKey(std::string key) {
    trophyKey = key;
}

bool GetLoadGameSizeEnabled() {
    return load_game_size;
}

std::filesystem::path GetSaveDataPath() {
    if (save_data_path.empty()) {
        return Common::FS::GetUserPath(Common::FS::PathType::SaveDataDir);
    }
    return save_data_path;
}

void setLoadGameSizeEnabled(bool enable) {
    load_game_size = enable;
}

bool isNeoModeConsole() {
    return isNeo;
}

bool getIsFullscreen() {
    return isFullscreen;
}

std::string getFullscreenMode() {
    return fullscreenMode;
}

bool getisTrophyPopupDisabled() {
    return isTrophyPopupDisabled;
}

bool getPlayBGM() {
    return playBGM;
}

int getBGMvolume() {
    return BGMvolume;
}

bool getEnableDiscordRPC() {
    return enableDiscordRPC;
}

u16 leftDeadZone() {
    return deadZoneLeft;
}

u16 rightDeadZone() {
    return deadZoneRight;
}

s16 getCursorState() {
    return cursorState;
}

int getCursorHideTimeout() {
    return cursorHideTimeout;
}

u32 getScreenWidth() {
    return screenWidth;
}

u32 getScreenHeight() {
    return screenHeight;
}

s32 getGpuId() {
    return gpuId;
}

std::string getLogFilter() {
    return logFilter;
}

std::string getLogType() {
    return logType;
}

std::string getUserName() {
    return userName;
}

std::string getUpdateChannel() {
    return updateChannel;
}

std::string getChooseHomeTab() {
    return chooseHomeTab;
}

std::string getBackButtonBehavior() {
    return backButtonBehavior;
}

bool getUseSpecialPad() {
    return useSpecialPad;
}

int getSpecialPadClass() {
    return specialPadClass;
}

bool getIsMotionControlsEnabled() {
    return isMotionControlsEnabled;
}

bool debugDump() {
    return isDebugDump;
}

bool collectShadersForDebug() {
    return isShaderDebug;
}

bool showSplash() {
    return isShowSplash;
}

bool autoUpdate() {
    return isAutoUpdate;
}

bool nullGpu() {
    return isNullGpu;
}

bool copyGPUCmdBuffers() {
    return shouldCopyGPUBuffers;
}

bool dumpShaders() {
    return shouldDumpShaders;
}

bool patchShaders() {
    return shouldPatchShaders;
}

bool isRdocEnabled() {
    return rdocEnable;
}

u32 vblankDiv() {
    return vblankDivider;
}

bool vkValidationEnabled() {
    return vkValidation;
}

bool vkValidationSyncEnabled() {
    return vkValidationSync;
}

bool vkValidationGpuEnabled() {
    return vkValidationGpu;
}

bool getVkCrashDiagnosticEnabled() {
    return vkCrashDiagnostic;
}

bool getVkHostMarkersEnabled() {
    return vkHostMarkers;
}

bool getVkGuestMarkersEnabled() {
    return vkGuestMarkers;
}

void setVkCrashDiagnosticEnabled(bool enable) {
    vkCrashDiagnostic = enable;
}

void setVkHostMarkersEnabled(bool enable) {
    vkHostMarkers = enable;
}

void setVkGuestMarkersEnabled(bool enable) {
    vkGuestMarkers = enable;
}

bool getSeparateUpdateEnabled() {
    return separateupdatefolder;
}

bool getCompatibilityEnabled() {
    return compatibilityData;
}

bool getCheckCompatibilityOnStartup() {
    return checkCompatibilityOnStartup;
}

void setGpuId(s32 selectedGpuId) {
    gpuId = selectedGpuId;
}

void setScreenWidth(u32 width) {
    screenWidth = width;
}

void setScreenHeight(u32 height) {
    screenHeight = height;
}

void setDebugDump(bool enable) {
    isDebugDump = enable;
}

void setCollectShaderForDebug(bool enable) {
    isShaderDebug = enable;
}

void setShowSplash(bool enable) {
    isShowSplash = enable;
}

void setAutoUpdate(bool enable) {
    isAutoUpdate = enable;
}

void setNullGpu(bool enable) {
    isNullGpu = enable;
}

void setCopyGPUCmdBuffers(bool enable) {
    shouldCopyGPUBuffers = enable;
}

void setDumpShaders(bool enable) {
    shouldDumpShaders = enable;
}

void setVkValidation(bool enable) {
    vkValidation = enable;
}

void setVkSyncValidation(bool enable) {
    vkValidationSync = enable;
}

void setRdocEnabled(bool enable) {
    rdocEnable = enable;
}

void setVblankDiv(u32 value) {
    vblankDivider = value;
}

void setIsFullscreen(bool enable) {
    isFullscreen = enable;
}

void setFullscreenMode(std::string mode) {
    fullscreenMode = mode;
}

void setisTrophyPopupDisabled(bool disable) {
    isTrophyPopupDisabled = disable;
}

void setPlayBGM(bool enable) {
    playBGM = enable;
}

void setBGMvolume(int volume) {
    BGMvolume = volume;
}

void setEnableDiscordRPC(bool enable) {
    enableDiscordRPC = enable;
}

void setCursorState(s16 newCursorState) {
    cursorState = newCursorState;
}

void setCursorHideTimeout(int newcursorHideTimeout) {
    cursorHideTimeout = newcursorHideTimeout;
}

void setLanguage(u32 language) {
    m_language = language;
}

void setNeoMode(bool enable) {
    isNeo = enable;
}

void setLogType(const std::string& type) {
    logType = type;
}

void setLogFilter(const std::string& type) {
    logFilter = type;
}

void setUserName(const std::string& type) {
    userName = type;
}

void setUpdateChannel(const std::string& type) {
    updateChannel = type;
}
void setChooseHomeTab(const std::string& type) {
    chooseHomeTab = type;
}

void setBackButtonBehavior(const std::string& type) {
    backButtonBehavior = type;
}

void setUseSpecialPad(bool use) {
    useSpecialPad = use;
}

void setSpecialPadClass(int type) {
    specialPadClass = type;
}

void setIsMotionControlsEnabled(bool use) {
    isMotionControlsEnabled = use;
}

void setSeparateUpdateEnabled(bool use) {
    separateupdatefolder = use;
}

void setCompatibilityEnabled(bool use) {
    compatibilityData = use;
}

void setCheckCompatibilityOnStartup(bool use) {
    checkCompatibilityOnStartup = use;
}

void setMainWindowGeometry(u32 x, u32 y, u32 w, u32 h) {
    main_window_geometry_x = x;
    main_window_geometry_y = y;
    main_window_geometry_w = w;
    main_window_geometry_h = h;
}

bool addGameInstallDir(const std::filesystem::path& dir) {
    if (std::find(settings_install_dirs.begin(), settings_install_dirs.end(), dir) ==
        settings_install_dirs.end()) {
        settings_install_dirs.push_back(dir);
        return true;
    }
    return false;
}

void removeGameInstallDir(const std::filesystem::path& dir) {
    auto iterator = std::find(settings_install_dirs.begin(), settings_install_dirs.end(), dir);
    if (iterator != settings_install_dirs.end()) {
        settings_install_dirs.erase(iterator);
    }
}

void setAddonInstallDir(const std::filesystem::path& dir) {
    settings_addon_install_dir = dir;
}

void setMainWindowTheme(u32 theme) {
    mw_themes = theme;
}

void setIconSize(u32 size) {
    m_icon_size = size;
}

void setIconSizeGrid(u32 size) {
    m_icon_size_grid = size;
}

void setSliderPosition(u32 pos) {
    m_slider_pos = pos;
}

void setSliderPositionGrid(u32 pos) {
    m_slider_pos_grid = pos;
}

void setTableMode(u32 mode) {
    m_table_mode = mode;
}

void setMainWindowWidth(u32 width) {
    m_window_size_W = width;
}

void setMainWindowHeight(u32 height) {
    m_window_size_H = height;
}

void setPkgViewer(const std::vector<std::string>& pkgList) {
    m_pkg_viewer.resize(pkgList.size());
    m_pkg_viewer = pkgList;
}

void setElfViewer(const std::vector<std::string>& elfList) {
    m_elf_viewer.resize(elfList.size());
    m_elf_viewer = elfList;
}

void setRecentFiles(const std::vector<std::string>& recentFiles) {
    m_recent_files.resize(recentFiles.size());
    m_recent_files = recentFiles;
}

void setEmulatorLanguage(std::string language) {
    emulator_language = language;
}

void setGameInstallDirs(const std::vector<std::filesystem::path>& settings_install_dirs_config) {
    settings_install_dirs = settings_install_dirs_config;
}

void setSaveDataPath(const std::filesystem::path& path) {
    save_data_path = path;
}

u32 getMainWindowGeometryX() {
    return main_window_geometry_x;
}

u32 getMainWindowGeometryY() {
    return main_window_geometry_y;
}

u32 getMainWindowGeometryW() {
    return main_window_geometry_w;
}

u32 getMainWindowGeometryH() {
    return main_window_geometry_h;
}

const std::vector<std::filesystem::path>& getGameInstallDirs() {
    return settings_install_dirs;
}

std::filesystem::path getAddonInstallDir() {
    if (settings_addon_install_dir.empty()) {
        // Default for users without a config file or a config file from before this option existed
        return Common::FS::GetUserPath(Common::FS::PathType::UserDir) / "addcont";
    }
    return settings_addon_install_dir;
}

u32 getMainWindowTheme() {
    return mw_themes;
}

u32 getIconSize() {
    return m_icon_size;
}

u32 getIconSizeGrid() {
    return m_icon_size_grid;
}

u32 getSliderPosition() {
    return m_slider_pos;
}

u32 getSliderPositionGrid() {
    return m_slider_pos_grid;
}

u32 getTableMode() {
    return m_table_mode;
}

u32 getMainWindowWidth() {
    return m_window_size_W;
}

u32 getMainWindowHeight() {
    return m_window_size_H;
}

std::vector<std::string> getPkgViewer() {
    return m_pkg_viewer;
}

std::vector<std::string> getElfViewer() {
    return m_elf_viewer;
}

std::vector<std::string> getRecentFiles() {
    return m_recent_files;
}

std::string getEmulatorLanguage() {
    return emulator_language;
}

u32 GetLanguage() {
    return m_language;
}

void setDefaultValues() {
    isNeo = false;
    isFullscreen = false;
    isTrophyPopupDisabled = false;
    playBGM = false;
    BGMvolume = 50;
    enableDiscordRPC = true;
    screenWidth = 1280;
    screenHeight = 720;
    logFilter = "";
    logType = "async";
    userName = "shadPS4";
    if (Common::isRelease) {
        updateChannel = "Release";
    } else {
        updateChannel = "Nightly";
    }
    chooseHomeTab = "General";
    cursorState = HideCursorState::Idle;
    cursorHideTimeout = 5;
    backButtonBehavior = "left";
    useSpecialPad = false;
    specialPadClass = 1;
    isDebugDump = false;
    isShaderDebug = false;
    isShowSplash = false;
    isAutoUpdate = false;
    isNullGpu = false;
    shouldDumpShaders = false;
    vblankDivider = 1;
    vkValidation = false;
    vkValidationSync = false;
    vkValidationGpu = false;
    vkCrashDiagnostic = false;
    vkHostMarkers = false;
    vkGuestMarkers = false;
    rdocEnable = false;
    emulator_language = "en";
    m_language = 1;
    gpuId = -1;
    separateupdatefolder = false;
    compatibilityData = false;
    checkCompatibilityOnStartup = false;
}

} // namespace Config