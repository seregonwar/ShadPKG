#include "StructuralAnalysis.h"
#include <iostream>

namespace ShadPKG::Decompiler::Analysis {

StructuralAnalysis::StructuralAnalysis(
    std::shared_ptr<IR::Function> func,
    std::shared_ptr<DominatorAnalysis> domAnalysis,
    std::shared_ptr<SymbolAnalysis> symbolAnalysis,
    std::shared_ptr<Lifter::VariableAnalysis> varAnalysis)
    : func_(func), dom_(domAnalysis), symbols_(symbolAnalysis), vars_(varAnalysis) {}

std::shared_ptr<AST::FunctionAST> StructuralAnalysis::analyze() {
  auto ast = std::make_shared<AST::FunctionAST>();
  ast->name = func_->name;
  ast->address = func_->address;

  if (!func_->basicBlocks.empty()) {
    uint64_t entry = func_->basicBlocks[0]->id;
    auto stmt = structureRegion(entry, 0);

    if (stmt) {
      if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
        ast->body = compound;
      } else {
        ast->body->addStatement(stmt);
      }
    }
  }

  return ast;
}

std::shared_ptr<IR::BasicBlock> StructuralAnalysis::getBlock(uint64_t id) {
  for (const auto &bb : func_->basicBlocks) {
    if (bb->id == id)
      return bb;
  }
  return nullptr;
}

std::shared_ptr<AST::Statement>
StructuralAnalysis::structureRegion(uint64_t entryBlock, uint64_t stopBlock) {
  if (entryBlock == 0 || entryBlock == stopBlock)
    return nullptr;

  auto sequence = std::make_shared<AST::CompoundStatement>();
  uint64_t current = entryBlock;

  while (current != 0 && current != stopBlock) {
    if (structuredBlocks_.count(current)) {
      sequence->addStatement(std::make_shared<AST::GotoStatement>(current));
      break;
    }
    structuredBlocks_.insert(current);

    if (dom_->isLoopHeader(current)) {
      auto loopStmt = matchLoop(current, stopBlock);
      if (loopStmt) {
        sequence->addStatement(loopStmt);
        const auto *loopInfo = dom_->getLoopFor(current);
        uint64_t ipdom = dom_->getImmediatePostDominator(current);
        while (ipdom != 0 && loopInfo->contains(ipdom)) {
          ipdom = dom_->getImmediatePostDominator(ipdom);
        }
        current = ipdom;
        continue;
      }
    }

    auto bb = getBlock(current);
    if (bb && bb->successors.size() == 2) {
      auto ifStmt = matchIf(current, stopBlock);
      if (ifStmt) {
        sequence->addStatement(ifStmt);
        uint64_t ipdom = dom_->getImmediatePostDominator(current);
        current = ipdom;
        continue;
      }
    }

    if (bb && bb->successors.size() > 2) {
      auto switchStmt = matchSwitch(current, stopBlock);
      if (switchStmt) {
        sequence->addStatement(switchStmt);
        uint64_t ipdom = dom_->getImmediatePostDominator(current);
        current = ipdom;
        continue;
      }
    }

    if (bb) {
      auto blockStmt = structureBlock(bb);
      for (auto &s : blockStmt->statements) {
        sequence->addStatement(s);
      }
      if (bb->successors.empty()) {
        current = 0;
      } else if (bb->successors.size() == 1) {
        current = bb->successors[0];
      } else {
        current = 0;
      }
    } else {
      break;
    }
  }

  if (sequence->statements.size() == 1) {
    return sequence->statements[0];
  }

  return sequence;
}

std::shared_ptr<AST::Statement>
StructuralAnalysis::matchLoop(uint64_t header, uint64_t stopBlock) {
  const auto *loopInfo = dom_->getLoopFor(header);
  if (!loopInfo)
    return nullptr;

  auto headerBB = getBlock(header);

  if (loopInfo->isDoWhile) {
    auto body = std::make_shared<AST::CompoundStatement>();
    body->addStatement(std::make_shared<AST::ExpressionStatement>(
        std::make_shared<AST::ConstantExpr>((int64_t)0)));
    auto cond = std::make_shared<AST::VariableExpr>("cond");
    return std::make_shared<AST::DoWhileStatement>(body, cond);
  } else {
    if (headerBB->successors.size() == 2) {
      uint64_t trueSucc = headerBB->successors[0];
      uint64_t falseSucc = headerBB->successors[1];
      bool inverted = false;
      if (!loopInfo->contains(trueSucc)) {
        std::swap(trueSucc, falseSucc);
        inverted = true;
      }
      auto condition = extractCondition(headerBB, inverted);
      auto bodyStmt = structureRegion(trueSucc, header);
      return std::make_shared<AST::WhileStatement>(condition, bodyStmt);
    }
  }

  return nullptr;
}

std::shared_ptr<AST::Statement>
StructuralAnalysis::matchIf(uint64_t block, uint64_t stopBlock) {
  auto bb = getBlock(block);
  uint64_t trueSucc = bb->successors[0];
  uint64_t falseSucc = bb->successors[1];
  uint64_t ipdom = dom_->getImmediatePostDominator(block);
  auto condition = extractCondition(bb, false);

  if (falseSucc == ipdom) {
    auto thenStmt = structureRegion(trueSucc, ipdom);
    return std::make_shared<AST::IfStatement>(condition, thenStmt, nullptr);
  }

  if (trueSucc == ipdom) {
    auto invertedCond = std::make_shared<AST::UnaryExpr>(AST::UnaryExpr::Op::Not, condition);
    auto thenStmt = structureRegion(falseSucc, ipdom);
    return std::make_shared<AST::IfStatement>(invertedCond, thenStmt, nullptr);
  }

  auto thenStmt = structureRegion(trueSucc, ipdom);
  auto elseStmt = structureRegion(falseSucc, ipdom);
  return std::make_shared<AST::IfStatement>(condition, thenStmt, elseStmt);
}

std::shared_ptr<AST::Statement>
StructuralAnalysis::matchSwitch(uint64_t block, uint64_t stopBlock) {
  auto bb = getBlock(block);
  if (!bb)
    return nullptr;

  uint64_t ipdom = dom_->getImmediatePostDominator(block);
  if (ipdom == 0)
    ipdom = stopBlock;

  auto condition = std::make_shared<AST::VariableExpr>("switch_val");
  auto switchStmt = std::make_shared<AST::SwitchStmt>(condition);
  std::map<uint64_t, std::vector<int64_t>> targetToValues;
  for (const auto &[val, target] : bb->switchMap) {
    targetToValues[target].push_back(val);
  }

  for (const auto &[targetId, values] : targetToValues) {
    auto caseStmt = std::make_shared<AST::CaseStmt>();
    caseStmt->values = values;
    auto stmt = structureRegion(targetId, ipdom);
    if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
      caseStmt->body = compound;
    } else {
      caseStmt->body = std::make_shared<AST::CompoundStatement>();
      if (stmt)
        caseStmt->body->addStatement(stmt);
    }
    caseStmt->body->addStatement(std::make_shared<AST::BreakStatement>());
    switchStmt->cases.push_back(caseStmt);
  }

  return switchStmt;
}

std::shared_ptr<AST::CompoundStatement>
StructuralAnalysis::structureBlock(const std::shared_ptr<IR::BasicBlock> &bb) {
  auto compound = std::make_shared<AST::CompoundStatement>();
  
  // Local state for call argument detection
  std::map<std::string, std::shared_ptr<AST::Expression>> regValues;

  for (size_t i = 0; i < bb->instructions.size(); ++i) {
    const auto &instr = bb->instructions[i];
    bool isLast = (i == bb->instructions.size() - 1);
    if (isLast) {
      if (instr.opcode >= IR::OpCode::JMP && instr.opcode <= IR::OpCode::RET) {
        continue;
      }
    }
    if (i + 1 < bb->instructions.size()) {
      const auto &next = bb->instructions[i + 1];
      if ((instr.opcode == IR::OpCode::CMP || instr.opcode == IR::OpCode::AND) &&
          (next.opcode >= IR::OpCode::JE && next.opcode <= IR::OpCode::JL)) {
        continue;
      }
    }

    auto astStmt = liftInstruction(instr);
    if (astStmt) {
      astStmt->sourceAddress = instr.address;
      astStmt->comment = instr.disassembly;
      compound->addStatement(astStmt);
      
      // Track assignments for call argument detection
      if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(astStmt)) {
          if (auto binExpr = std::dynamic_pointer_cast<AST::BinaryExpr>(exprStmt->expression)) {
              if (binExpr->op == AST::BinaryExpr::Op::Assign) {
                  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(binExpr->left)) {
                      regValues[var->name] = binExpr->right;
                  }
              }
          }
      }
      
      // Special handling for CALL: inject arguments if detected
      if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(astStmt)) {
          if (auto call = std::dynamic_pointer_cast<AST::CallExpr>(exprStmt->expression)) {
              static const std::vector<std::string> paramRegs = {
                  "rdi", "rsi", "rdx", "rcx", "r8", "r9",
                  "edi", "esi", "edx", "ecx", "r8d", "r9d",
                  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
              };
              for (const auto& rName : paramRegs) {
                  if (regValues.count(rName)) {
                      call->arguments.push_back(regValues[rName]);
                  }
              }
          }
      }
    }
  }

  return compound;
}

std::shared_ptr<AST::Expression>
StructuralAnalysis::extractCondition(const std::shared_ptr<IR::BasicBlock> &bb, bool invert) {
  if (bb->instructions.empty())
    return std::make_shared<AST::ConstantExpr>((int64_t)1);

  const auto &last = bb->instructions.back();
  const IR::Instruction *flagSetter = nullptr;
  for (auto it = bb->instructions.rbegin(); it != bb->instructions.rend(); ++it) {
    if (it->opcode == IR::OpCode::CMP || it->opcode == IR::OpCode::AND) {
      flagSetter = &(*it);
      break;
    }
  }

  if (!flagSetter)
    return std::make_shared<AST::VariableExpr>("cond");

  std::shared_ptr<AST::Expression> left = std::make_shared<AST::VariableExpr>("var_xx");
  std::shared_ptr<AST::Expression> right = std::make_shared<AST::VariableExpr>("var_yy");

  if (flagSetter->operands.size() >= 2) {
    auto liftOperand = [&](const IR::Operand &op) -> std::shared_ptr<AST::Expression> {
        if (op.type == IR::Operand::Type::Register) {
            if (vars_) {
                std::string pName = vars_->getParamForRegister(op.value);
                if (!pName.empty()) return std::make_shared<AST::VariableExpr>(pName);
            }
            return std::make_shared<AST::VariableExpr>(op.regName.empty() ? "reg_" + std::to_string(op.value) : op.regName);
        }
        if (op.type == IR::Operand::Type::Immediate)
            return std::make_shared<AST::ConstantExpr>((int64_t)op.value, true);
        return std::make_shared<AST::VariableExpr>("?");
    };
    left = liftOperand(flagSetter->operands[0]);
    right = liftOperand(flagSetter->operands[1]);
  }

  AST::BinaryExpr::Op op = AST::BinaryExpr::Op::Eq;
  switch (last.opcode) {
  case IR::OpCode::JE: op = AST::BinaryExpr::Op::Eq; break;
  case IR::OpCode::JNE: op = AST::BinaryExpr::Op::Ne; break;
  case IR::OpCode::JG: op = AST::BinaryExpr::Op::Gt; break;
  case IR::OpCode::JL: op = AST::BinaryExpr::Op::Lt; break;
  default: break;
  }

  if (invert) {
    switch (op) {
    case AST::BinaryExpr::Op::Eq: op = AST::BinaryExpr::Op::Ne; break;
    case AST::BinaryExpr::Op::Ne: op = AST::BinaryExpr::Op::Eq; break;
    case AST::BinaryExpr::Op::Gt: op = AST::BinaryExpr::Op::Le; break;
    case AST::BinaryExpr::Op::Lt: op = AST::BinaryExpr::Op::Ge; break;
    default: break;
    }
  }

  return std::make_shared<AST::BinaryExpr>(op, left, right);
}

std::shared_ptr<AST::Statement>
StructuralAnalysis::liftInstruction(const IR::Instruction &instr) {
  auto liftOp = [&](const IR::Operand &op) -> std::shared_ptr<AST::Expression> {
    if (op.type == IR::Operand::Type::Register) {
      if (vars_) {
          std::string pName = vars_->getParamForRegister(op.value);
          if (!pName.empty()) return std::make_shared<AST::VariableExpr>(pName);
      }
      return std::make_shared<AST::VariableExpr>(op.regName.empty() ? "reg_" + std::to_string(op.value) : op.regName);
    }
    if (op.type == IR::Operand::Type::Immediate) {
      if (symbols_) {
        auto str = symbols_->getStringLiteral(op.value);
        if (str) return std::make_shared<AST::ConstantExpr>("\"" + *str + "\"");
      }
      return std::make_shared<AST::ConstantExpr>((int64_t)op.value, true);
    }
    if (op.type == IR::Operand::Type::Memory) {
      if (op.value != 0 && symbols_) {
          auto str = symbols_->getStringLiteral(op.value);
          if (str) return std::make_shared<AST::ConstantExpr>("\"" + *str + "\"");
      }
      if (vars_) {
          std::string varName = vars_->resolveVariable(op);
          if (!varName.empty()) return std::make_shared<AST::VariableExpr>(varName);
      }

      // RIP-relative access: global variable or constant
      if (op.memBaseName == "rip" && op.value != 0) {
          if (symbols_) {
              auto symInfo = symbols_->getSymbol(op.value);
              if (symInfo) {
                  return std::make_shared<AST::VariableExpr>(symInfo->name);
              }
          }
          // Use proper MemoryExpr so CppEmitter can handle it with type inference
          auto base = std::make_shared<AST::VariableExpr>("rip");
          auto mem = std::make_shared<AST::MemoryExpr>(base);
          // For RIP-relative, we use the displacement from the operand
          mem->offset = std::make_shared<AST::ConstantExpr>((int64_t)op.memDisp);
          mem->sourceAddress = instr.address;
          return mem;
      }


      if (!op.memBaseName.empty()) {
          auto baseReg = std::make_shared<AST::VariableExpr>(op.memBaseName);
          auto memExpr = std::make_shared<AST::MemoryExpr>(baseReg);
          if (op.memDisp != 0) memExpr->offset = std::make_shared<AST::ConstantExpr>((int64_t)op.memDisp);
          return memExpr;
      }
      return std::make_shared<AST::VariableExpr>("MEM");
    }
    return std::make_shared<AST::VariableExpr>("?");
  };

  if (instr.opcode == IR::OpCode::PUSH || instr.opcode == IR::OpCode::POP) return nullptr;

  if (instr.opcode == IR::OpCode::LEAVE) return nullptr; // Remove leave instruction

  if (instr.opcode == IR::OpCode::ADD || instr.opcode == IR::OpCode::SUB) {
      if (instr.operands.size() >= 1 && instr.operands[0].type == IR::Operand::Type::Register) {
          if (instr.operands[0].regName == "rsp") return nullptr;
      }
  }

  if (instr.opcode == IR::OpCode::MOV || instr.opcode == IR::OpCode::LEA) {
    if (instr.operands.size() >= 2) {
      auto left = liftOp(instr.operands[0]);
      auto right = liftOp(instr.operands[1]);
      bool isLValue = std::dynamic_pointer_cast<AST::VariableExpr>(left) || 
                      std::dynamic_pointer_cast<AST::MemoryExpr>(left);
      if (isLValue) {
          return std::make_shared<AST::ExpressionStatement>(
              std::make_shared<AST::BinaryExpr>(AST::BinaryExpr::Op::Assign, left, right));
      }
    }
  }

  if (instr.opcode == IR::OpCode::MOVSX || instr.opcode == IR::OpCode::MOVZX) {
      std::string castType = (instr.opcode == IR::OpCode::MOVSX) ? "int64_t" : "uint64_t";
      if (instr.operands.size() >= 2) {
          auto left = liftOp(instr.operands[0]);
          auto right = liftOp(instr.operands[1]);
          auto castExpr = std::make_shared<AST::CastExpr>(right, castType);
          return std::make_shared<AST::ExpressionStatement>(
              std::make_shared<AST::BinaryExpr>(AST::BinaryExpr::Op::Assign, left, castExpr));
      }
  }

  if (instr.opcode == IR::OpCode::BSWAP) {
      if (instr.operands.size() == 1) {
          auto operand = liftOp(instr.operands[0]);
          auto call = std::make_shared<AST::CallExpr>("_byteswap_uint64"); // Assuming 64-bit for now
          call->arguments.push_back(operand);
          return std::make_shared<AST::ExpressionStatement>(
              std::make_shared<AST::BinaryExpr>(AST::BinaryExpr::Op::Assign, operand, call));
      }
  }

  if (instr.opcode == IR::OpCode::FISTTP) {
      if (instr.operands.size() == 1) {
          auto operand = liftOp(instr.operands[0]);
          std::string castType = "int"; // Needs better type inference
          auto castExpr = std::make_shared<AST::CastExpr>(operand, castType);
          return std::make_shared<AST::ExpressionStatement>(castExpr);
      }
  }

  auto liftBinary = [&](AST::BinaryExpr::Op op) -> std::shared_ptr<AST::Statement> {
      if (instr.operands.size() == 3) {
          auto dst = liftOp(instr.operands[0]);
          auto src1 = liftOp(instr.operands[1]);
          auto src2 = liftOp(instr.operands[2]);
          auto expr = std::make_shared<AST::BinaryExpr>(op, src1, src2);
          auto assign = std::make_shared<AST::BinaryExpr>(AST::BinaryExpr::Op::Assign, dst, expr);
          return std::make_shared<AST::ExpressionStatement>(assign);
      } else if (instr.operands.size() == 2) {
          auto dst = liftOp(instr.operands[0]);
          auto src = liftOp(instr.operands[1]);
          auto expr = std::make_shared<AST::BinaryExpr>(op, dst, src);
          auto assign = std::make_shared<AST::BinaryExpr>(AST::BinaryExpr::Op::Assign, dst, expr);
          return std::make_shared<AST::ExpressionStatement>(assign);
      }
      return nullptr;
  };

  switch (instr.opcode) {
      case IR::OpCode::ADD: return liftBinary(AST::BinaryExpr::Op::Add);
      case IR::OpCode::SUB: return liftBinary(AST::BinaryExpr::Op::Sub);
      case IR::OpCode::AND: return liftBinary(AST::BinaryExpr::Op::And);
      case IR::OpCode::OR:  return liftBinary(AST::BinaryExpr::Op::Or);
      case IR::OpCode::XOR: return liftBinary(AST::BinaryExpr::Op::Xor);
      case IR::OpCode::SHL: return liftBinary(AST::BinaryExpr::Op::Shl);
      case IR::OpCode::SHR: return liftBinary(AST::BinaryExpr::Op::Shr);
      case IR::OpCode::MUL: return liftBinary(AST::BinaryExpr::Op::Mul);
      case IR::OpCode::DIV: return liftBinary(AST::BinaryExpr::Op::Div);
      default: break;
  }

  if (instr.opcode == IR::OpCode::CALL) {
    uint64_t target = 0;
    if (!instr.operands.empty() && instr.operands[0].type == IR::Operand::Type::Immediate) {
      target = instr.operands[0].value;
      std::string funcName = "";
      if (symbols_) {
        auto parsedName = symbols_->getFunctionName(target);
        if (parsedName) funcName = *parsedName;
      }
      if (funcName.empty()) {
          std::stringstream ss;
          ss << "sub_" << std::hex << target;
          funcName = ss.str();
      }
      auto callExpr = std::make_shared<AST::CallExpr>(target);
      callExpr->functionName = funcName;

      return std::make_shared<AST::ExpressionStatement>(callExpr);
    }
  }

  auto asmCall = std::make_shared<AST::CallExpr>("__asm");
  asmCall->arguments.push_back(std::make_shared<AST::ConstantExpr>(instr.disassembly));
  return std::make_shared<AST::ExpressionStatement>(asmCall);
}

} // namespace ShadPKG::Decompiler::Analysis