#include "CppEmitter.h"
#include "../DecompilerContext.h"
#include "../analysis/SymbolDatabase.h"
#include "../analysis/TypeSystem.h"
#include <iomanip>
#include <functional>

namespace ShadPKG::Decompiler::Codegen {

// Helper to collect all labels from statements recursively
static void collectLabels(const std::shared_ptr<AST::Statement> &stmt, std::set<std::string> &labels) {
  if (!stmt) return;
  if (auto label = std::dynamic_pointer_cast<AST::LabelStatement>(stmt)) {
    labels.insert(label->name);
  } else if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
    for (const auto &s : compound->statements) collectLabels(s, labels);
  } else if (auto ifStmt = std::dynamic_pointer_cast<AST::IfStatement>(stmt)) {
    collectLabels(ifStmt->thenBranch, labels);
    collectLabels(ifStmt->elseBranch, labels);
  } else if (auto whileStmt = std::dynamic_pointer_cast<AST::WhileStatement>(stmt)) {
    collectLabels(whileStmt->body, labels);
  } else if (auto doWhile = std::dynamic_pointer_cast<AST::DoWhileStatement>(stmt)) {
    collectLabels(doWhile->body, labels);
  } else if (auto forStmt = std::dynamic_pointer_cast<AST::ForStatement>(stmt)) {
    collectLabels(forStmt->body, labels);
  } else if (auto switchStmt = std::dynamic_pointer_cast<AST::SwitchStmt>(stmt)) {
    for (const auto &c : switchStmt->cases) collectLabels(c->body, labels);
  }
}

// Helper to filter registers - only keep those actually used in expressions
static void filterUsedRegs(const std::shared_ptr<AST::Statement> &stmt, std::set<std::string> &regs) {
  if (!stmt) return;
  
  std::set<std::string> mentioned;
  
  // Forward declarations for mutual recursion
  std::function<void(const std::shared_ptr<AST::Expression>&)> collectRegsExpr;
  std::function<void(const std::shared_ptr<AST::Statement>&)> collectRegsStmt;
  
  collectRegsExpr = [&](const std::shared_ptr<AST::Expression> &expr) {
    if (!expr) return;
    if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(expr)) {
      if (var->name.find("reg_") == 0 || var->name.find("xmm") == 0) {
        mentioned.insert(var->name);
      }
    } else if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
      collectRegsExpr(bin->left);
      collectRegsExpr(bin->right);
    } else if (auto unary = std::dynamic_pointer_cast<AST::UnaryExpr>(expr)) {
      collectRegsExpr(unary->operand);
    } else if (auto call = std::dynamic_pointer_cast<AST::CallExpr>(expr)) {
      for (const auto &arg : call->arguments) collectRegsExpr(arg);
    } else if (auto cast = std::dynamic_pointer_cast<AST::CastExpr>(expr)) {
      collectRegsExpr(cast->expr);
    }
  };
  
  collectRegsStmt = [&](const std::shared_ptr<AST::Statement> &s) {
    if (!s) return;
    if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(s)) {
      for (const auto &st : compound->statements) collectRegsStmt(st);
    } else if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(s)) {
      collectRegsExpr(exprStmt->expression);
    } else if (auto ifStmt = std::dynamic_pointer_cast<AST::IfStatement>(s)) {
      collectRegsExpr(ifStmt->condition);
      collectRegsStmt(ifStmt->thenBranch);
      collectRegsStmt(ifStmt->elseBranch);
    } else if (auto whileStmt = std::dynamic_pointer_cast<AST::WhileStatement>(s)) {
      collectRegsExpr(whileStmt->condition);
      collectRegsStmt(whileStmt->body);
    } else if (auto doWhile = std::dynamic_pointer_cast<AST::DoWhileStatement>(s)) {
      collectRegsExpr(doWhile->condition);
      collectRegsStmt(doWhile->body);
    } else if (auto forStmt = std::dynamic_pointer_cast<AST::ForStatement>(s)) {
      collectRegsExpr(forStmt->init);
      collectRegsExpr(forStmt->condition);
      collectRegsExpr(forStmt->step);
      collectRegsStmt(forStmt->body);
    } else if (auto ret = std::dynamic_pointer_cast<AST::ReturnStatement>(s)) {
      collectRegsExpr(ret->value);
    }
  };
  
  collectRegsStmt(stmt);
  
  // Keep only registers that are actually mentioned
  std::set<std::string> filtered;
  for (const auto &reg : regs) {
    if (mentioned.count(reg)) {
      filtered.insert(reg);
    }
  }
  regs = filtered;
}

std::string
CppEmitter::generate(const std::shared_ptr<AST::FunctionAST> &func) {
  ss_.str("");
  tokens_.clear();
  usedRegs_.clear();
  emittedLabels_.clear();
  indentLevel_ = 0;
  currentFunc_ = func;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Pre-pass: collect all labels that exist in this function               │
  // └─────────────────────────────────────────────────────────────────────────┘
  if (func->body) {
    for (const auto &stmt : func->body->statements) {
      collectLabels(stmt, emittedLabels_);
    }
  }

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Collect used reg_ variables from the body first                        │
  // └─────────────────────────────────────────────────────────────────────────┘
  if (func->body) {
    for (const auto &stmt : func->body->statements) {
      collectUsedRegs(stmt);
    }
    // Filter to keep only registers actually used in the function body
    if (func->body && !func->body->statements.empty()) {
      filterUsedRegs(func->body, usedRegs_);
    }
  }

  // Signature - always use int64_t return type to match declarations
  std::string returnType = (func->returnType == "void") ? "int64_t" : func->returnType;
  emit(returnType, TokenType::Type);
  emit(" ");
  emit(func->name, TokenType::Function, func->address);
  emit("(");

  // Always use 6 parameters to match declarations
  emit("int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6");

  emit(") {\n");

  indentLevel_++;

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Declare collected reg_ variables                                       │
  // └─────────────────────────────────────────────────────────────────────────┘
  for (const auto &regName : usedRegs_) {
    indent();
    // Choose type based on register prefix
    std::string typeName = "int64_t";
    if (regName.find("xmm") != std::string::npos) {
      typeName = "double";
    } else if (regName.find("reg_e") != std::string::npos ||
               (regName.size() == 3 && regName[0] == 'e') ||
               (regName.size() > 1 && regName.back() == 'd')) {
      typeName = "int32_t"; // e* registers and r*d (r15d) are 32-bit
    }
    emit(typeName, TokenType::Type);
    emit(" ");
    emit(regName, TokenType::Identifier);
    // Initialize registers to 0 to prevent crashes from uninitialized access
    if (typeName == "double") {
      emit(" = 0.0; // register temp\n", TokenType::Comment);
    } else {
      emit(" = 0; // register temp\n", TokenType::Comment);
    }
  }

  if (!usedRegs_.empty()) {
    emit("\n");
    // Initialize argument registers from function parameters (System V AMD64 ABI)
    indent();
    emit("// Initialize registers from arguments (System V AMD64 ABI)\n");
    if (usedRegs_.count("reg_rdi")) { indent(); emit("reg_rdi = a1;\n"); }
    if (usedRegs_.count("reg_rsi")) { indent(); emit("reg_rsi = a2;\n"); }
    if (usedRegs_.count("reg_rdx")) { indent(); emit("reg_rdx = a3;\n"); }
    if (usedRegs_.count("reg_rcx")) { indent(); emit("reg_rcx = a4;\n"); }
    if (usedRegs_.count("reg_r8"))  { indent(); emit("reg_r8 = a5;\n"); }
    if (usedRegs_.count("reg_r9"))  { indent(); emit("reg_r9 = a6;\n"); }
    
    // Initialize other commonly used registers to safe values
    if (usedRegs_.count("reg_rax")) { indent(); emit("reg_rax = (int64_t)&g_ps4_memory[0]; // Safe base pointer\n"); }
    if (usedRegs_.count("reg_rbx")) { indent(); emit("reg_rbx = (int64_t)&g_ps4_memory[0]; // Safe base pointer\n"); }
    if (usedRegs_.count("reg_rbp")) { indent(); emit("reg_rbp = (int64_t)&g_ps4_memory[0]; // Safe base pointer\n"); }
    if (usedRegs_.count("reg_rsp")) { indent(); emit("reg_rsp = (int64_t)&g_ps4_memory[0]; // Safe base pointer\n"); }
    
    emit("\n");
  }

  // Local stack variable declarations
  for (const auto &var : func->locals) {
    indent();
    std::string typeName = "int";
    if (var.complexType)
      typeName = var.complexType->toString();
    emit(typeName, TokenType::Type);
    emit(" ");
    emit(var.name, TokenType::Identifier);
    emit("; ", TokenType::Text);
    emit("// [rbp", TokenType::Comment);
    emit(std::string(var.stackOffset < 0 ? " - " : " + ") + "0x",
         TokenType::Comment);

    std::stringstream hexOff;
    hexOff << std::hex << std::abs(var.stackOffset);
    emit(hexOff.str(), TokenType::Comment);
    emit("]\n", TokenType::Comment);
  }

  if (!func->locals.empty())
    emit("\n");

  // Body
  if (func->body) {
    for (const auto &stmt : func->body->statements) {
      stmt->accept(this);
    }
  }

  indentLevel_--;
  emit("}\n");

  return ss_.str();
}

void CppEmitter::indent() {
  for (int i = 0; i < indentLevel_; ++i)
    emit("    ");
}

void CppEmitter::emit(const std::string &str, TokenType type, uint64_t addr) {
  ss_ << str;
  tokens_.push_back({str, type, addr});
}

void CppEmitter::emitLine(const std::string &str, TokenType type) {
  indent();
  emit(str, type);
  emit("\n");
}

std::string CppEmitter::inferTypeFromInstruction(uint64_t addr) {
  auto irFunc = DecompilerContext::Get().GetFunctionAt(currentFunc_->address);
  if (!irFunc)
    return "int64_t";

  for (const auto &bb : irFunc->basicBlocks) {
    for (const auto &instr : bb->instructions) {
      if (instr.address == addr) {
        std::string disasm = instr.disassembly;
        // SSE/AVX scalar single
        if (disasm.find("ss") != std::string::npos)
          return "float";
        // SSE/AVX scalar double
        if (disasm.find("sd") != std::string::npos)
          return "double";
        // SSE/AVX packed single
        if (disasm.find("ps") != std::string::npos)
          return "float";
        // SSE/AVX packed double
        if (disasm.find("pd") != std::string::npos)
          return "double";

        // Integer sizes
        if (disasm.find("byte ptr") != std::string::npos)
          return "int8_t";
        if (disasm.find("word ptr") != std::string::npos)
          return "int16_t";
        if (disasm.find("dword ptr") != std::string::npos)
          return "int32_t";
        if (disasm.find("qword ptr") != std::string::npos)
          return "int64_t";

        // Fallback to register sizes
        for (const auto &op : instr.operands) {
          if (op.type == IR::Operand::Type::Register) {
            std::string r = op.regName;
            if (r[0] == 'e')
              return "int32_t";
            if (r[0] == 'r')
              return "int64_t";
            if (r.find("xmm") != std::string::npos)
              return "float";
          }
        }
      }
    }
  }
  return "int64_t";
}


// ═══════════════════════════════════════════════════════════════════════════
//  Expressions
// ═══════════════════════════════════════════════════════════════════════════

void CppEmitter::visit(AST::ConstantExpr *node) {
  TokenType type = TokenType::Number;
  if (node->kind == AST::ConstantExpr::Kind::String)
    type = TokenType::String;
  emit(node->toString(), type);
}

void CppEmitter::visit(AST::VariableExpr *node) {
  emit(node->name, TokenType::Identifier);
}

// Helper function for constant folding
static std::shared_ptr<AST::Expression> foldConstants(
    AST::BinaryExpr::Op op,
    std::shared_ptr<AST::Expression> left,
    std::shared_ptr<AST::Expression> right) {
  
  // Only fold if both operands are constants
  auto leftConst = std::dynamic_pointer_cast<AST::ConstantExpr>(left);
  auto rightConst = std::dynamic_pointer_cast<AST::ConstantExpr>(right);
  
  if (!leftConst || !rightConst) {
    return nullptr; // Can't fold
  }
  
  // Only fold integer constants for now
  if (leftConst->kind != AST::ConstantExpr::Kind::Integer ||
      rightConst->kind != AST::ConstantExpr::Kind::Integer) {
    return nullptr;
  }
  
  int64_t lval = leftConst->intValue;
  int64_t rval = rightConst->intValue;
  int64_t result = 0;
  bool canFold = true;
  
  switch (op) {
  case AST::BinaryExpr::Op::Add:
    result = lval + rval;
    break;
  case AST::BinaryExpr::Op::Sub:
    result = lval - rval;
    break;
  case AST::BinaryExpr::Op::Mul:
    result = lval * rval;
    break;
  case AST::BinaryExpr::Op::Div:
    if (rval != 0) result = lval / rval;
    else canFold = false;
    break;
  case AST::BinaryExpr::Op::Mod:
    if (rval != 0) result = lval % rval;
    else canFold = false;
    break;
  case AST::BinaryExpr::Op::And:
    result = lval & rval;
    break;
  case AST::BinaryExpr::Op::Or:
    result = lval | rval;
    break;
  case AST::BinaryExpr::Op::Xor:
    result = lval ^ rval;
    break;
  case AST::BinaryExpr::Op::Shl:
    result = lval << rval;
    break;
  case AST::BinaryExpr::Op::Shr:
    result = lval >> rval;
    break;
  case AST::BinaryExpr::Op::Eq:
    result = (lval == rval) ? 1 : 0;
    break;
  case AST::BinaryExpr::Op::Ne:
    result = (lval != rval) ? 1 : 0;
    break;
  case AST::BinaryExpr::Op::Lt:
    result = (lval < rval) ? 1 : 0;
    break;
  case AST::BinaryExpr::Op::Le:
    result = (lval <= rval) ? 1 : 0;
    break;
  case AST::BinaryExpr::Op::Gt:
    result = (lval > rval) ? 1 : 0;
    break;
  case AST::BinaryExpr::Op::Ge:
    result = (lval >= rval) ? 1 : 0;
    break;
  default:
    canFold = false;
  }
  
  if (canFold) {
    return std::make_shared<AST::ConstantExpr>(result, leftConst->isHex || rightConst->isHex);
  }
  
  return nullptr;
}

void CppEmitter::visit(AST::BinaryExpr *node) {
  // Attempt constant folding first
  if (auto folded = foldConstants(node->op, node->left, node->right)) {
    folded->accept(this);
    return;
  }
  
  // Check for Bitwise operations on doubles/floats (xmm registers)
  if (node->op == AST::BinaryExpr::Op::Xor ||
      node->op == AST::BinaryExpr::Op::And ||
      node->op == AST::BinaryExpr::Op::Or) {
    // Simple heuristic: check if operands are xmm registers
    auto checkXmm = [](std::shared_ptr<AST::Expression> e) {
      if (auto v = std::dynamic_pointer_cast<AST::VariableExpr>(e)) {
        return v->name.find("xmm") != std::string::npos;
      }
      return false;
    };
    if (checkXmm(node->left) || checkXmm(node->right)) {
      // If self-xor (clearing), emit 0.0
      if (node->op == AST::BinaryExpr::Op::Xor &&
          node->left->toString() == node->right->toString()) {
        emit("0.0");
        return;
      }
      // Otherwise emit bitwise cast wrapper using helper functions
      emit("as_double((as_uint64(", TokenType::Text);
      node->left->accept(this);
      emit(") " + AST::BinaryExpr::opToString(node->op) + " as_uint64(",
           TokenType::Text);
      node->right->accept(this);
      emit(")))", TokenType::Text);
      return;
    }
  }

  // For assignments, don't wrap in parentheses
  if (node->op == AST::BinaryExpr::Op::Assign) {
    node->left->accept(this);
    emit(" = ");
    node->right->accept(this);
    return;
  }

  // For simple expressions (variable op constant), skip outer parentheses
  bool leftSimple = std::dynamic_pointer_cast<AST::VariableExpr>(node->left) ||
                    std::dynamic_pointer_cast<AST::ConstantExpr>(node->left);
  bool rightSimple = std::dynamic_pointer_cast<AST::VariableExpr>(node->right) ||
                     std::dynamic_pointer_cast<AST::ConstantExpr>(node->right);
  
  if (leftSimple && rightSimple) {
    node->left->accept(this);
    emit(" " + AST::BinaryExpr::opToString(node->op) + " ");
    node->right->accept(this);
  } else {
    emit("(");
    node->left->accept(this);
    emit(" " + AST::BinaryExpr::opToString(node->op) + " ");
    node->right->accept(this);
    emit(")");
  }
}

void CppEmitter::visit(AST::UnaryExpr *node) {
  if (node->op == AST::UnaryExpr::Op::Deref) {
    emit("*");
    // If unwrapping a register variable, we must cast it to a pointer first
    if (auto var =
            std::dynamic_pointer_cast<AST::VariableExpr>(node->operand)) {
      if (var->name.find("reg_") == 0 || var->name.find("r") == 0 ||
          var->name.find("e") == 0) {
        emit("(int64_t*)");
      }
    }
    emit("(");
    node->operand->accept(this);
    emit(")");
  } else if (node->op == AST::UnaryExpr::Op::Negate) {
    emit("-");
    emit("(");
    node->operand->accept(this);
    emit(")");
  } else if (node->op == AST::UnaryExpr::Op::BitwiseNot) {
    // Check for bitwise not on xmm
    if (auto v = std::dynamic_pointer_cast<AST::VariableExpr>(node->operand)) {
      if (v->name.find("xmm") != std::string::npos) {
        emit("as_double(~as_uint64(", TokenType::Text);
        node->operand->accept(this);
        emit("))", TokenType::Text);
        return;
      }
    }
    emit("~");
    node->operand->accept(this);
  } else {
    emit(AST::UnaryExpr::opToString(node->op));
    node->operand->accept(this);
  }
}

void CppEmitter::visit(AST::CallExpr *node) {
  // Cleanup Assembly instructions if they are wrapped in __asm()
  if (node->functionName == "__asm" && !node->arguments.empty()) {
    if (auto constExpr =
            std::dynamic_pointer_cast<AST::ConstantExpr>(node->arguments[0])) {
      std::string asmCode = constExpr->strValue;

      // 1. Filter LEAVE (stack frame management)
      if (asmCode.find("leave") != std::string::npos) {
        return; // Do not emit anything for leave
      }

      // 2. Translate BSWAP to intrinsic
      if (asmCode.find("bswap") != std::string::npos) {
        emit("_byteswap_ulong", TokenType::Function);
        emit("(");
        // Fallback to whatever register was in the asm if we can't extract it
        // perfectly Here we just use a comment placeholder as before but we are
        // inside the call
        emit("/* " + asmCode + " */", TokenType::Comment);
        emit(")");
        return;
      }

      // 3. Translate FISTTP (FPU to Integer with truncation)
      if (asmCode.find("fisttp") != std::string::npos) {
        emit("(int)", TokenType::Type);
        emit("/* " + asmCode + " */", TokenType::Comment);
        return;
      }

      // 4. Translate vcvttss2si (SSE Float to Int)
      if (asmCode.find("vcvttss2si") != std::string::npos) {
        // Try to identify destination and source from arguments if possible
        // Otherwise emit cast
        emit("(int32_t)", TokenType::Type);
        emit("/* " + asmCode + " */", TokenType::Comment);
        return;
      }
    }
  }

  emit(node->functionName, TokenType::Function, node->targetAddress);
  emit("(");
  for (size_t i = 0; i < node->arguments.size(); ++i) {
    if (i > 0)
      emit(", ");
    node->arguments[i]->accept(this);
  }

  // Pad with zeros if we missed arguments in analysis
  // This ensures compilation against the generated header
  if (node->targetAddress != 0) {
    int expectedParams =
        DecompilerContext::Get().GetFunctionParamCount(node->targetAddress);
    for (size_t i = node->arguments.size(); i < expectedParams; ++i) {
      if (i > 0)
        emit(", ");
      emit("0");
    }
  }

  emit(")");
}

void CppEmitter::visit(AST::MemoryExpr *node) {
  // Check if base is RIP
  bool isRip = false;
  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(node->base)) {
    if (var->name == "rip" || var->name == "RIP") {
      isRip = true;
    }
  }

  if (isRip && node->offset) {
    if (auto cnst =
            std::dynamic_pointer_cast<AST::ConstantExpr>(node->offset)) {
      uint64_t ripNext = node->sourceAddress + 7;
      auto irFunc =
          DecompilerContext::Get().GetFunctionAt(currentFunc_->address);
      if (irFunc) {
        for (const auto &bb : irFunc->basicBlocks) {
          for (size_t i = 0; i < bb->instructions.size(); ++i) {
            if (bb->instructions[i].address == node->sourceAddress) {
              if (i + 1 < bb->instructions.size()) {
                ripNext = bb->instructions[i + 1].address;
              } else {
                ripNext = bb->endAddress;
              }
              goto found_rip_mem;
            }
          }
        }
      }
    found_rip_mem:
      uint64_t targetAddr = ripNext + cnst->intValue;
      auto symDb = DecompilerContext::Get().GetSymbolDatabase();
      std::string symName = symDb ? symDb->getSymbolName(targetAddr) : "";

      if (!symName.empty() && symName.find("var_") == std::string::npos &&
          symName.find("sub_") == std::string::npos &&
          symName.find("loc_") == std::string::npos) {
        emit(symName, TokenType::Global, targetAddr);
      } else {
        // ┌─────────────────────────────────────────────────────────────────────┐
        // │  AUTO-GENERATE global name: g_<type>_<addr> │
        // └─────────────────────────────────────────────────────────────────────┘
        std::string typeName = inferTypeFromInstruction(node->sourceAddress);
        std::stringstream globalName;
        globalName << "g_" << typeName << "_" << std::hex << targetAddr;
        std::string name = globalName.str();

        if (symDb) {
          symDb->addSymbol(targetAddr, name,
                           Analysis::SymbolType::GlobalVariable);
        }

        emit(name, TokenType::Global, targetAddr);
      }
      return;
    }
  }

  emit(node->toString()); // Fallback to basic string repr, could be improved
}

void CppEmitter::visit(AST::MemberAccessExpr *node) {
  // Check if base is RIP
  bool isRip = false;
  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(node->base)) {
    if (var->name == "rip" || var->name == "RIP") {
      isRip = true;
    }
  }

  if (isRip) {
    // Address = RIP_Next + Displacement (node->offset)
    // Heuristic for RIP_Next: sourceAddress + instruction_size.
    // We try to find the actual instruction size from IR if available.
    uint64_t ripNext = node->sourceAddress + 7; // Default fallback for x64

    auto irFunc = DecompilerContext::Get().GetFunctionAt(currentFunc_->address);
    if (irFunc) {
      for (const auto &bb : irFunc->basicBlocks) {
        for (size_t i = 0; i < bb->instructions.size(); ++i) {
          if (bb->instructions[i].address == node->sourceAddress) {
            if (i + 1 < bb->instructions.size()) {
              ripNext = bb->instructions[i + 1].address;
            } else {
              // Last instruction in block, RIP_Next is start of next block or
              // end of current
              ripNext = bb->endAddress;
            }
            goto found_rip;
          }
        }
      }
    }
  found_rip:
    uint64_t targetAddr = ripNext + node->offset;
    auto symDb = DecompilerContext::Get().GetSymbolDatabase();
    std::string symName = symDb ? symDb->getSymbolName(targetAddr) : "";

    if (!symName.empty() && symName.find("var_") == std::string::npos &&
        symName.find("sub_") == std::string::npos) {
      emit(symName, TokenType::Global, targetAddr);
    } else {
      // ┌─────────────────────────────────────────────────────────────────────┐
      // │  AUTO-GENERATE global name: g_<type>_<addr>                         │
      // │  Register in SymbolDatabase for consistent naming                   │
      // └─────────────────────────────────────────────────────────────────────┘
      std::string typeName = inferTypeFromInstruction(node->sourceAddress);
      std::stringstream globalName;
      globalName << "g_" << typeName << "_" << std::hex << targetAddr;
      std::string name = globalName.str();

      // Register for future references
      if (symDb) {
        symDb->addSymbol(targetAddr, name,
                         Analysis::SymbolType::GlobalVariable);
      }

      emit(name, TokenType::Global, targetAddr);
    }
    return;
  }

  // Check if base is a register variable that needs casting
  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(node->base)) {
    std::string name = var->name;
    // Normalize name (remove reg_ prefix if present, though collecting adds it,
    // usage might not?) Actually usage in AST is usually "rbx" or "reg_rbx".
    if (name.find("reg_") == 0)
      name = name.substr(4);

    // Check if it's a standard access register
    if (name == "rax" || name == "rbx" || name == "rcx" || name == "rdx" ||
        name == "rsi" || name == "rdi" || name == "rbp" || name == "rsp" ||
        (name.size() >= 2 && name[0] == 'r' && isdigit(name[1]))) {

      emit("((Struct_", TokenType::Type);
      emit(name, TokenType::Type);
      emit("*)", TokenType::Type);
      node->base->accept(this);
      emit(")", TokenType::Text);
      emit("->", TokenType::Text);
      emit(node->memberName, TokenType::Identifier);
      return;
    }
  }

  node->base->accept(this);
  emit("->", TokenType::Text);
  emit(node->memberName, TokenType::Identifier);
  // Maybe add a comment relative to offset if debug mode is on?
}

void CppEmitter::visit(AST::CastExpr *node) {
  emit("(", TokenType::Text);
  emit(node->targetType, TokenType::Type);
  emit(")", TokenType::Text);
  // Wrap complex expressions in parentheses to ensure correct precedence
  bool needsParens = std::dynamic_pointer_cast<AST::BinaryExpr>(node->expr) ||
                     std::dynamic_pointer_cast<AST::UnaryExpr>(node->expr);
  if (needsParens) emit("(");
  node->expr->accept(this);
  if (needsParens) emit(")");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Statements
// ═══════════════════════════════════════════════════════════════════════════

void CppEmitter::visit(AST::CompoundStatement *node) {
  emitLine("{ ", TokenType::Text);
  indentLevel_++;
  for (const auto &stmt : node->statements) {
    stmt->accept(this);
  }
  indentLevel_--;
  emitLine("}", TokenType::Text);
}

void CppEmitter::visit(AST::ExpressionStatement *node) {
  indent();

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  VALIDATE: Only emit expressions that are valid C++ statements          │
  // │  - Assignments (x = y)                                                  │
  // │  - Function calls (foo())                                               │
  // │  - Other expressions become comments to preserve debug info             │
  // └─────────────────────────────────────────────────────────────────────────┘

  bool isValidStatement = false;

  // Check for function call
  if (std::dynamic_pointer_cast<AST::CallExpr>(node->expression)) {
    isValidStatement = true;
  }
  // Check for assignment expression
  else if (auto binExpr =
               std::dynamic_pointer_cast<AST::BinaryExpr>(node->expression)) {
    if (binExpr->op == AST::BinaryExpr::Op::Assign) {
      isValidStatement = true;
    }
  }
  // Check for cast expression (valid in C++)
  else if (std::dynamic_pointer_cast<AST::CastExpr>(node->expression)) {
    isValidStatement = true;
  }

  if (isValidStatement) {
    node->expression->accept(this);
    emit(";\n", TokenType::Text);
  } else {
    // Emit as comment to preserve debug info without syntax errors
    emit("// ", TokenType::Comment);
    node->expression->accept(this);
    emit(";\n", TokenType::Comment);
  }
}

void CppEmitter::visit(AST::IfStatement *node) {
  indent();
  emit("if", TokenType::Keyword);
  emit(" (", TokenType::Text);
  node->condition->accept(this);
  emit(") {\n", TokenType::Text);

  indentLevel_++;
  if (auto compound =
          std::dynamic_pointer_cast<AST::CompoundStatement>(node->thenBranch)) {
    for (const auto &s : compound->statements)
      s->accept(this);
  } else if (node->thenBranch) {
    node->thenBranch->accept(this);
  }
  indentLevel_--;

  indent();
  emit("}", TokenType::Text);

  if (node->elseBranch) {
    emit(" ", TokenType::Text);
    emit("else", TokenType::Keyword);
    emit(" {\n", TokenType::Text);
    indentLevel_++;
    if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(
            node->elseBranch)) {
      for (const auto &s : compound->statements)
        s->accept(this);
    } else {
      node->elseBranch->accept(this);
    }
    indentLevel_--;
    indent();
    emit("}", TokenType::Text);
  }
  emit("\n", TokenType::Text);
}

void CppEmitter::visit(AST::WhileStatement *node) {
  indent();
  emit("while", TokenType::Keyword);
  emit(" (", TokenType::Text);
  node->condition->accept(this);
  emit(") {\n", TokenType::Text);
  indentLevel_++;
  if (node->body) {
    if (auto compound =
            std::dynamic_pointer_cast<AST::CompoundStatement>(node->body)) {
      for (const auto &s : compound->statements)
        s->accept(this);
    } else {
      node->body->accept(this);
    }
  }
  indentLevel_--;
  emitLine("}", TokenType::Text);
}

void CppEmitter::visit(AST::DoWhileStatement *node) {
  indent();
  emit("do", TokenType::Keyword);
  emit(" {\n", TokenType::Text);
  indentLevel_++;
  if (node->body) {
    if (auto compound =
            std::dynamic_pointer_cast<AST::CompoundStatement>(node->body)) {
      for (const auto &s : compound->statements)
        s->accept(this);
    } else {
      node->body->accept(this);
    }
  }
  indentLevel_--;
  indent();
  emit("} ", TokenType::Text);
  emit("while", TokenType::Keyword);
  emit(" (", TokenType::Text);
  node->condition->accept(this);
  emit(");\n", TokenType::Text);
}

void CppEmitter::visit(AST::ForStatement *node) {
  // Unimplemented basic for
  indent();
  emit("for (...) {}\n");
}

void CppEmitter::visit(AST::ReturnStatement *node) {
  indent();
  emit("return", TokenType::Keyword);
  if (node->value) {
    emit(" ", TokenType::Text);
    node->value->accept(this);
  }
  emit(";\n", TokenType::Text);
}

void CppEmitter::visit(AST::BreakStatement *node) {
  emitLine("break;", TokenType::Keyword);
}

void CppEmitter::visit(AST::ContinueStatement *node) {
  emitLine("continue;", TokenType::Keyword);
}

void CppEmitter::visit(AST::GotoStatement *node) {
  indent();
  // Only emit goto if the target label exists in this function
  if (emittedLabels_.count(node->label)) {
    emit("goto", TokenType::Keyword);
    emit(" ", TokenType::Text);
    emit(node->label, TokenType::Identifier, node->targetAddress);
    emit(";\n", TokenType::Text);
  } else {
    // Label doesn't exist - emit as comment to avoid compilation error
    emit("// goto ", TokenType::Comment);
    emit(node->label, TokenType::Comment);
    emit("; // label not in scope\n", TokenType::Comment);
  }
}

void CppEmitter::visit(AST::LabelStatement *node) {
  // Labels are dedented slightly usually
  // indent();
  emit(node->name, TokenType::Identifier, node->address);
  emit(":\n", TokenType::Text);
}

void CppEmitter::visit(AST::CaseStmt *node) {
  // case val:
  for (auto val : node->values) {
    if (indentLevel_ > 0)
      indentLevel_--; // Case labels are dedented
    indent();
    if (indentLevel_ > 0)
      indentLevel_++; // Restore

    emit("case ", TokenType::Keyword);
    emit(std::to_string(val), TokenType::Number);
    emit(":\n", TokenType::Text);
  }
  if (node->isDefault) {
    if (indentLevel_ > 0)
      indentLevel_--;
    indent();
    if (indentLevel_ > 0)
      indentLevel_++;
    emit("default:\n", TokenType::Keyword);
  }

  // Body (CompoundStatement handles braces, but often cases don't enforce
  // braces in C++) However, our CaseStmt has a CompoundStatement body. Standard
  // C++: case 1: { ... }
  if (node->body) {
    node->body->accept(this);
  }
}

void CppEmitter::visit(AST::SwitchStmt *node) {
  indent();
  emit("switch", TokenType::Keyword);
  emit(" (", TokenType::Text);
  node->condition->accept(this);
  emit(") {\n", TokenType::Text);

  indentLevel_++;
  for (const auto &cse : node->cases) {
    cse->accept(this);
  }
  indentLevel_--;

  indent();
  emit("}\n", TokenType::Text);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Collect Used Register Variables (reg_*, xmm*, unknown_op, g_*)
// ═══════════════════════════════════════════════════════════════════════════
void CppEmitter::collectUsedRegsExpr(
    const std::shared_ptr<AST::Expression> &expr) {
  if (!expr)
    return;

  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(expr)) {
    const std::string &n = var->name;
    // Collect: reg_*, xmm*, unknown_op, and standard registers (rax, rbx, etc.)
    if (n.find("reg_") == 0 || n.find("xmm") == 0 || n == "unknown_op" ||
        n == "rax" || n == "rbx" || n == "rcx" || n == "rdx" || n == "rsi" ||
        n == "rdi" || n == "rbp" || n == "rsp" ||
        (n.size() >= 2 && n[0] == 'r' && isdigit(n[1])) || // r8, r9, r10...
        (n.size() == 3 && n[0] == 'e')                     // eax, ebx...
    ) {
      usedRegs_.insert(n);
    }
  } else if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
    collectUsedRegsExpr(bin->left);
    collectUsedRegsExpr(bin->right);
  } else if (auto unary = std::dynamic_pointer_cast<AST::UnaryExpr>(expr)) {
    collectUsedRegsExpr(unary->operand);
  } else if (auto call = std::dynamic_pointer_cast<AST::CallExpr>(expr)) {
    for (const auto &arg : call->arguments)
      collectUsedRegsExpr(arg);
  } else if (auto mem =
                 std::dynamic_pointer_cast<AST::MemberAccessExpr>(expr)) {
    collectUsedRegsExpr(mem->base);
  } else if (auto memex = std::dynamic_pointer_cast<AST::MemoryExpr>(expr)) {
    collectUsedRegsExpr(memex->base);
    collectUsedRegsExpr(memex->offset);
  } else if (auto cast = std::dynamic_pointer_cast<AST::CastExpr>(expr)) {
    collectUsedRegsExpr(cast->expr);
  }
}

void CppEmitter::collectUsedRegs(const std::shared_ptr<AST::Statement> &stmt) {
  if (!stmt)
    return;

  if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
    for (const auto &s : compound->statements)
      collectUsedRegs(s);
  } else if (auto exprStmt =
                 std::dynamic_pointer_cast<AST::ExpressionStatement>(stmt)) {
    collectUsedRegsExpr(exprStmt->expression);
  } else if (auto ifStmt = std::dynamic_pointer_cast<AST::IfStatement>(stmt)) {
    collectUsedRegsExpr(ifStmt->condition);
    collectUsedRegs(ifStmt->thenBranch);
    collectUsedRegs(ifStmt->elseBranch);
  } else if (auto whileStmt =
                 std::dynamic_pointer_cast<AST::WhileStatement>(stmt)) {
    collectUsedRegsExpr(whileStmt->condition);
    collectUsedRegs(whileStmt->body);
  } else if (auto doWhile =
                 std::dynamic_pointer_cast<AST::DoWhileStatement>(stmt)) {
    collectUsedRegsExpr(doWhile->condition);
    collectUsedRegs(doWhile->body);
  } else if (auto forStmt =
                 std::dynamic_pointer_cast<AST::ForStatement>(stmt)) {
    collectUsedRegsExpr(forStmt->init);
    collectUsedRegsExpr(forStmt->condition);
    collectUsedRegsExpr(forStmt->step);
    collectUsedRegs(forStmt->body);
  } else if (auto ret = std::dynamic_pointer_cast<AST::ReturnStatement>(stmt)) {
    collectUsedRegsExpr(ret->value);
  } else if (auto sw = std::dynamic_pointer_cast<AST::SwitchStmt>(stmt)) {
    collectUsedRegsExpr(sw->condition);
    for (const auto &cs : sw->cases)
      collectUsedRegs(cs);
  } else if (auto cs = std::dynamic_pointer_cast<AST::CaseStmt>(stmt)) {
    // CaseStmt::values is vector<int64_t>, no expressions to check
    if (cs->body) {
      for (const auto &s : cs->body->statements)
        collectUsedRegs(s);
    }
  }
}

} // namespace ShadPKG::Decompiler::Codegen