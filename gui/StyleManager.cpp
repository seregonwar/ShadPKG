// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "include/StyleManager.h"
#include "include/FontAwesomeSolid900.h"
#include "include/GuiLogSink.h"
#include "include/IconsFontAwesome6.h"
#include <assert.h>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  Font Storage                                                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
static ImFont *g_DefaultFont = nullptr;
static ImFont *g_MonospaceFont = nullptr;

ImFont *GetDefaultFont() { return g_DefaultFont; }
ImFont *GetMonospaceFont() { return g_MonospaceFont; }
ImFont *GetIconFont() { return g_DefaultFont; } // Icons merged with default

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  SetupImGuiStyle: Apply Cyber-Dark theme to ImGui                       │
// └─────────────────────────────────────────────────────────────────────────┘
void SetupImGuiStyle() {
  ImGuiStyle &style = ImGui::GetStyle();

  // ═══════════════════════════════════════════════════════════════════════
  //  Geometry: Soft, modern rounded corners
  // ═══════════════════════════════════════════════════════════════════════
  style.WindowRounding = 6.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 9.0f;
  style.GrabRounding = 3.0f;
  style.TabRounding = 4.0f;

  // Sizing
  style.WindowPadding = ImVec2(12.0f, 12.0f);
  style.FramePadding = ImVec2(8.0f, 4.0f);
  style.ItemSpacing = ImVec2(8.0f, 6.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  style.ScrollbarSize = 12.0f;
  style.GrabMinSize = 10.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.AntiAliasedLines = true;
  style.AntiAliasedLinesUseTex = true;
  style.AntiAliasedFill = true;

  // ═══════════════════════════════════════════════════════════════════════
  //  Colors: Cyber-Dark Palette
  // ═══════════════════════════════════════════════════════════════════════
  ImVec4 *colors = style.Colors;

  // Base backgrounds
  colors[ImGuiCol_WindowBg] = Colors::WindowBg;
  colors[ImGuiCol_ChildBg] = Colors::ChildBg;
  colors[ImGuiCol_PopupBg] = Colors::PopupBg;
  colors[ImGuiCol_Border] = Colors::Border;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

  // Text
  colors[ImGuiCol_Text] = Colors::Text;
  colors[ImGuiCol_TextDisabled] = Colors::TextDim;

  // Frame backgrounds (inputs, sliders, etc.)
  colors[ImGuiCol_FrameBg] = Colors::FrameBg;
  colors[ImGuiCol_FrameBgHovered] = Colors::FrameBgHover;
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);

  // Title bar
  colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.09f, 0.5f);

  // Menu bar
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.14f, 0.14f, 0.14f, 0.5f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

  // Checkbox & Radio
  colors[ImGuiCol_CheckMark] = Colors::Primary;

  // Slider
  colors[ImGuiCol_SliderGrab] = Colors::Primary;
  colors[ImGuiCol_SliderGrabActive] = Colors::PrimaryHover;

  // Buttons
  colors[ImGuiCol_Button] = Colors::FrameBg;
  colors[ImGuiCol_ButtonHovered] = Colors::Primary;
  colors[ImGuiCol_ButtonActive] = Colors::PrimaryActive;

  // Headers (collapsing headers, menu items)
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.68f, 0.71f, 0.4f);

  // Separator
  colors[ImGuiCol_Separator] = Colors::Border;
  colors[ImGuiCol_SeparatorHovered] = Colors::Primary;
  colors[ImGuiCol_SeparatorActive] = Colors::PrimaryActive;

  // Resize grip
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.26f, 0.26f, 0.5f);
  colors[ImGuiCol_ResizeGripHovered] = Colors::Primary;
  colors[ImGuiCol_ResizeGripActive] = Colors::PrimaryActive;

  // Tabs
  colors[ImGuiCol_Tab] =
      ImVec4(0.16f, 0.16f, 0.16f, 1.0f); // FIXED type error here too
  colors[ImGuiCol_TabHovered] = Colors::Primary;
  colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

  // Docking
  colors[ImGuiCol_DockingPreview] =
      ImVec4(Colors::Primary.x, Colors::Primary.y, Colors::Primary.z, 0.7f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);

  // Plot
  colors[ImGuiCol_PlotLines] = Colors::Primary;
  colors[ImGuiCol_PlotLinesHovered] = Colors::PrimaryHover;
  colors[ImGuiCol_PlotHistogram] = Colors::Primary;
  colors[ImGuiCol_PlotHistogramHovered] = Colors::PrimaryHover;

  // Table
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.0f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.23f, 1.0f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);

  // Text selection
  colors[ImGuiCol_TextSelectedBg] =
      ImVec4(Colors::Primary.x, Colors::Primary.y, Colors::Primary.z, 0.35f);

  // Drag and drop
  colors[ImGuiCol_DragDropTarget] = Colors::Primary;

  // Navigation highlight
  colors[ImGuiCol_NavHighlight] = Colors::Primary;
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

  // Modal window dim
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  LoadFonts: Load Roboto, JetBrainsMono, and merge FontAwesome           │
// └─────────────────────────────────────────────────────────────────────────┘
static void AppendUnique(std::vector<std::filesystem::path> &paths,
                         const std::filesystem::path &path) {
  if (path.empty())
    return;
  for (const auto &existing : paths) {
    if (existing == path)
      return;
  }
  paths.push_back(path);
}

static std::vector<std::filesystem::path>
FindFontsDirs(const std::vector<std::filesystem::path> &searchRoots) {
  std::error_code ec;
  std::vector<std::filesystem::path> results;
  for (const auto &root : searchRoots) {
    if (root.empty()) {
      continue;
    }
    const auto candidate = root / "resources" / "fonts";
    if (std::filesystem::exists(candidate, ec) &&
        std::filesystem::is_directory(candidate, ec)) {
      AppendUnique(results, candidate);
    }
  }
  return results;
}

static bool FileUsable(const std::filesystem::path &path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) ||
      !std::filesystem::is_regular_file(path, ec)) {
    return false;
  }
  const auto size = std::filesystem::file_size(path, ec);
  return !ec && size > 0;
}

bool LoadFonts(ImGuiIO &io, const std::filesystem::path &executableDir,
               float baseFontSize) {
  io.Fonts->Clear();
  if (baseFontSize <= 0.0f) {
    baseFontSize = 16.0f;
  }

  std::error_code ec;
  const auto currentPath = std::filesystem::current_path(ec);
  std::vector<std::filesystem::path> searchRoots;
  if (!executableDir.empty()) {
    searchRoots.push_back(executableDir);
  }
  if (!ec && !currentPath.empty()) {
    searchRoots.push_back(currentPath);
    searchRoots.push_back(currentPath / "build");
    searchRoots.push_back(currentPath.parent_path());
  }

  if (!executableDir.empty()) {
    searchRoots.push_back(executableDir.parent_path());
  }

  std::vector<std::filesystem::path> fontsDirs = FindFontsDirs(searchRoots);
  if (!ec && !currentPath.empty()) {
    const auto repoCandidate = currentPath / "gui" / "resources" / "fonts";
    if (std::filesystem::exists(repoCandidate, ec) &&
        std::filesystem::is_directory(repoCandidate, ec)) {
      AppendUnique(fontsDirs, repoCandidate);
    }
  }

  ImFont *defaultFont = nullptr;
  ImFont *monospaceFont = nullptr;
  ImFontConfig fontConfig;
  fontConfig.OversampleH = 3;
  fontConfig.OversampleV = 3;
  fontConfig.PixelSnapH = true;

  for (const auto &fontsDir : fontsDirs) {
    if (defaultFont)
      break;
    const auto robotoPath = fontsDir / "Roboto-Medium.ttf";
    if (FileUsable(robotoPath)) {
      defaultFont = io.Fonts->AddFontFromFileTTF(robotoPath.string().c_str(),
                                                 baseFontSize, &fontConfig);
    }
  }

  if (!defaultFont) {
    defaultFont = io.Fonts->AddFontDefault(&fontConfig);
  }

  if (defaultFont) {
    // ┌─────────────────────────────────────────────────────────────────────┐
    // │  FontAwesome 6 splits icons across TWO Unicode ranges:              │
    // │  Range 1: 0xE000-0xE6FF (new FA6 icons)                             │
    // │  Range 2: 0xF000-0xF8FF (legacy FA icons)                           │
    // │  Both ranges must be specified for all icons to display correctly  │
    // └─────────────────────────────────────────────────────────────────────┘
    static const ImWchar iconRanges[] = {
        0xE000, 0xE6FF, // FA6 new icons range
        0xF000, 0xF8FF, // FA legacy icons range
        0               // Null terminator (required by ImGui)
    };

    // Load icons at same size as base font
    float iconFontSize = baseFontSize;
    if (iconFontSize <= 0.0f)
      iconFontSize = 16.0f;

    bool iconsLoaded = false;

    // ┌─────────────────────────────────────────────────────────────────────┐
    // │  Load FontAwesome icons MERGED into the default font                │
    // │  Using clear/reload approach which ensures proper glyph merging     │
    // └─────────────────────────────────────────────────────────────────────┘
    if (font_awesome_solid_900_ttf_len > 0) {
      // First pass: trigger font parsing by loading as separate font
      ImFontConfig initConfig;
      initConfig.MergeMode = false;
      initConfig.PixelSnapH = true;
      initConfig.FontDataOwnedByAtlas = false;

      io.Fonts->AddFontFromMemoryTTF(
          (void *)font_awesome_solid_900_ttf,
          static_cast<int>(font_awesome_solid_900_ttf_len), iconFontSize,
          &initConfig, iconRanges);

      // Clear and reload properly
      io.Fonts->Clear();

      // Reload default font
      ImFontConfig fontConfig2;
      fontConfig2.OversampleH = 3;
      fontConfig2.OversampleV = 3;
      fontConfig2.PixelSnapH = true;

      for (const auto &fontsDir : fontsDirs) {
        const auto robotoPath = fontsDir / "Roboto-Medium.ttf";
        if (FileUsable(robotoPath)) {
          defaultFont = io.Fonts->AddFontFromFileTTF(
              robotoPath.string().c_str(), baseFontSize, &fontConfig2);
          if (defaultFont)
            break;
        }
      }
      if (!defaultFont) {
        defaultFont = io.Fonts->AddFontDefault(&fontConfig2);
      }

      // Now load icon font MERGED with proper config
      ImFontConfig iconsConfig;
      iconsConfig.MergeMode = true;
      iconsConfig.PixelSnapH = true;
      iconsConfig.OversampleH = 3; // High quality anti-aliasing
      iconsConfig.OversampleV = 3;
      iconsConfig.GlyphMinAdvanceX = 22.0f; // Slightly wider for 20px icons
      iconsConfig.GlyphOffset = ImVec2(0.0f, 1.0f); // Re-center for 20px
      iconsConfig.FontDataOwnedByAtlas = false;

      // Load icons LARGER than text for better visibility and definition
      float highResIconSize = 20.0f;

      ImFont *iconsFont = io.Fonts->AddFontFromMemoryTTF(
          (void *)font_awesome_solid_900_ttf,
          static_cast<int>(font_awesome_solid_900_ttf_len), highResIconSize,
          &iconsConfig, iconRanges);

      if (iconsFont) {
        iconsLoaded = true;
        GuiLogSink::Instance().Info("FontAwesome Icons loaded (Embedded)");
      } else {
        GuiLogSink::Instance().Error("Failed to load FontAwesome Icons");
      }
    }

    // Fallback to file if memory load failed (unlikely)
    if (!iconsLoaded) {
      for (const auto &fontsDir : fontsDirs) {
        const auto iconsPath = fontsDir / "fa-solid-900.ttf";
        if (!FileUsable(iconsPath))
          continue;

        ImFontConfig fileFontConfig;
        fileFontConfig.MergeMode = true;
        fileFontConfig.PixelSnapH = true;
        fileFontConfig.GlyphMinAdvanceX = iconFontSize;
        fileFontConfig.GlyphOffset = ImVec2(0.0f, 3.0f);

        ImFont *iconsFont = io.Fonts->AddFontFromFileTTF(
            iconsPath.string().c_str(), iconFontSize, &fileFontConfig,
            iconRanges);

        if (iconsFont) {
          iconsLoaded = true;
          GuiLogSink::Instance().Info("FontAwesome Icons loaded from file: " +
                                      iconsPath.string());
          break;
        }
      }
    }

    if (!iconsLoaded) {
      GuiLogSink::Instance().Error("Could not load FontAwesome icons "
                                   "(embedding failed and file not found)");
    }
  }

  // Load Monospace font (after potential atlas clear)
  for (const auto &fontsDir : fontsDirs) {
    if (monospaceFont)
      break;
    const auto monoPath = fontsDir / "JetBrainsMono-Regular.ttf";
    if (FileUsable(monoPath)) {
      monospaceFont = io.Fonts->AddFontFromFileTTF(
          monoPath.string().c_str(), baseFontSize - 2.0f, &fontConfig);
    }
  }

  if (!monospaceFont) {
    monospaceFont = defaultFont;
  }

  g_DefaultFont = defaultFont;
  g_MonospaceFont = monospaceFont;
  io.FontDefault = g_DefaultFont;

  const bool built = io.Fonts->Build();

  // ┌───────────────────────────────────────────────────────────────────────┐
  // │  Post-build: Count loaded icon glyphs for diagnostic info            │
  // └───────────────────────────────────────────────────────────────────────┘
  if (built && g_DefaultFont) {
    int glyphsFoundE = 0, glyphsFoundF = 0;

    // Count glyphs in Range 1: FA6 new icons (0xE000 - 0xE6FF)
    for (ImWchar c = 0xE000; c <= 0xE6FF; c++) {
      if (g_DefaultFont->FindGlyphNoFallback(c)) {
        glyphsFoundE++;
      }
    }
    // Count glyphs in Range 2: FA legacy icons (0xF000 - 0xF8FF)
    for (ImWchar c = 0xF000; c <= 0xF8FF; c++) {
      if (g_DefaultFont->FindGlyphNoFallback(c)) {
        glyphsFoundF++;
      }
    }
  }

  return g_DefaultFont != nullptr && built;
}

bool LoadFonts(ImGuiIO &io, float baseFontSize) {
  return LoadFonts(io, std::filesystem::path(), baseFontSize);
}

} // namespace ShadPKG::GUI
