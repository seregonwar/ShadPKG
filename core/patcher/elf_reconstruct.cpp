#include "elf_reconstruct.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace ShadPKG::Patcher {

ElfReconstructor::ElfReconstructor() {}

bool ElfReconstructor::loadFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "[ElfReconstructor] Failed to open: " << path << std::endl;
    return false;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  rawData_.resize(size);
  if (!file.read(reinterpret_cast<char *>(rawData_.data()), size)) {
    return false;
  }

  isLoaded_ = true;
  return true;
}

bool ElfReconstructor::loadData(const std::vector<uint8_t> &data) {
  rawData_ = data;
  isLoaded_ = true;
  return true;
}

bool ElfReconstructor::isSelf() const {
  if (rawData_.size() < sizeof(SelfHeader))
    return false;
  const auto *header = reinterpret_cast<const SelfHeader *>(rawData_.data());
  // Check magic 0x1D3D154F (Little Endian: 4F 15 3D 1D)
  return header->magic == 0x1D3D154F;
}

bool ElfReconstructor::isElf() const {
  if (rawData_.size() < 4)
    return false;
  // ELF magic: 0x7F 'E' 'L' 'F'
  return rawData_[0] == 0x7F && rawData_[1] == 'E' && rawData_[2] == 'L' &&
         rawData_[3] == 'F';
}

bool ElfReconstructor::reconstructElf(const std::string &outputPath) {
  if (!isLoaded_)
    return false;

  if (isElf()) {
    std::cout << "[ElfReconstructor] Already an ELF file." << std::endl;
    elfData_ = rawData_;
    if (!outputPath.empty()) {
      std::ofstream out(outputPath, std::ios::binary);
      out.write(reinterpret_cast<const char *>(elfData_.data()),
                elfData_.size());
    }
    return true;
  }

  if (!isSelf()) {
    std::cerr << "[ElfReconstructor] Input is not a valid SELF or ELF."
              << std::endl;
    return false;
  }

  std::cout << "[ElfReconstructor] Converting SELF to ELF..." << std::endl;

  const auto *selfHeader =
      reinterpret_cast<const SelfHeader *>(rawData_.data());

  std::vector<Elf64_Phdr> phdrs;
  std::vector<std::vector<uint8_t>> segmentData;

  size_t segmentTableOffset = sizeof(SelfHeader);

  const auto *segments = reinterpret_cast<const SelfSegment *>(
      rawData_.data() + 0x20); // 32 bytes offset

  // Prepare ELF Header
  Elf64_Ehdr ehdr = {};
  ehdr.e_ident[0] = 0x7F;
  ehdr.e_ident[1] = 'E';
  ehdr.e_ident[2] = 'L';
  ehdr.e_ident[3] = 'F';
  ehdr.e_ident[4] = 2; // ELFCLASS64
  ehdr.e_ident[5] = 1; // ELFDATA2LSB
  ehdr.e_ident[6] = 1; // EV_CURRENT
  ehdr.e_ident[7] = 9; // FreeBSD ABI (PS4 uses this)
  ehdr.e_type = 2;     // ET_EXEC
  ehdr.e_machine = 62; // EM_X86_64
  ehdr.e_version = 1;
  ehdr.e_phentsize = sizeof(Elf64_Phdr);
  ehdr.e_phnum = 0;
  ehdr.e_shentsize = 0;
  ehdr.e_shnum = 0;
  ehdr.e_shstrndx = 0;
  ehdr.e_ehsize = sizeof(Elf64_Ehdr);

  // Calculate global offset for data
  // ELF Header + Phdrs
  uint64_t currentOffset =
      sizeof(Elf64_Ehdr) + (selfHeader->segment_count * sizeof(Elf64_Phdr));
  // Align to page size?
  currentOffset = (currentOffset + 0xFFF) & ~0xFFF;

  ehdr.e_phoff = sizeof(Elf64_Ehdr); // Phdrs follow Ehdr

  for (int i = 0; i < selfHeader->segment_count; ++i) {
    const auto &seg = segments[i];

    if (seg.file_size == 0 && seg.memory_size == 0)
      continue;

    // Define ELF Phdr
    Elf64_Phdr phdr = {};

    phdr.p_type = 1; // PT_LOAD default
    phdr.p_flags = 0;
    if (seg.flags & 4)
      phdr.p_flags |= 4; // R
    if (seg.flags & 2)
      phdr.p_flags |= 2; // W
    if (seg.flags & 1)
      phdr.p_flags |= 1; // X

    // Use SELF segment properties
    phdr.p_offset = currentOffset; // We will pack data here
    phdr.p_vaddr = seg.vaddr;
    phdr.p_paddr = seg.vaddr;
    phdr.p_filesz = seg.file_size;
    phdr.p_memsz = seg.memory_size;
    phdr.p_align = seg.align;

    // Need to read data from SELF
    // seg.offset in SELF file -> data
    if (seg.offset + seg.file_size > rawData_.size()) {
      std::cerr << "[ElfReconstructor] Segment " << i << " out of bounds!"
                << std::endl;
      continue;
    }

    std::vector<uint8_t> data(seg.file_size);
    std::memcpy(data.data(), rawData_.data() + seg.offset, seg.file_size);
    segmentData.push_back(std::move(data));

    phdrs.push_back(phdr);

    // Update offset for next segment
    currentOffset += seg.file_size;
  }

  ehdr.e_phnum = phdrs.size();

  for (size_t i = 0; i < rawData_.size() - 4; ++i) {
    if (rawData_[i] == 0x7F && rawData_[i + 1] == 'E' &&
        rawData_[i + 2] == 'L' && rawData_[i + 3] == 'F') {
      // Found internal ELF!
      std::cout << "[ElfReconstructor] Found internal ELF at offset 0x"
                << std::hex << i << std::dec << std::endl;

      const auto *internalEhdr =
          reinterpret_cast<const Elf64_Ehdr *>(rawData_.data() + i);
      // Check if it looks valid
      if (internalEhdr->e_machine == 62) {

        std::cout << "[ElfReconstructor] Found embedded ELF64 header."
                  << std::endl;

        // 1. Validate Program Headers
        uint64_t phoff = internalEhdr->e_phoff;
        uint16_t phnum = internalEhdr->e_phnum;

        // Calculate where the PHT end is relative to the ELF start
        uint64_t phtEnd = phoff + (phnum * internalEhdr->e_phentsize);

        if (i + phtEnd > rawData_.size()) {
          std::cerr << "[ElfReconstructor] Error: Program Headers extend "
                       "beyond file bounds."
                    << std::endl;
          continue;
        }

        // 2. Scan Segments to find the actual file size required
        // The ELF file size on disk is determined by the segment with the
        // highest (p_offset + p_filesz)
        uint64_t maxOffset = phtEnd; // Min size is up to PHT

        const Elf64_Phdr *phdrs =
            reinterpret_cast<const Elf64_Phdr *>(rawData_.data() + i + phoff);

        for (int k = 0; k < phnum; ++k) {
          uint64_t segEnd = phdrs[k].p_offset + phdrs[k].p_filesz;
          if (segEnd > maxOffset) {
            maxOffset = segEnd;
          }
        }

        // 3. Validate Section Headers (Optional for execution, but good for
        // tools)
        bool stripSections = false;
        uint64_t shoff = internalEhdr->e_shoff;
        uint16_t shnum = internalEhdr->e_shnum;
        uint64_t shtEnd = shoff + (shnum * internalEhdr->e_shentsize);

        if (shoff != 0 && (i + shtEnd > rawData_.size())) {
          std::cerr << "[ElfReconstructor] Warning: Section Headers point "
                       "outside file (Offset: "
                    << shoff << "). Stripping..." << std::endl;
          stripSections = true;
        } else if (shoff != 0) {
          // If sections exist and are valid, include them in size
          if (shtEnd > maxOffset) {
            maxOffset = shtEnd;
          }
        }

        std::cout << "[ElfReconstructor] Calculated actual ELF size: "
                  << maxOffset << " bytes." << std::endl;

        // Double check bounds against input file
        if (i + maxOffset > rawData_.size()) {
          std::cerr << "[ElfReconstructor] Warning: Segments extend beyond "
                       "input file! Truncating to available data."
                    << std::endl;
          maxOffset = rawData_.size() - i;
        }

        // 4. Create new ELF buffer
        elfData_.resize(maxOffset);

        // Copy data from input
        // We copy byte-for-byte from the start of the embedded ELF to the
        // calculated end
        std::memcpy(elfData_.data(), rawData_.data() + i, maxOffset);

        // 5. Patch header if we stripped sections
        if (stripSections) {
          auto *newEhdr = reinterpret_cast<Elf64_Ehdr *>(elfData_.data());
          newEhdr->e_shoff = 0;
          newEhdr->e_shnum = 0;
          newEhdr->e_shentsize = 0;
          newEhdr->e_shstrndx = 0;
        }

        if (!outputPath.empty()) {
          std::ofstream out(outputPath, std::ios::binary);
          out.write(reinterpret_cast<const char *>(elfData_.data()),
                    elfData_.size());
          std::cout << "[ElfReconstructor] Saved reconstructed ELF ("
                    << elfData_.size() << " bytes)" << std::endl;
        }
        return true;
      }
    }
  }

  ehdr.e_entry = 0x400000;

  std::cerr << "[ElfReconstructor] Could not find embedded ELF header."
            << std::endl;
  return false;
}

} // namespace ShadPKG::Patcher
