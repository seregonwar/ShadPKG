#include "CppEmitter.h"
#include "../analysis/TypeSystem.h"
#include "../DecompilerContext.h"
#include "../analysis/SymbolDatabase.h"
#include <iomanip>

namespace ShadPKG::Decompiler::Codegen {

std::string
CppEmitter::generate(const std::shared_ptr<AST::FunctionAST> &func) {
  ss_.str("");
  tokens_.clear();
  indentLevel_ = 0;
  currentFunc_ = func;

  // Signature
  emit(func->returnType, TokenType::Type);
  emit(" ");
  emit(func->name, TokenType::Function, func->address);
  emit("(");
  
  for (size_t i = 0; i < func->parameters.size(); ++i) {
      if (i > 0) emit(", ");
      const auto& p = func->parameters[i];
      emit(p.getTypeName(), TokenType::Type);
      emit(" ");
      emit(p.name, TokenType::Identifier);
  }

  emit(") {\n");

  indentLevel_++;

  // Variable Declarations
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
    if (!irFunc) return "int64_t";
    
    for (const auto& bb : irFunc->basicBlocks) {
        for (const auto& instr : bb->instructions) {
            if (instr.address == addr) {
                std::string disasm = instr.disassembly;
                // SSE/AVX scalar single
                if (disasm.find("ss") != std::string::npos) return "float";
                // SSE/AVX scalar double
                if (disasm.find("sd") != std::string::npos) return "double";
                // SSE/AVX packed single
                if (disasm.find("ps") != std::string::npos) return "float";
                // SSE/AVX packed double
                if (disasm.find("pd") != std::string::npos) return "double";
                
                // Integer sizes
                if (disasm.find("byte ptr") != std::string::npos) return "int8_t";
                if (disasm.find("word ptr") != std::string::npos) return "int16_t";
                if (disasm.find("dword ptr") != std::string::npos) return "int32_t";
                if (disasm.find("qword ptr") != std::string::npos) return "int64_t";
                
                // Fallback to register sizes
                for (const auto& op : instr.operands) {
                    if (op.type == IR::Operand::Type::Register) {
                        std::string r = op.regName;
                        if (r[0] == 'e') return "int32_t";
                        if (r[0] == 'r') return "int64_t";
                        if (r.find("xmm") != std::string::npos) return "float";
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

void CppEmitter::visit(AST::BinaryExpr *node) {
  emit("(");
  node->left->accept(this);
  emit(" " + AST::BinaryExpr::opToString(node->op) + " ");
  node->right->accept(this);
  emit(")");
}

void CppEmitter::visit(AST::UnaryExpr *node) {
  emit(AST::UnaryExpr::opToString(node->op));
  emit("(");
  node->operand->accept(this);
  emit(")");
}

void CppEmitter::visit(AST::CallExpr *node) {
  // Cleanup Assembly instructions if they are wrapped in __asm()
  if (node->functionName == "__asm" && !node->arguments.empty()) {
      if (auto constExpr = std::dynamic_pointer_cast<AST::ConstantExpr>(node->arguments[0])) {
          std::string asmCode = constExpr->strValue;
          
          // 1. Filter LEAVE (stack frame management)
          if (asmCode.find("leave") != std::string::npos) {
              return; // Do not emit anything for leave
          }

          // 2. Translate BSWAP to intrinsic
          if (asmCode.find("bswap") != std::string::npos) {
              emit("_byteswap_ulong", TokenType::Function);
              emit("(");
              // Fallback to whatever register was in the asm if we can't extract it perfectly
              // Here we just use a comment placeholder as before but we are inside the call
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
      if (auto cnst = std::dynamic_pointer_cast<AST::ConstantExpr>(node->offset)) {
          uint64_t ripNext = node->sourceAddress + 7;
          auto irFunc = DecompilerContext::Get().GetFunctionAt(currentFunc_->address);
          if (irFunc) {
              for (const auto& bb : irFunc->basicBlocks) {
                  for (size_t i = 0; i < bb->instructions.size(); ++i) {
                      if (bb->instructions[i].address == node->sourceAddress) {
                          if (i + 1 < bb->instructions.size()) {
                              ripNext = bb->instructions[i+1].address;
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

          if (!symName.empty() && symName.find("var_") == std::string::npos && symName.find("sub_") == std::string::npos) {
              emit("/* rip-> */", TokenType::Comment);
              emit(symName, TokenType::Global, targetAddr);
          } else {
              std::string typeName = inferTypeFromInstruction(node->sourceAddress);
              emit("*(" + typeName + "*)0x", TokenType::Text);
              std::stringstream ssAddr;
              ssAddr << std::hex << targetAddr;
              emit(ssAddr.str(), TokenType::Number, targetAddr);
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
          for (const auto& bb : irFunc->basicBlocks) {
              for (size_t i = 0; i < bb->instructions.size(); ++i) {
                  if (bb->instructions[i].address == node->sourceAddress) {
                      if (i + 1 < bb->instructions.size()) {
                          ripNext = bb->instructions[i+1].address;
                      } else {
                          // Last instruction in block, RIP_Next is start of next block or end of current
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

      if (!symName.empty() && symName.find("var_") == std::string::npos && symName.find("sub_") == std::string::npos) {
          emit("/* rip-> */", TokenType::Comment);
          emit(symName, TokenType::Global, targetAddr);
      } else {
          // Fallback: Typed global access
          std::string typeName = inferTypeFromInstruction(node->sourceAddress);
          emit("*(" + typeName + "*)0x", TokenType::Text);
          std::stringstream ssAddr;
          ssAddr << std::hex << targetAddr;
          emit(ssAddr.str(), TokenType::Number, targetAddr);
      }
      return;
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
  node->expr->accept(this);
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
  node->expression->accept(this);
  emit(";\n", TokenType::Text);
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
  emit("goto", TokenType::Keyword);
  emit(" ", TokenType::Text);
  emit(node->label, TokenType::Identifier, node->targetAddress);
  emit(";\n", TokenType::Text);
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

} // namespace ShadPKG::Decompiler::Codegen