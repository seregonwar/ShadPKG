// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#include "include/GUIContext.h"
#include "common/logging/log.h"
#include "common/version.h"
#include "core/file_format/pkg.h"
#include "core/file_format/psf.h"
#include <filesystem>

namespace ShadPKG::GUI {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  GUIContext Implementation                                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

GUIContext::~GUIContext() {
  CancelExtraction();
  if (workerThread_.joinable()) {
    workerThread_.join();
  }
}

void GUIContext::StartExtraction(const ExtractionJob &job) {
  if (workerState_.isBusy) {
    LOG_WARNING(Common, "Extraction already in progress");
    return;
  }

  // Reset state
  workerState_.Reset();
  workerState_.isBusy = true;

  // Detach old thread if any
  if (workerThread_.joinable()) {
    workerThread_.join();
  }

  // Launch worker thread
  workerThread_ = std::thread(&GUIContext::WorkerFunction, this, job);
}

void GUIContext::CancelExtraction() { workerState_.stopRequested = true; }

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Worker Thread: Runs PKG extraction in background                       │
// └─────────────────────────────────────────────────────────────────────────┘
void GUIContext::WorkerFunction(ExtractionJob job) {
  LOG_INFO(Common, "[GUI] Starting extraction: {} -> {}", job.pkgPath,
           job.outPath);

  try {
    std::filesystem::path pkgPath(job.pkgPath);
    std::filesystem::path outPath(job.outPath);

    // Validate paths
    if (!std::filesystem::exists(pkgPath)) {
      workerState_.SetError("PKG file does not exist: " + job.pkgPath);
      workerState_.success = false;
      workerState_.completed = true;
      workerState_.isBusy = false;
      return;
    }

    // Create output directory
    if (!std::filesystem::exists(outPath)) {
      std::filesystem::create_directories(outPath);
      LOG_INFO(Common, "[GUI] Created output directory: {}", outPath.string());
    }

    workerState_.SetOperation("Opening PKG...");
    workerState_.progress = 0.05f;

    // Open PKG
    PKG pkg;
    std::string failreason;
    if (!pkg.Open(pkgPath, failreason)) {
      workerState_.SetError("Failed to open PKG: " + failreason);
      workerState_.success = false;
      workerState_.completed = true;
      workerState_.isBusy = false;
      return;
    }

    // Check for cancellation
    if (workerState_.stopRequested) {
      workerState_.SetOperation("Cancelled");
      workerState_.success = false;
      workerState_.completed = true;
      workerState_.isBusy = false;
      return;
    }

    // Smart output path: append CONTENT_ID
    if (job.createSubfolder && !pkg.sfo.empty()) {
      PSF psf;
      if (psf.Open(pkg.sfo)) {
        if (auto cid = psf.GetString("CONTENT_ID"); cid.has_value()) {
          outPath /= std::string(*cid);
          if (!std::filesystem::exists(outPath)) {
            std::filesystem::create_directories(outPath);
          }
          LOG_INFO(Common, "[GUI] Using CONTENT_ID subfolder: {}",
                   outPath.string());
        }
      }
    }

    workerState_.SetOperation("Preparing extraction...");
    workerState_.progress = 0.1f;

    // Extract PKG structure
    if (!pkg.Extract(pkgPath, outPath, failreason)) {
      workerState_.SetError("Extraction failed: " + failreason);
      workerState_.success = false;
      workerState_.completed = true;
      workerState_.isBusy = false;
      return;
    }

    if (workerState_.stopRequested) {
      workerState_.SetOperation("Cancelled");
      workerState_.success = false;
      workerState_.completed = true;
      workerState_.isBusy = false;
      return;
    }

    workerState_.SetOperation("Extracting files...");
    workerState_.progress = 0.15f;

    // Extract files with progress (uses internal progress for now)
    // TODO: Use callback version when pkg.h is updated
    pkg.ExtractAllFilesWithProgress();

    workerState_.progress = 1.0f;
    workerState_.SetOperation("Complete!");
    workerState_.success = true;
    workerState_.completed = true;
    workerState_.isBusy = false;

    {
      std::lock_guard<std::mutex> lock(workerState_.stateMutex);
      workerState_.extractedPath = outPath.string();
    }

    LOG_INFO(Common, "[GUI] Extraction completed successfully: {}",
             outPath.string());

  } catch (const std::exception &e) {
    workerState_.SetError(std::string("Exception: ") + e.what());
    workerState_.success = false;
    workerState_.completed = true;
    workerState_.isBusy = false;
    LOG_ERROR(Common, "[GUI] Extraction exception: {}", e.what());
  }
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Update Checker Implementation                                          │
// └─────────────────────────────────────────────────────────────────────────┘
void GUIContext::CheckForUpdates() {
  if (updateStatus_.checked)
    return;

  // Run in detached thread to avoid blocking GUI
  std::thread([this]() {
    updateStatus_.checked = true;
    const std::string api_url =
        "https://api.github.com/repos/seregonwar/ShadPKG/releases/latest";
    std::string cmd = "curl -s -H \"User-Agent: ShadPKG\" " + api_url;

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
      return;

    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
      if (fgets(buffer, 128, pipe) != NULL)
        result += buffer;
    }
    pclose(pipe);

    // Simple JSON parsing to find "tag_name"
    // Format: "tag_name": "v1.2.3",
    size_t tagPos = result.find("\"tag_name\":");
    if (tagPos != std::string::npos) {
      size_t start =
          result.find("\"", tagPos + 11) + 1; // +11 for length of "tag_name":
      size_t end = result.find("\"", start);
      std::string version = result.substr(start, end - start);

      updateStatus_.latestVersion = version;
      updateStatus_.releaseUrl =
          "https://github.com/seregonwar/ShadPKG/releases/latest";

      // Compare versions (very basic string comparison for now)
      if (version != Common::VERSION &&
          version != ("v" + std::string(Common::VERSION))) {
        updateStatus_.hasUpdate = true;
        LOG_INFO(Common, "Update available: {} (Current: {})", version,
                 Common::VERSION);
      } else {
        LOG_INFO(Common, "ShadPKG is up to date.");
      }
    }
  }).detach();
}

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Contributors Fetcher Implementation                                    │
// └─────────────────────────────────────────────────────────────────────────┘
void GUIContext::FetchContributors() {
  if (!contributors.empty())
    return;

  // Run in detached thread
  std::thread([this]() {
    const std::string api_url =
        "https://api.github.com/repos/seregonwar/ShadPKG/contributors";
    std::string cmd = "curl -s -H \"User-Agent: ShadPKG\" " + api_url;

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
      return;

    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
      if (fgets(buffer, 128, pipe) != NULL)
        result += buffer;
    }
    pclose(pipe);

    // DEBUG: Log the raw result (truncated)
    if (!result.empty()) {
      LOG_INFO(Common, "GitHub API Response Size: {}", result.size());
      LOG_INFO(Common, "GitHub API Response (First 300 chars): {}",
               result.substr(0, 300));
    } else {
      LOG_ERROR(Common, "GitHub API returned empty result");
      return;
    }

    // Simple JSON parsing for array of objects
    std::vector<Contributor> tempContributors;
    size_t pos = 0;
    while ((pos = result.find("\"login\":", pos)) != std::string::npos) {
      // Find opening quote of value
      size_t quoteStart = result.find("\"", pos + 8);
      if (quoteStart == std::string::npos)
        break;

      size_t quoteEnd = result.find("\"", quoteStart + 1);
      if (quoteEnd == std::string::npos)
        break;

      std::string name =
          result.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

      size_t urlKeyPos = result.find("\"html_url\":", quoteEnd);
      if (urlKeyPos != std::string::npos) {
        size_t urlQuoteStart = result.find("\"", urlKeyPos + 11);
        if (urlQuoteStart == std::string::npos)
          break;

        size_t urlQuoteEnd = result.find("\"", urlQuoteStart + 1);
        if (urlQuoteEnd == std::string::npos)
          break;

        std::string url =
            result.substr(urlQuoteStart + 1, urlQuoteEnd - urlQuoteStart - 1);

        if (name.find("[bot]") == std::string::npos) {
          tempContributors.push_back({name, url});
          LOG_INFO(Common, "Parsed Contributor: Name='{}'", name);
        }
        pos = urlQuoteEnd;
      } else {
        pos = quoteEnd;
      }
    }

    if (!tempContributors.empty()) {
      contributors = tempContributors;
      LOG_INFO(Common, "Fetched {} contributors", contributors.size());
    } else {
      LOG_WARNING(Common, "No contributors parsed.");
    }
  }).detach();
}

} // namespace ShadPKG::GUI
