#pragma once

#include "../decompiler/DecompilerContext.h"
#include "common/backup_manager.h"
#include <memory>
#include <string>
#include <vector>

namespace ShadPKG {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                      PKG PROCESSING PIPELINE                              ║
// ║                                                                           ║
// ║  Automated pipeline that:                                                 ║
// ║  1. Loads PKG file                                                        ║
// ║  2. Decompiles to C++                                                     ║
// ║  3. Applies automatic corrections (loop fixes, etc.)                      ║
// ║  4. Exports corrected source files                                        ║
// ║  5. Backs up results to ps4MEL/backups/                                   ║
// ║                                                                           ║
// ║  Usage:                                                                   ║
// ║    PKGProcessor processor(ps4melPath);                                    ║
// ║    processor.processPKG("path/to/file.pkg");                              ║
// ║    auto results = processor.getResults();                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class PKGProcessor {
public:
  struct ProcessingResult {
    bool success = false;
    std::string pkgPath;
    std::string outputDirectory;
    std::vector<std::string> generatedFiles;
    std::vector<std::string> appliedCorrections;
    std::string backupVersion;
    std::string errorMessage;
  };

  PKGProcessor(const std::string &ps4melPath);

  // Process a single PKG file through the complete pipeline
  ProcessingResult processPKG(const std::string &pkgPath);

  // Process multiple PKG files
  std::vector<ProcessingResult> processPKGBatch(const std::vector<std::string> &pkgPaths);

  // Get processing result
  const ProcessingResult &getLastResult() const { return lastResult_; }

  // List all processed PKGs
  std::vector<std::string> getProcessedPKGs() const;

  // Enable/disable auto-backup
  void setAutoBackup(bool enabled) { autoBackup_ = enabled; }

  // Set output directory
  void setOutputDirectory(const std::string &path) { outputDir_ = path; }

private:
  std::string ps4melPath_;
  std::string outputDir_;
  PKGBackupManager backupManager_;
  ProcessingResult lastResult_;
  bool autoBackup_ = true;

  // Load PKG file and initialize decompiler context
  bool loadPKG(const std::string &pkgPath);

  // Run decompilation analysis
  bool decompile();

  // Generate corrected C++ code
  bool generateCode();

  // Export project files
  bool exportFiles();

  // Track and report corrections
  std::vector<std::string> getAppliedCorrections() const;
};

} // namespace ShadPKG
