// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../include/views/SettingsView.h"
#include "../include/GuiLogSink.h"
#include "../include/StyleManager.h"
#include "common/assert.h"
#include "common/version.h"

// STL includes for stb_image
#include <cassert>
#include <cstdio>
#include <cstdlib> // malloc, free, etc
#include <cstring>

#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "../include/IconsFontAwesome6.h"
#include "imgui.h"

// Define stb_image implementation
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) (void)(0)
#include "../include/stb_image.h"

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

namespace ShadPKG::GUI {

// Global texture state for Avatar
static GLuint g_AvatarTexture = 0;
static int g_AvatarWidth = 0;
static int g_AvatarHeight = 0;
static bool g_AvatarLoaded = false;

// Helper to load texture
static bool LoadTextureFromFile(const char *filename, GLuint *out_texture,
                                int *out_width, int *out_height) {
  int image_width = 0;
  int image_height = 0;
  unsigned char *image_data =
      stbi_load(filename, &image_width, &image_height, NULL, 4);
  if (image_data == NULL)
    return false;

  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image_data);
  stbi_image_free(image_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  SettingsView::Draw - Main render function                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void SettingsView::Draw(GUIContext &ctx) {
  ctx.CheckForUpdates();

  static bool contributorsTriggered = false;
  if (!contributorsTriggered) {
    ctx.FetchContributors();
    contributorsTriggered = true;
  }

  // Load Avatar on first draw
  if (!g_AvatarLoaded) {
    // Try loading from assets folder
    if (LoadTextureFromFile("assets/logo.jpeg", &g_AvatarTexture,
                            &g_AvatarWidth, &g_AvatarHeight)) {
      g_AvatarLoaded = true;
    } else if (LoadTextureFromFile("../assets/logo.jpeg", &g_AvatarTexture,
                                   &g_AvatarWidth, &g_AvatarHeight)) {
      g_AvatarLoaded = true;
    } else {
      // If execution dir is different, try looking relative to common paths
      // Just mark as loaded to stop trying if it fails
      g_AvatarLoaded = true;
      g_AvatarTexture = 0;
    }
  }

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  DrawGeneralSection(ctx);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawExtractionSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawAppearanceSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawCreditsSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawContributorsSection(ctx);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawAboutSection();

  ImGui::PopStyleVar();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  General Settings                                                       │
// └─────────────────────────────────────────────────────────────────────────┘
void SettingsView::DrawGeneralSection(GUIContext &ctx) {
  // Update Notification Banner
  const auto &status = ctx.GetUpdateStatus();
  if (status.hasUpdate) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
    if (ImGui::BeginChild("UpdateBanner", ImVec2(0, 45), true)) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f),
                         ICON_FA_DOWNLOAD "  New Version Available: %s",
                         status.latestVersion.c_str());

      ImGui::SameLine();
      ImGui::Dummy(ImVec2(20, 0));
      ImGui::SameLine();

      if (ImGui::Button("Download")) {
        system(("open " + status.releaseUrl).c_str());
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  ImGui::TextColored(Colors::Primary, ICON_FA_GEAR "  GENERAL");
  ImGui::Spacing();

  ImGui::Checkbox("Auto-detect RIF files", &settings_.autoDetectRif);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically search for matching RIF files in the same "
                      "directory as the PKG");
  }

  ImGui::SliderInt("Max Log Entries", &settings_.maxLogEntries, 100, 5000);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Maximum number of log entries to keep in memory");
  }

  GuiLogSink::Instance().SetMaxEntries(
      static_cast<size_t>(settings_.maxLogEntries));
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Extraction Settings                                                    │
// └─────────────────────────────────────────────────────────────────────────┘
void SettingsView::DrawExtractionSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_DOWNLOAD "  EXTRACTION");
  ImGui::Spacing();

  ImGui::Checkbox("Create TitleID subfolder",
                  &settings_.createTitleIdSubfolder);
  ImGui::Checkbox("Show hex values in Inspector",
                  &settings_.showHexInInspector);
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Appearance Settings                                                    │
// └─────────────────────────────────────────────────────────────────────────┘
void SettingsView::DrawAppearanceSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_IMAGE "  APPEARANCE");
  ImGui::Spacing();

  ImGui::BeginDisabled(true);
  ImGui::Checkbox("Dark Theme (Cyber-Dark)", &settings_.darkTheme);
  ImGui::EndDisabled();
  ImGui::TextColored(Colors::TextDim, "Only dark theme is currently supported");
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  About Section                                                          │
// └─────────────────────────────────────────────────────────────────────────┘
void SettingsView::DrawAboutSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_CIRCLE_INFO "  ABOUT");
  ImGui::Spacing();

  ImGui::PushStyleColor(ImGuiCol_Text, Colors::Primary);
  ImFont *monoFont = ShadPKG::GUI::GetMonospaceFont();
  if (monoFont) {
    ImGui::PushFont(monoFont);
  }
  ImGui::TextUnformatted(R"(

         __              ______  __ ________
   _____/ /_  ____ _____/ / __ \/ //_/ ____/
  / ___/ __ \/ __ `/ __  / /_/ / ,< / / __  
 (__  ) / / / /_/ / /_/ / ____/ /| / /_/ /  
/____/_/ /_/\__,_/\__,_/_/   /_/ |_\____/   

    )");
  if (monoFont) {
    ImGui::PopFont();
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::Text("Version: %s", Common::VERSION);
  ImGui::TextColored(Colors::TextDim, "PKG Extractor and Inspector");

  ImGui::Spacing();
  ImGui::Text("GitHub:");
  ImGui::SameLine();
  ImGui::TextColored(Colors::Primary, "https://github.com/seregonwar/shadPKG");

  ImGui::Spacing();
  ImGui::TextColored(Colors::TextDim, "Licensed under LGPL-2.1-or-later");

  ImGui::Spacing();
  ImGui::Spacing();
  if (ImGui::Button("Clear Logs")) {
    GuiLogSink::Instance().Clear();
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Credits Section                                                        │
// └─────────────────────────────────────────────────────────────────────────┘
static void DrawAvatarPlaceholder(float size) {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();
  float radius = size * 0.5f;
  ImVec2 center = ImVec2(p.x + radius, p.y + radius);

  draw_list->AddCircleFilled(center, radius, IM_COL32(0, 173, 181, 255));

  const char *initials = "SW";
  ImVec2 text_size = ImGui::CalcTextSize(initials);
  ImVec2 text_pos =
      ImVec2(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), initials);
  draw_list->AddCircle(center, radius, IM_COL32(255, 255, 255, 100), 0, 2.0f);
  ImGui::Dummy(ImVec2(size, size));
}

void SettingsView::DrawCreditsSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_USER "  CREDITS");
  ImGui::Spacing();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));

  if (ImGui::BeginChild("ProfileCard", ImVec2(0, 110), true,
                        ImGuiWindowFlags_NoScrollbar)) {
    // Avatar
    if (g_AvatarTexture != 0) {
      // Draw real avatar
      ImGui::Image((ImTextureID)(uintptr_t)g_AvatarTexture, ImVec2(64, 64));
    } else {
      // Fallback
      DrawAvatarPlaceholder(64.0f);
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();

    // Info
    ImGui::BeginGroup();
    {
      ImGui::PushFont(ShadPKG::GUI::GetDefaultFont());
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "seregonwar");
      ImGui::PopFont();

      ImGui::TextColored(Colors::TextDim, "Lead Developer");
      ImGui::Spacing();

      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));

      if (ImGui::Button(ICON_FA_TERMINAL " GitHub")) {
        system("open https://github.com/seregonwar");
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Visit GitHub Profile");

      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.66f, 0.89f, 1.0f));
      if (ImGui::Button(ICON_FA_MUG_HOT " Support")) {
        system("open https://ko-fi.com/seregon");
      }
      ImGui::PopStyleColor();
      ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      if (ImGui::Button(ICON_FA_HEART " Sponsor")) {
        system("open https://github.com/sponsors/seregonwar");
      }
      ImGui::PopStyleColor();

      ImGui::PopStyleVar();
    }
    ImGui::EndGroup();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Contributors Section                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
void SettingsView::DrawContributorsSection(GUIContext &ctx) {
  ImGui::TextColored(Colors::Primary,
                     ICON_FA_USER "  CONTRIBUTORS & COMMUNITY");
  ImGui::Spacing();

  // Background frame for contributors
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 0.5f));
  ImGui::BeginChild("ContributorsList", ImVec2(0, 80), true);

  const auto &contributors = ctx.GetContributors();

  if (contributors.empty()) {
    ImGui::TextColored(Colors::TextDim, "Loading community from GitHub...");
  } else {
    // Simple flowing layout
    ImGuiStyle &style = ImGui::GetStyle();
    float window_visible_x2 =
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    for (size_t i = 0; i < contributors.size(); i++) {
      const auto &c = contributors[i];
      ImGui::PushID((int)i); // Fixes ID conflict error

      // Draw button
      std::string label = c.name.empty() ? "Contributor" : c.name;
      if (ImGui::Button(label.c_str())) {
        system(("open " + c.url).c_str());
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("View Profile: %s", c.url.c_str());

      // Layout wrapping
      float last_button_x2 = ImGui::GetItemRectMax().x;
      float next_button_x2 =
          last_button_x2 + style.ItemSpacing.x + 100.0f; // estimation
      if (i + 1 < contributors.size() && next_button_x2 < window_visible_x2)
        ImGui::SameLine();

      ImGui::PopID();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

} // namespace ShadPKG::GUI
