#include "DecompilerContext.h"
#include "analysis/MemberAccessAnalysis.h"
#include "common/logging/log.h"
#include <capstone/capstone.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#define JSON_ASSERT(x)
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ShadPKG::Decompiler {

void DecompilerContext::LoadBinary(const std::vector<uint8_t> &data,
                                   uint64_t baseAddress) {
  rawData_ = data;
  baseAddress_ = baseAddress;
  isAnalyzed_ = false;
  functions_.clear();
  // Initialize SymbolDatabase
  symbolDatabase_ = std::make_shared<Analysis::SymbolDatabase>();
}

bool DecompilerContext::LoadELF(const std::vector<uint8_t> &data) {
  if (data.size() < 64)
    return false;

  size_t elfOffset = 0;
  bool found = false;

  // Check Magic: 7F 45 4C 46
  if (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
    found = true;
  } else {
    // Search for it (limit search to first 4KB)
    for (size_t i = 0; i < std::min(data.size() - 4, (size_t)0x1000); ++i) {
      if (data[i] == 0x7F && data[i + 1] == 'E' && data[i + 2] == 'L' &&
          data[i + 3] == 'F') {
        elfOffset = i;
        found = true;
        break;
      }
    }
  }

  if (!found) {
    LOG_ERROR(Common, "No ELF header found in data.");
    return false;
  }

  elfOffset_ = elfOffset;
  const uint8_t *elfBase = data.data() + elfOffset_;

  // Parse ELF Header (64-bit)
  uint64_t entryPoint = 0;
  uint64_t phOff = 0;
  uint16_t phEntSize = 0;
  uint16_t phNum = 0;

  if (data.size() < elfOffset + 64)
    return false;

  std::memcpy(&entryPoint, elfBase + 0x18, 8);
  std::memcpy(&phOff, elfBase + 0x20, 8);
  std::memcpy(&phEntSize, elfBase + 0x36, 2);
  std::memcpy(&phNum, elfBase + 0x38, 2);
  // Capture entry point for later use in ExportProject
  entryPoint_ = entryPoint;

  LOG_INFO(Common,
           "ELF Found at offset 0x{:X}. Entry=0x{:X}, PHOff=0x{:X}, PHNum={}",
           elfOffset, entryPoint, phOff, phNum);

  segments_.clear();
  uint64_t minVA = UINT64_MAX;

  // Parse Program Headers
  for (int i = 0; i < phNum; ++i) {
    size_t currentPhOffset = phOff + (i * phEntSize);
    if (elfOffset + currentPhOffset + phEntSize > data.size())
      break;

    const uint8_t *phBase = elfBase + currentPhOffset;

    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_filesz;
    uint64_t p_memsz;

    std::memcpy(&p_type, phBase, 4);
    std::memcpy(&p_flags, phBase + 4, 4);
    std::memcpy(&p_offset, phBase + 8, 8);
    std::memcpy(&p_vaddr, phBase + 0x10, 8);
    std::memcpy(&p_filesz, phBase + 0x20, 8);
    std::memcpy(&p_memsz, phBase + 0x28, 8);

    if (p_type == 1) { // PT_LOAD
      Segment seg;
      seg.virtualAddress = p_vaddr;
      // Absolute offset in rawData_
      seg.fileOffset = elfOffset + p_offset;
      seg.size = p_filesz;
      segments_.push_back(seg);

      if (p_vaddr < minVA)
        minVA = p_vaddr;

      LOG_INFO(Common,
               "Segment: VA=0x{:X}, Offset=0x{:X} (Abs: 0x{:X}), Size=0x{:X}",
               p_vaddr, p_offset, seg.fileOffset, p_filesz);
    }
  }

  // Load raw data and set base
  LoadBinary(data, minVA != UINT64_MAX ? minVA : 0x400000);

  if (segments_.empty()) {
    LOG_WARNING(Common, "No PT_LOAD segments found. Falling back to flat "
                        "mapping from ELF base.");
    Segment seg;
    seg.virtualAddress = 0x400000;
    seg.fileOffset = elfOffset;
    seg.size = data.size() - elfOffset;
    segments_.push_back(seg);
    baseAddress_ = 0x400000;
  }

  return true;
}

bool DecompilerContext::VirtualAddressToFileOffset(uint64_t va,
                                                   uint64_t &offset) const {
  for (const auto &seg : segments_) {
    if (va >= seg.virtualAddress && va < seg.virtualAddress + seg.size) {
      offset = seg.fileOffset + (va - seg.virtualAddress);
      return true;
    }
  }
  if (segments_.empty() && va >= baseAddress_) {
    offset = va - baseAddress_;
    return true;
  }
  return false;
}

void DecompilerContext::Analyze() {
  if (rawData_.empty())
    return;

  functions_.clear();
  std::set<uint64_t> visitedGlobal;

  uint64_t entryPoint = baseAddress_;
  if (rawData_.size() >= elfOffset_ + 0x20 && rawData_[elfOffset_] == 0x7F &&
      rawData_[elfOffset_ + 1] == 'E') {
    std::memcpy(&entryPoint, rawData_.data() + elfOffset_ + 0x18, 8);
  }

  std::queue<uint64_t> functionQueue;
  functionQueue.push(entryPoint);
  visitedGlobal.insert(entryPoint);

  uint64_t epOffset = 0;
  if (VirtualAddressToFileOffset(entryPoint, epOffset)) {
    if (epOffset + 8 > rawData_.size()) {
      LOG_ERROR(Common, "Entry Point 0x{:X} is out of file bounds!",
                entryPoint);
      return;
    }
  } else {
    LOG_ERROR(Common, "Could not map Entry Point 0x{:X} to file offset!",
              entryPoint);
    if (entryPoint >= baseAddress_)
      epOffset = entryPoint - baseAddress_;
  }

  int functionsAnalyzed = 0;
  const int MAX_FUNCTIONS = 50000;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Run Symbol Analysis early to recover function names from ELF          │
  // │  This populates symbolDatabase_ with names from .dynsym / .rela.plt    │
  // └─────────────────────────────────────────────────────────────────────────┘
  {
    auto symbolAnalysis = std::make_shared<Analysis::SymbolAnalysis>(
        rawData_, baseAddress_, symbolDatabase_);
    symbolAnalysis->analyze();
    LOG_INFO(Common, "Symbol Analysis complete: {} symbols loaded.",
             symbolDatabase_ ? symbolDatabase_->getSymbols().size() : 0);
  }

  LOG_INFO(Common, "Starting Function Discovery at 0x{:X}...", entryPoint);

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │                    PROLOGUE SCAN FOR ALL FUNCTIONS                      │
  // │  Many functions are only called via vtables or indirect jumps.          │
  // │  Scan .text for common x86-64 function prologues:                       │
  // │    push rbp; mov rbp, rsp  -> 55 48 89 E5                               │
  // │    push rbx                -> 53                                        │
  // │    sub rsp, N              -> 48 83 EC XX / 48 81 EC XX XX XX XX        │
  // └─────────────────────────────────────────────────────────────────────────┘
  LOG_INFO(Common, "Scanning segments for function prologues...");
  progress_.currentPhase = "scanning";
  progress_.prologuesFound = 0;
  progress_.functionsAnalyzed = 0;
  progress_.isComplete = false;
  int prologuesFound = 0;

  for (const auto &seg : segments_) {
    if (seg.size < 8)
      continue;
    uint64_t scanEnd =
        std::min(seg.fileOffset + seg.size, (uint64_t)rawData_.size()) - 4;

    for (uint64_t off = seg.fileOffset; off < scanEnd; ++off) {
      uint64_t funcVA = seg.virtualAddress + (off - seg.fileOffset);

      if (visitedGlobal.count(funcVA))
        continue;

      // Pattern 1: push rbp; mov rbp, rsp (0x55 0x48 0x89 0xE5)
      bool match = (rawData_[off] == 0x55 && rawData_[off + 1] == 0x48 &&
                    rawData_[off + 2] == 0x89 && rawData_[off + 3] == 0xE5);

      // Pattern 2: push rbp; mov rsp variants
      if (!match && rawData_[off] == 0x55) {
        // Check for sub rsp, imm8 following (48 83 EC XX)
        if (off + 5 < scanEnd && rawData_[off + 1] == 0x48 &&
            rawData_[off + 2] == 0x83 && rawData_[off + 3] == 0xEC) {
          match = true;
        }
      }

      if (match) {
        functionQueue.push(funcVA);
        visitedGlobal.insert(funcVA);
        prologuesFound++;
        progress_.prologuesFound = prologuesFound;
      }
    }
  }

  LOG_INFO(Common, "Found {} candidate functions via prologue scan.",
           prologuesFound);

  while (!functionQueue.empty() && functionsAnalyzed < MAX_FUNCTIONS) {
    uint64_t funcAddr = functionQueue.front();
    functionQueue.pop();

    auto func = std::make_shared<IR::Function>();
    func->address = funcAddr;

    // ┌─────────────────────────────────────────────────────────────────────────┐
    // │  Try to recover function name from ELF symbols │
    // └─────────────────────────────────────────────────────────────────────────┘
    if (symbolDatabase_) {
      auto sym = symbolDatabase_->getSymbol(funcAddr);
      if (sym && !sym->name.empty() &&
          sym->name.find("sub_") == std::string::npos) {
        func->name = sym->name;
      }
    }
    if (func->name.empty()) {
      std::stringstream nameSS;
      nameSS << "sub_" << std::hex << funcAddr;
      func->name = nameSS.str();
    }

    func->signature = "void " + func->name + "()";
    func->category = IR::Function::Category::GameLogic;

    csh handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
      continue;
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    std::queue<uint64_t> blockQueue;
    std::set<uint64_t> visitedBlocks;

    blockQueue.push(funcAddr);
    visitedBlocks.insert(funcAddr);

    while (!blockQueue.empty()) {
      uint64_t blockAddr = blockQueue.front();
      blockQueue.pop();

      auto bb = std::make_shared<IR::BasicBlock>();
      bb->id = blockAddr;
      bb->startAddress = blockAddr;

      uint64_t currentAddr = blockAddr;
      bool blockEnded = false;
      int zeroCount = 0;

      for (int i = 0; i < 1000 && !blockEnded; ++i) {
        uint64_t fileOffset = 0;
        if (!VirtualAddressToFileOffset(currentAddr, fileOffset))
          break;

        if (fileOffset >= rawData_.size())
          break;

        const uint8_t *code = rawData_.data() + fileOffset;
        size_t code_size = rawData_.size() - fileOffset;

        if (code[0] == 0x00 && code[1] == 0x00) {
          zeroCount++;
          if (zeroCount > 8) {
            blockEnded = true;
            break;
          }
        } else {
          zeroCount = 0;
        }

        cs_insn *insn;
        size_t count = cs_disasm(handle, code, 15, currentAddr, 1, &insn);

        if (count > 0) {
          IR::Instruction instr;
          instr.address = insn[0].address;

          instr.opcode = IR::OpCode::NOP;
          switch (insn[0].id) {
          case X86_INS_MOV:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_MOVABS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_MOVAPS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_MOVUPS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_MOVDQA:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_MOVDQU:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_ADD:
            instr.opcode = IR::OpCode::ADD;
            break;
          case X86_INS_SUB:
            instr.opcode = IR::OpCode::SUB;
            break;
          case X86_INS_RET:
            instr.opcode = IR::OpCode::RET;
            break;
          case X86_INS_CALL:
            instr.opcode = IR::OpCode::CALL;
            break;
          case X86_INS_JMP:
            instr.opcode = IR::OpCode::JMP;
            break;
          case X86_INS_JE:
            instr.opcode = IR::OpCode::JE;
            break;
          case X86_INS_JNE:
            instr.opcode = IR::OpCode::JNE;
            break;
          case X86_INS_CMP:
            instr.opcode = IR::OpCode::CMP;
            break;
          case X86_INS_LEA:
            instr.opcode = IR::OpCode::LEA;
            break;

          case X86_INS_MOVSX:
            instr.opcode = IR::OpCode::MOVSX;
            break;
          case X86_INS_MOVSXD:
            instr.opcode = IR::OpCode::MOVSX;
            break;
          case X86_INS_MOVZX:
            instr.opcode = IR::OpCode::MOVZX;
            break;
          case X86_INS_MOVSD:
            instr.opcode = IR::OpCode::MOV;
            break; // Treat as MOV
          case X86_INS_MOVSS:
            instr.opcode = IR::OpCode::MOV;
            break; // Treat as MOV

          case X86_INS_BSWAP:
            instr.opcode = IR::OpCode::BSWAP;
            break;
          case X86_INS_FISTTP:
            instr.opcode = IR::OpCode::FISTTP;
            break;
          case X86_INS_LEAVE:
            instr.opcode = IR::OpCode::LEAVE;
            break;
          case X86_INS_INT:
            instr.opcode = IR::OpCode::INT;
            break; // Software interrupt, often for debug/syscall

          // AVX / VEX Support
          case X86_INS_VMOVSS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VMOVSD:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VMOVAPS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VMOVUPS:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VMOVAPD:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VMOVUPD:
            instr.opcode = IR::OpCode::MOV;
            break;
          case X86_INS_VADDSS:
            instr.opcode = IR::OpCode::ADD;
            break;
          case X86_INS_VADDSD:
            instr.opcode = IR::OpCode::ADD;
            break;
          case X86_INS_VSUBSS:
            instr.opcode = IR::OpCode::SUB;
            break;
          case X86_INS_VSUBSD:
            instr.opcode = IR::OpCode::SUB;
            break;
          case X86_INS_VMULSS:
            instr.opcode = IR::OpCode::MUL;
            break;
          case X86_INS_VMULSD:
            instr.opcode = IR::OpCode::MUL;
            break;
          case X86_INS_VDIVSS:
            instr.opcode = IR::OpCode::DIV;
            break;
          case X86_INS_VDIVSD:
            instr.opcode = IR::OpCode::DIV;
            break;
          case X86_INS_VXORPS:
            instr.opcode = IR::OpCode::XOR;
            break;
          case X86_INS_VXORPD:
            instr.opcode = IR::OpCode::XOR;
            break;
          case X86_INS_VANDPS:
            instr.opcode = IR::OpCode::AND;
            break;
          case X86_INS_VANDPD:
            instr.opcode = IR::OpCode::AND;
            break;
          case X86_INS_VORPS:
            instr.opcode = IR::OpCode::OR;
            break;
          case X86_INS_VORPD:
            instr.opcode = IR::OpCode::OR;
            break;
          case X86_INS_VUCOMISS:
            instr.opcode = IR::OpCode::CMP;
            break;
          case X86_INS_VUCOMISD:
            instr.opcode = IR::OpCode::CMP;
            break;
          case X86_INS_VPCMPGTB:
          case X86_INS_VPCMPGTW:
          case X86_INS_VPCMPGTD:
          case X86_INS_VPCMPGTQ:
            instr.opcode = IR::OpCode::CMP;
            break;

          // Stack
          case X86_INS_PUSH:
            instr.opcode = IR::OpCode::PUSH;
            break;
          case X86_INS_POP:
            instr.opcode = IR::OpCode::POP;
            break;

          // Logic/Shift
          case X86_INS_AND:
            instr.opcode = IR::OpCode::AND;
            break;
          case X86_INS_OR:
            instr.opcode = IR::OpCode::OR;
            break;
          case X86_INS_XOR:
            instr.opcode = IR::OpCode::XOR;
            break;
          case X86_INS_SHL:
            instr.opcode = IR::OpCode::SHL;
            break;
          case X86_INS_SHR:
            instr.opcode = IR::OpCode::SHR;
            break;
          case X86_INS_SAL:
            instr.opcode = IR::OpCode::SHL;
            break;
          case X86_INS_SAR:
            instr.opcode = IR::OpCode::SHR;
            break; // Treat arithmetic shift as logical for now or add SAR
          case X86_INS_TEST:
            instr.opcode = IR::OpCode::AND;
            break; // TEST is mostly AND for flags

          // Arithmetic
          case X86_INS_INC:
            instr.opcode = IR::OpCode::ADD;
            break; // Handled specially or mapped to ADD 1
          case X86_INS_DEC:
            instr.opcode = IR::OpCode::SUB;
            break; // Handled specially or mapped to SUB 1
          case X86_INS_NEG:
            instr.opcode = IR::OpCode::SUB;
            break; // 0 - x? Need proper unary op
          case X86_INS_NOT:
            instr.opcode = IR::OpCode::XOR;
            break; // ~x (Need unary op really)
          case X86_INS_MUL:
            instr.opcode = IR::OpCode::MUL;
            break;
          case X86_INS_IMUL:
            instr.opcode = IR::OpCode::MUL;
            break;
          case X86_INS_DIV:
            instr.opcode = IR::OpCode::DIV;
            break;
          case X86_INS_IDIV:
            instr.opcode = IR::OpCode::DIV;
            break;

          default:
            break;
          }
          instr.disassembly =
              std::string(insn[0].mnemonic) + " " + insn[0].op_str;

          if (insn[0].detail) {
            for (int j = 0; j < insn[0].detail->x86.op_count; ++j) {
              const auto &op = insn[0].detail->x86.operands[j];
              IR::Operand irOp;
              if (op.type == X86_OP_REG) {
                irOp.type = IR::Operand::Type::Register;
                irOp.value = op.reg;
                irOp.regName = cs_reg_name(handle, op.reg);
              } else if (op.type == X86_OP_IMM) {
                irOp.type = IR::Operand::Type::Immediate;
                irOp.value = op.imm;
              } else if (op.type == X86_OP_MEM) {
                irOp.type = IR::Operand::Type::Memory;
                // Store raw info for advanced lifting
                irOp.memBase = op.mem.base;
                irOp.memBaseName = (op.mem.base != X86_REG_INVALID)
                                       ? cs_reg_name(handle, op.mem.base)
                                       : "";
                irOp.memDisp = op.mem.disp;

                if (op.mem.base == X86_REG_RIP) {
                  irOp.value = insn[0].address + insn[0].size + op.mem.disp;
                  irOp.name = "RIP";
                } else if (op.mem.base == X86_REG_RSP) {
                  irOp.name = "RSP";
                  irOp.value = (uint64_t)op.mem.disp;
                } else if (op.mem.base == X86_REG_RBP) {
                  irOp.name = "RBP";
                  irOp.value = (uint64_t)op.mem.disp;
                } else {
                  irOp.value = 0;
                  irOp.name = "MEM";
                }
              } else {
                irOp.type = IR::Operand::Type::Variable;
              }
              instr.operands.push_back(irOp);
            }
          }

          bb->instructions.push_back(instr);

          bool isBranch = false;
          bool isCall = (insn[0].id == X86_INS_CALL);
          bool isRet = (insn[0].id == X86_INS_RET);

          if (insn[0].id == X86_INS_JMP ||
              (insn[0].id >= X86_INS_JA && insn[0].id <= X86_INS_JS)) {
            isBranch = true;
          }

          if (isCall) {
            if (insn[0].detail->x86.op_count > 0 &&
                insn[0].detail->x86.operands[0].type == X86_OP_IMM) {
              uint64_t target = insn[0].detail->x86.operands[0].imm;
              if (visitedGlobal.find(target) == visitedGlobal.end()) {
                visitedGlobal.insert(target);
                functionQueue.push(target);
              }
            }
          } else if (isBranch) {
            blockEnded = true;
            if (insn[0].detail->x86.op_count > 0 &&
                insn[0].detail->x86.operands[0].type == X86_OP_IMM) {
              uint64_t target = insn[0].detail->x86.operands[0].imm;
              bb->successors.push_back(target);
              if (visitedBlocks.find(target) == visitedBlocks.end()) {
                visitedBlocks.insert(target);
                blockQueue.push(target);
              }
            }
            if (insn[0].id != X86_INS_JMP) {
              uint64_t nextAddr = insn[0].address + insn[0].size;
              bb->successors.push_back(nextAddr);
              if (visitedBlocks.find(nextAddr) == visitedBlocks.end()) {
                visitedBlocks.insert(nextAddr);
                blockQueue.push(nextAddr);
              }
            }
          } else if (isRet) {
            blockEnded = true;
          }

          currentAddr += insn[0].size;
          cs_free(insn, count);
        } else {
          blockEnded = true;
        }
      }
      bb->endAddress = currentAddr;
      func->basicBlocks.push_back(bb);
    }

    cs_close(&handle);
    functions_.push_back(func);
    functionsAnalyzed++;
    progress_.functionsAnalyzed = functionsAnalyzed;
    progress_.currentPhase = "analyzing";
  }

  LOG_INFO(Common, "Discovered {} functions.", functions_.size());
  progress_.currentPhase = "complete";
  progress_.isComplete = true;
  isAnalyzed_ = true;
}

void DecompilerContext::AnalyzeFunction(uint64_t startAddress,
                                        std::set<uint64_t> &visitedGlobal) {}

void DecompilerContext::ExportProject(const std::string &outPath) {
  std::filesystem::path root(outPath);
  std::filesystem::create_directories(root / "include");
  std::filesystem::create_directories(root / "src");
  std::filesystem::create_directories(root / "data");

  LOG_INFO(Common, "Starting full project analysis and export...");
  
  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Export binary data segments for runtime loading                        │
  // └─────────────────────────────────────────────────────────────────────────┘
  {
    // Find minimum VA to use as base
    uint64_t minVA = baseAddress_;
    for (const auto &seg : segments_) {
      if (seg.virtualAddress < minVA) minVA = seg.virtualAddress;
    }
    
    // Calculate total size needed
    uint64_t maxEnd = 0;
    for (const auto &seg : segments_) {
      uint64_t end = (seg.virtualAddress - minVA) + seg.size;
      if (end > maxEnd) maxEnd = end;
    }
    
    if (maxEnd > 0 && maxEnd < 64 * 1024 * 1024) { // Max 64MB
      std::vector<uint8_t> memoryImage(maxEnd, 0);
      
      for (const auto &seg : segments_) {
        uint64_t destOffset = seg.virtualAddress - minVA;
        if (seg.fileOffset + seg.size <= rawData_.size()) {
          std::memcpy(memoryImage.data() + destOffset, 
                      rawData_.data() + seg.fileOffset, seg.size);
          LOG_INFO(Common, "Exported segment: VA=0x{:X} -> offset 0x{:X}, size=0x{:X}",
                   seg.virtualAddress, destOffset, seg.size);
        }
      }
      
      std::ofstream memFile(root / "data" / "memory.bin", std::ios::binary);
      memFile.write(reinterpret_cast<const char*>(memoryImage.data()), memoryImage.size());
      memFile.close();
      
      // Write metadata
      std::ofstream metaFile(root / "data" / "memory.meta");
      metaFile << "base_address=0x" << std::hex << minVA << "\n";
      metaFile << "size=0x" << maxEnd << "\n";
      metaFile << "segments=" << segments_.size() << "\n";
      metaFile.close();
      
      LOG_INFO(Common, "Exported memory image: {} bytes (base=0x{:X})", maxEnd, minVA);
    }
  }

  auto symbols = std::make_shared<Analysis::SymbolAnalysis>(
      rawData_, baseAddress_, symbolDatabase_);
  symbols->analyze();

  std::vector<std::shared_ptr<AST::FunctionAST>> analyzedASTs;
  analyzedASTs.reserve(functions_.size());

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  PASS 1: Full Analysis with progress output and timeout protection      │
  // └─────────────────────────────────────────────────────────────────────────┘
  size_t skippedCount = 0;
  auto globalStart = std::chrono::steady_clock::now();

  for (size_t i = 0; i < functions_.size(); ++i) {
    auto func = functions_[i];

    // Progress output every 100 functions
    if (i % 100 == 0 || i == functions_.size() - 1) {
      auto elapsed = std::chrono::steady_clock::now() - globalStart;
      auto secs =
          std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
      std::cout << "\r[" << (i + 1) << "/" << functions_.size() << "] "
                << "Analyzing... (" << secs << "s, " << skippedCount
                << " skipped)" << std::flush;
    }

    size_t funcSize = 0;
    for (const auto &bb : func->basicBlocks) {
      if (!bb->instructions.empty())
        funcSize += (bb->endAddress - bb->startAddress);
    }

    // Skip very large functions (likely data, not code)
    if (funcSize > 100000) {
      LOG_INFO(Common, "Skipping huge function {} ({}KB)", func->name,
               funcSize / 1024);
      skippedCount++;
      continue;
    }

    auto funcStart = std::chrono::steady_clock::now();

    try {
      auto dom = std::make_shared<Analysis::DominatorAnalysis>();
      dom->analyze(func);

      // Timeout check after dominator analysis
      auto elapsed = std::chrono::steady_clock::now() - funcStart;
      if (elapsed > std::chrono::seconds(5)) {
        LOG_INFO(Common, "Skipping {} (dominator timeout)", func->name);
        skippedCount++;
        continue;
      }

      // 3. Variable Lifting
      auto lifter = std::make_shared<Lifter::VariableAnalysis>(func);
      lifter->analyze();

      // 4. Structural Analysis
      Analysis::StructuralAnalysis structural(func, dom, symbols, lifter);
      auto ast = structural.analyze();

      // Timeout check after structural analysis
      elapsed = std::chrono::steady_clock::now() - funcStart;
      if (elapsed > std::chrono::seconds(10)) {
        LOG_INFO(Common, "Skipping {} (structural timeout)", func->name);
        skippedCount++;
        continue;
      }

      if (!ast->body || ast->body->statements.empty()) {
        // Empty function - still add stub
      }

      // Apply locals to AST
      lifter->applyToAST(ast);

      // 4b. Apply User Types
      for (auto &local : ast->locals) {
        auto userType = GetUserVarType(func->address, local.stackOffset);
        if (userType) {
          local.complexType = userType;
        }
      }

      Analysis::DataFlowAnalysis dataflow(ast);
      dataflow.analyze();

      Analysis::MemberAccessAnalysis memberAccess(typeManager_);
      memberAccess.analyze(ast);

      analyzedASTs.push_back(ast);
      SetFunctionParamCount(func->address, (int)ast->parameters.size());

    } catch (const std::exception &e) {
      LOG_ERROR(Common, "Exception analyzing {}: {}", func->name, e.what());
      skippedCount++;
      continue;
    }
  }

  std::cout << std::endl; // Newline after progress
  LOG_INFO(Common, "Analysis complete: {} functions OK, {} skipped",
           analyzedASTs.size(), skippedCount);

  // --- PASS 2: Export Types ---
  {
    std::ofstream hTypes(root / "include" / "types.h");
    hTypes << "#pragma once\n";
    hTypes << "#include <cstdint>\n\n";
    hTypes << "#if defined(__GNUC__) || defined(__clang__)\n";
    hTypes << "#define _byteswap_uint64 __builtin_bswap64\n";
    hTypes << "#endif\n\n";
    hTypes << "static inline double as_double(uint64_t i) { return "
              "*(double*)&i; }\n";
    hTypes
        << "static inline float as_float(uint32_t i) { return *(float*)&i; }\n";
    hTypes << "static inline uint64_t as_uint64(double d) { return "
              "*(uint64_t*)&d; }\n";
    hTypes << "static inline uint32_t as_uint32(float f) { return "
              "*(uint32_t*)&f; }\n\n";

    for (const auto &[name, type] : typeManager_->getAllTypes()) {
      if (type->getKind() == Analysis::Type::Kind::Struct) {
        hTypes << "struct " << name << ";\n";
      }
    }
    hTypes << "\n";

    for (const auto &[name, type] : typeManager_->getAllTypes()) {
      if (type->getKind() == Analysis::Type::Kind::Struct) {
        auto structType = std::dynamic_pointer_cast<Analysis::StructType>(type);
        hTypes << "struct " << name << " {\n";
        for (const auto &member : structType->getMembers()) {
          hTypes << "    " << member.type->toString() << " " << member.name
                 << "; // Offset: " << member.offset << "\n";
        }
        hTypes << "};\n\n";
      }
    }
  }

  // --- PASS 3: Export Functions ---
  {
    std::map<std::string, std::shared_ptr<AST::FunctionAST>> nameToAst;
    for (auto &ast : analyzedASTs) {
      nameToAst[ast->name] = ast;
    }

    std::ofstream hFuncs(root / "include" / "functions.h");
    hFuncs << "#pragma once\n";
    hFuncs << "#include \"types.h\"\n";
    hFuncs << "#include <cstdint>\n\n";
    
    // Runtime dispatch functions for vtable/indirect calls
    hFuncs << "// Runtime dispatch functions for vtable/indirect calls\n";
    hFuncs << "extern \"C\" {\n";
    hFuncs << "    int64_t ps4_vtable_dispatch(void* obj, uint64_t offset,\n";
    hFuncs << "                                 uint64_t a1, uint64_t a2, uint64_t a3,\n";
    hFuncs << "                                 uint64_t a4, uint64_t a5, uint64_t a6);\n";
    hFuncs << "    \n";
    hFuncs << "    int64_t ps4_indirect_dispatch(void* fnPtr,\n";
    hFuncs << "                                   uint64_t a1, uint64_t a2, uint64_t a3,\n";
    hFuncs << "                                   uint64_t a4, uint64_t a5, uint64_t a6);\n";
    hFuncs << "}\n\n";

    for (const auto &func : functions_) {
      // Check if this is a PLT/system function (negative address or special prefix)
      bool isPltFunc = (func->address > 0xFFFFFFFF00000000ULL) || 
                       (func->name.find("sub_fffff") != std::string::npos) ||
                       (func->name.find("sub_cb") != std::string::npos) ||
                       (func->name.find("sub_ca") != std::string::npos) ||
                       (func->name.find("sub_c9") != std::string::npos);
      
      // Always use generic 6-parameter signature for all functions
      // This ensures declarations match all possible call sites
      {
        std::string retType = "int64_t";
        if (nameToAst.count(func->name)) {
          auto &ast = nameToAst[func->name];
          if (ast->returnType != "void") {
            retType = ast->returnType;
          }
        }
        hFuncs << retType << " " << func->name << "(";
        hFuncs << "int64_t a1 = 0, int64_t a2 = 0, int64_t a3 = 0, "
                  "int64_t a4 = 0, int64_t a5 = 0, int64_t a6 = 0";
        hFuncs << ");\n";
      }
    }
  }

  // --- PASS 4: Export Source ---
  std::cout << "Exporting source code..." << std::endl;
  LOG_INFO(Common, "Starting source export of {} functions",
           analyzedASTs.size());

  int fileCounter = 0;
  int funcsPerFile = 50;

  for (size_t i = 0; i < analyzedASTs.size(); i += funcsPerFile) {
    if (i % 1000 == 0) {
      std::cout << "\rExporting chunks... " << i << "/" << analyzedASTs.size()
                << std::flush;
    }

    std::string filename = "segment_" + std::to_string(fileCounter++) + ".cpp";
    std::ofstream cppFile(root / "src" / filename);

    cppFile << "#include \"../include/functions.h\"\n";
    cppFile << "#include \"../include/types.h\"\n";
    cppFile << "#include \"../include/globals.h\"\n\n";

    for (size_t j = i; j < i + funcsPerFile && j < analyzedASTs.size(); ++j) {
      auto ast = analyzedASTs[j];
      try {
        Codegen::CppEmitter emitter;
        cppFile << emitter.generate(ast) << "\n\n";
      } catch (const std::exception &e) {
        LOG_ERROR(Common, "Failed to generate code for {}: {}", ast->name,
                  e.what());
        cppFile << "// Error generating " << ast->name << ": " << e.what()
                << "\n\n";
      } catch (...) {
        LOG_ERROR(Common, "Unknown failure generating code for {}", ast->name);
        cppFile << "// Unknown error generating " << ast->name << "\n\n";
      }
    }
  }
  std::cout << std::endl;

  // --- PASS 5: Export Globals ---
  {
    std::ofstream hGlobals(root / "include" / "globals.h");
    std::ofstream cppGlobals(root / "src" / "globals.cpp");

    hGlobals << "#pragma once\n";
    hGlobals << "#include \"types.h\"\n";
    hGlobals << "#include <cstdint>\n\n";

    cppGlobals << "#include \"../include/globals.h\"\n\n";

    // We need to iterate the symbol database for globals
    if (symbolDatabase_) {
      const auto &syms = symbolDatabase_->getSymbols();
      for (const auto &[addr, sym] : syms) {
        if (sym.type == Analysis::SymbolType::GlobalVariable) {
          // Parse type from name: g_TYPE_ADDR
          std::string typeName = "int64_t";

          size_t firstUnderscore = sym.name.find('_');
          size_t lastUnderscore = sym.name.rfind('_');

          if (firstUnderscore != std::string::npos &&
              lastUnderscore != std::string::npos &&
              lastUnderscore > firstUnderscore) {
            typeName = sym.name.substr(firstUnderscore + 1,
                                       lastUnderscore - (firstUnderscore + 1));
          }

          // Read initial value from memory
          std::string initVal = "0";
          uint64_t offset = addr - baseAddress_;

          if (offset < rawData_.size()) {
            std::stringstream ss;
            ss << "0x" << std::hex;

            if (typeName.find("int8") != std::string::npos ||
                typeName == "char" || typeName == "bool") {
              uint16_t val =
                  rawData_[offset]; // Use uint16 for stream formatting of uint8
              ss << val;
              initVal = ss.str();
            } else if (typeName.find("int16") != std::string::npos ||
                       typeName == "short") {
              if (offset + 2 <= rawData_.size()) {
                uint16_t val =
                    *reinterpret_cast<const uint16_t *>(&rawData_[offset]);
                ss << val;
                initVal = ss.str();
              }
            } else if (typeName.find("int32") != std::string::npos ||
                       typeName == "int" || typeName == "float") {
              if (offset + 4 <= rawData_.size()) {
                uint32_t val =
                    *reinterpret_cast<const uint32_t *>(&rawData_[offset]);
                ss << val;
                initVal = ss.str();
              }
              if (typeName == "float")
                initVal = "0.0f"; // Placeholder
            } else {              // int64, long, double, pointer
              if (offset + 8 <= rawData_.size()) {
                uint64_t val =
                    *reinterpret_cast<const uint64_t *>(&rawData_[offset]);
                ss << val;
                initVal = ss.str();
              }
              if (typeName == "double")
                initVal = "0.0"; // Placeholder
              if (typeName == "float")
                initVal = "0.0f";
            }
          } else if (typeName == "double") {
            initVal = "0.0";
          } else if (typeName == "float") {
            initVal = "0.0f";
          } else {
            initVal = "{}"; // Aggregate init/default
          }

          hGlobals << "extern " << typeName << " " << sym.name << "; // 0x"
                   << std::hex << addr << "\n";
          cppGlobals << typeName << " " << sym.name << " = " << initVal
                     << ";\n";
        }
      }
    }
  }

  // --- PASS 6: Copy ps4MEL Runtime Files ---
  {
    std::filesystem::create_directories(root / "ps4mel");
    
    // List of ps4MEL files to copy
    std::vector<std::string> ps4melFiles = {
        "ps4_memory.cpp", "ps4_memory.h",
        "ps4_kernel.cpp", "ps4_kernel.h",
        "ps4_pthread.cpp", "ps4_pthread.h",
        "ps4_tls.cpp", "ps4_tls.h",
        "ps4_stubs.cpp",
        "runtime.cpp", "runtime.h"
    };
    
    // Try to find ps4MEL directory relative to executable or source
    std::filesystem::path ps4melSrc;
    std::vector<std::filesystem::path> searchPaths = {
        std::filesystem::current_path() / "ps4MEL",
        std::filesystem::current_path().parent_path() / "ps4MEL",
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "ps4MEL"
    };
    
    for (const auto& p : searchPaths) {
      if (std::filesystem::exists(p)) {
        ps4melSrc = p;
        break;
      }
    }
    
    if (!ps4melSrc.empty()) {
      LOG_INFO(Common, "Copying ps4MEL files from: {}", ps4melSrc.string());
      for (const auto& file : ps4melFiles) {
        std::filesystem::path srcFile = ps4melSrc / file;
        std::filesystem::path dstFile = root / "ps4mel" / file;
        if (std::filesystem::exists(srcFile)) {
          try {
            std::filesystem::copy_file(srcFile, dstFile, 
                std::filesystem::copy_options::overwrite_existing);
          } catch (...) {
            LOG_WARNING(Common, "Failed to copy: {}", file);
          }
        }
      }
    } else {
      LOG_WARNING(Common, "ps4MEL directory not found, generating minimal stubs");
    }
  }

  // --- PASS 7: Export Stubs, Runtime and Main ---
  {
    std::filesystem::create_directories(root / "assets");

    std::map<std::string, std::shared_ptr<AST::FunctionAST>> nameToAst;
    for (auto &ast : analyzedASTs) {
      nameToAst[ast->name] = ast;
    }

    // 1. Export Stubs
    {
      std::ofstream stubs(root / "src" / "stubs.cpp");
      stubs << "#include \"../include/functions.h\"\n\n";
      for (const auto &func : functions_) {
        if (nameToAst.find(func->name) == nameToAst.end()) {
          stubs << "int64_t " << func->name
                << "(int64_t a1, int64_t a2, int64_t a3, "
                   "int64_t a4, int64_t a5, int64_t a6) { return 0; }\n";
        }
      }
    }

    // 2. Export Runtime Header
    {
      std::ofstream rtH(root / "include" / "runtime.h");
      rtH << "#pragma once\n";
      rtH << "#include <cstdint>\n";
      rtH << "#include <string>\n\n";
      rtH << "void runtime_init();\n";
      rtH << "std::string resolve_path(const std::string& ps4_path);\n\n";
      rtH << "// List of mocked PS4 system calls\n";
      rtH << "extern \"C\" {\n";
      rtH << "    int sceKernelOpen(const char* path, int flags, int mode);\n";
      rtH << "    int sceKernelRead(int fd, void* buf, size_t nbyte);\n";
      rtH << "    int sceKernelClose(int fd);\n";
      rtH << "    // Add more mocks as needed...\n";
      rtH << "}\n";
    }

    // 3. Export Runtime Implementation
    {
      std::ofstream rtCpp(root / "src" / "runtime.cpp");
      rtCpp << "#include \"../include/runtime.h\"\n";
      rtCpp << "#include <iostream>\n";
      rtCpp << "#include <filesystem>\n";
      rtCpp << "#include <map>\n\n";
      rtCpp << "void runtime_init() {\n";
      rtCpp << "    std::cout << \"[RUNTIME] Initializing PS4 Mock "
               "Runtime...\" << std::endl;\n";
      rtCpp << "    std::cout << \"[RUNTIME] VFS Root: \" << "
               "std::filesystem::current_path() / \"assets\" << std::endl;\n";
      rtCpp << "}\n\n";
      rtCpp << "std::string resolve_path(const std::string& ps4_path) {\n";
      rtCpp << "    // Implementation of path redirection (e.g. /app0/ -> "
               "assets/)\n";
      rtCpp << "    std::string path = ps4_path;\n";
      rtCpp << "    if (path.find(\"/app0/\") == 0) {\n";
      rtCpp << "        return \"assets/\" + path.substr(6);\n";
      rtCpp << "    }\n";
      rtCpp << "    return \"assets/\" + path;\n";
      rtCpp << "}\n\n";
      rtCpp << "extern \"C\" {\n";
      rtCpp
          << "    int sceKernelOpen(const char* path, int flags, int mode) {\n";
      rtCpp << "        std::cout << \"[RUNTIME] sceKernelOpen: \" << path << "
               "std::endl;\n";
      rtCpp << "        return -1; // Mock return\n";
      rtCpp << "    }\n";
      rtCpp << "    int sceKernelRead(int fd, void* buf, size_t nbyte) { "
               "return 0; }\n";
      rtCpp << "    int sceKernelClose(int fd) { return 0; }\n";
      rtCpp << "}\n";
    }

    // 4. Export Main (using ps4MEL runtime)
    {
      std::ofstream mainCpp(root / "src" / "main.cpp");
      mainCpp << "/*\n";
      mainCpp << " * Decompiled Game - Main Entry Point\n";
      mainCpp << " * Generated by ShadPKG Decompiler\n";
      mainCpp << " * Uses ps4MEL (PS4 Memory Emulation Layer) for runtime support\n";
      mainCpp << " */\n\n";
      mainCpp << "#include \"../include/functions.h\"\n";
      mainCpp << "#include \"../ps4mel/runtime.h\"\n";
      mainCpp << "#include \"../ps4mel/ps4_memory.h\"\n";
      mainCpp << "#include <iostream>\n";
      mainCpp << "#include <csignal>\n";
      mainCpp << "#include <cstdlib>\n\n";
      
      mainCpp << "// Signal handler for graceful shutdown\n";
      mainCpp << "static volatile bool g_running = true;\n";
      mainCpp << "void signal_handler(int sig) {\n";
      mainCpp << "    std::cout << \"\\n[RUNTIME] Caught signal \" << sig << \", shutting down...\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] Final frame count: \" << PS4Emu::GetFrameCounter() << std::endl;\n";
      mainCpp << "    g_running = false;\n";
      mainCpp << "}\n\n";
      
      mainCpp << "int main(int argc, char* argv[]) {\n";
      mainCpp << "    // Setup signal handlers\n";
      mainCpp << "    std::signal(SIGINT, signal_handler);\n";
      mainCpp << "    std::signal(SIGTERM, signal_handler);\n\n";
      mainCpp << "    std::cout << \"[RUNTIME] ══════════════════════════════════════════════\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] ShadPKG Decompiled Game Runtime\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] ══════════════════════════════════════════════\" << std::endl;\n\n";
      mainCpp << "    // Initialize ps4MEL runtime (memory, TLS, VFS)\n";
      mainCpp << "    runtime_init();\n\n";

      // Find entry point name
      std::string entryName = "sub_" + std::to_string(entryPoint_);
      auto it =
          std::find_if(functions_.begin(), functions_.end(),
                       [this](auto f) { return f->address == entryPoint_; });
      if (it != functions_.end()) {
        entryName = (*it)->name;
      }

      mainCpp << "    std::cout << \"[RUNTIME] Entry point: " << entryName 
              << " (0x" << std::hex << entryPoint_ << std::dec << ")\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] Initializing global context...\" << std::endl;\n\n";
      
      mainCpp << "    // Uncomment the line below to start game execution\n";
      mainCpp << "    // WARNING: The game may crash or hang if not all dependencies are satisfied\n";
      mainCpp << "    // " << entryName << "();\n\n";
      
      mainCpp << "    // For debugging: run a test loop to verify runtime is working\n";
      mainCpp << "    std::cout << \"[RUNTIME] Runtime initialized successfully.\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] To run the game, uncomment the entry point call in main.cpp\" << std::endl;\n";
      mainCpp << "    std::cout << \"[RUNTIME] Frame counter location: 0x9d9e30\" << std::endl;\n\n";
      
      mainCpp << "    // Cleanup\n";
      mainCpp << "    runtime_shutdown();\n";
      mainCpp << "    std::cout << \"[RUNTIME] Shutdown complete.\" << std::endl;\n";
      mainCpp << "    return 0;\n";
      mainCpp << "}\n";
    }
  }

  {
    std::ofstream cmake(root / "CMakeLists.txt");
    cmake << "cmake_minimum_required(VERSION 3.10)\n";
    cmake << "project(DecompiledGame CXX)\n\n";
    cmake << "set(CMAKE_CXX_STANDARD 17)\n";
    cmake << "add_compile_options(-fbracket-depth=1024)\n";
    cmake << "add_compile_options(-Wno-unused-variable -Wno-unused-but-set-variable)\n\n";
    cmake << "# Include directories\n";
    cmake << "include_directories(include)\n";
    cmake << "include_directories(ps4mel)\n\n";
    cmake << "# Game source files\n";
    cmake << "file(GLOB GAME_SOURCES \"src/*.cpp\")\n\n";
    cmake << "# PS4 Runtime Emulation Layer (ps4MEL)\n";
    cmake << "set(PS4MEL_SOURCES\n";
    cmake << "    ps4mel/ps4_memory.cpp\n";
    cmake << "    ps4mel/ps4_kernel.cpp\n";
    cmake << "    ps4mel/ps4_pthread.cpp\n";
    cmake << "    ps4mel/ps4_tls.cpp\n";
    cmake << "    ps4mel/ps4_stubs.cpp\n";
    cmake << "    ps4mel/runtime.cpp\n";
    cmake << ")\n\n";
    cmake << "add_executable(Game ${GAME_SOURCES} ${PS4MEL_SOURCES})\n\n";
    cmake << "# Platform-specific linking\n";
    cmake << "if(APPLE)\n";
    cmake << "    target_link_libraries(Game PRIVATE pthread)\n";
    cmake << "elseif(UNIX)\n";
    cmake << "    target_link_libraries(Game PRIVATE pthread)\n";
    cmake << "endif()\n";
  }

  std::cout << "Project exported to: " << outPath << "\n";
}

std::string DecompilerContext::GenerateCode() {
  if (!isAnalyzed_)
    return "// Not analyzed yet";

  std::stringstream ss;
  ss << "// Decompiled by ShadPKG\n";
  ss << "// Architecture: x86-64\n\n";

  for (const auto &func : functions_) {
    ss << func->signature << " {\n";
    for (const auto &bb : func->basicBlocks) {
      ss << "  // Block: 0x" << std::hex << bb->startAddress << "\n";
      for (const auto &instr : bb->instructions) {
        ss << "    " << instr.disassembly << ";\n";
      }
      if (!bb->successors.empty()) {
        ss << "    goto ";
        for (auto succ : bb->successors)
          ss << "0x" << std::hex << succ << " ";
        ss << ";\n";
      }
      ss << "\n";
    }
    ss << "}\n\n";
  }

  return ss.str();
}

std::string DecompilerContext::GenerateStructuredCode() {
  if (!isAnalyzed_)
    return "// Not analyzed yet (Run Analysis first)";

  std::stringstream ss;
  ss << "// Decompiled by ShadPKG (Structured Analysis)\n";
  ss << "// Architecture: x86-64\n\n";

  auto symbols = std::make_shared<Analysis::SymbolAnalysis>(
      rawData_, baseAddress_, symbolDatabase_);
  symbols->analyze();

  for (const auto &func : functions_) {
    auto dom = std::make_shared<Analysis::DominatorAnalysis>();
    dom->analyze(func);

    auto lifter = std::make_shared<Lifter::VariableAnalysis>(func);
    lifter->analyze();

    Analysis::StructuralAnalysis structural(func, dom, symbols, lifter);
    auto ast = structural.analyze();

    lifter->applyToAST(ast);

    for (auto &local : ast->locals) {
      auto userType = GetUserVarType(func->address, local.stackOffset);
      if (userType) {
        local.complexType = userType;
      }
    }

    Analysis::DataFlowAnalysis dataflow(ast);
    dataflow.analyze();

    Analysis::MemberAccessAnalysis memberAccess(typeManager_);
    memberAccess.analyze(ast);

    Codegen::CppEmitter emitter;
    ss << emitter.generate(ast);
    functionTokens_[func->address] = emitter.getTokens();
    ss << "\n";
  }

  return ss.str();
}

const std::vector<Codegen::CppEmitter::Token> &
DecompilerContext::GetFunctionTokens(uint64_t address) const {
  if (functionTokens_.count(address)) {
    return functionTokens_.at(address);
  }
  static std::vector<Codegen::CppEmitter::Token> empty;
  return empty;
}

uint64_t DecompilerContext::ReadPointer(uint64_t address) const {
  if (address < baseAddress_)
    return 0;
  uint64_t offset = address - baseAddress_;
  if (offset + 8 > rawData_.size())
    return 0;
  return *reinterpret_cast<const uint64_t *>(&rawData_[offset]);
}

int32_t DecompilerContext::ReadInt32(uint64_t address) const {
  if (address < baseAddress_)
    return 0;
  uint64_t offset = address - baseAddress_;
  if (offset + 4 > rawData_.size())
    return 0;
  return *reinterpret_cast<const int32_t *>(&rawData_[offset]);
}

std::shared_ptr<IR::Function>
DecompilerContext::GetFunctionAt(uint64_t address) {
  for (const auto &func : functions_) {
    if (func->address == address)
      return func;
  }
  return nullptr;
}

bool DecompilerContext::SaveProject(const std::string &path) {
  json j;

  // Symbols
  if (symbolDatabase_) {
    for (const auto &[addr, sym] : symbolDatabase_->getSymbols()) {
      j["symbols"].push_back({{"address", addr},
                              {"name", sym.name},
                              {"type", (int)sym.type},
                              {"source", (int)sym.source}});
    }
  }

  // Structs
  for (const auto &[name, type] : typeManager_->getAllTypes()) {
    if (type->getKind() == Analysis::Type::Kind::Struct) {
      auto s = std::dynamic_pointer_cast<Analysis::StructType>(type);
      json sJson;
      sJson["name"] = name;
      for (const auto &m : s->getMembers()) {
        sJson["members"].push_back({{"name", m.name},
                                    {"offset", m.offset},
                                    {"type", m.type->toString()}});
      }
      j["structs"].push_back(sJson);
    }
  }

  // User Overrides (Stack Types)
  // userVarTypes_: FunctionAddr -> (StackOffset -> Type)
  for (const auto &[funcAddr, offsets] : userVarTypes_) {
    for (const auto &[offset, type] : offsets) {
      j["overrides"].push_back(
          {{"func", funcAddr}, {"offset", offset}, {"type", type->toString()}});
    }
  }

  std::ofstream o(path);
  o << std::setw(4) << j << std::endl;
  return true;
}

bool DecompilerContext::LoadProject(const std::string &path) {
  std::ifstream i(path);
  if (!i.is_open())
    return false;

  json j;
  i >> j;

  // Symbols
  if (j.contains("symbols") && symbolDatabase_) {
    symbolDatabase_->clear(); // Maybe clear existing?
    for (const auto &sym : j["symbols"]) {
      symbolDatabase_->addSymbol(sym["address"], sym["name"],
                                 (Analysis::SymbolType)sym["type"],
                                 (Analysis::SymbolSource)sym["source"]);
    }
  }

  // Structs
  if (j.contains("structs")) {
    for (const auto &sJson : j["structs"]) {
      std::string name = sJson["name"];
      // Use createStruct which registers it
      auto s = typeManager_->createStruct(name);
      for (const auto &mJson : sJson["members"]) {
        std::string typeName = mJson["type"];
        auto type = typeManager_->getType(typeName);
        if (!type) {
          // Basic primitive fallback or implicit creation
          if (typeName.back() == '*') {
            // Assume pointer to something?
            // For now default to int
            type = typeManager_->getType("int");
          } else {
            type = typeManager_->getType("int");
          }
        }
        if (type) {
          s->addMember(mJson["name"], type, mJson["offset"]);
        }
      }
    }
  }

  // Overrides
  if (j.contains("overrides")) {
    for (const auto &ov : j["overrides"]) {
      uint64_t funcAddr = ov["func"];
      int offset = ov["offset"];
      std::string typeName = ov["type"];

      auto type = typeManager_->getType(typeName);
      if (type) {
        SetUserVarType(funcAddr, offset, type);
      }
    }
  }

  return true;
}

} // namespace ShadPKG::Decompiler