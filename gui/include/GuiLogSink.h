// SPDX-FileCopyrightText: Copyright 2025 shadPKG
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ctime>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║  GuiLogSink: Thread-safe log backend for ImGui console display            ║
// ║                                                                           ║
// ║  Captures LOG_INFO, LOG_WARNING, LOG_ERROR from Common::Log and stores    ║
// ║  them for real-time display in the ImGui console panel.                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

namespace ShadPKG::GUI {

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  LogLevel: Severity levels for log entries                              │
// │                                                                         │
// │  INFO  ─── General information (green)                                  │
// │  WARN  ─── Warnings (yellow/orange)                                     │
// │  ERROR ─── Errors (red)                                                 │
// └─────────────────────────────────────────────────────────────────────────┘
enum class LogLevel { Info = 0, Warning = 1, Error = 2 };

struct LogEntry {
  LogLevel level;
  std::string message;
  std::string timestamp;
  std::string category;
};

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  GuiLogSink: Singleton log collector                                    │
// └─────────────────────────────────────────────────────────────────────────┘
class GuiLogSink {
public:
  static GuiLogSink &Instance() {
    static GuiLogSink instance;
    return instance;
  }

  // Thread-safe log addition
  void Add(LogLevel level, const std::string &category,
           const std::string &msg) {
    std::lock_guard<std::mutex> lock(logMutex_);

    LogEntry entry;
    entry.level = level;
    entry.message = msg;
    entry.category = category;
    entry.timestamp = GetTimeString();

    logs_.push_back(entry);

    // Limit max entries to prevent memory issues
    if (logs_.size() > maxEntries_) {
      logs_.erase(logs_.begin());
    }
  }

  // Convenience methods
  void Info(const std::string &msg) { Add(LogLevel::Info, "GUI", msg); }
  void Warn(const std::string &msg) { Add(LogLevel::Warning, "GUI", msg); }
  void Error(const std::string &msg) { Add(LogLevel::Error, "GUI", msg); }

  // Get a copy of logs for rendering (thread-safe)
  std::vector<LogEntry> GetLogs() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return logs_;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(logMutex_);
    logs_.clear();
  }

  bool IsAutoScroll() const { return autoScroll_; }
  void SetAutoScroll(bool enabled) { autoScroll_ = enabled; }

  size_t GetMaxEntries() const { return maxEntries_; }
  void SetMaxEntries(size_t max) { maxEntries_ = max; }

private:
  GuiLogSink() = default;

  static std::string GetTimeString() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", t->tm_hour, t->tm_min,
             t->tm_sec);
    return std::string(buffer);
  }

  mutable std::mutex logMutex_;
  std::vector<LogEntry> logs_;
  bool autoScroll_ = true;
  size_t maxEntries_ = 1000;
};

} // namespace ShadPKG::GUI
