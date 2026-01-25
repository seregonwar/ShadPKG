// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../GUIContext.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  SettingsView: Application settings and configuration                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class SettingsView {
public:
  SettingsView() = default;
  ~SettingsView() = default;

  void Draw(GUIContext &ctx);

  // Settings state (could be persisted to config file)
  struct Settings {
    bool autoDetectRif = true;
    bool createTitleIdSubfolder = true;
    bool showHexInInspector = true;
    int maxLogEntries = 1000;
    bool darkTheme = true;
  };

  Settings &GetSettings() { return settings_; }

private:
  Settings settings_;

  void DrawGeneralSection(GUIContext &ctx);
  void DrawExtractionSection();
  void DrawAppearanceSection();
  void DrawCreditsSection();                     // New About Author section
  void DrawContributorsSection(GUIContext &ctx); // New Contributors section
  void DrawAboutSection();
};

} // namespace ShadPKG::GUI
