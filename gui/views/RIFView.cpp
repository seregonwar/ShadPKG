// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../include/views/RIFView.h"
#include "../include/GuiLogSink.h"
#include "../include/StyleManager.h"
#include "core/file_format/rif_generator.h"
#include <assert.h>
#include "common/assert.h"
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "imgui.h"
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <commdlg.h>
#include <shlobj.h>
#include <windows.h>
#endif

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  RIFView::Draw - Main render function                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void RIFView::Draw(GUIContext &ctx) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  DrawGenerateSection();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawValidateSection();

  ImGui::PopStyleVar();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Generate RIF Section                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
void RIFView::DrawGenerateSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_KEY "  GENERATE RIF");
  ImGui::Spacing();

  ImGui::Text("Content ID:");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##contentid",
                           "EP0001-CUSA12345_00-GAMENAME00000001",
                           contentIdBuf_, sizeof(contentIdBuf_));

  ImGui::Spacing();

  ImGui::Text("Output Directory:");
  ImGui::SetNextItemWidth(-100);
  ImGui::InputTextWithHint("##rifoutdir", "Output directory (default: current)",
                           outputDirBuf_, sizeof(outputDirBuf_));
  ImGui::SameLine();
  if (ImGui::Button("Browse##rifout", ImVec2(90, 0))) {
    ShowSelectFolderDialog("Select Output Directory", outputDirBuf_,
                           sizeof(outputDirBuf_));
  }

  ImGui::Spacing();

  // Validate Content ID format
  bool validFormat = RIFGenerator::ValidateContentID(contentIdBuf_);
  bool canGenerate = strlen(contentIdBuf_) > 0 && validFormat;

  if (strlen(contentIdBuf_) > 0 && !validFormat) {
    ImGui::TextColored(
        Colors::Warning, ICON_FA_TRIANGLE_EXCLAMATION
        " Content ID format: XX0000-CUSA00000_00-XXXXXXXXXXXXXXXX");
  }

  // Generate button
  ImGui::PushStyleColor(ImGuiCol_Button,
                        canGenerate ? Colors::Primary : Colors::FrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        canGenerate ? Colors::PrimaryHover : Colors::FrameBg);

  if (!canGenerate) {
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::TextDim);
  }

  if (ImGui::Button(ICON_FA_KEY "  Generate RIF", ImVec2(150, 35))) {
    if (canGenerate) {
      GenerateRif();
    }
  }

  if (!canGenerate) {
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor(2);

  // Result display
  if (generateSuccess_) {
    ImGui::Spacing();
    ImGui::TextColored(Colors::Success,
                       ICON_FA_CHECK " RIF generated successfully!");
    ImGui::TextColored(Colors::TextDim, "Saved to: %s", generatedPath_.c_str());
  } else if (generateFailed_) {
    ImGui::Spacing();
    ImGui::TextColored(Colors::Error, ICON_FA_XMARK " Failed to generate RIF");
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Validate RIF Section                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
void RIFView::DrawValidateSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_CHECK "  VALIDATE RIF");
  ImGui::Spacing();

  ImGui::Text("RIF File:");
  ImGui::SetNextItemWidth(-100);

  bool pathValid =
      strlen(rifPathBuf_) > 0 && std::filesystem::exists(rifPathBuf_);
  if (strlen(rifPathBuf_) > 0) {
    ImGui::PushStyleColor(ImGuiCol_Border,
                          pathValid ? Colors::Success : Colors::Error);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
  }

  if (ImGui::InputTextWithHint("##rifvalidpath", "Path to RIF file...",
                               rifPathBuf_, sizeof(rifPathBuf_))) {
    validated_ = false; // Reset on change
  }

  if (strlen(rifPathBuf_) > 0) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  if (ImGui::Button("Browse##rifvalid", ImVec2(90, 0))) {
    if (ShowOpenFileDialog("Select RIF File",
                           "RIF Files\0*.rif\0All Files\0*.*\0", rifPathBuf_,
                           sizeof(rifPathBuf_))) {
      validated_ = false;
    }
  }

  ImGui::Spacing();

  // Validate button
  bool canValidate = strlen(rifPathBuf_) > 0 && pathValid;

  ImGui::PushStyleColor(ImGuiCol_Button,
                        canValidate ? Colors::Primary : Colors::FrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        canValidate ? Colors::PrimaryHover : Colors::FrameBg);

  if (!canValidate) {
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::TextDim);
  }

  if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS "  Validate", ImVec2(120, 35))) {
    if (canValidate) {
      ValidateRif();
    }
  }

  if (!canValidate) {
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor(2);

  // Result display
  if (validated_) {
    ImGui::Spacing();
    if (rifValid_) {
      ImGui::TextColored(Colors::Success, ICON_FA_CHECK " Valid RIF file");
      if (!rifInfo_.empty()) {
        ImGui::TextColored(Colors::TextDim, "%s", rifInfo_.c_str());
      }
    } else {
      ImGui::TextColored(Colors::Error, ICON_FA_XMARK " Invalid RIF file");
    }
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Actions                                                                │
// └─────────────────────────────────────────────────────────────────────────┘
void RIFView::GenerateRif() {
  generateSuccess_ = false;
  generateFailed_ = false;

  std::string contentId = contentIdBuf_;
  std::filesystem::path outputDir =
      strlen(outputDirBuf_) > 0 ? outputDirBuf_ : ".";

  RIFGenerator generator;
  if (generator.GenerateRIF(contentId, outputDir)) {
    generateSuccess_ = true;
    generatedPath_ = (outputDir / (contentId + ".rif")).string();
    GuiLogSink::Instance().Info("Generated RIF: " + generatedPath_);
  } else {
    generateFailed_ = true;
    GuiLogSink::Instance().Error("Failed to generate RIF for: " + contentId);
  }
}

void RIFView::ValidateRif() {
  validated_ = true;
  std::filesystem::path rifPath(rifPathBuf_);
  rifValid_ = RIFGenerator::ValidateRIF(rifPath);

  if (rifValid_) {
    // Get file size info
    auto size = std::filesystem::file_size(rifPath);
    rifInfo_ = "Size: " + std::to_string(size) + " bytes";
    GuiLogSink::Instance().Info("RIF validated: " + std::string(rifPathBuf_));
  } else {
    rifInfo_.clear();
    GuiLogSink::Instance().Warn("Invalid RIF: " + std::string(rifPathBuf_));
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  File Dialogs (Platform-specific)                                      │
// └─────────────────────────────────────────────────────────────────────────┘
#ifdef _WIN32
bool RIFView::ShowOpenFileDialog(const char *title, const char *filter,
                                 char *outPath, size_t pathSize) {
  OPENFILENAMEA ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter = filter;
  ofn.lpstrFile = outPath;
  ofn.nMaxFile = static_cast<DWORD>(pathSize);
  ofn.lpstrTitle = title;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  return GetOpenFileNameA(&ofn) != 0;
}

bool RIFView::ShowSelectFolderDialog(const char *title, char *outPath,
                                     size_t pathSize) {
  BROWSEINFOA bi = {};
  bi.lpszTitle = title;
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
  if (pidl) {
    SHGetPathFromIDListA(pidl, outPath);
    CoTaskMemFree(pidl);
    return true;
  }
  return false;
}
#else
bool RIFView::ShowOpenFileDialog(const char *title, const char *filter,
                                 char *outPath, size_t pathSize) {
  GuiLogSink::Instance().Warn(
      "File dialog not yet implemented for this platform");
  return false;
}

bool RIFView::ShowSelectFolderDialog(const char *title, char *outPath,
                                     size_t pathSize) {
  GuiLogSink::Instance().Warn(
      "Folder dialog not yet implemented for this platform");
  return false;
}
#endif

} // namespace ShadPKG::GUI
