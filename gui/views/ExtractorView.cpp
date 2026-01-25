// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../include/views/ExtractorView.h"
#include "../include/GuiLogSink.h"
#include "../include/StyleManager.h"
#include "common/assert.h"
#include "core/file_format/pkg.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif_generator.h"
#include <assert.h>
#ifndef IM_ASSERT
#define IM_ASSERT(_EXPR) ASSERT(_EXPR)
#endif
#include "imgui.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <commdlg.h>
#include <shlobj.h>
#include <windows.h>
#else
// macOS/Linux: Will use portable file dialog later
#endif

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  ExtractorView::Draw - Main render function                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void ExtractorView::Draw(GUIContext &ctx) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  DrawSourceSection(ctx);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawDecryptionSection();
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawOutputSection();
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  DrawExtractButton(ctx);

  if (ctx.IsExtracting() || ctx.IsCompleted()) {
    ImGui::Spacing();
    DrawProgressSection(ctx);
  }

  ImGui::PopStyleVar();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  SOURCE FILE Section                                                    │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::DrawSourceSection(GUIContext &ctx) {
  ImGui::TextColored(Colors::Primary, ICON_FA_FILE_ZIPPER "  SOURCE FILE");
  ImGui::Spacing();

  // PKG path input
  ImGui::SetNextItemWidth(-100);

  // Color border based on validation
  bool pathValid = ValidatePath(pkgPathBuf_);
  if (strlen(pkgPathBuf_) > 0) {
    ImGui::PushStyleColor(ImGuiCol_Border,
                          pathValid ? Colors::Success : Colors::Error);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
  }

  if (ImGui::InputTextWithHint("##pkgpath",
                               "Drag & drop or browse for PKG file...",
                               pkgPathBuf_, sizeof(pkgPathBuf_))) {
    ValidatePkgFile(&ctx);
  }

  if (strlen(pkgPathBuf_) > 0) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  if (ImGui::Button("Browse##pkg", ImVec2(90, 0))) {
    if (ShowOpenFileDialog("Select PKG File",
                           "PKG Files\0*.pkg\0All Files\0*.*\0", pkgPathBuf_,
                           sizeof(pkgPathBuf_))) {
      ValidatePkgFile(&ctx);
      AutoDetectRif();
    }
  }

  // Show PKG info if loaded
  if (pkgLoaded_ && pkgInfo_.valid) {
    ImGui::Indent();
    ImGui::TextColored(Colors::TextDim, "Title ID: ");
    ImGui::SameLine();
    ImGui::Text("%s", pkgInfo_.titleId.c_str());

    ImGui::TextColored(Colors::TextDim, "Content ID: ");
    ImGui::SameLine();
    ImGui::Text("%s", pkgInfo_.contentId.c_str());

    ImGui::TextColored(Colors::TextDim, "Size: ");
    ImGui::SameLine();

    // Format size nicely
    double sizeGB =
        static_cast<double>(pkgInfo_.pkgSize) / (1024.0 * 1024.0 * 1024.0);
    if (sizeGB > 1.0) {
      ImGui::Text("%.2f GB", sizeGB);
    } else {
      double sizeMB = static_cast<double>(pkgInfo_.pkgSize) / (1024.0 * 1024.0);
      ImGui::Text("%.2f MB", sizeMB);
    }

    ImGui::Unindent();
  }

  // Drag & drop hint
  ImGui::TextColored(Colors::TextDim,
                     ICON_FA_DOWNLOAD "  Drag & drop PKG file here");
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  DECRYPTION SETTINGS Section                                            │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::DrawDecryptionSection() {
  ImGui::TextColored(Colors::Primary, ICON_FA_KEY "  DECRYPTION SETTINGS");
  ImGui::Spacing();

  if (ImGui::Checkbox("Enable RIF Decryption", &useRif_)) {
    if (useRif_ && strlen(rifPathBuf_) > 0) {
      ValidateRifFile();
    }
  }

  if (useRif_) {
    ImGui::Indent();

    ImGui::SetNextItemWidth(-100);

    // Color border based on validation
    if (strlen(rifPathBuf_) > 0) {
      ImGui::PushStyleColor(ImGuiCol_Border,
                            rifValid_ ? Colors::Success : Colors::Error);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    }

    if (ImGui::InputTextWithHint("##rifpath", "Path to RIF file...",
                                 rifPathBuf_, sizeof(rifPathBuf_))) {
      ValidateRifFile();
    }

    if (strlen(rifPathBuf_) > 0) {
      ImGui::PopStyleVar();
      ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse##rif", ImVec2(90, 0))) {
      if (ShowOpenFileDialog("Select RIF File",
                             "RIF Files\0*.rif\0All Files\0*.*\0", rifPathBuf_,
                             sizeof(rifPathBuf_))) {
        ValidateRifFile();
      }
    }

    // RIF Status
    if (strlen(rifPathBuf_) > 0) {
      ImGui::Spacing();
      if (rifValid_) {
        ImGui::TextColored(Colors::Success, ICON_FA_CHECK " VALID");
        if (!rifContentId_.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(Colors::TextDim, " | Content-ID: %s",
                             rifContentId_.c_str());
        }
      } else {
        ImGui::TextColored(Colors::Error, ICON_FA_XMARK " INVALID");
      }
    }

    ImGui::Unindent();
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  OUTPUT DESTINATION Section                                             │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::DrawOutputSection() {
  ImGui::TextColored(Colors::Primary,
                     ICON_FA_FOLDER_OPEN "  OUTPUT DESTINATION");
  ImGui::Spacing();

  ImGui::SetNextItemWidth(-100);

  bool pathValid = ValidatePath(outPathBuf_) || strlen(outPathBuf_) == 0;
  if (strlen(outPathBuf_) > 0) {
    ImGui::PushStyleColor(ImGuiCol_Border,
                          pathValid ? Colors::Success : Colors::Warning);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
  }

  ImGui::InputTextWithHint("##outpath", "Output directory (optional)...",
                           outPathBuf_, sizeof(outPathBuf_));

  if (strlen(outPathBuf_) > 0) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  if (ImGui::Button("Browse##out", ImVec2(90, 0))) {
    ShowSelectFolderDialog("Select Output Directory", outPathBuf_,
                           sizeof(outPathBuf_));
  }

  ImGui::Checkbox("Create subfolder with TitleID", &createSubfolder_);
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  EXTRACT BUTTON                                                         │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::DrawExtractButton(GUIContext &ctx) {
  bool canExtract = pkgLoaded_ && pkgInfo_.valid && !ctx.IsExtracting();

  // Center the button
  float buttonWidth = 200.0f;
  float buttonHeight = 50.0f;
  float windowWidth = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX((windowWidth - buttonWidth) / 2.0f);

  // Style the main extract button
  ImGui::PushStyleColor(ImGuiCol_Button,
                        canExtract ? Colors::Primary : Colors::FrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        canExtract ? Colors::PrimaryHover : Colors::FrameBg);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        canExtract ? Colors::PrimaryActive : Colors::FrameBg);

  if (!canExtract) {
    ImGui::PushStyleColor(ImGuiCol_Text, Colors::TextDim);
  }

  if (ImGui::Button(ICON_FA_DOWNLOAD "  EXTRACT PKG",
                    ImVec2(buttonWidth, buttonHeight))) {
    if (canExtract) {
      ExtractionJob job;
      job.pkgPath = pkgPathBuf_;
      job.outPath = strlen(outPathBuf_) > 0 ? outPathBuf_ : ".";
      job.rifPath = rifPathBuf_;
      job.useRif = useRif_ && rifValid_;
      job.createSubfolder = createSubfolder_;

      GuiLogSink::Instance().Info("Starting extraction: " + job.pkgPath);
      ctx.StartExtraction(job);
    }
  }

  if (!canExtract) {
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleColor(3);

  // Show hint if can't extract
  if (!canExtract && !ctx.IsExtracting()) {
    ImGui::SetCursorPosX((windowWidth - 200.0f) / 2.0f);
    ImGui::TextColored(Colors::TextDim, "Select a valid PKG file to extract");
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PROGRESS Section                                                       │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::DrawProgressSection(GUIContext &ctx) {
  // Status text
  std::string status = ctx.GetCurrentOperation();
  ImGui::Text("Status: %s", status.c_str());

  // Progress bar
  float progress = ctx.GetProgress();

  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Colors::Primary);
  char progressText[32];
  snprintf(progressText, sizeof(progressText), "%.0f%%", progress * 100.0f);
  ImGui::ProgressBar(progress, ImVec2(-1, 25), progressText);
  ImGui::PopStyleColor();

  // Cancel button during extraction
  if (ctx.IsExtracting()) {
    ImGui::Spacing();
    float cancelWidth = 100.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((windowWidth - cancelWidth) / 2.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, Colors::Error);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button(ICON_FA_XMARK " Cancel", ImVec2(cancelWidth, 0))) {
      ctx.CancelExtraction();
      GuiLogSink::Instance().Warn("Extraction cancelled by user");
    }
    ImGui::PopStyleColor(2);
  }

  // Completion message
  if (ctx.IsCompleted() && !ctx.IsExtracting()) {
    ImGui::Spacing();
    if (ctx.WasSuccessful()) {
      ImGui::TextColored(Colors::Success,
                         ICON_FA_CHECK " Extraction completed successfully!");
    } else {
      ImGui::TextColored(Colors::Error, ICON_FA_XMARK " Extraction failed: %s",
                         ctx.GetLastError().c_str());
    }
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Drag & Drop Handler                                                    │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::OnFileDrop(GUIContext &ctx, const std::string &path) {
  // Check if it's a PKG file
  std::filesystem::path p(path);
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".pkg") {
    strncpy(pkgPathBuf_, path.c_str(), sizeof(pkgPathBuf_) - 1);
    pkgPathBuf_[sizeof(pkgPathBuf_) - 1] = '\0';
    ValidatePkgFile(&ctx);
    AutoDetectRif();
    GuiLogSink::Instance().Info("PKG file dropped: " + path);
  } else if (ext == ".rif") {
    strncpy(rifPathBuf_, path.c_str(), sizeof(rifPathBuf_) - 1);
    rifPathBuf_[sizeof(rifPathBuf_) - 1] = '\0';
    useRif_ = true;
    ValidateRifFile();
    GuiLogSink::Instance().Info("RIF file dropped: " + path);
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Validation Functions                                                   │
// └─────────────────────────────────────────────────────────────────────────┘
void ExtractorView::ValidatePkgFile(GUIContext *ctx) {
  pkgLoaded_ = false;
  pkgInfo_ = PkgInfo{};

  if (strlen(pkgPathBuf_) == 0) {
    return;
  }

  std::filesystem::path pkgPath(pkgPathBuf_);
  if (!std::filesystem::exists(pkgPath)) {
    return;
  }

  // Try to open and parse PKG
  PKG pkg;
  std::string failreason;
  if (!pkg.Open(pkgPath, failreason)) {
    GuiLogSink::Instance().Error("Failed to open PKG: " + failreason);
    return;
  }

  pkgInfo_.titleId = std::string(pkg.GetTitleID());
  pkgInfo_.pkgSize = pkg.GetPkgSize();
  pkgInfo_.valid = true;

  // Get content ID from SFO
  if (!pkg.sfo.empty()) {
    PSF psf;
    if (psf.Open(pkg.sfo)) {
      if (auto cid = psf.GetString("CONTENT_ID"); cid.has_value()) {
        pkgInfo_.contentId = std::string(*cid);
      }
      if (auto ver = psf.GetString("APP_VER"); ver.has_value()) {
        pkgInfo_.appVersion = std::string(*ver);
      }
    }
  }

  pkgLoaded_ = true;
  GuiLogSink::Instance().Info("PKG loaded: " + pkgInfo_.titleId + " (" +
                              pkgInfo_.contentId + ")");

  // Sync to shared context if provided
  if (ctx) {
    ctx->loadedPkgPath = pkgPath.string();
    ctx->pkgLoaded = true;
  }
}

void ExtractorView::ValidateRifFile() {
  rifValid_ = false;
  rifContentId_.clear();

  if (strlen(rifPathBuf_) == 0) {
    return;
  }

  std::filesystem::path rifPath(rifPathBuf_);
  rifValid_ = RIFGenerator::ValidateRIF(rifPath);

  // TODO: Extract content ID from RIF for display
}

bool ExtractorView::ValidatePath(const char *path) {
  if (strlen(path) == 0) {
    return false;
  }
  return std::filesystem::exists(path);
}

void ExtractorView::AutoDetectRif() {
  if (strlen(pkgPathBuf_) == 0 || !pkgInfo_.valid) {
    return;
  }

  // Look for RIF file in same directory as PKG
  std::filesystem::path pkgPath(pkgPathBuf_);
  std::filesystem::path pkgDir = pkgPath.parent_path();

  // Try common RIF naming patterns
  std::vector<std::string> patterns = {pkgInfo_.contentId + ".rif",
                                       pkgInfo_.titleId + ".rif",
                                       pkgPath.stem().string() + ".rif"};

  for (const auto &pattern : patterns) {
    std::filesystem::path rifPath = pkgDir / pattern;
    if (std::filesystem::exists(rifPath)) {
      strncpy(rifPathBuf_, rifPath.string().c_str(), sizeof(rifPathBuf_) - 1);
      rifPathBuf_[sizeof(rifPathBuf_) - 1] = '\0';
      useRif_ = true;
      ValidateRifFile();
      GuiLogSink::Instance().Info("Auto-detected RIF file: " +
                                  rifPath.string());
      return;
    }
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  File Dialog Functions (Platform-specific)                              │
// └─────────────────────────────────────────────────────────────────────────┘
#ifdef _WIN32
bool ExtractorView::ShowOpenFileDialog(const char *title, const char *filter,
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

bool ExtractorView::ShowSelectFolderDialog(const char *title, char *outPath,
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
// Helper to execute command and get output
static std::string ExecCmd(const char *cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    return "";
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  // Remove trailing newline
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool ExtractorView::ShowOpenFileDialog(const char *title, const char *filter,
                                       char *outPath, size_t pathSize) {
  // Use osascript to open native macOS file picker
  // Note: Filtering by extension in AppleScript is possible but simplified here

  std::string cmd = "osascript -e 'POSIX path of (choose file with prompt \"" +
                    std::string(title) + "\"";

  // Basic extension filtering hint
  if (strstr(filter, "*.pkg")) {
    cmd += " of type {\"pkg\"}";
  } else if (strstr(filter, "*.rif")) {
    cmd += " of type {\"rif\"}";
  }

  cmd += ")'";

  std::string result = ExecCmd(cmd.c_str());
  if (!result.empty() && result.find("User canceled") == std::string::npos) {
    strncpy(outPath, result.c_str(), pathSize - 1);
    outPath[pathSize - 1] = '\0';
    return true;
  }
  return false;
}

bool ExtractorView::ShowSelectFolderDialog(const char *title, char *outPath,
                                           size_t pathSize) {
  // Use osascript to open native macOS folder picker
  std::string cmd =
      "osascript -e 'POSIX path of (choose folder with prompt \"" +
      std::string(title) + "\")'";

  std::string result = ExecCmd(cmd.c_str());
  if (!result.empty() && result.find("User canceled") == std::string::npos) {
    strncpy(outPath, result.c_str(), pathSize - 1);
    outPath[pathSize - 1] = '\0';
    return true;
  }
  return false;
}
#endif

} // namespace ShadPKG::GUI
