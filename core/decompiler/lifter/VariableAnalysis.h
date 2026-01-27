#pragma once

#include "../ir/AST.h"
#include "../ir/IR.h"
#include <map>
#include <memory>
#include <set>

namespace ShadPKG::Decompiler::Lifter {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           VARIABLE ANALYSIS                               ║
// ║                                                                           ║
// ║  Scans the function for stack frame accesses (RBP/RSP based).             ║
// ║  Creates named LocalVariables (var_4, var_8).                             ║
// ║  Replaces low-level Memory Expressions with higher-level Variable
// Expressions.║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class VariableAnalysis {
public:
  explicit VariableAnalysis(std::shared_ptr<IR::Function> func);

  // ═══════════════════════════════════════════════════════════════════════
  //  Main Analysis
  // ═══════════════════════════════════════════════════════════════════════

  void analyze();

  // ═══════════════════════════════════════════════════════════════════════
  //  Lifting
  // ═══════════════════════════════════════════════════════════════════════

  // Updates the FunctionAST with detected locals and parameters
  void applyToAST(std::shared_ptr<AST::FunctionAST> ast);

  // Inferred Signature
  const std::vector<AST::LocalVariable>& getDetectedParams() const { return detectedParams_; }
  std::string getInferredReturnType() const { return inferredReturnType_; }
  std::string getParamForRegister(uint64_t regId) const;

  // Resolves a memory operand to a variable name if possible
  // Returns empty string if not a known variable
  std::string resolveVariable(const IR::Operand &op);

private:
  std::shared_ptr<IR::Function> func_;

  // Stack Offset -> LocalVariable
  // Offset is usually negative for locals (rbp - x) and positive for args (rbp
  // + x)
  std::map<int, AST::LocalVariable> stackVariables_;
  std::vector<AST::LocalVariable> detectedParams_;
  std::string inferredReturnType_ = "void";

  // Registers used for passing arguments (System V AMD64 ABI)
  // RDI, RSI, RDX, RCX, R8, R9
  std::map<uint64_t, int> registerParams_; // RegID -> ParamIndex
  std::set<uint64_t> assignedRegisters_;   // To detect use-before-def
  std::map<uint64_t, std::string> regToParam_;

  void scanInstruction(const IR::Instruction &instr);
  void createVariable(int offset, int size, bool isParam);
};

} // namespace ShadPKG::Decompiler::Lifter
