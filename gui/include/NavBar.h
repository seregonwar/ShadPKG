// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <assert.h>
#include "common/assert.h"
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "GUIContext.h"
#include "imgui.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  NavBar: Vertical navigation sidebar (Discord/VSCode style)               ║
// ║                                                                           ║
// ║  Layout:                                                                  ║
// ║  ┌────────┐                                                               ║
// ║  │  📦    │  ← Extract (PKG extraction)                                   ║
// ║  │        │                                                               ║
// ║  │  🔍    │  ← Inspect (PKG metadata viewer)                              ║
// ║  │        │                                                               ║
// ║  │  🔑    │  ← RIF (Generation & validation)                              ║
// ║  │        │                                                               ║
// ║  │        │                                                               ║
// ║  │  ⚙️    │  ← Settings                                                   ║
// ║  └────────┘                                                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class NavBar {
public:
  NavBar() = default;
  ~NavBar() = default;

  // Draw the navigation bar
  // Returns the selected view if changed, otherwise the current view
  void Draw(GUIContext &ctx);

  static constexpr float Width = 60.0f;

private:
  bool DrawNavButton(const char *icon, const char *tooltip, bool selected);
};

} // namespace ShadPKG::GUI
