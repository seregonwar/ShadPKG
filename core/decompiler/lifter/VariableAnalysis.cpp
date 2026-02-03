#include "VariableAnalysis.h"
#include "../DecompilerContext.h"
#include <algorithm>
#include <capstone/capstone.h>
#include <iostream>
#include <set>
#include <sstream>

namespace ShadPKG::Decompiler::Lifter {

VariableAnalysis::VariableAnalysis(std::shared_ptr<IR::Function> func)
    : func_(func) {
  // Initialize standard calling convention registers (System V AMD64)
  registerParams_[X86_REG_RDI] = 0;
  registerParams_[X86_REG_RSI] = 1;
  registerParams_[X86_REG_RDX] = 2;
  registerParams_[X86_REG_RCX] = 3;
  registerParams_[X86_REG_R8] = 4;
  registerParams_[X86_REG_R9] = 5;

  // Also check 32-bit versions
  registerParams_[X86_REG_EDI] = 0;
  registerParams_[X86_REG_ESI] = 1;
  registerParams_[X86_REG_EDX] = 2;
  registerParams_[X86_REG_ECX] = 3;
  registerParams_[X86_REG_R8D] = 4;
  registerParams_[X86_REG_R9D] = 5;

  // 16-bit
  registerParams_[X86_REG_DI] = 0;
  registerParams_[X86_REG_SI] = 1;
  registerParams_[X86_REG_DX] = 2;
  registerParams_[X86_REG_CX] = 3;

  // 8-bit
  registerParams_[X86_REG_DIL] = 0;
  registerParams_[X86_REG_SIL] = 1;
  registerParams_[X86_REG_DL] = 2;
  registerParams_[X86_REG_CL] = 3;
}

void VariableAnalysis::analyze() {
  if (!func_)
    return;

  assignedRegisters_.clear();
  regToParam_.clear();
  detectedParams_.clear();
  std::set<int> paramsDetected;

  // Scan all instructions in all basic blocks
  for (const auto &bb : func_->basicBlocks) {
    for (const auto &instr : bb->instructions) {

      // Check for return value assignment (rax)
      if (instr.opcode == IR::OpCode::MOV || instr.opcode == IR::OpCode::ADD) {
        if (!instr.operands.empty() &&
            instr.operands[0].type == IR::Operand::Type::Register) {
          uint64_t reg = instr.operands[0].value;
          if (reg == X86_REG_RAX || reg == X86_REG_EAX || reg == X86_REG_AX ||
              reg == X86_REG_AL) {
            inferredReturnType_ = "int64_t";
          }
        }
      }

      // Detect parameters (use before def in parameter registers)
      for (size_t i = 0; i < instr.operands.size(); ++i) {
        const auto &op = instr.operands[i];
        if (op.type == IR::Operand::Type::Register) {
          uint64_t reg = op.value;

          bool isDest = (i == 0 && (instr.opcode == IR::OpCode::MOV ||
                                    instr.opcode == IR::OpCode::LEA ||
                                    instr.opcode == IR::OpCode::POP ||
                                    instr.opcode == IR::OpCode::MOVSX ||
                                    instr.opcode == IR::OpCode::MOVZX));

          if (isDest) {
            assignedRegisters_.insert(reg);
          } else {
            // It's a source usage. Is it a param register not yet assigned?
            if (registerParams_.count(reg) &&
                assignedRegisters_.find(reg) == assignedRegisters_.end()) {
              int paramIdx = registerParams_[reg];
              if (paramsDetected.find(paramIdx) == paramsDetected.end()) {
                paramsDetected.insert(paramIdx);

                std::string pName = "a" + std::to_string(paramIdx + 1);
                AST::LocalVariable param;
                param.name = pName;
                param.isParameter = true;
                param.paramIndex = paramIdx;
                param.type = AST::Expression::Type::Int64;
                detectedParams_.push_back(param);

                // Map all aliases of this register to this parameter
                // Simple version: just this reg for now
                regToParam_[reg] = pName;
              }
            }
          }
        }
      }

      scanInstruction(instr);
    }
  }

  // Sort parameters by index
  std::sort(
      detectedParams_.begin(), detectedParams_.end(),
      [](const auto &a, const auto &b) { return a.paramIndex < b.paramIndex; });
}

std::string VariableAnalysis::getParamForRegister(uint64_t regId) const {
  if (regToParam_.count(regId))
    return regToParam_.at(regId);
  return "";
}

// ═══════════════════════════════════════════════════════════════════════════
//  SSA Register Tracking
// ═══════════════════════════════════════════════════════════════════════════

void VariableAnalysis::trackRegisterDef(uint64_t regId, uint64_t instrAddr) {
  RegDef key{regId, instrAddr};
  if (ssaTemps_.find(key) == ssaTemps_.end()) {
    std::stringstream ss;
    ss << "temp_" << nextTempId_++;
    ssaTemps_[key] = ss.str();
  }
}

std::string VariableAnalysis::getTempForRegister(uint64_t regId,
                                                 uint64_t instrAddr) const {
  // First check if it's a parameter
  if (regToParam_.count(regId)) {
    return regToParam_.at(regId);
  }

  // Look for most recent definition at or before instrAddr
  std::string bestMatch;
  uint64_t bestAddr = 0;
  for (const auto &[def, name] : ssaTemps_) {
    if (def.regId == regId && def.defAddr <= instrAddr) {
      if (def.defAddr > bestAddr) {
        bestAddr = def.defAddr;
        bestMatch = name;
      }
    }
  }
  return bestMatch; // Empty if not tracked
}

void VariableAnalysis::scanInstruction(const IR::Instruction &instr) {
  for (const auto &op : instr.operands) {
    if (op.type == IR::Operand::Type::Memory) {
      // Use structured data from IR
      if (op.memBase == X86_REG_RBP || op.memBase == X86_REG_RSP) {
        int offset = (int)op.memDisp;
        createVariable(offset, 4, false); // Default size 4
      }
    }
  }
}

void VariableAnalysis::createVariable(int offset, int size, bool isParam) {
  if (stackVariables_.find(offset) == stackVariables_.end()) {
    AST::LocalVariable var;
    var.stackOffset = offset;
    var.size = size;
    var.isParameter = isParam;

    std::stringstream ss;
    
    // Generate more meaningful names based on offset and size
    if (offset > 0 && isParam) {
      ss << "param_" << std::abs(offset);
    } else if (offset < 0) {
      // Use more descriptive names for common stack offsets
      int absOffset = std::abs(offset);
      
      // Common pattern: small offsets are often loop counters or temp variables
      if (absOffset <= 8) {
        ss << "temp_" << absOffset;
      }
      // Medium offsets are often local variables
      else if (absOffset <= 32) {
        ss << "local_" << std::hex << absOffset;
      }
      // Larger offsets might be arrays or structures
      else {
        ss << "var_" << std::hex << absOffset;
      }
    } else {
      ss << "var_p" << std::hex << offset;
    }
    var.name = ss.str();

    // Infer type based on size
    if (size == 1)
      var.type = AST::Expression::Type::Int8;
    else if (size == 2)
      var.type = AST::Expression::Type::Int16;
    else if (size == 8)
      var.type = AST::Expression::Type::Int64;
    else if (size == 16)
      var.type = AST::Expression::Type::Int64; // Treat 16-byte as int64 pair
    else
      var.type = AST::Expression::Type::Int32;

    stackVariables_[offset] = var;
  }
}

void VariableAnalysis::applyToAST(std::shared_ptr<AST::FunctionAST> ast) {
  if (!ast)
    return;

  ast->parameters = detectedParams_;
  ast->returnType = inferredReturnType_;

  for (const auto &[offset, var] : stackVariables_) {
    ast->locals.push_back(var);
  }
}

std::string VariableAnalysis::resolveVariable(const IR::Operand &op) {
  if (op.type == IR::Operand::Type::Memory) {
    // Check RBP/RSP by name (insensitive)
    bool isStack = op.memBaseName == "rbp" || op.memBaseName == "RBP" ||
                   op.memBaseName == "rsp" || op.memBaseName == "RSP";

    if (isStack) {
      int offset = (int)op.memDisp;
      if (stackVariables_.count(offset)) {
        return stackVariables_[offset].name;
      }
      // Lazy creation for untracked locals
      // createVariable(offset, 4, false);
      // return stackVariables_[offset].name;
    }

    // Handle Global (RIP-relative)
    // IR::Operand sets value to target address for RIP-relative ops
    if (op.name == "RIP" || op.memBaseName == "rip" ||
        op.memBaseName == "RIP") {
      auto symDb = DecompilerContext::Get().GetSymbolDatabase();
      if (symDb) {
        auto sym = symDb->getSymbol(op.value);
        if (sym && sym->type == Analysis::SymbolType::GlobalVariable)
          return sym->name;
        // Don't return labels or functions as variables here, fallback to ptr
        // deref
      }
    }
  }
  return "";
}

} // namespace ShadPKG::Decompiler::Lifter
