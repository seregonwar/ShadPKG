// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "include/ConsoleLog.h"
#include "include/StyleManager.h"
#include <assert.h>

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  ConsoleLog::Draw - Render the collapsible console panel                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void ConsoleLog::Draw(float height) {
  auto &logSink = GuiLogSink::Instance();

  // Custom header drawing to ensure stability
  const float headerHeight = 30.0f;
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec2 contentAvail = ImGui::GetContentRegionAvail();
  float windowWidth = ImGui::GetWindowWidth();
  
  // Background
  ImGui::GetWindowDrawList()->AddRectFilled(
      p, ImVec2(p.x + windowWidth, p.y + headerHeight),
      ImGui::GetColorU32(Colors::NavBarBg));

  // Toggle button (invisible, covers the left part)
  ImGui::SetCursorScreenPos(p);
  if (ImGui::InvisibleButton("##console_toggle", ImVec2(150, headerHeight))) {
    expanded_ = !expanded_;
  }
  
  // Icon and Title
  ImGui::SetCursorScreenPos(ImVec2(p.x + 8, p.y + 6)); // Align text
  const char *headerIcon = expanded_ ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
  ImGui::Text("%s  Console", headerIcon);

  // If expanded, draw content
  if (expanded_) {
    const ImGuiStyle &style = ImGui::GetStyle();
    const float buttonSpacing = style.ItemSpacing.x;
    auto buttonWidth = [&style](const char *label) {
      const ImVec2 size = ImGui::CalcTextSize(label);
      return size.x + style.FramePadding.x * 2.0f;
    };
    const float toolbarWidth =
        buttonWidth("I") + buttonWidth("W") + buttonWidth("E") +
        buttonWidth(ICON_FA_DOWNLOAD) + buttonWidth(ICON_FA_FILE " Copy") + buttonWidth("Clear") +
        buttonSpacing * 5.0f;
    
    // Position toolbar on the right
    float cursorX = windowWidth - toolbarWidth - 8.0f; // Right align with padding
    ImGui::SetCursorScreenPos(ImVec2(p.x + cursorX, p.y + 4)); // Vertical align

    ImGui::PushID("console_toolbar");
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    ImGui::PushStyleColor(ImGuiCol_Button,
                          showInfo_ ? Colors::Success : Colors::FrameBg);
    if (ImGui::SmallButton("I")) {
      showInfo_ = !showInfo_;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Toggle Info messages");

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          showWarnings_ ? Colors::Warning : Colors::FrameBg);
    if (ImGui::SmallButton("W")) {
      showWarnings_ = !showWarnings_;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Toggle Warning messages");

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,
                          showErrors_ ? Colors::Error : Colors::FrameBg);
    if (ImGui::SmallButton("E")) {
      showErrors_ = !showErrors_;
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Toggle Error messages");

    ImGui::SameLine();

    // Auto-scroll toggle
    bool autoScroll = logSink.IsAutoScroll();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          autoScroll ? Colors::Primary : Colors::FrameBg);
    if (ImGui::SmallButton(ICON_FA_DOWNLOAD)) {
      logSink.SetAutoScroll(!autoScroll);
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Auto-scroll");

    ImGui::SameLine();

    // Copy button
    if (ImGui::SmallButton(ICON_FA_FILE " Copy")) {
      std::string clipboardText;
      auto logs = logSink.GetLogs();
      for (const auto &entry : logs) {
        // Apply current filters to clipboard copy too
        if (entry.level == LogLevel::Info && !showInfo_) continue;
        if (entry.level == LogLevel::Warning && !showWarnings_) continue;
        if (entry.level == LogLevel::Error && !showErrors_) continue;

        clipboardText += "[" + entry.timestamp + "] ";
        switch (entry.level) {
          case LogLevel::Info: clipboardText += "[INFO] "; break;
          case LogLevel::Warning: clipboardText += "[WARN] "; break;
          case LogLevel::Error: clipboardText += "[ERROR] "; break;
        }
        clipboardText += entry.message + "\n";
      }
      ImGui::SetClipboardText(clipboardText.c_str());
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Copy visible logs to clipboard");

    ImGui::SameLine();

    // Clear button
    if (ImGui::SmallButton("Clear")) {
      logSink.Clear();
    }

    ImGui::PopStyleVar();
    ImGui::PopID();

    // Move cursor below header
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + headerHeight));

    // Log content
    ImGui::BeginChild("##log_content", ImVec2(0, height), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    auto logs = logSink.GetLogs();

    for (const auto &entry : logs) {
      // Filter by level
      if (entry.level == LogLevel::Info && !showInfo_)
        continue;
      if (entry.level == LogLevel::Warning && !showWarnings_)
        continue;
      if (entry.level == LogLevel::Error && !showErrors_)
        continue;

      // Timestamp
      ImGui::TextColored(Colors::TextDim, "[%s]", entry.timestamp.c_str());
      ImGui::SameLine();

      // Level indicator
      switch (entry.level) {
      case LogLevel::Info:
        ImGui::TextColored(Colors::Success, "[INFO]");
        break;
      case LogLevel::Warning:
        ImGui::TextColored(Colors::Warning, "[WARN]");
        break;
      case LogLevel::Error:
        ImGui::TextColored(Colors::Error, "[ERROR]");
        break;
      }
      ImGui::SameLine();

      // Message
      ImGui::TextUnformatted(entry.message.c_str());
    }

    // Auto-scroll to bottom
    if (logSink.IsAutoScroll() &&
        ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
  }
}

} // namespace ShadPKG::GUI
