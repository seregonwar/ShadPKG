// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "include/NavBar.h"
#include <assert.h>
#include "imgui.h"
#include "include/StyleManager.h"

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  NavBar::Draw - Render the vertical navigation sidebar                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void NavBar::Draw(GUIContext &ctx) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::NavBarBg);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));

  ImGui::BeginChild("##navbar", ImVec2(Width, 0), false,
                    ImGuiWindowFlags_NoScrollbar);

  // Center content
  float centerX = (Width - 40.0f) / 2.0f;
  ImGui::SetCursorPosX(centerX);

  // Top section: Main navigation
  GUIContext::View currentView = ctx.GetCurrentView();

  ImGui::Spacing();
  ImGui::Spacing();

  // Extract button
  ImGui::SetCursorPosX(centerX);
  if (DrawNavButton(ICON_FA_FILE_ZIPPER, "Extract PKG",
                    currentView == GUIContext::View::Extractor)) {
    ctx.SetCurrentView(GUIContext::View::Extractor);
  }

  ImGui::Spacing();
  ImGui::Spacing();

  // Inspect button
  ImGui::SetCursorPosX(centerX);
  if (DrawNavButton(ICON_FA_MAGNIFYING_GLASS, "Inspect PKG",
                    currentView == GUIContext::View::Inspector)) {
    ctx.SetCurrentView(GUIContext::View::Inspector);
  }

  ImGui::Spacing();
  ImGui::Spacing();

  // RIF button
  ImGui::SetCursorPosX(centerX);
  if (DrawNavButton(ICON_FA_KEY, "RIF Tools",
                    currentView == GUIContext::View::RIF)) {
    ctx.SetCurrentView(GUIContext::View::RIF);
  }

  ImGui::Spacing();
  ImGui::Spacing();

  // Decompiler button
  ImGui::SetCursorPosX(centerX);
  if (DrawNavButton(ICON_FA_TERMINAL, "Decompiler",
                    currentView == GUIContext::View::Decompiler)) {
    ctx.SetCurrentView(GUIContext::View::Decompiler);
  }

  // Spacer to push settings to bottom
  float availHeight = ImGui::GetContentRegionAvail().y;
  ImGui::Dummy(ImVec2(0, availHeight - 60));

  // Settings button (at bottom)
  ImGui::SetCursorPosX(centerX);
  if (DrawNavButton(ICON_FA_GEAR, "Settings",
                    currentView == GUIContext::View::Settings)) {
    ctx.SetCurrentView(GUIContext::View::Settings);
  }

  ImGui::EndChild();

  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  DrawNavButton: Render a single navigation button                      │
// └─────────────────────────────────────────────────────────────────────────┘
bool NavBar::DrawNavButton(const char *icon, const char *tooltip,
                           bool selected) {
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

  // Different colors for selected state
  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, Colors::Primary);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::PrimaryHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::PrimaryActive);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::PrimaryActive);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

  bool clicked = ImGui::Button(icon, ImVec2(40, 40));

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return clicked;
}

} // namespace ShadPKG::GUI
