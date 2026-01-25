// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <assert.h>
#include "common/assert.h"
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "GuiLogSink.h"
#include "imgui.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  ConsoleLog: Collapsible log panel widget for bottom of main window       ║
// ║                                                                           ║
// ║  Layout:                                                                  ║
// ║  ┌──────────────────────────────────────────────────────────────────────┐ ║
// ║  │  ▼ Console                                          [Clear] [Auto⬤] │ ║
// ║  ├──────────────────────────────────────────────────────────────────────┤ ║
// ║  │  [12:34:56] [INFO] PKG Loaded: CUSA12345                             │ ║
// ║  │  [12:34:57] [WARN] RIF file checksum validation...                   │ ║
// ║  │  [12:34:58] [ERROR] Failed to extract file: ...                      │ ║
// ║  └──────────────────────────────────────────────────────────────────────┘ ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class ConsoleLog {
public:
  ConsoleLog() = default;
  ~ConsoleLog() = default;

  // Draw the console panel
  // Returns true if the console is expanded
  void Draw(float height = 150.0f);

  // Control
  bool IsExpanded() const { return expanded_; }
  void SetExpanded(bool expanded) { expanded_ = expanded; }

private:
  bool expanded_ = true;
  bool showInfo_ = true;
  bool showWarnings_ = true;
  bool showErrors_ = true;
};

} // namespace ShadPKG::GUI
