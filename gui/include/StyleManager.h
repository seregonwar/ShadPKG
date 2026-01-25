// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include <assert.h>
#include <filesystem>
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "IconsFontAwesome6.h"
#include "imgui.h"

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  StyleManager: Cyber-Dark Theme for ShadPKG GUI                          ║
// ║                                                                          ║
// ║  Color Palette:                                                          ║
// ║  ┌──────────────────┬───────────┬─────────────────────────────────────┐  ║
// ║  │ Element          │ HEX       │ Description                         │  ║
// ║  ├──────────────────┼───────────┼─────────────────────────────────────┤  ║
// ║  │ Window BG        │ #1E1E1E │ Deep dark gray                      │  ║
// ║  │ Panels/Child     │ #252526 │ Slightly lighter for separation     │  ║
// ║  │ Accent/Primary   │ #00ADB5 │ ShadPKG Cyan                        │  ║
// ║  │ Text             │ #EEEEEE │ Off-white for readability           │  ║
// ║  │ Success          │ #4CAF50 │ Green                               │  ║
// ║  │ Error            │ #F44336 │ Red pastel                          │  ║
// ║  │ Warning          │ #FF9800 │ Orange                              │  ║
// ║  └──────────────────┴───────────┴─────────────────────────────────────┘  ║
// ╚══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

// Color constants for use throughout the application
namespace Colors {
// Base colors (ImVec4 format: R, G, B, A in 0.0-1.0 range)
// Base colors (ImVec4 format: R, G, B, A in 0.0-1.0 range)
constexpr ImVec4 WindowBg = ImVec4(0.09f, 0.09f, 0.10f, 1.0f); // Deep rich dark
constexpr ImVec4 ChildBg =
    ImVec4(0.13f, 0.13f, 0.14f, 1.0f); // Slightly lighter panels
constexpr ImVec4 PopupBg =
    ImVec4(0.11f, 0.11f, 0.12f, 0.98f); // Almost opaque popup

// Accent colors
constexpr ImVec4 Primary =
    ImVec4(0.0f, 0.75f, 0.78f, 1.0f); // More vibrant Cyan
constexpr ImVec4 PrimaryHover =
    ImVec4(0.0f, 0.85f, 0.88f, 1.0f); // Bright Cyan hover
constexpr ImVec4 PrimaryActive =
    ImVec4(0.0f, 0.60f, 0.63f, 1.0f); // Deep Cyan active

// Text colors
constexpr ImVec4 Text =
    ImVec4(1.00f, 1.00f, 1.00f, 1.00f); // Pure White for max contrast
constexpr ImVec4 TextDim =
    ImVec4(0.70f, 0.70f, 0.70f, 1.00f); // Lighter gray for better readability

// Status colors
constexpr ImVec4 Success = ImVec4(0.35f, 0.75f, 0.35f, 1.0f); // Brighter Green
constexpr ImVec4 Error = ImVec4(0.95f, 0.30f, 0.25f, 1.0f);   // Vivid Red
constexpr ImVec4 Warning = ImVec4(1.0f, 0.65f, 0.0f, 1.0f);   // Bright Orange

// UI elements
constexpr ImVec4 Border =
    ImVec4(0.35f, 0.35f, 0.40f, 0.40f); // Subtle blue-ish border
constexpr ImVec4 FrameBg =
    ImVec4(0.18f, 0.18f, 0.20f, 1.0f); // Dark frame background
constexpr ImVec4 FrameBgHover = ImVec4(0.24f, 0.24f, 0.26f, 1.0f);
constexpr ImVec4 NavBarBg =
    ImVec4(0.07f, 0.07f, 0.08f, 1.0f); // Very dark navbar
} // namespace Colors

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Icon Mappings (FontAwesome 6)                                          │
// │  Usage: ImGui::Text(ICON_FA_FILE " PKG File")                           │
// └─────────────────────────────────────────────────────────────────────────┘
// Already defined in IconsFontAwesome6.h, just ensuring compatibility
#define ICON_FA_MAGNIFYING_GLASS ICON_FA_SEARCH
#define ICON_FA_CIRCLE_INFO ICON_FA_INFO_CIRCLE

// Setup the complete ImGui style
void SetupImGuiStyle();

// Load custom fonts (Roboto, JetBrainsMono, FontAwesome)
// Returns true if fonts were loaded successfully
bool LoadFonts(ImGuiIO &io, float baseFontSize = 16.0f);
bool LoadFonts(ImGuiIO &io, const std::filesystem::path &executableDir,
               float baseFontSize = 16.0f);

// Get font pointers after loading
ImFont *GetDefaultFont();   // Roboto-Medium 16px
ImFont *GetMonospaceFont(); // JetBrainsMono 14px
ImFont *GetIconFont();      // FontAwesome (merged with default)

} // namespace ShadPKG::GUI
