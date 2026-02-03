#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ShadPKG::Patcher {

// Standard ELF64 Constants and Structures
// We redefine them here to be independent of SymbolAnalysis private structs

struct Elf64_Ehdr {
  unsigned char e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};

struct Elf64_Phdr {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
};

// PS4 SELF Header structures (Reverse engineered)
struct SelfHeader {
  uint32_t magic; // 0x1D3D154F
  uint32_t version;
  uint16_t mode;
  uint16_t endian;
  uint32_t attr;
  uint32_t header_size;
  uint64_t metadata_offset;
  uint64_t header_len;
  uint64_t file_len;
  uint16_t segment_count;
  uint16_t section_count;
  // ... more fields if needed
};

struct SelfSegment {
  uint64_t flags;
  uint64_t offset;
  uint64_t encrypted_file_size;
  uint64_t file_size;
  uint64_t memory_size;
  uint64_t vaddr;
  uint64_t align;
};

class ElfReconstructor {
public:
  ElfReconstructor();

  // Load file data (ELF, SELF, or FSELF)
  bool loadFile(const std::string &path);
  bool loadData(const std::vector<uint8_t> &data);

  // Check likely format
  bool isSelf() const;
  bool isElf() const;

  // Convert loaded SELF to ELF64
  // If output is provided, saves to file. Otherwise keeps in memory.
  bool reconstructElf(const std::string &outputPath = "");

  // Get the resulting ELF data
  const std::vector<uint8_t> &getElfData() const { return elfData_; }

private:
  std::vector<uint8_t> rawData_;
  std::vector<uint8_t> elfData_;
  bool isLoaded_ = false;

  // Helper functions
  bool parseSelf();
};

} // namespace ShadPKG::Patcher
