#pragma once

#include "../ir/IR.h"
#include "SymbolDatabase.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           SYMBOL ANALYSIS                                 ║
// ║                                                                           ║
// ║  Parses ELF structures to resolve function names and data literals.       ║
// ║  - Dynamic Symbols (.dynsym)                                              ║
// ║  - PLT Relocations (.rela.plt) -> External calls                          ║
// ║  - String Literals (.rodata)                                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class SymbolAnalysis {
public:
  explicit SymbolAnalysis(const std::vector<uint8_t> &rawData,
                          uint64_t baseAddress,
                          std::shared_ptr<SymbolDatabase> db);

  void analyze();

  // ═══════════════════════════════════════════════════════════════════════
  //  Queries
  // ═══════════════════════════════════════════════════════════════════════

  // Get function name for a given address (internal or PLT)
  // Returns std::nullopt if not found
  std::optional<std::string> getFunctionName(uint64_t address) const;

  // Check if an address points to a C-string in .rodata
  std::optional<std::string> getStringLiteral(uint64_t address) const;

  // Get full symbol info
  std::optional<SymbolInfo> getSymbol(uint64_t address) const;

  // Check if address is a known PLT stub
  bool isPLTStub(uint64_t address) const;

private:
  const std::vector<uint8_t> &data_;
  uint64_t baseAddress_;
  std::shared_ptr<SymbolDatabase> db_;

  // Address -> Name
  std::map<uint64_t, std::string> symbols_;

  // PLT Address -> Target Name (e.g. 0x400560 -> "malloc")
  std::map<uint64_t, std::string> pltEntries_;

  // Read-only data sections
  struct RodataSection {
      uint64_t va;
      uint64_t size;
      uint64_t fileOffset;
  };
  std::vector<RodataSection> rodataSections_;

  // Helpers
  void parseELF();
  void parseSections(uint64_t shoff, uint16_t shnum, uint16_t shstrndx);
  void parseSymbols(uint64_t symOffset, uint64_t strOffset, uint64_t size,
                    uint64_t entSize);
  void parseRelocations(uint64_t relOffset, uint64_t size, uint64_t entSize,
                        uint64_t dynStrOffset, uint64_t dynSymOffset,
                        bool isRela);

  std::string readString(uint64_t offset) const;

  // Minimal ELF structures
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

  struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
  };

  struct Elf64_Sym {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
  };

  struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
  };
};

} // namespace ShadPKG::Decompiler::Analysis
