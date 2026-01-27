#include "SymbolAnalysis.h"
#include "SymbolDatabase.h"
#include <cstring>
#include <iostream>

namespace ShadPKG::Decompiler::Analysis {

SymbolAnalysis::SymbolAnalysis(const std::vector<uint8_t> &rawData,
                               uint64_t baseAddress,
                               std::shared_ptr<SymbolDatabase> db)
    : data_(rawData), baseAddress_(baseAddress), db_(db) {}

void SymbolAnalysis::analyze() {
  if (data_.size() < sizeof(Elf64_Ehdr))
    return;

  // Check magic
  if (data_[0] != 0x7F || data_[1] != 'E' || data_[2] != 'L' || data_[3] != 'F')
    return;

  parseELF();
}

std::optional<std::string>
SymbolAnalysis::getFunctionName(uint64_t address) const {
  if (db_) {
    auto name = db_->getSymbolName(address);
    if (!name.empty() && name.find("loc_") == std::string::npos) {
      return name;
    }
    // If db returned a default loc_ name, check if we want to return nullopt
    // to indicate "no symbol found" vs "default name".
    // For call targets, returning a name is fine.
    // But preserving specific nullopt semantic for "unknown" might be safer?
    // Let's rely on DB's getSymbol returning optional.
    auto sym = db_->getSymbol(address);
    if (sym)
      return sym->name;
  }
  return std::nullopt;
}

std::optional<std::string>
SymbolAnalysis::getStringLiteral(uint64_t address) const {
  // Check if address falls into .rodata
  for (const auto &section : rodataSections_) {
    if (address >= section.va && address < section.va + section.size) {
      uint64_t offsetInSec = address - section.va;
      uint64_t fileOffset = section.fileOffset + offsetInSec;

      if (fileOffset >= data_.size())
        return std::nullopt;

      // Scan for null terminator
      std::string str;
      size_t maxLen = 256;
      for (size_t i = 0; i < maxLen; ++i) {
        if (fileOffset + i >= data_.size())
          break;
        char c = static_cast<char>(data_[fileOffset + i]);
        if (c == 0)
          break;
        // Relaxed check: printable ASCII or common whitespace
        if ((c < 32 || c >= 127) && c != '\n' && c != '\t' && c != '\r')
          return std::nullopt; // Not a clean string
        str += c;
      }
      if (str.length() >= 3) // Min length 3 to avoid noise
        return str;
    }
  }
  return std::nullopt;
}

std::optional<SymbolInfo>
SymbolAnalysis::getSymbol(uint64_t address) const {
  if (db_) return db_->getSymbol(address);
  return std::nullopt;
}

bool SymbolAnalysis::isPLTStub(uint64_t address) const {
  return pltEntries_.find(address) != pltEntries_.end();
}

// ═══════════════════════════════════════════════════════════════════════════
//  ELF Parsing Logic
// ═══════════════════════════════════════════════════════════════════════════

void SymbolAnalysis::parseELF() {
  const auto *ehdr = reinterpret_cast<const Elf64_Ehdr *>(data_.data());

  if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0)
    return;

  parseSections(ehdr->e_shoff, ehdr->e_shnum, ehdr->e_shstrndx);
}

void SymbolAnalysis::parseSections(uint64_t shoff, uint16_t shnum,
                                   uint16_t shstrndx) {
  if (shoff + shnum * sizeof(Elf64_Shdr) > data_.size())
    return;

  const auto *shdrs =
      reinterpret_cast<const Elf64_Shdr *>(data_.data() + shoff);

  // Get Section Header String Table
  if (shstrndx >= shnum)
    return;
  const auto &strSec = shdrs[shstrndx];
  if (strSec.sh_offset + strSec.sh_size > data_.size())
    return;
  const char *strTab =
      reinterpret_cast<const char *>(data_.data() + strSec.sh_offset);

  // Scan sections
  uint64_t dynSymOffset = 0, dynSymSize = 0, dynSymEnt = 0;
  uint64_t dynStrOffset = 0;
  uint64_t relaPltOffset = 0, relaPltSize = 0, relaPltEnt = 0;
  uint64_t pltAddr = 0, pltSize = 0;

  for (int i = 0; i < shnum; ++i) {
    const auto &sh = shdrs[i];
    const char *name = strTab + sh.sh_name;

    // .dynsym
    if (sh.sh_type == 11 /* SHT_DYNSYM */) { // or via name
      dynSymOffset = sh.sh_offset;
      dynSymSize = sh.sh_size;
      dynSymEnt = sh.sh_entsize;
    }
    // .dynstr
    else if (sh.sh_type == 3 /* SHT_STRTAB */ &&
             std::strcmp(name, ".dynstr") == 0) {
      dynStrOffset = sh.sh_offset;
    }
    // .rela.plt
    else if (sh.sh_type == 4 /* SHT_RELA */ &&
             std::strcmp(name, ".rela.plt") == 0) {
      relaPltOffset = sh.sh_offset;
      relaPltSize = sh.sh_size;
      relaPltEnt = sh.sh_entsize;
    }
    // .plt
    else if (std::strcmp(name, ".plt") == 0) {
      pltAddr = sh.sh_addr;
      pltSize = sh.sh_size;
    }
    // .rodata or .data
    else if (std::strcmp(name, ".rodata") == 0 || std::strcmp(name, ".data") == 0) {
      rodataSections_.push_back({sh.sh_addr, sh.sh_size, sh.sh_offset});
    }
  }

  // Parse Symbols if found
  if (dynSymOffset && dynStrOffset) {
    parseSymbols(dynSymOffset, dynStrOffset, dynSymSize, dynSymEnt);
  }

  // Parse Relocations to identify PLT calls
  if (relaPltOffset && dynSymOffset && dynStrOffset && pltAddr) {
    parseRelocations(relaPltOffset, relaPltSize, relaPltEnt, dynStrOffset,
                     dynSymOffset, true);
  }
}

void SymbolAnalysis::parseSymbols(uint64_t symOffset, uint64_t strOffset,
                                  uint64_t size, uint64_t entSize) {
  if (entSize < sizeof(Elf64_Sym))
    return;

  size_t count = size / entSize;
  const uint8_t *ptr = data_.data() + symOffset;

  for (size_t i = 0; i < count; ++i) {
    const auto *sym = reinterpret_cast<const Elf64_Sym *>(ptr + i * entSize);
    uint8_t type = (sym->st_info & 0xf);

    if (sym->st_value != 0 && (type == 2 /* STT_FUNC */ || type == 1 /* STT_OBJECT */)) {
      std::string name = readString(strOffset + sym->st_name);
      if (!name.empty()) {
        if (db_) {
          SymbolType stype = (type == 2) ? SymbolType::Function : SymbolType::GlobalVariable;
          db_->addSymbol(sym->st_value, name, stype, SymbolSource::Auto);
        }
      }
    }
  }
}

void SymbolAnalysis::parseRelocations(uint64_t relOffset, uint64_t size,
                                      uint64_t entSize, uint64_t dynStrOffset,
                                      uint64_t dynSymOffset, bool isRela) {
  if (entSize < sizeof(Elf64_Rela))
    return;

  size_t count = size / entSize;
  const uint8_t *ptr = data_.data() + relOffset;
  const auto *syms =
      reinterpret_cast<const Elf64_Sym *>(data_.data() + dynSymOffset);

  // We attempt to map PLT Stubs to Symbols.
  // Assuming PLT starts at .plt address, and each entry is 16 bytes.
  // PLT[0] is special. PLT[1] matches rela[0].

  // Find .plt address again? Passed via member or scan again?
  // Let's assume we map GOT address (r_offset) to name?
  // Use heuristic: We need to know who calls 'malloc'.
  // The code calls 0x400560 (PLTStub). PLTStub jumps to *GOT_Entry.
  // We want 0x400560 -> "malloc".

  // We don't easily know the PLT Stub address for a given relocation without
  // parsing the PLT section instructions or assuming the 16-byte fixed layout.
  // Let's try locating the .plt section header again.

  uint64_t pltAddr = 0;
  // (Re-scan sections for .plt sh_addr - inefficient but ok for now)
  // Actually we iterate sections in parseSections.
  // We need to store it or accept a small hack.
  const auto *ehdr = reinterpret_cast<const Elf64_Ehdr *>(data_.data());
  const auto *shdrs =
      reinterpret_cast<const Elf64_Shdr *>(data_.data() + ehdr->e_shoff);
  const char *strTab = reinterpret_cast<const char *>(
      data_.data() + shdrs[ehdr->e_shstrndx].sh_offset);

  for (int i = 0; i < ehdr->e_shnum; ++i) {
    if (std::strcmp(strTab + shdrs[i].sh_name, ".plt") == 0) {
      pltAddr = shdrs[i].sh_addr;
      break;
    }
  }

  if (pltAddr == 0)
    return;

  // Process Relocations
  for (size_t i = 0; i < count; ++i) {
    const auto *rela = reinterpret_cast<const Elf64_Rela *>(ptr + i * entSize);
    uint32_t symIdx = (rela->r_info >> 32);
    // uint32_t type = (rela->r_info & 0xffffffff);

    // Get Symbol Name
    // (Bounds check ignored for brevity)
    const auto &sym = syms[symIdx];
    std::string name = readString(dynStrOffset + sym.st_name);

    if (!name.empty()) {
      // Assume PLT stub index corresponds to relocation index
      // PLT entries usually 16 bytes. First is PLT header (0).
      uint64_t stubAddr = pltAddr + (i + 1) * 16;

      if (db_)
        db_->addSymbol(stubAddr, name, SymbolType::Function,
                       SymbolSource::Auto);
    }
  }
}

std::string SymbolAnalysis::readString(uint64_t offset) const {
  if (offset >= data_.size())
    return "";
  return std::string(reinterpret_cast<const char *>(data_.data() + offset));
}

} // namespace ShadPKG::Decompiler::Analysis
