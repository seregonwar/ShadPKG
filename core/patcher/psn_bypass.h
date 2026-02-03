#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ShadPKG::Patcher {

// PSN Bypass Patch Types
enum class PatchType {
  NOP,        // Replace with NOP (0x90)
  RET_ZERO,   // Return 0 (xor eax, eax; ret)
  RET_ONE,    // Return 1 (mov eax, 1; ret)
  JMP_ALWAYS, // Change conditional jump to unconditional
  SKIP_CALL,  // Skip function call
  CUSTOM      // Custom byte replacement
};

struct PatchEntry {
  std::string name;
  std::string description;
  uint64_t offset;               // Offset in file
  std::vector<uint8_t> original; // Original bytes (for verification)
  std::vector<uint8_t> patched;  // Patched bytes
  PatchType type;
  bool applied = false;
};

struct GamePatchSet {
  std::string gameId;   // e.g., "CUSA00265"
  std::string gameName; // e.g., "Minecraft"
  std::string version;  // e.g., "01.00"
  std::vector<PatchEntry> patches;
};

class PSNBypass {
public:
  PSNBypass();
  ~PSNBypass() = default;

  // Load eboot.bin for patching
  bool loadEboot(const std::string &path);

  // Save patched eboot.bin
  bool saveEboot(const std::string &outputPath);

  // Auto-detect game and apply appropriate patches
  bool autoDetectAndPatch();

  // Apply specific patch set
  bool applyPatchSet(const GamePatchSet &patchSet);

  // Apply single patch
  bool applyPatch(const PatchEntry &patch);

  // Verify patch can be applied (check original bytes)
  bool verifyPatch(const PatchEntry &patch);

  // Search for PSN-related function signatures
  std::vector<uint64_t> findPSNFunctions();

  // Search for specific string references (LEA/pointers)
  std::vector<uint64_t> findStringReferences(uint64_t stringOffset);

  // Search for specific byte pattern
  std::vector<uint64_t> searchPattern(const std::vector<uint8_t> &pattern,
                                      const std::vector<uint8_t> &mask = {});

  // Get available patch sets
  static std::vector<GamePatchSet> getAvailablePatchSets();

  // Get Minecraft-specific patches
  static GamePatchSet getMinecraftPatches(const std::string &version);

  // Generate options.txt for Minecraft PSN bypass
  static std::string generateMinecraftOptions();

  // Patch a specific offset with custom bytes
  bool patchOffset(uint64_t offset, const std::vector<uint8_t> &bytes);

private:
  std::vector<uint8_t> ebootData_;
  std::string loadedPath_;
  bool isLoaded_ = false;

  // Known PSN function signatures (x86-64)
  static const std::vector<std::pair<std::string, std::vector<uint8_t>>>
      psnSignatures_;

  // Helper to create NOP sled
  static std::vector<uint8_t> createNopSled(size_t size);

  // Helper to create return 0 stub
  static std::vector<uint8_t> createReturnZero();

  // Helper to create return 1 stub
  static std::vector<uint8_t> createReturnOne();
};

} // namespace ShadPKG::Patcher
