#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace ShadPKG {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                      ISO EXTRACTOR (PS2/PS3)                              ║
// ║                                                                           ║
// ║  Extracts executable and data files from PS2/PS3 ISO images.              ║
// ║  Supports both ISO 9660 and UDF filesystems.                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class ISOExtractor {
public:
  struct ExtractionResult {
    bool success = false;
    std::string errorMessage;
    std::filesystem::path extractedExecutable;
    std::vector<std::filesystem::path> extractedFiles;
    std::string gameTitle;
    std::string gameID;
  };

  ISOExtractor();
  ~ISOExtractor();

  // Detect ISO type (PS2, PS3, etc.)
  static bool isValidISO(const std::filesystem::path &isoPath);
  static std::string detectISOType(const std::filesystem::path &isoPath);

  // Extract files from ISO
  ExtractionResult extract(const std::filesystem::path &isoPath,
                          const std::filesystem::path &outputDir);

  // Extract specific file from ISO
  bool extractFile(const std::filesystem::path &isoPath,
                  const std::string &internalPath,
                  const std::filesystem::path &outputPath);

private:
  // Helper functions for ISO parsing
  bool mountISO(const std::filesystem::path &isoPath,
               std::filesystem::path &mountPoint);
  bool unmountISO(const std::filesystem::path &mountPoint);
  
  // Find main executable in ISO
  std::filesystem::path findMainExecutable(const std::filesystem::path &mountPoint);
  
  // Extract game metadata
  bool extractMetadata(const std::filesystem::path &mountPoint,
                      std::string &gameTitle, std::string &gameID);
};

} // namespace ShadPKG
