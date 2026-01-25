// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  GUIContext: Bridge between ImGui frontend and PKG extraction backend    ║
// ║                                                                           ║
// ║  This module manages async operations to prevent GUI freezing during     ║
// ║  long-running PKG extractions. Uses Producer-Consumer pattern.           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  ExtractionJob: Encapsulates all parameters for a PKG extraction       │
// └─────────────────────────────────────────────────────────────────────────┘
struct ExtractionJob {
  std::string pkgPath;         // Source PKG file path
  std::string outPath;         // Output directory
  std::string rifPath;         // Optional RIF file path
  bool useRif = false;         // Enable RIF decryption
  bool createSubfolder = true; // Create TitleID subfolder
};

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  WorkerState: Thread-safe state shared between GUI and worker thread   │
// │                                                                          │
// │  GUI Thread reads: isBusy, progress, currentOperation, completed        │
// │  Worker Thread writes: all fields except stopRequested                  │
// │  GUI Thread writes: stopRequested (to cancel)                           │
// └─────────────────────────────────────────────────────────────────────────┘
struct WorkerState {
  std::atomic<bool> isBusy{false};
  std::atomic<float> progress{0.0f}; // 0.0 to 1.0
  std::atomic<bool> stopRequested{false};
  std::atomic<bool> completed{false};
  std::atomic<bool> success{false};

  // Protected by stateMutex
  std::mutex stateMutex;
  char currentOperation[256] = "Idle";
  std::string lastError;
  std::string extractedPath;

  void SetOperation(const std::string &op) {
    std::lock_guard<std::mutex> lock(stateMutex);
    strncpy(currentOperation, op.c_str(), sizeof(currentOperation) - 1);
    currentOperation[sizeof(currentOperation) - 1] = '\0';
  }

  std::string GetOperation() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(stateMutex));
    return std::string(currentOperation);
  }

  void SetError(const std::string &err) {
    std::lock_guard<std::mutex> lock(stateMutex);
    lastError = err;
  }

  std::string GetError() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(stateMutex));
    return lastError;
  }

  void Reset() {
    isBusy = false;
    progress = 0.0f;
    stopRequested = false;
    completed = false;
    success = false;
    SetOperation("Idle");
    SetError("");
    extractedPath.clear();
  }
};

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  GUIContext: Main application context managing views and state         │
// └─────────────────────────────────────────────────────────────────────────┘
class GUIContext {
public:
  enum class View { Extractor, Inspector, RIF, Settings };

  GUIContext() = default;
  ~GUIContext();

  // View management
  void SetCurrentView(View view) { currentView_ = view; }
  View GetCurrentView() const { return currentView_; }

  // Extraction control
  void StartExtraction(const ExtractionJob &job);
  void CancelExtraction();
  bool IsExtracting() const { return workerState_.isBusy; }
  float GetProgress() const { return workerState_.progress; }
  std::string GetCurrentOperation() const {
    return workerState_.GetOperation();
  }
  bool IsCompleted() const { return workerState_.completed; }
  bool WasSuccessful() const { return workerState_.success; }
  std::string GetLastError() const { return workerState_.GetError(); }

  WorkerState &GetWorkerState() { return workerState_; }

  // Loaded PKG info (for Inspector)
  std::string loadedPkgPath;
  bool pkgLoaded = false;

  // Update Checker
  struct UpdateStatus {
    bool hasUpdate = false;
    std::string latestVersion;
    std::string releaseUrl;
    bool checked = false;
  };

  void CheckForUpdates();
  const UpdateStatus &GetUpdateStatus() const { return updateStatus_; }

  // Contributors
  struct Contributor {
    std::string name;
    std::string url;
  };
  void FetchContributors();
  const std::vector<Contributor> &GetContributors() const {
    return contributors;
  }

private:
  View currentView_ = View::Extractor;
  WorkerState workerState_;
  std::thread workerThread_;
  UpdateStatus updateStatus_;
  std::vector<Contributor> contributors;

  void WorkerFunction(ExtractionJob job);
};

} // namespace ShadPKG::GUI
