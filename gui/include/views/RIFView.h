// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../GUIContext.h"
#include <string>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  RIFView: RIF file generation and validation                              ║
// ║                                                                           ║
// ║  Features:                                                                ║
// ║  - Generate RIF from Content ID                                           ║
// ║  - Validate existing RIF files                                            ║
// ║  - Display RIF file information                                           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class RIFView {
public:
  RIFView() = default;
  ~RIFView() = default;

  void Draw(GUIContext &ctx);

private:
  // Generate section
  char contentIdBuf_[64] = "";
  char outputDirBuf_[512] = "";
  bool generateSuccess_ = false;
  bool generateFailed_ = false;
  std::string generatedPath_;

  // Validate section
  char rifPathBuf_[512] = "";
  bool validated_ = false;
  bool rifValid_ = false;
  std::string rifInfo_;

  // UI sections
  void DrawGenerateSection();
  void DrawValidateSection();

  // Actions
  void GenerateRif();
  void ValidateRif();

  bool ShowOpenFileDialog(const char *title, const char *filter, char *outPath,
                          size_t pathSize);
  bool ShowSelectFolderDialog(const char *title, char *outPath,
                              size_t pathSize);
};

} // namespace ShadPKG::GUI
