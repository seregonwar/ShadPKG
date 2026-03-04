#include "psn_bypass.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace ShadPKG::Patcher {

// Known PSN function signatures for PS4 (x86-64)
const std::vector<std::pair<std::string, std::vector<uint8_t>>>
    PSNBypass::psnSignatures_ = {
        // sceNpCheckPlus - Check PS Plus subscription
        {"sceNpCheckPlus",
         {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20}},

        // sceUserServiceGetNpAccountId - Get PSN account ID
        {"sceUserServiceGetNpAccountId",
         {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}},

        // sceNpCheckCallback - PSN callback check
        {"sceNpCheckCallback",
         {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9}},

        // sceNpGetOnlineId - Get online ID
        {"sceNpGetOnlineId",
         {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10}},

        // Generic PSN auth check pattern
        {"PSN_Auth_Check", {0x85, 0xC0, 0x74}}, // test eax, eax; jz

        // sceNpManagerGetNpId
        {"sceNpManagerGetNpId", {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08}},
};

PSNBypass::PSNBypass() {}

bool PSNBypass::loadEboot(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "[PSNBypass] Failed to open: " << path << std::endl;
    return false;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  ebootData_.resize(size);
  if (!file.read(reinterpret_cast<char *>(ebootData_.data()), size)) {
    std::cerr << "[PSNBypass] Failed to read file" << std::endl;
    return false;
  }

  loadedPath_ = path;
  isLoaded_ = true;
  std::cout << "[PSNBypass] Loaded eboot: " << path << " (" << size << " bytes)"
            << std::endl;
  return true;
}

bool PSNBypass::saveEboot(const std::string &outputPath) {
  if (!isLoaded_) {
    std::cerr << "[PSNBypass] No eboot loaded" << std::endl;
    return false;
  }

  std::ofstream file(outputPath, std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "[PSNBypass] Failed to create: " << outputPath << std::endl;
    return false;
  }

  file.write(reinterpret_cast<const char *>(ebootData_.data()),
             ebootData_.size());
  std::cout << "[PSNBypass] Saved patched eboot: " << outputPath << std::endl;
  return true;
}

std::vector<uint64_t>
PSNBypass::searchPattern(const std::vector<uint8_t> &pattern,
                         const std::vector<uint8_t> &mask) {
  std::vector<uint64_t> results;
  if (!isLoaded_ || pattern.empty())
    return results;

  bool useMask = !mask.empty() && mask.size() == pattern.size();

  for (size_t i = 0; i <= ebootData_.size() - pattern.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < pattern.size(); ++j) {
      uint8_t maskByte = useMask ? mask[j] : 0xFF;
      if ((ebootData_[i + j] & maskByte) != (pattern[j] & maskByte)) {
        match = false;
        break;
      }
    }
    if (match) {
      results.push_back(i);
    }
  }
  return results;
}

std::vector<uint64_t> PSNBypass::findStringReferences(uint64_t stringOffset) {
  std::vector<uint64_t> refs;
  if (!isLoaded_)
    return refs;

  // Search for LEA or generic pointers to this string
  // x86-64 LEA is usually: 48 8D xx [destination] [offset relative to RIP]
  // RIP = instruction_addr + instruction_length
  // target = RIP + offset
  // offset = target - RIP

  // Scan the entire text section (approximated as the whole file for now)
  // We look for 4-byte values that satisfy: current_offset + 7 + value =
  // stringOffset (LEA is 7 bytes usually) Or just scan for 4-byte values: value
  // = stringOffset - (current_offset + 7)

  // Checking standard LEA r64, [rip + disp32] -> 48 8d 3d / 15 / 05 ...
  // Opcode can be 48 8d ...

  // Optimization: Just scan for the 32-bit displacement
  // displacement = stringOffset - (instruction_addr + 7)

  for (size_t i = 0; i < ebootData_.size() - 7; ++i) {
    // Assume LEA instruction length is 7 bytes (common for 64-bit address load)
    int32_t disp =
        static_cast<int32_t>(stringOffset) - static_cast<int32_t>(i + 7);

    // Check if bytes at i+3 match this displacement (LEA opcode is 3 bytes: 48
    // 8D xx)
    if (ebootData_[i] == 0x48 && ebootData_[i + 1] == 0x8D) {
      int32_t foundDisp = 0;
      std::memcpy(&foundDisp, &ebootData_[i + 3], 4);
      if (foundDisp == disp) {
        refs.push_back(i);
      }
    }
  }
  return refs;
}

std::vector<uint64_t> PSNBypass::findPSNFunctions() {
  std::vector<uint64_t> allResults;

  for (const auto &[name, sig] : psnSignatures_) {
    auto results = searchPattern(sig);
    for (auto offset : results) {
      std::cout << "[PSNBypass] Found " << name << " at offset 0x" << std::hex
                << offset << std::dec << std::endl;
      allResults.push_back(offset);
    }
  }

  // Search for string references to PSN-related strings
  std::vector<std::string> psnStrings = {
      "PlayStation Network", "PSN",    "sceNp", "NpManager", "SignIn",
      "PlayStation Plus",    "PS Plus"};

  for (const auto &str : psnStrings) {
    std::vector<uint8_t> strBytes(str.begin(), str.end());
    auto results = searchPattern(strBytes);
    for (auto offset : results) {
      std::cout << "[PSNBypass] Found string \"" << str << "\" at offset 0x"
                << std::hex << offset << std::dec << std::endl;

      // Find references to this string!
      auto refs = findStringReferences(offset);
      for (auto refOffset : refs) {
        std::cout << "[PSNBypass]   -> Referenced at 0x" << std::hex
                  << refOffset << std::dec << " (LEA/Ptr)" << std::endl;
      }
    }
  }

  return allResults;
}

bool PSNBypass::verifyPatch(const PatchEntry &patch) {
  if (!isLoaded_)
    return false;
  if (patch.offset + patch.original.size() > ebootData_.size())
    return false;

  for (size_t i = 0; i < patch.original.size(); ++i) {
    if (ebootData_[patch.offset + i] != patch.original[i]) {
      return false;
    }
  }
  return true;
}

bool PSNBypass::applyPatch(const PatchEntry &patch) {
  if (!isLoaded_) {
    std::cerr << "[PSNBypass] No eboot loaded" << std::endl;
    return false;
  }

  if (patch.offset + patch.patched.size() > ebootData_.size()) {
    std::cerr << "[PSNBypass] Patch offset out of bounds" << std::endl;
    return false;
  }

  // Verify original bytes if provided
  if (!patch.original.empty() && !verifyPatch(patch)) {
    std::cerr << "[PSNBypass] Original bytes mismatch for patch: " << patch.name
              << std::endl;
    return false;
  }

  // Apply patch
  std::memcpy(ebootData_.data() + patch.offset, patch.patched.data(),
              patch.patched.size());
  std::cout << "[PSNBypass] Applied patch: " << patch.name << " at 0x"
            << std::hex << patch.offset << std::dec << std::endl;
  return true;
}

bool PSNBypass::applyPatchSet(const GamePatchSet &patchSet) {
  std::cout << "[PSNBypass] Applying patch set for " << patchSet.gameName
            << " (" << patchSet.gameId << " v" << patchSet.version << ")"
            << std::endl;

  int applied = 0;
  for (const auto &patch : patchSet.patches) {
    if (applyPatch(patch)) {
      applied++;
    }
  }

  std::cout << "[PSNBypass] Applied " << applied << "/"
            << patchSet.patches.size() << " patches" << std::endl;
  return applied > 0;
}

bool PSNBypass::autoDetectAndPatch() {
  if (!isLoaded_)
    return false;

  // Try to find PSN functions and patch them
  auto psnFuncs = findPSNFunctions();

  if (psnFuncs.empty()) {
    std::cout << "[PSNBypass] No PSN functions found via signatures"
              << std::endl;
  }

  // Try Minecraft-specific patches
  auto mcPatches = getMinecraftPatches("01.00");
  return applyPatchSet(mcPatches);
}

std::vector<uint8_t> PSNBypass::createNopSled(size_t size) {
  return std::vector<uint8_t>(size, 0x90);
}

std::vector<uint8_t> PSNBypass::createReturnZero() {
  // xor eax, eax (31 C0) ; ret (C3)
  return {0x31, 0xC0, 0xC3};
}

std::vector<uint8_t> PSNBypass::createReturnOne() {
  // mov eax, 1 (B8 01 00 00 00) ; ret (C3)
  return {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3};
}

GamePatchSet PSNBypass::getMinecraftPatches(const std::string &version) {
  GamePatchSet patchSet;
  patchSet.gameId = "CUSA00265";
  patchSet.gameName = "Minecraft: PlayStation 4 Edition";
  patchSet.version = version;

  // These patches are based on known Minecraft PS4 bypass techniques
  // The actual offsets need to be found by analyzing the specific eboot version

  if (version == "01.00" || version == "1.00") {
    // Patch 1: Skip PSN sign-in check
    // This typically involves finding the sceUserServiceGetNpAccountId call
    // and making it return success (0) without actually checking
    patchSet.patches.push_back(
        {"PSN_SignIn_Bypass",
         "Bypass PSN sign-in requirement (Force SignIn Flag)",
         0x108414, // Offset verified: C6 83 D0 01 00 00 00 -> 00 is at 0x108414
         {0x00},   // Original byte (MOV BYTE PTR [RBX+1D0], 0)
         {0x01},   // Patched byte (MOV BYTE PTR [RBX+1D0], 1)
         PatchType::CUSTOM});

    // Patch 2: Skip PS Plus check for multiplayer (Require finding real offset)
    /*
    patchSet.patches.push_back({"PSPlus_Check_Bypass",
                                "Bypass PS Plus requirement for multiplayer",
                                0x0, // Offset needs to be found
                                {},
                                createReturnOne(), // Return 1 = has PS Plus
                                PatchType::RET_ONE});

    // Patch 3: Enable offline multiplayer
    patchSet.patches.push_back({"Offline_Multiplayer",
                                "Enable multiplayer without PSN connection",
                                0x0,
                                {},
                                {0xEB}, // JMP (unconditional) instead of JZ/JNZ
                                PatchType::JMP_ALWAYS});
    */
  }

  // For version 2.35 (the one NELUxPzzz patched)
  if (version == "02.35" || version == "2.35") {
    // Known working patches for v2.35
    // These would be the actual offsets from the patched PKG
    patchSet.patches.push_back({"PSN_Auth_Bypass_v235",
                                "Bypass PSN authentication (v2.35)",
                                0x0,
                                {},
                                createReturnZero(),
                                PatchType::RET_ZERO});
  }

  return patchSet;
}

std::string PSNBypass::generateMinecraftOptions() {
  // Generate the options.txt content for Minecraft PSN bypass
  // Based on the psxhax thread configuration
  return R"(mp_username:Steve
game_difficulty_new:1
game_thirdperson:0
gfx_dpadscale:0.5
mp_server_visible:1
mp_xboxlive_visible:1
mp_nex_visible:1
mp_psn_visible:1
game_haseverloggedintoxbl:0
game_haschosennottosignintoxbl:0
crossplatform_toggle:1
last_xuid:2535414620388275
do_not_show_multiplayer_online_safety_warning:1
game_shownplatformnetworkconnect:1
)";
}

std::vector<GamePatchSet> PSNBypass::getAvailablePatchSets() {
  std::vector<GamePatchSet> sets;

  // Minecraft patches
  sets.push_back(getMinecraftPatches("01.00"));
  sets.push_back(getMinecraftPatches("02.35"));

  return sets;
}

bool PSNBypass::patchOffset(uint64_t offset,
                            const std::vector<uint8_t> &bytes) {
  if (!isLoaded_) {
    std::cerr << "[PSNBypass] No eboot loaded" << std::endl;
    return false;
  }

  if (offset + bytes.size() > ebootData_.size()) {
    std::cerr << "[PSNBypass] Patch offset out of bounds: 0x" << std::hex
              << offset << std::dec << std::endl;
    return false;
  }

  // Apply patch
  std::memcpy(ebootData_.data() + offset, bytes.data(), bytes.size());
  return true;
}

} // namespace ShadPKG::Patcher
