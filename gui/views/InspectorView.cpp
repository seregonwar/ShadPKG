// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../include/views/InspectorView.h"
#include "../include/GuiLogSink.h"
#include "../include/StyleManager.h"
#include "common/assert.h"
#include "core/file_format/psf.h"
#include <algorithm>
#include <assert.h>
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "imgui.h"
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  InspectorView::Draw                                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void InspectorView::Draw(GUIContext &ctx) {
  // Sync with global context if a PKG was loaded elsewhere (e.g. Home)
  if (ctx.pkgLoaded && !pkgLoaded_ && !ctx.loadedPkgPath.empty()) {
    GuiLogSink::Instance().Info("Inspector: Syncing PKG from context: " +
                                ctx.loadedPkgPath);
    strncpy(pkgPathBuf_, ctx.loadedPkgPath.c_str(), sizeof(pkgPathBuf_) - 1);
    pkgPathBuf_[sizeof(pkgPathBuf_) - 1] = '\0';
    LoadPkg(ctx, pkgPathBuf_);

    // Clear path from context to prevent re-syncing every frame
    // But keep pkgLoaded true so we know *a* package is active globally
    ctx.loadedPkgPath.clear();
  }

  DrawLoadSection(ctx);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  if (!pkgLoaded_) {
    // Show placeholder when no PKG is loaded
    ImVec2 center = ImGui::GetContentRegionAvail();
    center.x = center.x / 2.0f;
    center.y = center.y / 2.0f;

    ImGui::SetCursorPos(center);
    ImGui::TextColored(Colors::TextDim,
                       ICON_FA_MAGNIFYING_GLASS "  Load a PKG file to inspect");
    ImGui::PopStyleVar();
    return;
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Split view
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float metadataWidth = avail.x * splitRatio_;

  // Left panel: Metadata
  ImGui::BeginChild("##metadata_panel", ImVec2(metadataWidth - 5, avail.y),
                    true);
  DrawMetadataPanel();
  ImGui::EndChild();

  ImGui::SameLine();

  // Splitter
  ImGui::InvisibleButton("##splitter", ImVec2(8, avail.y));
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  if (ImGui::IsItemActive()) {
    float delta = ImGui::GetIO().MouseDelta.x / avail.x;
    splitRatio_ = std::clamp(splitRatio_ + delta, 0.2f, 0.6f);
  }

  ImGui::SameLine();

  // Right panel: File system
  ImGui::BeginChild("##filesystem_panel", ImVec2(0, avail.y), true);
  DrawFilesystemPanel();
  ImGui::EndChild();

  ImGui::PopStyleVar();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Load Section                                                           │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::DrawLoadSection(GUIContext &ctx) {
  ImGui::TextColored(Colors::Primary,
                     ICON_FA_MAGNIFYING_GLASS "  PKG INSPECTOR");
  ImGui::Spacing();

  ImGui::SetNextItemWidth(-100);
  ImGui::InputTextWithHint("##inspectpath", "PKG file path...", pkgPathBuf_,
                           sizeof(pkgPathBuf_));

  ImGui::SameLine();
  if (ImGui::Button("Load", ImVec2(90, 0))) {
    if (strlen(pkgPathBuf_) > 0) {
      LoadPkg(ctx, pkgPathBuf_);
    }
  }

  if (pkgLoaded_) {
    ImGui::SameLine();
    ImGui::TextColored(Colors::Success, ICON_FA_CHECK " %s", titleId_.c_str());
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Metadata Panel (SFO)                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::DrawMetadataPanel() {
  ImGui::TextColored(Colors::Primary, "METADATA (SFO)");
  ImGui::Spacing();

  if (sfoEntries_.empty()) {
    ImGui::TextColored(Colors::TextDim, "No SFO data available");
    return;
  }

  // Table of SFO entries
  if (ImGui::BeginTable("##sfo_table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {

    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto &entry : sfoEntries_) {
      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      ImGui::TextColored(Colors::TextDim, "%s", entry.key.c_str());

      ImGui::TableNextColumn();
      // Color special entries
      if (entry.key == "TITLE_ID" || entry.key == "CONTENT_ID") {
        ImGui::TextColored(Colors::Primary, "%s", entry.value.c_str());
      } else {
        ImGui::Text("%s", entry.value.c_str());
      }
    }

    ImGui::EndTable();
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Filesystem Panel (PFS)                                                 │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::DrawFilesystemPanel() {
  ImGui::TextColored(Colors::Primary, "FILE SYSTEM (PFS)");
  ImGui::Spacing();

  // Search filter
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##filefilter",
                           ICON_FA_MAGNIFYING_GLASS " Search files...",
                           searchFilter_, sizeof(searchFilter_));
  ImGui::Spacing();

  if (!fileTreeLoaded_ || !fileTreeRoot_) {
    if (pkgLoaded_) {
      // Show load button for lazy loading
      ImGui::TextColored(Colors::TextDim, "File tree not loaded.");
      if (ImGui::Button("Load File List")) {
        // Re-scan PKG to get file list
        PKG pkg;
        std::string failreason;
        if (pkg.Scan(pkgPathBuf_, failreason)) {
          LoadFileTree(pkg);
        }
      }
    } else {
      ImGui::TextColored(Colors::TextDim, "No PKG loaded");
    }
    return;
  }

  // File tree table
  if (ImGui::BeginTable("##filetree", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {

    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableHeadersRow();

    // Draw root children
    if (fileTreeRoot_) {
      for (const auto &child : fileTreeRoot_->children) {
        DrawFileNode(child.get());
      }
    }

    ImGui::EndTable();
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Draw single file/directory node                                        │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::DrawFileNode(FileNode *node, int depth) {
  if (!node)
    return;

  const bool filterActive = strlen(searchFilter_) > 0;
  if (filterActive && !NodeMatchesFilter(node)) {
    return;
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  // Push unique ID based on pointer address to avoid collisions
  ImGui::PushID(node);

  const char *icon = GetFileIcon(node->name, node->isDirectory);
  bool open = false;
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;

  if (node->isDirectory && !node->children.empty()) {
    if (filterActive) {
      ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    open = ImGui::TreeNodeEx(node->name.c_str(), flags, "%s %s", icon,
                             node->name.c_str());
  } else {
    // Leaf node
    ImGui::TreeNodeEx(node->name.c_str(),
                      flags | ImGuiTreeNodeFlags_Leaf |
                          ImGuiTreeNodeFlags_NoTreePushOnOpen,
                      "%s %s", icon, node->name.c_str());
  }

  // Context menu (must be after item)
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem(ICON_FA_DOWNLOAD "  Extract...")) {
      ExtractNode(node);
    }
    ImGui::EndPopup();
  }

  ImGui::TableNextColumn();
  if (!node->isDirectory) {
    ImGui::Text("%s", FormatSize(node->size).c_str());
  } else {
    ImGui::TextColored(Colors::TextDim, "-");
  }

  ImGui::TableNextColumn();
  ImGui::Text("%s", node->isDirectory ? "DIR" : "FILE");

  if (open) {
    for (const auto &child : node->children) {
      DrawFileNode(child.get(), depth + 1);
    }
    ImGui::TreePop();
  }

  ImGui::PopID();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Data Loading                                                           │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::LoadPkg(GUIContext &ctx, const std::string &path) {
  Clear();

  strncpy(pkgPathBuf_, path.c_str(), sizeof(pkgPathBuf_) - 1);
  pkgPathBuf_[sizeof(pkgPathBuf_) - 1] = '\0';

  auto pkg = std::make_shared<PKG>();
  std::string failreason;

  // Use Open ensuring SFO is loaded
  if (!pkg->Open(path, failreason)) {
    GuiLogSink::Instance().Error("Failed to open PKG: " + failreason);
    return;
  }

  titleId_ = std::string(pkg->GetTitleID());
  pkgSize_ = pkg->GetPkgSize();
  pkgLoaded_ = true;

  // Load SFO data (Open populates this)
  LoadSfoData(*pkg);

  // Use Scan to populate file table for PFS
  if (pkg->Scan(path, failreason)) {
    LoadFileTree(*pkg);
  } else {
    GuiLogSink::Instance().Warn("Failed to scan PKG filesystem: " + failreason);
  }

  // Set shared instance
  ctx.currentPkg = pkg;

  GuiLogSink::Instance().Info("PKG inspected: " + titleId_);
}

void InspectorView::LoadSfoData(const PKG &pkg) {
  sfoEntries_.clear();

  if (pkg.sfo.empty()) {
    return;
  }

  PSF psf;
  // Need non-const access for PSF::Open
  std::vector<u8> sfoData = pkg.sfo;
  if (!psf.Open(sfoData)) {
    GuiLogSink::Instance().Warn("Failed to parse param.sfo");
    return;
  }

  const auto &entries = psf.GetEntries();
  for (const auto &entry : entries) {
    SfoEntry sfoEntry;
    sfoEntry.key = entry.key;

    switch (entry.param_fmt) {
    case PSFEntryFmt::Text:
      if (auto v = psf.GetString(entry.key); v.has_value()) {
        sfoEntry.value = std::string(*v);
        sfoEntry.type = "String";

        if (entry.key == "CONTENT_ID") {
          contentId_ = sfoEntry.value;
        }
      }
      break;

    case PSFEntryFmt::Integer:
      if (auto v = psf.GetInteger(entry.key); v.has_value()) {
        sfoEntry.value = std::to_string(*v);
        sfoEntry.type = "Integer";
      }
      break;

    case PSFEntryFmt::Binary:
      if (auto v = psf.GetBinary(entry.key); v.has_value()) {
        std::ostringstream ss;
        ss << "0x";
        for (size_t i = 0; i < std::min(v->size(), size_t(8)); ++i) {
          ss << std::hex << std::setfill('0') << std::setw(2)
             << static_cast<int>((*v)[i]);
        }
        if (v->size() > 8)
          ss << "...";
        sfoEntry.value = ss.str();
        sfoEntry.type = "Binary";
      }
      break;
    }

    sfoEntries_.push_back(sfoEntry);
  }
}

void InspectorView::LoadFileTree(const PKG &pkg) {
  auto entries = pkg.GetEntriesInfo();
  BuildFileTree(entries);
  fileTreeLoaded_ = true;
}

void InspectorView::BuildFileTree(const std::vector<PKG::EntryInfo> &entries) {
  fileTreeRoot_ = std::make_unique<FileNode>();
  fileTreeRoot_->name = "/";
  fileTreeRoot_->isDirectory = true;

  // Map for quick parent lookup
  std::map<std::string, FileNode *> pathMap;
  pathMap[""] = fileTreeRoot_.get();
  pathMap["/"] = fileTreeRoot_.get();

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
    std::string path = entry.path.empty() ? entry.name : entry.path;

    // Find or create parent directories
    std::filesystem::path fsPath(path);
    FileNode *parent = fileTreeRoot_.get();

    std::string accumulated;
    for (auto it = fsPath.begin(); it != fsPath.end(); ++it) {
      std::string component = it->string();
      // Skip current/parent dir markers to avoid garbage entries
      if (component.empty() || component == "/" || component == "." ||
          component == "..")
        continue;

      auto next = it;
      ++next;
      bool isLast = (next == fsPath.end());

      std::string newPath =
          accumulated.empty() ? component : accumulated + "/" + component;

      if (isLast) {
        // This is the actual entry
        auto node = std::make_unique<FileNode>();
        node->name = component;
        node->fullPath = path;
        node->size = entry.size;
        node->isDirectory = (entry.type == 1); // PFS_DIR
        if (!node->isDirectory) {
          node->fileIndex = static_cast<int>(i);
        }

        parent->children.push_back(std::move(node));
      } else {
        // Intermediate directory
        auto it2 = pathMap.find(newPath);
        if (it2 != pathMap.end()) {
          parent = it2->second;
        } else {
          auto node = std::make_unique<FileNode>();
          node->name = component;
          node->fullPath = newPath;
          node->isDirectory = true;

          FileNode *rawPtr = node.get();
          parent->children.push_back(std::move(node));
          pathMap[newPath] = rawPtr;
          parent = rawPtr;
        }
      }

      accumulated = newPath;
    }
  }
}

void InspectorView::Clear() {
  pkgLoaded_ = false;
  sfoEntries_.clear();
  fileTreeRoot_.reset();
  fileTreeLoaded_ = false;
  titleId_.clear();
  contentId_.clear();
  pkgSize_ = 0;
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Helper Functions                                                       │
// └─────────────────────────────────────────────────────────────────────────┘
std::string InspectorView::FormatSize(uint64_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unitIndex = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  std::ostringstream ss;
  if (unitIndex == 0) {
    ss << bytes << " " << units[unitIndex];
  } else {
    ss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
  }
  return ss.str();
}

const char *InspectorView::GetFileIcon(const std::string &name, bool isDir) {
  if (isDir) {
    return ICON_FA_FOLDER;
  }

  // Get extension
  std::filesystem::path p(name);
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Icon mapping
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".dds") {
    return ICON_FA_IMAGE;
  }
  if (ext == ".sfo" || ext == ".json" || ext == ".xml" || ext == ".txt") {
    return ICON_FA_FILE;
  }
  if (ext == ".pkg" || ext == ".zip" || ext == ".rar" || ext == ".7z") {
    return ICON_FA_BOX_ARCHIVE;
  }

  return ICON_FA_FILE;
}

bool InspectorView::MatchesFilterText(const std::string &text) const {
  if (strlen(searchFilter_) == 0)
    return true;

  std::string lowerName = text;
  std::string lowerFilter = searchFilter_;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                 ::tolower);
  std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(),
                 ::tolower);

  return lowerName.find(lowerFilter) != std::string::npos;
}

bool InspectorView::NodeMatchesFilter(const FileNode *node) const {
  if (!node)
    return false;
  if (strlen(searchFilter_) == 0)
    return true;
  if (MatchesFilterText(node->name) || MatchesFilterText(node->fullPath)) {
    return true;
  }
  if (node->isDirectory) {
    for (const auto &child : node->children) {
      if (NodeMatchesFilter(child.get())) {
        return true;
      }
    }
  }
  return false;
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Extraction Logic                                                       │
// └─────────────────────────────────────────────────────────────────────────┘
void InspectorView::ExtractNode(const FileNode *node) {
  if (!node)
    return;

  std::string destFolder = PickFolder();
  if (destFolder.empty())
    return;

  // Re-open PKG to extract
  PKG pkg;
  std::string failreason;

  // We use Scan with extract_root to set the base path for extraction.
  // This allows ExtractFiles(index) to reconstruct the full path correctly
  // under our destination folder.
  if (!pkg.Scan(pkgPathBuf_, failreason, destFolder)) {
    GuiLogSink::Instance().Error("Extraction failed (Scan): " + failreason);
    return;
  }

  // Recursive extraction helper
  std::function<void(const FileNode *)> extractRecursive =
      [&](const FileNode *n) {
        if (!n->isDirectory && n->fileIndex >= 0) {
          pkg.ExtractFiles(n->fileIndex);
          GuiLogSink::Instance().Info("Extracted: " + n->name);
        } else if (n->isDirectory) {
          for (const auto &child : n->children) {
            extractRecursive(child.get());
          }
        }
      };

  extractRecursive(node);
  GuiLogSink::Instance().Info("Extraction complete.");
}

std::string InspectorView::PickFolder() {
  // Simple macOS folder picker using osascript
  // In a real multi-platform app, use NFD or similar.
  const char *cmd = "osascript -e 'POSIX path of (choose folder with prompt "
                    "\"Select Output Directory\")'";
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    GuiLogSink::Instance().Error("Failed to open folder picker.");
    return "";
  }

  char buffer[1024];
  std::string result = "";
  if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result = buffer;
    // Remove newline
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
  }
  pclose(pipe);
  return result;
}

} // namespace ShadPKG::GUI
