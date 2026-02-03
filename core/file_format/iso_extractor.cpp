#include "iso_extractor.h"
#include "common/logging/log.h"
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace ShadPKG {

ISOExtractor::ISOExtractor() = default;

ISOExtractor::~ISOExtractor() = default;

bool ISOExtractor::isValidISO(const std::filesystem::path &isoPath) {
  if (!std::filesystem::exists(isoPath)) {
    return false;
  }

  std::ifstream file(isoPath, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  // Check for ISO 9660 signature at offset 0x8001
  file.seekg(0x8001);
  char sig[5] = {0};
  file.read(sig, 5);
  
  // Check for "CD001" (ISO 9660) or UDF signatures
  if (std::strncmp(sig, "CD001", 5) == 0) {
    return true;
  }

  // Check for UDF signature
  file.seekg(0x8000);
  file.read(sig, 5);
  if (std::strncmp(sig, "BEA01", 5) == 0) {
    return true;
  }

  return false;
}

std::string ISOExtractor::detectISOType(const std::filesystem::path &isoPath) {
  std::ifstream file(isoPath, std::ios::binary);
  if (!file.is_open()) {
    return "unknown";
  }

  // Check for PS3 SYSTEM.CNF marker
  file.seekg(0);
  std::vector<char> buffer(1024);
  file.read(buffer.data(), buffer.size());
  
  std::string content(buffer.begin(), buffer.end());
  if (content.find("BOOT2") != std::string::npos) {
    return "PS3";
  }
  if (content.find("BOOT") != std::string::npos) {
    return "PS2";
  }

  return "unknown";
}

ISOExtractor::ExtractionResult 
ISOExtractor::extract(const std::filesystem::path &isoPath,
                     const std::filesystem::path &outputDir) {
  ExtractionResult result;

  if (!isValidISO(isoPath)) {
    result.errorMessage = "Invalid ISO file";
    LOG_ERROR(Common, "Invalid ISO: {}", isoPath.string());
    return result;
  }

  std::filesystem::path mountPoint = outputDir / "iso_mount";
  std::filesystem::create_directories(mountPoint);

  // Mount ISO
  if (!mountISO(isoPath, mountPoint)) {
    result.errorMessage = "Failed to mount ISO";
    LOG_ERROR(Common, "Failed to mount ISO: {}", isoPath.string());
    return result;
  }

  // Extract metadata
  extractMetadata(mountPoint, result.gameTitle, result.gameID);

  // Find main executable
  std::filesystem::path exePath = findMainExecutable(mountPoint);
  if (exePath.empty()) {
    result.errorMessage = "Could not find main executable";
    LOG_ERROR(Common, "No executable found in ISO");
    unmountISO(mountPoint);
    return result;
  }

  // Copy executable to output directory BEFORE unmounting
  std::filesystem::path outputExe = outputDir / exePath.filename();
  try {
    LOG_INFO(Common, "Copying executable from {} to {}", exePath.string(), outputExe.string());
    std::filesystem::copy_file(exePath, outputExe,
                              std::filesystem::copy_options::overwrite_existing);
    result.extractedExecutable = outputExe;
    result.extractedFiles.push_back(outputExe);
    LOG_INFO(Common, "Successfully copied executable");
  } catch (const std::exception &e) {
    result.errorMessage = std::string("Failed to copy executable: ") + e.what();
    LOG_ERROR(Common, "{}", result.errorMessage);
    unmountISO(mountPoint);
    return result;
  }

  // Unmount ISO
  unmountISO(mountPoint);

  result.success = true;
  LOG_INFO(Common, "ISO extraction successful: {}", result.extractedExecutable.string());
  return result;
}

bool ISOExtractor::extractFile(const std::filesystem::path &isoPath,
                              const std::string &internalPath,
                              const std::filesystem::path &outputPath) {
  std::filesystem::path mountPoint = std::filesystem::temp_directory_path() / "iso_temp";
  std::filesystem::create_directories(mountPoint);

  if (!mountISO(isoPath, mountPoint)) {
    return false;
  }

  std::filesystem::path sourcePath = mountPoint / internalPath;
  bool success = false;

  if (std::filesystem::exists(sourcePath)) {
    try {
      std::filesystem::copy_file(sourcePath, outputPath,
                                std::filesystem::copy_options::overwrite_existing);
      success = true;
    } catch (const std::exception &e) {
      LOG_ERROR(Common, "Failed to copy file: {}", e.what());
    }
  }

  unmountISO(mountPoint);
  return success;
}

bool ISOExtractor::mountISO(const std::filesystem::path &isoPath,
                           std::filesystem::path &mountPoint) {
#ifdef __APPLE__
  // macOS: use hdiutil
  std::string cmd = "hdiutil attach -readonly \"" + isoPath.string() + 
                   "\" -mountpoint \"" + mountPoint.string() + "\" 2>/dev/null";
  int ret = std::system(cmd.c_str());
  return ret == 0;
#elif defined(__linux__)
  // Linux: use mount
  std::string cmd = "sudo mount -o loop,ro \"" + isoPath.string() + 
                   "\" \"" + mountPoint.string() + "\" 2>/dev/null";
  int ret = std::system(cmd.c_str());
  return ret == 0;
#else
  LOG_ERROR(Common, "ISO mounting not supported on this platform");
  return false;
#endif
}

bool ISOExtractor::unmountISO(const std::filesystem::path &mountPoint) {
#ifdef __APPLE__
  std::string cmd = "hdiutil detach \"" + mountPoint.string() + "\" 2>/dev/null";
  std::system(cmd.c_str());
  return true;
#elif defined(__linux__)
  std::string cmd = "sudo umount \"" + mountPoint.string() + "\" 2>/dev/null";
  std::system(cmd.c_str());
  return true;
#else
  return false;
#endif
}

std::filesystem::path ISOExtractor::findMainExecutable(
    const std::filesystem::path &mountPoint) {
  // PS3: Look for .elf files in BIN/ directory
  std::filesystem::path binDir = mountPoint / "BIN";
  if (std::filesystem::exists(binDir)) {
    for (const auto &entry : std::filesystem::directory_iterator(binDir)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        // PS3 executables are usually *.BIN or *.elf
        if (filename.find(".BIN") != std::string::npos ||
            filename.find(".elf") != std::string::npos ||
            filename.find(".ELF") != std::string::npos) {
          return entry.path();
        }
      }
    }
  }

  // PS2: Look for *.ELF in root or BIN/
  for (const auto &entry : std::filesystem::recursive_directory_iterator(mountPoint)) {
    if (entry.is_regular_file()) {
      std::string filename = entry.path().filename().string();
      if (filename.find(".ELF") != std::string::npos ||
          filename.find(".elf") != std::string::npos) {
        return entry.path();
      }
    }
  }

  return std::filesystem::path();
}

bool ISOExtractor::extractMetadata(const std::filesystem::path &mountPoint,
                                  std::string &gameTitle,
                                  std::string &gameID) {
  // Try to read SYSTEM.CNF for PS2/PS3
  std::filesystem::path systemCnf = mountPoint / "SYSTEM.CNF";
  if (std::filesystem::exists(systemCnf)) {
    std::ifstream file(systemCnf);
    std::string line;
    while (std::getline(file, line)) {
      if (line.find("TITLE") != std::string::npos) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
          gameTitle = line.substr(pos + 1);
          // Trim whitespace
          gameTitle.erase(0, gameTitle.find_first_not_of(" \t"));
          gameTitle.erase(gameTitle.find_last_not_of(" \t") + 1);
        }
      }
    }
  }

  // Try to extract game ID from filename or directory
  for (const auto &entry : std::filesystem::directory_iterator(mountPoint)) {
    std::string name = entry.path().filename().string();
    if (name.find("SLUS") != std::string::npos ||
        name.find("SLPM") != std::string::npos ||
        name.find("SCUS") != std::string::npos) {
      gameID = name;
      break;
    }
  }

  return !gameTitle.empty() || !gameID.empty();
}

} // namespace ShadPKG
