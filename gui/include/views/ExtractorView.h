// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../GUIContext.h"
#include <string>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  ExtractorView: Main PKG extraction interface                             ║
// ║                                                                           ║
// ║  Layout:                                                                  ║
// ║  ┌──────────────────────────────────────────────────────────────────────┐ ║
// ║  │  SOURCE FILE                                                         │ ║
// ║  │  [📦] [ path/to/file.pkg .......................... ] [ Browse ]     │ ║
// ║  │       *Drag & Drop enabled* │ ║
// ║  ├──────────────────────────────────────────────────────────────────────┤ ║
// ║  │  DECRYPTION SETTINGS                                                 │ ║
// ║  │  [X] Enable RIF Decryption                                           │ ║
// ║  │      └── Key: [ path/to/file.rif ................... ] [ Browse ]    │ ║
// ║  │               Status: ✓ VALID | Content-ID: EP0001...                │ ║
// ║  ├──────────────────────────────────────────────────────────────────────┤ ║
// ║  │  OUTPUT DESTINATION                                                  │ ║
// ║  │  [📁] [ output/directory .......................... ] [ Browse ]     │ ║
// ║  │  [X] Create subfolder with TitleID                                   │ ║
// ║  ├──────────────────────────────────────────────────────────────────────┤ ║
// ║  │                                                                      │ ║
// ║  │                    [      EXTRACT PKG      ]                         │ ║
// ║  │                                                                      │ ║
// ║  │  Status: Extracting file.ext...                                      │ ║
// ║  │  [████████████████████░░░░░░░░░░░░] 65%                              │ ║
// ║  └──────────────────────────────────────────────────────────────────────┘ ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class ExtractorView {
public:
  ExtractorView() = default;
  ~ExtractorView() = default;

  // Render the view
  void Draw(GUIContext &ctx);

  // Drag & drop callback (called from main window)
  void OnFileDrop(GUIContext &ctx, const std::string &path);

  // PKG info after loading
  struct PkgInfo {
    std::string titleId;
    std::string contentId;
    std::string appVersion;
    uint64_t pkgSize = 0;
    bool valid = false;
  };

  const PkgInfo &GetPkgInfo() const { return pkgInfo_; }

private:
  // Input paths
  char pkgPathBuf_[512] = "";
  char rifPathBuf_[512] = "";
  char outPathBuf_[512] = "";

  // Options
  bool useRif_ = false;
  bool createSubfolder_ = true;

  // RIF validation state
  bool rifValid_ = false;
  std::string rifContentId_;

  // PKG info
  PkgInfo pkgInfo_;
  bool pkgLoaded_ = false;

  // UI helpers
  void DrawSourceSection(GUIContext &ctx);
  void DrawDecryptionSection();
  void DrawOutputSection();
  void DrawExtractButton(GUIContext &ctx);
  void DrawProgressSection(GUIContext &ctx);

  // File dialog helpers
  bool ShowOpenFileDialog(const char *title, const char *filter, char *outPath,
                          size_t pathSize);
  bool ShowSelectFolderDialog(const char *title, char *outPath,
                              size_t pathSize);

  // Validation
  void ValidatePkgFile(GUIContext *ctx = nullptr);
  void ValidateRifFile();
  bool ValidatePath(const char *path);

  // Auto-detect RIF in same folder as PKG
  void AutoDetectRif();
};

} // namespace ShadPKG::GUI
