// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../GUIContext.h"
#include "core/file_format/pkg.h"
#include <memory>
#include <string>
#include <vector>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  InspectorView: PKG metadata and filesystem browser                      ║
// ║                                                                           ║
// ║  Split Layout:                                                            ║
// ║  ┌─────────────────────────────┬─────────────────────────────────────────┐
// ║ ║  │  METADATA (SFO)             │  FILE SYSTEM (PFS) │ ║ ║  │
// ┌───────────┬────────────┐ │  [ 🔍 Search Filter...               ]  │ ║ ║  │
// │ Key       │ Value      │ │                                         │ ║ ║  │
// │───────────│────────────│ │  Name                | Size   | Type   │ ║ ║  │
// │ TITLE_ID  │ CUSA12345  │ │  ├── 📁 assets       │ -      │ DIR    │ ║ ║  │
// │ APP_VER   │ 01.05      │ │  │   └── 📁 textures │ -      │ DIR    │ ║ ║  │
// │ PUB_TOOL  │ 4.50       │ │  │       ├── hero.tex│ 4MB    │ FILE   │ ║ ║  │
// │ TOTAL_SIZE│ 45 GB      │ │  │       └── map.tex │ 12MB   │ FILE   │ ║ ║  │
// └───────────┴────────────┘ │  ├── data.bin        │ 1GB    │ FILE   │ ║ ║  │
// │  ├── 🖼️ icon0.png    │ 50KB   │ FILE   │ ║ ║  │ │  └── 📄 param.sfo    │
// 2KB    │ FILE   │ ║ ║
// └─────────────────────────────┴─────────────────────────────────────────┘ ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

class InspectorView {
public:
  InspectorView() = default;
  ~InspectorView() = default;

  void Draw(GUIContext &ctx);
  void LoadPkg(const std::string &path);
  void Clear();

  bool IsPkgLoaded() const { return pkgLoaded_; }

private:
  // ═══════════════════════════════════════════════════════════════════════
  //  SFO Entry for display
  // ═══════════════════════════════════════════════════════════════════════
  struct SfoEntry {
    std::string key;
    std::string value;
    std::string type; // "String", "Integer", "Binary"
  };

  // ═══════════════════════════════════════════════════════════════════════
  //  File tree node for PFS display
  // ═══════════════════════════════════════════════════════════════════════
  struct FileNode {
    std::string name;
    std::string fullPath;
    uint64_t size = 0;
    bool isDirectory = false;
    std::vector<std::unique_ptr<FileNode>> children;
    bool expanded = false;
    int fileIndex = -1; // Index in fsTable for extraction
  };

  // PKG data
  char pkgPathBuf_[512] = "";
  bool pkgLoaded_ = false;
  std::string titleId_;
  std::string contentId_;
  uint64_t pkgSize_ = 0;

  // SFO entries
  std::vector<SfoEntry> sfoEntries_;

  // File tree
  std::unique_ptr<FileNode> fileTreeRoot_;
  bool fileTreeLoaded_ = false;
  char searchFilter_[128] = "";

  // Split position
  float splitRatio_ = 0.35f;

  // UI drawing
  void DrawLoadSection();
  void DrawMetadataPanel();
  void DrawFilesystemPanel();
  void DrawFileNode(FileNode *node, int depth = 0);

  // Data loading
  void LoadSfoData(const PKG &pkg);
  void LoadFileTree(const PKG &pkg);
  void BuildFileTree(const std::vector<PKG::EntryInfo> &entries);

  // Extraction
  void ExtractNode(const FileNode *node);
  std::string PickFolder();

  // Helpers
  static std::string FormatSize(uint64_t bytes);
  static const char *GetFileIcon(const std::string &name, bool isDir);
  bool MatchesFilterText(const std::string &text) const;
  bool NodeMatchesFilter(const FileNode *node) const;
};

} // namespace ShadPKG::GUI
