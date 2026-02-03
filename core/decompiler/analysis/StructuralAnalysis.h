#pragma once

#include "../ir/AST.h"
#include "../ir/IR.h"
#include "../lifter/VariableAnalysis.h"
#include "DominatorAnalysis.h"
#include "SymbolAnalysis.h"
#include <map>
#include <memory>
#include <set>
#include <stack>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STRUCTURAL ANALYSIS                               ║
// ║                                                                           ║
// ║  Transforms the Command Flow Graph (CFG) into a high-level AST.           ║
// ║  Detects Control Structures:                                              ║
// ║  - Linear Sequences (Basic Blocks)                                        ║
// ║  - If-Then / If-Then-Else                                                 ║
// ║  - While Loops / Do-While Loops                                           ║
// ║                                                                           ║
// ║  Algorithm: Recursive Refinement / Region Structuring                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class StructuralAnalysis {
public:
  StructuralAnalysis(
      std::shared_ptr<IR::Function> func,
      std::shared_ptr<DominatorAnalysis> domAnalysis,
      std::shared_ptr<SymbolAnalysis> symbolAnalysis,
      std::shared_ptr<Lifter::VariableAnalysis> varAnalysis = nullptr);

  // ═══════════════════════════════════════════════════════════════════════
  //  Main Analysis Method
  // ═══════════════════════════════════════════════════════════════════════

  std::shared_ptr<AST::FunctionAST> analyze();

private:
  std::shared_ptr<IR::Function> func_;
  std::shared_ptr<DominatorAnalysis> dom_;
  std::shared_ptr<SymbolAnalysis> symbols_;
  std::shared_ptr<Lifter::VariableAnalysis> vars_;

  // Set of blocks already structured/visited to prevent infinite recursion
  std::set<uint64_t> structuredBlocks_;

  // Set of loop headers currently being matched to prevent infinite recursion
  std::set<uint64_t> activeLoops_;

  // Set of labels that have already been emitted for a goto statement
  std::set<uint64_t> emittedLabels_;
  
  // Set of blocks that are targets of goto statements (need labels)
  std::set<uint64_t> gotoTargets_;

  // ═══════════════════════════════════════════════════════════════════════
  //  Recursive Structuring
  // ═══════════════════════════════════════════════════════════════════════

  // Structure a region starting at 'entry'
  std::shared_ptr<AST::Statement> structureRegion(uint64_t entryBlock,
                                                  uint64_t stopBlock = 0);

  // Structure a single basic block into a list of statements
  std::shared_ptr<AST::CompoundStatement>
  structureBlock(const std::shared_ptr<IR::BasicBlock> &bb);

  // ═══════════════════════════════════════════════════════════════════════
  //  Pattern Matchers
  // ═══════════════════════════════════════════════════════════════════════

  // Try to match a loop starting at header
  std::shared_ptr<AST::Statement> matchLoop(uint64_t header,
                                            uint64_t stopBlock);

  // Try to match an If-Then-Else starting at block
  std::shared_ptr<AST::Statement> matchIf(uint64_t block, uint64_t stopBlock);

  // Try to match a Switch starting at block
  std::shared_ptr<AST::Statement> matchSwitch(uint64_t block,
                                              uint64_t stopBlock);

  // ═══════════════════════════════════════════════════════════════════════
  //  Instruction Lifting Helpers
  // ═══════════════════════════════════════════════════════════════════════

  // Convert IR instruction to AST statement(s)
  std::shared_ptr<AST::Statement> liftInstruction(const IR::Instruction &instr);

  // Extract condition from a block ending in a conditional jump
  std::shared_ptr<AST::Expression>
  extractCondition(const std::shared_ptr<IR::BasicBlock> &bb, bool invert);

  // Helper to get basic block by ID
  std::shared_ptr<IR::BasicBlock> getBlock(uint64_t id);
};

} // namespace ShadPKG::Decompiler::Analysis
