#include "DecompilerContext.h"
#include "analysis/MemberAccessAnalysis.h"
#include "common/logging/log.h"
#include <capstone/capstone.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <cstdlib>
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
  const uint8_t* elfBase = data.data() + elfOffset_;

  // Parse ELF Header (64-bit)
  uint64_t entryPoint = 0;
  uint64_t phOff = 0;
  uint16_t phEntSize = 0;
  uint16_t phNum = 0;

  if (data.size() < elfOffset + 64) return false;

  std::memcpy(&entryPoint, elfBase + 0x18, 8);
  std::memcpy(&phOff, elfBase + 0x20, 8);
  std::memcpy(&phEntSize, elfBase + 0x36, 2);
  std::memcpy(&phNum, elfBase + 0x38, 2);

  LOG_INFO(Common, "ELF Found at offset 0x{:X}. Entry=0x{:X}, PHOff=0x{:X}, PHNum={}", elfOffset, entryPoint, phOff, phNum);

  segments_.clear();
  uint64_t minVA = UINT64_MAX;

  // Parse Program Headers
  for (int i = 0; i < phNum; ++i) {
    size_t currentPhOffset = phOff + (i * phEntSize);
    if (elfOffset + currentPhOffset + phEntSize > data.size()) break;

    const uint8_t* phBase = elfBase + currentPhOffset;

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

      if (p_vaddr < minVA) minVA = p_vaddr;

      LOG_INFO(Common, "Segment: VA=0x{:X}, Offset=0x{:X} (Abs: 0x{:X}), Size=0x{:X}", 
               p_vaddr, p_offset, seg.fileOffset, p_filesz);
    }
  }

  // Load raw data and set base
  LoadBinary(data, minVA != UINT64_MAX ? minVA : 0x400000);
  
  if (segments_.empty()) {
      LOG_WARNING(Common, "No PT_LOAD segments found. Falling back to flat mapping from ELF base.");
      Segment seg;
      seg.virtualAddress = 0x400000;
      seg.fileOffset = elfOffset;
      seg.size = data.size() - elfOffset;
      segments_.push_back(seg);
      baseAddress_ = 0x400000;
  }

  return true;
}

bool DecompilerContext::VirtualAddressToFileOffset(uint64_t va, uint64_t &offset) const {
  for (const auto& seg : segments_) {
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
  if (rawData_.size() >= elfOffset_ + 0x20 && 
      rawData_[elfOffset_] == 0x7F && rawData_[elfOffset_ + 1] == 'E') {
       std::memcpy(&entryPoint, rawData_.data() + elfOffset_ + 0x18, 8);
  }

  std::queue<uint64_t> functionQueue;
  functionQueue.push(entryPoint);
  visitedGlobal.insert(entryPoint);

  uint64_t epOffset = 0;
  if (VirtualAddressToFileOffset(entryPoint, epOffset)) {
      if (epOffset + 8 > rawData_.size()) {
          LOG_ERROR(Common, "Entry Point 0x{:X} is out of file bounds!", entryPoint);
          return;
      }
  } else {
       LOG_ERROR(Common, "Could not map Entry Point 0x{:X} to file offset!", entryPoint);
       if (entryPoint >= baseAddress_) epOffset = entryPoint - baseAddress_;
  }

  int functionsAnalyzed = 0;
  const int MAX_FUNCTIONS = 50000;

  LOG_INFO(Common, "Starting Function Discovery at 0x{:X}...", entryPoint);

  while (!functionQueue.empty() && functionsAnalyzed < MAX_FUNCTIONS) {
    uint64_t funcAddr = functionQueue.front();
    functionQueue.pop();

    auto func = std::make_shared<IR::Function>();
    std::stringstream nameSS;
    nameSS << "sub_" << std::hex << funcAddr;
    func->name = nameSS.str();
    func->address = funcAddr;
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
        if (!VirtualAddressToFileOffset(currentAddr, fileOffset)) break;
        
        if (fileOffset >= rawData_.size()) break;

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
        size_t count =
            cs_disasm(handle, code, 15, currentAddr, 1, &insn);

        if (count > 0) {
          IR::Instruction instr;
          instr.address = insn[0].address;

          instr.opcode = IR::OpCode::NOP;
          switch (insn[0].id) {
          case X86_INS_MOV: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_MOVABS: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_MOVAPS: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_MOVUPS: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_MOVDQA: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_MOVDQU: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_ADD: instr.opcode = IR::OpCode::ADD; break;
          case X86_INS_SUB: instr.opcode = IR::OpCode::SUB; break;
          case X86_INS_RET: instr.opcode = IR::OpCode::RET; break;
          case X86_INS_CALL: instr.opcode = IR::OpCode::CALL; break;
          case X86_INS_JMP: instr.opcode = IR::OpCode::JMP; break;
          case X86_INS_JE: instr.opcode = IR::OpCode::JE; break;
          case X86_INS_JNE: instr.opcode = IR::OpCode::JNE; break;
          case X86_INS_CMP: instr.opcode = IR::OpCode::CMP; break;
          case X86_INS_LEA: instr.opcode = IR::OpCode::LEA; break;
          
          case X86_INS_MOVSX: instr.opcode = IR::OpCode::MOVSX; break;
          case X86_INS_MOVSXD: instr.opcode = IR::OpCode::MOVSX; break;
          case X86_INS_MOVZX: instr.opcode = IR::OpCode::MOVZX; break;
          case X86_INS_MOVSD: instr.opcode = IR::OpCode::MOV; break; // Treat as MOV
          case X86_INS_MOVSS: instr.opcode = IR::OpCode::MOV; break; // Treat as MOV
          
          case X86_INS_BSWAP: instr.opcode = IR::OpCode::BSWAP; break;
          case X86_INS_FISTTP: instr.opcode = IR::OpCode::FISTTP; break;
          case X86_INS_LEAVE: instr.opcode = IR::OpCode::LEAVE; break;
          case X86_INS_INT: instr.opcode = IR::OpCode::INT; break; // Software interrupt, often for debug/syscall

          // AVX / VEX Support
          case X86_INS_VMOVSS:  instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VMOVSD:  instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VMOVAPS: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VMOVUPS: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VMOVAPD: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VMOVUPD: instr.opcode = IR::OpCode::MOV; break;
          case X86_INS_VADDSS:  instr.opcode = IR::OpCode::ADD; break;
          case X86_INS_VADDSD:  instr.opcode = IR::OpCode::ADD; break;
          case X86_INS_VSUBSS:  instr.opcode = IR::OpCode::SUB; break;
          case X86_INS_VSUBSD:  instr.opcode = IR::OpCode::SUB; break;
          case X86_INS_VMULSS:  instr.opcode = IR::OpCode::MUL; break;
          case X86_INS_VMULSD:  instr.opcode = IR::OpCode::MUL; break;
          case X86_INS_VDIVSS:  instr.opcode = IR::OpCode::DIV; break;
          case X86_INS_VDIVSD:  instr.opcode = IR::OpCode::DIV; break;
          case X86_INS_VXORPS:  instr.opcode = IR::OpCode::XOR; break;
          case X86_INS_VXORPD:  instr.opcode = IR::OpCode::XOR; break;
          case X86_INS_VANDPS:  instr.opcode = IR::OpCode::AND; break;
          case X86_INS_VANDPD:  instr.opcode = IR::OpCode::AND; break;
          case X86_INS_VORPS:   instr.opcode = IR::OpCode::OR; break;
          case X86_INS_VORPD:   instr.opcode = IR::OpCode::OR; break;
          case X86_INS_VUCOMISS: instr.opcode = IR::OpCode::CMP; break;
          case X86_INS_VUCOMISD: instr.opcode = IR::OpCode::CMP; break;
          case X86_INS_VPCMPGTB: case X86_INS_VPCMPGTW: case X86_INS_VPCMPGTD: case X86_INS_VPCMPGTQ:
              instr.opcode = IR::OpCode::CMP; break;

          // Stack
          case X86_INS_PUSH: instr.opcode = IR::OpCode::PUSH; break;
          case X86_INS_POP: instr.opcode = IR::OpCode::POP; break;

          // Logic/Shift
          case X86_INS_AND: instr.opcode = IR::OpCode::AND; break;
          case X86_INS_OR: instr.opcode = IR::OpCode::OR; break;
          case X86_INS_XOR: instr.opcode = IR::OpCode::XOR; break;
          case X86_INS_SHL: instr.opcode = IR::OpCode::SHL; break;
          case X86_INS_SHR: instr.opcode = IR::OpCode::SHR; break;
          case X86_INS_SAL: instr.opcode = IR::OpCode::SHL; break;
          case X86_INS_SAR: instr.opcode = IR::OpCode::SHR; break; // Treat arithmetic shift as logical for now or add SAR
          case X86_INS_TEST: instr.opcode = IR::OpCode::AND; break; // TEST is mostly AND for flags

          // Arithmetic
          case X86_INS_INC: instr.opcode = IR::OpCode::ADD; break; // Handled specially or mapped to ADD 1
          case X86_INS_DEC: instr.opcode = IR::OpCode::SUB; break; // Handled specially or mapped to SUB 1
          case X86_INS_NEG: instr.opcode = IR::OpCode::SUB; break; // 0 - x? Need proper unary op
          case X86_INS_NOT: instr.opcode = IR::OpCode::XOR; break; // ~x (Need unary op really)
          case X86_INS_MUL: instr.opcode = IR::OpCode::MUL; break;
          case X86_INS_IMUL: instr.opcode = IR::OpCode::MUL; break;
          case X86_INS_DIV: instr.opcode = IR::OpCode::DIV; break;
          case X86_INS_IDIV: instr.opcode = IR::OpCode::DIV; break;

          default: break;
          }
          instr.disassembly =
              std::string(insn[0].mnemonic) + " " + insn[0].op_str;
          
          if (insn[0].detail) {
              for (int j = 0; j < insn[0].detail->x86.op_count; ++j) {
                  const auto& op = insn[0].detail->x86.operands[j];
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
                      irOp.memBaseName = (op.mem.base != X86_REG_INVALID) ? cs_reg_name(handle, op.mem.base) : "";
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
  }

  LOG_INFO(Common, "Discovered {} functions.", functions_.size());
  isAnalyzed_ = true;
}

void DecompilerContext::AnalyzeFunction(uint64_t startAddress,
                                        std::set<uint64_t> &visitedGlobal) {
}

void DecompilerContext::ExportProject(const std::string &outPath) {
  std::filesystem::path root(outPath);
  std::filesystem::create_directories(root / "include");
  std::filesystem::create_directories(root / "src");

  LOG_INFO(Common, "Starting full project analysis and export...");

  auto symbols = std::make_shared<Analysis::SymbolAnalysis>(
      rawData_, baseAddress_, symbolDatabase_);
  symbols->analyze();

  std::vector<std::shared_ptr<AST::FunctionAST>> analyzedASTs;
  analyzedASTs.reserve(functions_.size());

  // --- PASS 1: Full Analysis ---
  for (size_t i = 0; i < functions_.size(); ++i) {
    auto func = functions_[i];

    size_t funcSize = 0;
    for(const auto& bb : func->basicBlocks) {
        if(!bb->instructions.empty())
            funcSize += (bb->endAddress - bb->startAddress);
    }

    LOG_INFO(Common, "Analyzing function {} (Size: {} bytes)...", func->name, funcSize);

    auto dom = std::make_shared<Analysis::DominatorAnalysis>();
    dom->analyze(func);

    // 3. Variable Lifting
    auto lifter = std::make_shared<Lifter::VariableAnalysis>(func);
    lifter->analyze();

    // 4. Structural Analysis
    Analysis::StructuralAnalysis structural(func, dom, symbols, lifter);
    auto ast = structural.analyze();

    if (!ast->body || ast->body->statements.empty()) {
        LOG_INFO(Common, "Warning: Function {} produced empty AST.", func->name);
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
  }

  // --- PASS 2: Export Types ---
  {
    std::ofstream hTypes(root / "include" / "types.h");
    hTypes << "#pragma once\n";
    hTypes << "#include <cstdint>\n\n";

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
    std::ofstream hFuncs(root / "include" / "functions.h");
    hFuncs << "#pragma once\n";
    hFuncs << "#include \"types.h\"\n\n";
    for (const auto &func : functions_) {
      hFuncs << func->signature << ";\n";
    }
  }

  // --- PASS 4: Export Source ---
  int fileCounter = 0;
  int funcsPerFile = 50;

  for (size_t i = 0; i < analyzedASTs.size(); i += funcsPerFile) {
    std::string filename = "segment_" + std::to_string(fileCounter++) + ".cpp";
    std::ofstream cppFile(root / "src" / filename);

    cppFile << "#include \"../include/functions.h\"\n";
    cppFile << "#include \"../include/types.h\"\n\n";

    for (size_t j = i; j < i + funcsPerFile && j < analyzedASTs.size(); ++j) {
      auto ast = analyzedASTs[j];

      Codegen::CppEmitter emitter;
      cppFile << emitter.generate(ast) << "\n\n";
    }
  }

  {
    std::ofstream cmake(root / "CMakeLists.txt");
    cmake << "cmake_minimum_required(VERSION 3.10)\n";
    cmake << "project(DecompiledGame CXX)\n\n";
    cmake << "set(CMAKE_CXX_STANDARD 17)\n";
    cmake << "include_directories(include)\n\n";
    cmake << "file(GLOB SOURCES \"src/*.cpp\")\n";
    cmake << "add_executable(Game ${SOURCES})\n";
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
      for (const auto& [addr, sym] : symbolDatabase_->getSymbols()) {
          j["symbols"].push_back({
              {"address", addr},
              {"name", sym.name},
              {"type", (int)sym.type},
              {"source", (int)sym.source}
          });
      }
  }

  // Structs
  for (const auto& [name, type] : typeManager_->getAllTypes()) {
      if (type->getKind() == Analysis::Type::Kind::Struct) {
          auto s = std::dynamic_pointer_cast<Analysis::StructType>(type);
          json sJson;
          sJson["name"] = name;
          for (const auto& m : s->getMembers()) {
              sJson["members"].push_back({
                  {"name", m.name},
                  {"offset", m.offset},
                  {"type", m.type->toString()} 
              });
          }
          j["structs"].push_back(sJson);
      }
  }

  // User Overrides (Stack Types)
  // userVarTypes_: FunctionAddr -> (StackOffset -> Type)
  for (const auto& [funcAddr, offsets] : userVarTypes_) {
      for (const auto& [offset, type] : offsets) {
          j["overrides"].push_back({
              {"func", funcAddr},
              {"offset", offset},
              {"type", type->toString()}
          });
      }
  }
  
  std::ofstream o(path);
  o << std::setw(4) << j << std::endl;
  return true;
}

bool DecompilerContext::LoadProject(const std::string &path) {
  std::ifstream i(path);
  if (!i.is_open()) return false;
  
  json j;
  i >> j;
  
  // Symbols
  if (j.contains("symbols") && symbolDatabase_) {
      symbolDatabase_->clear(); // Maybe clear existing? 
      for (const auto& sym : j["symbols"]) {
          symbolDatabase_->addSymbol(
              sym["address"],
              sym["name"], 
              (Analysis::SymbolType)sym["type"], 
              (Analysis::SymbolSource)sym["source"]
          );
      }
  }
  
  // Structs
  if (j.contains("structs")) {
      for (const auto& sJson : j["structs"]) {
          std::string name = sJson["name"];
          // Use createStruct which registers it
          auto s = typeManager_->createStruct(name);
          for (const auto& mJson : sJson["members"]) {
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
      for (const auto& ov : j["overrides"]) {
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