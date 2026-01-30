#include "LoopCorrector.h"
#include "common/logging/log.h"
#include <algorithm>
#include <sstream>

namespace ShadPKG::Decompiler::Analysis {

bool LoopCorrector::isTautologyCondition(const std::shared_ptr<AST::Expression> &cond) {
  if (!cond)
    return false;

  // Check for self-comparison: (a == a), (a != b where a==b)
  if (isSelfComparison(cond)) {
    LOG_WARN(Decompiler, "Detected tautology condition in loop");
    return true;
  }

  // Check for constant true/false conditions
  if (auto constExpr = std::dynamic_pointer_cast<AST::ConstantExpr>(cond)) {
    // Constants like 1, -1, true are infinite loop conditions
    if (constExpr->value != 0) {
      LOG_WARN(Decompiler, "Detected constant true condition in loop");
      return true;
    }
  }

  return false;
}

bool LoopCorrector::isSelfComparison(const std::shared_ptr<AST::Expression> &expr) {
  if (!expr)
    return false;

  // Check for BinaryExpr like (reg == reg)
  if (auto binExpr = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
    // Check equality/inequality operators
    if (binExpr->op == AST::BinaryExpr::Op::Equal ||
        binExpr->op == AST::BinaryExpr::Op::NotEqual) {
      
      // Extract variable names from both sides
      auto leftVars = extractVariables(binExpr->left);
      auto rightVars = extractVariables(binExpr->right);

      // If both sides use the same variable, it's a self-comparison
      for (const auto &var : leftVars) {
        if (rightVars.count(var) > 0) {
          return true;
        }
      }
    }
  }

  return false;
}

std::set<std::string> LoopCorrector::extractVariables(const std::shared_ptr<AST::Expression> &expr) {
  std::set<std::string> vars;

  if (!expr)
    return vars;

  if (auto varExpr = std::dynamic_pointer_cast<AST::VariableExpr>(expr)) {
    vars.insert(varExpr->name);
  } else if (auto binExpr = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
    auto leftVars = extractVariables(binExpr->left);
    auto rightVars = extractVariables(binExpr->right);
    vars.insert(leftVars.begin(), leftVars.end());
    vars.insert(rightVars.begin(), rightVars.end());
  } else if (auto unaryExpr = std::dynamic_pointer_cast<AST::UnaryExpr>(expr)) {
    auto innerVars = extractVariables(unaryExpr->expr);
    vars.insert(innerVars.begin(), innerVars.end());
  } else if (auto derefExpr = std::dynamic_pointer_cast<AST::DerefExpr>(expr)) {
    auto innerVars = extractVariables(derefExpr->expr);
    vars.insert(innerVars.begin(), innerVars.end());
  } else if (auto memberExpr = std::dynamic_pointer_cast<AST::MemberExpr>(expr)) {
    auto objVars = extractVariables(memberExpr->object);
    vars.insert(objVars.begin(), objVars.end());
  }

  return vars;
}

bool LoopCorrector::isSentinelValue(int64_t value) {
  // Common sentinel values: NULL, -1, max uint64
  return value == 0 || value == -1 || value == 0xffffffffffffffff;
}

bool LoopCorrector::isPointerWalkPattern(const std::shared_ptr<AST::Expression> &cond,
                                        std::string &baseRegister) {
  if (!cond)
    return false;

  // Look for patterns like: ptr != nullptr, ptr != 0, ptr != 0xffffffff
  if (auto binExpr = std::dynamic_pointer_cast<AST::BinaryExpr>(cond)) {
    if (binExpr->op == AST::BinaryExpr::Op::NotEqual ||
        binExpr->op == AST::BinaryExpr::Op::Equal) {
      
      // Check if comparing against constant
      std::shared_ptr<AST::Expression> ptrSide = nullptr;
      std::shared_ptr<AST::ConstantExpr> constSide = nullptr;

      if (auto constRight = std::dynamic_pointer_cast<AST::ConstantExpr>(binExpr->right)) {
        if (isSentinelValue(constRight->value)) {
          ptrSide = binExpr->left;
          constSide = constRight;
        }
      } else if (auto constLeft = std::dynamic_pointer_cast<AST::ConstantExpr>(binExpr->left)) {
        if (isSentinelValue(constLeft->value)) {
          ptrSide = binExpr->right;
          constSide = constLeft;
        }
      }

      // Extract pointer register name
      if (ptrSide) {
        auto vars = extractVariables(ptrSide);
        if (vars.size() == 1) {
          baseRegister = *vars.begin();
          return true;
        }
      }
    }
  }

  return false;
}

bool LoopCorrector::detectsPointerArithmetic(const std::shared_ptr<AST::Statement> &body,
                                            std::string &ptrReg, int64_t &offset) {
  if (!body)
    return false;

  // Check if body contains assignments like: ptr += 8, ptr -= 8, ptr = ptr + constant
  if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(body)) {
    if (auto assignExpr = std::dynamic_pointer_cast<AST::AssignmentExpr>(exprStmt->expr)) {
      // lhs should be a variable
      if (auto lhsVar = std::dynamic_pointer_cast<AST::VariableExpr>(assignExpr->lhs)) {
        ptrReg = lhsVar->name;
        
        // Check if rhs is arithmetic: ptr + constant
        if (auto rhsBin = std::dynamic_pointer_cast<AST::BinaryExpr>(assignExpr->rhs)) {
          if ((rhsBin->op == AST::BinaryExpr::Op::Add ||
               rhsBin->op == AST::BinaryExpr::Op::Sub) &&
              std::dynamic_pointer_cast<AST::ConstantExpr>(rhsBin->right)) {
            offset = std::dynamic_pointer_cast<AST::ConstantExpr>(rhsBin->right)->value;
            if (rhsBin->op == AST::BinaryExpr::Op::Sub)
              offset = -offset;
            return true;
          }
        }
      }
    }
  } else if (auto compoundStmt = std::dynamic_pointer_cast<AST::CompoundStatement>(body)) {
    // Check the statements in compound body
    for (const auto &stmt : compoundStmt->statements) {
      if (detectsPointerArithmetic(stmt, ptrReg, offset)) {
        return true;
      }
    }
  }

  return false;
}

std::shared_ptr<AST::VariableDecl>
LoopCorrector::generateSafetyCounter(const std::string &counterName) {
  auto decl = std::make_shared<AST::VariableDecl>();
  decl->name = counterName;
  decl->type = "int";
  decl->initialValue = std::make_shared<AST::ConstantExpr>(0LL);
  return decl;
}

std::shared_ptr<AST::Expression>
LoopCorrector::generateBoundedCondition(const std::string &ptrReg, int maxIterations) {
  // Generate: (ptr != nullptr && ptr != -1 && ++safety < maxIterations)
  
  // ptr != nullptr
  auto ptrNotNull = std::make_shared<AST::BinaryExpr>(
      AST::BinaryExpr::Op::NotEqual,
      std::make_shared<AST::VariableExpr>(ptrReg),
      std::make_shared<AST::ConstantExpr>(0LL));

  // ptr != -1
  auto ptrNotSentinel = std::make_shared<AST::BinaryExpr>(
      AST::BinaryExpr::Op::NotEqual,
      std::make_shared<AST::VariableExpr>(ptrReg),
      std::make_shared<AST::ConstantExpr>(-1LL));

  // ++safety < maxIterations
  auto safetyCheck = std::make_shared<AST::BinaryExpr>(
      AST::BinaryExpr::Op::LessThan,
      std::make_shared<AST::UnaryExpr>(AST::UnaryExpr::Op::PreIncrement,
                                       std::make_shared<AST::VariableExpr>("safety")),
      std::make_shared<AST::ConstantExpr>((int64_t)maxIterations));

  // Combine: (ptrNotNull && ptrNotSentinel && safetyCheck)
  auto combined1 = std::make_shared<AST::BinaryExpr>(
      AST::BinaryExpr::Op::LogicalAnd, ptrNotNull, ptrNotSentinel);

  auto combined2 = std::make_shared<AST::BinaryExpr>(
      AST::BinaryExpr::Op::LogicalAnd, combined1, safetyCheck);

  return combined2;
}

std::shared_ptr<AST::Statement>
LoopCorrector::correctWhileLoop(const std::shared_ptr<AST::WhileStatement> &whileStmt,
                               const std::shared_ptr<IR::BasicBlock> &headerBB) {
  if (!whileStmt || !whileStmt->condition)
    return whileStmt;

  // Check for tautology
  if (isTautologyCondition(whileStmt->condition)) {
    LOG_INFO(Decompiler, "Correcting infinite while loop with tautology condition");

    std::string ptrReg;
    // Try to detect pointer walk pattern and generate proper condition
    if (isPointerWalkPattern(whileStmt->condition, ptrReg)) {
      LOG_INFO(Decompiler, "Detected pointer walk pattern on register: {}", ptrReg);
      
      auto boundedCond = generateBoundedCondition(ptrReg, 1000);
      auto correctedStmt = std::make_shared<AST::WhileStatement>(boundedCond, whileStmt->body);
      return correctedStmt;
    } else {
      // Generic tautology fix: replace with bounded loop on internal counter
      auto boundedCond = generateBoundedCondition("reg_rax", 1000);
      auto correctedStmt = std::make_shared<AST::WhileStatement>(boundedCond, whileStmt->body);
      return correctedStmt;
    }
  }

  // Check if loop body has pointer arithmetic
  std::string ptrReg;
  int64_t offset;
  if (detectsPointerArithmetic(whileStmt->body, ptrReg, offset)) {
    LOG_INFO(Decompiler, "Detected pointer arithmetic in loop body: {} += {}", ptrReg, offset);
    
    // Check if condition already validates the pointer
    if (!isPointerWalkPattern(whileStmt->condition, ptrReg)) {
      // Add bounds checking to the existing condition
      auto boundedCond = generateBoundedCondition(ptrReg, 1000);
      auto correctedStmt = std::make_shared<AST::WhileStatement>(boundedCond, whileStmt->body);
      return correctedStmt;
    }
  }

  // No correction needed
  return whileStmt;
}

std::shared_ptr<AST::Statement>
LoopCorrector::correctDoWhileLoop(const std::shared_ptr<AST::DoWhileStatement> &doWhileStmt,
                                 const std::shared_ptr<IR::BasicBlock> &latchBB) {
  if (!doWhileStmt || !doWhileStmt->condition)
    return doWhileStmt;

  // Check for tautology
  if (isTautologyCondition(doWhileStmt->condition)) {
    LOG_INFO(Decompiler, "Correcting infinite do-while loop with tautology condition");

    std::string ptrReg;
    if (isPointerWalkPattern(doWhileStmt->condition, ptrReg)) {
      LOG_INFO(Decompiler, "Detected pointer walk pattern on register: {}", ptrReg);
      
      auto boundedCond = generateBoundedCondition(ptrReg, 1000);
      auto correctedStmt = std::make_shared<AST::DoWhileStatement>(doWhileStmt->body, boundedCond);
      return correctedStmt;
    } else {
      auto boundedCond = generateBoundedCondition("reg_rax", 1000);
      auto correctedStmt = std::make_shared<AST::DoWhileStatement>(doWhileStmt->body, boundedCond);
      return correctedStmt;
    }
  }

  // Check if loop body has pointer arithmetic
  std::string ptrReg;
  int64_t offset;
  if (detectsPointerArithmetic(doWhileStmt->body, ptrReg, offset)) {
    LOG_INFO(Decompiler, "Detected pointer arithmetic in do-while body: {} += {}", ptrReg, offset);
    
    if (!isPointerWalkPattern(doWhileStmt->condition, ptrReg)) {
      auto boundedCond = generateBoundedCondition(ptrReg, 1000);
      auto correctedStmt = std::make_shared<AST::DoWhileStatement>(doWhileStmt->body, boundedCond);
      return correctedStmt;
    }
  }

  return doWhileStmt;
}

} // namespace ShadPKG::Decompiler::Analysis
