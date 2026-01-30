#pragma once

#include "../ir/AST.h"
#include "../ir/IR.h"
#include <memory>
#include <set>
#include <string>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          LOOP CORRECTOR                                   ║
// ║                                                                           ║
// ║  Detects and fixes malformed infinite loops in decompiled code:           ║
// ║  - Pattern: (reg_x == reg_x) -> always true                              ║
// ║  - Pattern: register walking loops without proper terminators            ║
// ║  - Pattern: NULL/0xFFFFFFFFFFFFFFFF pointer terminators                  ║
// ║                                                                           ║
// ║  Common Issues:                                                           ║
// ║  1. Loop condition is a tautology (e.g., reg_rax == reg_rax)            ║
// ║  2. Loop has no bounds check before dereference                          ║
// ║  3. Loop should terminate on NULL or sentinel value                      ║
// ║                                                                           ║
// ║  Strategies:                                                              ║
// ║  - Replace with bounded pointer walk (check NULL or 0xffffffff)          ║
// ║  - Add safety counter to prevent runaway loops                           ║
// ║  - Generate proper termination checks                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class LoopCorrector {
public:
  // Analyze and correct a while loop statement
  static std::shared_ptr<AST::Statement>
  correctWhileLoop(const std::shared_ptr<AST::WhileStatement> &whileStmt,
                   const std::shared_ptr<IR::BasicBlock> &headerBB);

  // Analyze and correct a do-while loop statement
  static std::shared_ptr<AST::Statement>
  correctDoWhileLoop(const std::shared_ptr<AST::DoWhileStatement> &doWhileStmt,
                     const std::shared_ptr<IR::BasicBlock> &latchBB);

  // Check if a condition is a tautology (always true/false)
  static bool isTautologyCondition(const std::shared_ptr<AST::Expression> &cond);

  // Check if a condition is a pointer walk pattern
  static bool isPointerWalkPattern(const std::shared_ptr<AST::Expression> &cond,
                                   std::string &baseRegister);

  // Generate a safe bounded loop condition
  static std::shared_ptr<AST::Expression>
  generateBoundedCondition(const std::string &ptrReg, int maxIterations = 1000);

  // Add safety counter variable declaration
  static std::shared_ptr<AST::VariableDecl>
  generateSafetyCounter(const std::string &counterName = "safety");

  // Detect if loop body performs pointer arithmetic (ptr += offset)
  static bool detectsPointerArithmetic(const std::shared_ptr<AST::Statement> &body,
                                      std::string &ptrReg, int64_t &offset);

private:
  // Helper: Check if expr is a self-comparison (a == a)
  static bool isSelfComparison(const std::shared_ptr<AST::Expression> &expr);

  // Helper: Extract variable names from expressions
  static std::set<std::string> extractVariables(const std::shared_ptr<AST::Expression> &expr);

  // Helper: Check for common sentinel values (0, -1, 0xffffffff, etc.)
  static bool isSentinelValue(int64_t value);
};

} // namespace ShadPKG::Decompiler::Analysis
