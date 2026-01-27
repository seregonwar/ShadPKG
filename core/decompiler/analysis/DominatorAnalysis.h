#pragma once

#include "../ir/IR.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           DOMINATOR ANALYSIS                              ║
// ║                                                                           ║
// ║  Computes dominator tree and identifies natural loops via back-edges.     ║
// ║  Uses Cooper-Harvey-Kennedy algorithm (efficient iterative approach).     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ─────────────────────────────────────────────────────────────────────────────
//  LoopInfo: Represents a detected natural loop
// ─────────────────────────────────────────────────────────────────────────────

struct LoopInfo {
  uint64_t headerBlock;             // Loop header (back-edge target)
  uint64_t latchBlock;              // Back-edge source
  std::set<uint64_t> bodyBlocks;    // All blocks in loop body
  std::vector<uint64_t> exitBlocks; // Blocks that exit the loop
  bool isDoWhile = false;           // true if condition at end

  bool contains(uint64_t blockId) const {
    return bodyBlocks.find(blockId) != bodyBlocks.end();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DominatorAnalysis: Main analysis class
// ─────────────────────────────────────────────────────────────────────────────

class DominatorAnalysis {
public:
  DominatorAnalysis() = default;

  // ═══════════════════════════════════════════════════════════════════════
  //  Main Analysis Entry Point
  // ═══════════════════════════════════════════════════════════════════════

  void analyze(const std::shared_ptr<IR::Function> &func);

  // ═══════════════════════════════════════════════════════════════════════
  //  Dominator Queries
  // ═══════════════════════════════════════════════════════════════════════

  // Get immediate dominator of a block
  uint64_t getImmediateDominator(uint64_t blockId) const;

  // Check if A dominates B (A is on every path from entry to B)
  bool dominates(uint64_t dominator, uint64_t dominated) const;

  // Check if A strictly dominates B (A dom B and A != B)
  bool strictlyDominates(uint64_t dominator, uint64_t dominated) const;

  // Get all dominators of a block
  const std::set<uint64_t> &getDominators(uint64_t blockId) const;

  // ═══════════════════════════════════════════════════════════════════════
  //  Post-Dominator Queries (for if-then-else detection)
  // ═══════════════════════════════════════════════════════════════════════

  uint64_t getImmediatePostDominator(uint64_t blockId) const;
  bool postDominates(uint64_t postDom, uint64_t dominated) const;

  // ═══════════════════════════════════════════════════════════════════════
  //  Loop Detection
  // ═══════════════════════════════════════════════════════════════════════

  const std::vector<LoopInfo> &getLoops() const { return loops_; }

  // Check if block is a loop header
  bool isLoopHeader(uint64_t blockId) const;

  // Get the loop containing a block (nullptr if none)
  const LoopInfo *getLoopFor(uint64_t blockId) const;

  // ═══════════════════════════════════════════════════════════════════════
  //  Utility
  // ═══════════════════════════════════════════════════════════════════════

  // Get blocks in reverse post-order (for iteration)
  const std::vector<uint64_t> &getReversePostOrder() const { return rpo_; }

  // Get entry block ID
  uint64_t getEntryBlock() const { return entryBlock_; }

private:
  // ═══════════════════════════════════════════════════════════════════════
  //  Internal Algorithms
  // ═══════════════════════════════════════════════════════════════════════

  void computeReversePostOrder(const std::shared_ptr<IR::Function> &func);
  void computeDominators();
  void computePostDominators();
  void detectBackEdges();
  void buildLoops();

  // Intersect helper for iterative dominator computation
  uint64_t intersect(uint64_t b1, uint64_t b2) const;

  // Collect loop body blocks using reverse DFS from latch
  void collectLoopBody(uint64_t header, uint64_t latch,
                       std::set<uint64_t> &body);

  // ═══════════════════════════════════════════════════════════════════════
  //  Data
  // ═══════════════════════════════════════════════════════════════════════

  // Block adjacency
  std::map<uint64_t, std::vector<uint64_t>> successors_;
  std::map<uint64_t, std::vector<uint64_t>> predecessors_;

  // RPO numbering
  std::vector<uint64_t> rpo_;         // Blocks in reverse post-order
  std::map<uint64_t, int> rpoNumber_; // Block ID → RPO index

  // Dominators
  std::map<uint64_t, uint64_t> idom_;           // Immediate dominator
  std::map<uint64_t, std::set<uint64_t>> doms_; // All dominators

  // Post-dominators
  std::map<uint64_t, uint64_t> ipdom_;           // Immediate post-dominator
  std::map<uint64_t, std::set<uint64_t>> pdoms_; // All post-dominators

  // Back-edges & loops
  std::vector<std::pair<uint64_t, uint64_t>> backEdges_; // (source, target)
  std::vector<LoopInfo> loops_;
  std::map<uint64_t, size_t> blockToLoop_; // Block → loop index

  uint64_t entryBlock_ = 0;
  uint64_t exitBlock_ = 0; // Virtual exit for post-dom

  static const std::set<uint64_t> emptySet_;
};

} // namespace ShadPKG::Decompiler::Analysis
