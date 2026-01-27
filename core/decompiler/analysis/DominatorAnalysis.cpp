#include "DominatorAnalysis.h"
#include <algorithm>
#include <limits>
#include <queue>
#include <stack>

namespace ShadPKG::Decompiler::Analysis {

// Static empty set for returning references
const std::set<uint64_t> DominatorAnalysis::emptySet_;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           MAIN ANALYSIS ENTRY                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::analyze(const std::shared_ptr<IR::Function> &func) {
  if (!func || func->basicBlocks.empty())
    return;

  // ─────────────────────────────────────────────────────────────────────────
  //  Step 1: Build adjacency lists from basic blocks
  // ─────────────────────────────────────────────────────────────────────────

  successors_.clear();
  predecessors_.clear();

  for (const auto &bb : func->basicBlocks) {
    uint64_t id = bb->id;
    successors_[id] = bb->successors;

    for (uint64_t succ : bb->successors) {
      predecessors_[succ].push_back(id);
    }
  }

  entryBlock_ = func->basicBlocks[0]->id;

  // ─────────────────────────────────────────────────────────────────────────
  //  Step 2: Compute reverse post-order
  // ─────────────────────────────────────────────────────────────────────────

  computeReversePostOrder(func);

  // ─────────────────────────────────────────────────────────────────────────
  //  Step 3: Compute dominators (Cooper-Harvey-Kennedy)
  // ─────────────────────────────────────────────────────────────────────────

  computeDominators();

  // ─────────────────────────────────────────────────────────────────────────
  //  Step 4: Compute post-dominators (reverse CFG)
  // ─────────────────────────────────────────────────────────────────────────

  computePostDominators();

  // ─────────────────────────────────────────────────────────────────────────
  //  Step 5: Detect back-edges and build loops
  // ─────────────────────────────────────────────────────────────────────────

  detectBackEdges();
  buildLoops();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         REVERSE POST-ORDER                                ║
// ║                                                                           ║
// ║  DFS traversal, number nodes in post-order, then reverse.                 ║
// ║  This gives us an order where dominators come before dominated nodes.     ║
// ║                                                                           ║
// ║    Entry                                                                  ║
// ║      │        DFS: Entry→A→B→C (post: C=1,B=2,A=3,Entry=4)                ║
// ║      ▼        RPO: Entry,A,B,C (reversed post-order)                      ║
// ║      A                                                                    ║
// ║      │                                                                    ║
// ║      ▼                                                                    ║
// ║      B                                                                    ║
// ║      │                                                                    ║
// ║      ▼                                                                    ║
// ║      C                                                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::computeReversePostOrder(
    const std::shared_ptr<IR::Function> &func) {
  rpo_.clear();
  rpoNumber_.clear();

  std::set<uint64_t> visited;
  std::vector<uint64_t> postOrder;

  // Iterative DFS with explicit stack
  std::stack<std::pair<uint64_t, int>> stack;
  stack.push({entryBlock_, 0});

  while (!stack.empty()) {
    auto &[blockId, succIdx] = stack.top();

    if (visited.find(blockId) == visited.end() && succIdx == 0) {
      visited.insert(blockId);
    }

    auto &succs = successors_[blockId];

    // Find next unvisited successor
    bool foundNext = false;
    while (succIdx < static_cast<int>(succs.size())) {
      uint64_t succ = succs[succIdx];
      succIdx++;
      if (visited.find(succ) == visited.end()) {
        stack.push({succ, 0});
        foundNext = true;
        break;
      }
    }

    if (!foundNext) {
      // All successors visited, add to post-order
      postOrder.push_back(blockId);
      stack.pop();
    }
  }

  // Reverse to get RPO
  rpo_.assign(postOrder.rbegin(), postOrder.rend());

  // Assign RPO numbers
  for (size_t i = 0; i < rpo_.size(); ++i) {
    rpoNumber_[rpo_[i]] = static_cast<int>(i);
  }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                    COOPER-HARVEY-KENNEDY ALGORITHM                        ║
// ║                                                                           ║
// ║  Iterative dominator computation. Efficient for reducible CFGs.           ║
// ║                                                                           ║
// ║  For each node n (in RPO, except entry):                                  ║
// ║    new_idom = first processed predecessor                                 ║
// ║    for each other predecessor p:                                          ║
// ║      if idom[p] already computed:                                         ║
// ║        new_idom = intersect(new_idom, p)                                  ║
// ║    idom[n] = new_idom                                                     ║
// ║                                                                           ║
// ║  Repeat until no changes.                                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::computeDominators() {
  idom_.clear();
  doms_.clear();

  // Entry dominates itself
  idom_[entryBlock_] = entryBlock_;

  bool changed = true;
  while (changed) {
    changed = false;

    // Process all blocks in RPO (skip entry)
    for (size_t i = 1; i < rpo_.size(); ++i) {
      uint64_t b = rpo_[i];
      auto &preds = predecessors_[b];

      if (preds.empty())
        continue;

      // Find first predecessor with computed idom
      uint64_t newIdom = 0;
      bool foundFirst = false;

      for (uint64_t p : preds) {
        if (idom_.find(p) != idom_.end()) {
          newIdom = p;
          foundFirst = true;
          break;
        }
      }

      if (!foundFirst)
        continue;

      // Intersect with other processed predecessors
      for (uint64_t p : preds) {
        if (p == newIdom)
          continue;
        if (idom_.find(p) != idom_.end()) {
          newIdom = intersect(p, newIdom);
        }
      }

      // Update if changed
      if (idom_.find(b) == idom_.end() || idom_[b] != newIdom) {
        idom_[b] = newIdom;
        changed = true;
      }
    }
  }

  // Build full dominator sets from idom tree
  for (const auto &[block, _] : idom_) {
    std::set<uint64_t> &domSet = doms_[block];
    uint64_t current = block;
    while (true) {
      domSet.insert(current);
      if (current == entryBlock_)
        break;
      current = idom_[current];
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Intersect: Find common dominator by walking up the dominator tree
// ─────────────────────────────────────────────────────────────────────────────

uint64_t DominatorAnalysis::intersect(uint64_t b1, uint64_t b2) const {
  auto getNumber = [this](uint64_t b) -> int {
    auto it = rpoNumber_.find(b);
    return it != rpoNumber_.end() ? it->second
                                  : std::numeric_limits<int>::max();
  };

  uint64_t finger1 = b1;
  uint64_t finger2 = b2;

  while (finger1 != finger2) {
    while (getNumber(finger1) > getNumber(finger2)) {
      auto it = idom_.find(finger1);
      if (it == idom_.end() || it->second == finger1)
        return entryBlock_;
      finger1 = it->second;
    }
    while (getNumber(finger2) > getNumber(finger1)) {
      auto it = idom_.find(finger2);
      if (it == idom_.end() || it->second == finger2)
        return entryBlock_;
      finger2 = it->second;
    }
  }

  return finger1;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          POST-DOMINATORS                                  ║
// ║                                                                           ║
// ║  Same algorithm but on reversed CFG, starting from exit nodes.            ║
// ║  Used to detect if-then-else merge points.                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::computePostDominators() {
  ipdom_.clear();
  pdoms_.clear();

  // Find exit blocks (no successors or RET)
  std::vector<uint64_t> exitBlocks;
  for (const auto &[blockId, succs] : successors_) {
    if (succs.empty()) {
      exitBlocks.push_back(blockId);
    }
  }

  if (exitBlocks.empty())
    return;

  // Create virtual exit node
  exitBlock_ = 0xFFFFFFFFFFFFFFFF;

  // Build reverse CFG
  std::map<uint64_t, std::vector<uint64_t>>
      revSucc; // Reversed: succs become preds
  std::map<uint64_t, std::vector<uint64_t>> revPred;

  for (const auto &[blockId, succs] : successors_) {
    for (uint64_t s : succs) {
      revSucc[s].push_back(blockId);
      revPred[blockId].push_back(s);
    }
  }

  // Connect exit blocks to virtual exit
  for (uint64_t exit : exitBlocks) {
    revSucc[exitBlock_].push_back(exit);
    revPred[exit].push_back(exitBlock_);
  }

  // Compute RPO on reverse CFG
  std::vector<uint64_t> revRpo;
  std::map<uint64_t, int> revRpoNumber;
  std::set<uint64_t> visited;
  std::vector<uint64_t> postOrder;

  std::stack<std::pair<uint64_t, int>> stack;
  stack.push({exitBlock_, 0});

  while (!stack.empty()) {
    auto &[blockId, succIdx] = stack.top();

    if (visited.find(blockId) == visited.end() && succIdx == 0) {
      visited.insert(blockId);
    }

    auto &succs = revSucc[blockId];

    bool foundNext = false;
    while (succIdx < static_cast<int>(succs.size())) {
      uint64_t succ = succs[succIdx];
      succIdx++;
      if (visited.find(succ) == visited.end()) {
        stack.push({succ, 0});
        foundNext = true;
        break;
      }
    }

    if (!foundNext) {
      postOrder.push_back(blockId);
      stack.pop();
    }
  }

  revRpo.assign(postOrder.rbegin(), postOrder.rend());
  for (size_t i = 0; i < revRpo.size(); ++i) {
    revRpoNumber[revRpo[i]] = static_cast<int>(i);
  }

  // Compute post-dominators using same algorithm
  ipdom_[exitBlock_] = exitBlock_;

  bool changed = true;
  while (changed) {
    changed = false;

    for (size_t i = 1; i < revRpo.size(); ++i) {
      uint64_t b = revRpo[i];
      auto &preds = revPred[b];

      if (preds.empty())
        continue;

      uint64_t newIpdom = 0;
      bool foundFirst = false;

      for (uint64_t p : preds) {
        if (ipdom_.find(p) != ipdom_.end()) {
          newIpdom = p;
          foundFirst = true;
          break;
        }
      }

      if (!foundFirst)
        continue;

      for (uint64_t p : preds) {
        if (p == newIpdom)
          continue;
        if (ipdom_.find(p) != ipdom_.end()) {
          // Intersect on reverse RPO
          uint64_t f1 = p, f2 = newIpdom;
          while (f1 != f2) {
            while (revRpoNumber[f1] > revRpoNumber[f2]) {
              if (ipdom_.find(f1) == ipdom_.end())
                break;
              f1 = ipdom_[f1];
            }
            while (revRpoNumber[f2] > revRpoNumber[f1]) {
              if (ipdom_.find(f2) == ipdom_.end())
                break;
              f2 = ipdom_[f2];
            }
          }
          newIpdom = f1;
        }
      }

      if (ipdom_.find(b) == ipdom_.end() || ipdom_[b] != newIpdom) {
        ipdom_[b] = newIpdom;
        changed = true;
      }
    }
  }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          BACK-EDGE DETECTION                              ║
// ║                                                                           ║
// ║  A back-edge is an edge A → B where B dominates A.                        ║
// ║  Each back-edge indicates a natural loop with B as the header.            ║
// ║                                                                           ║
// ║       ┌──────────────────────┐                                            ║
// ║       │                      │                                            ║
// ║       ▼                      │ Back-edge (A→B)                            ║
// ║   ┌───────┐                  │                                            ║
// ║   │   B   │ ◄── Loop Header  │                                            ║
// ║   └───────┘                  │                                            ║
// ║       │                      │                                            ║
// ║       ▼                      │                                            ║
// ║   ┌───────┐                  │                                            ║
// ║   │   A   │ ─────────────────┘                                            ║
// ║   └───────┘                                                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::detectBackEdges() {
  backEdges_.clear();

  for (const auto &[blockId, succs] : successors_) {
    for (uint64_t succ : succs) {
      // Check if succ dominates blockId → back-edge
      if (dominates(succ, blockId)) {
        backEdges_.push_back({blockId, succ});
      }
    }
  }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            LOOP CONSTRUCTION                              ║
// ║                                                                           ║
// ║  For each back-edge (latch → header):                                     ║
// ║  1. Header is the loop header                                             ║
// ║  2. Collect all blocks between header and latch (reverse DFS)             ║
// ║  3. Find exit blocks (edges leaving the loop)                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void DominatorAnalysis::buildLoops() {
  loops_.clear();
  blockToLoop_.clear();

  for (const auto &[latch, header] : backEdges_) {
    LoopInfo loop;
    loop.headerBlock = header;
    loop.latchBlock = latch;

    // Collect loop body
    collectLoopBody(header, latch, loop.bodyBlocks);

    // Find exit blocks
    for (uint64_t block : loop.bodyBlocks) {
      for (uint64_t succ : successors_[block]) {
        if (loop.bodyBlocks.find(succ) == loop.bodyBlocks.end()) {
          loop.exitBlocks.push_back(succ);
        }
      }
    }

    // Determine if do-while: if header has only one predecessor from outside
    // loop and condition is at latch, it's do-while
    auto &headerPreds = predecessors_[header];
    int externalPreds = 0;
    for (uint64_t p : headerPreds) {
      if (loop.bodyBlocks.find(p) == loop.bodyBlocks.end()) {
        externalPreds++;
      }
    }

    // Simple heuristic: if latch has the conditional branch, it's likely
    // do-while
    if (successors_[latch].size() == 2 && externalPreds == 1) {
      loop.isDoWhile = true;
    }

    // Map blocks to loop
    size_t loopIdx = loops_.size();
    for (uint64_t block : loop.bodyBlocks) {
      blockToLoop_[block] = loopIdx;
    }

    loops_.push_back(std::move(loop));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Collect loop body via reverse DFS from latch to header
// ─────────────────────────────────────────────────────────────────────────────

void DominatorAnalysis::collectLoopBody(uint64_t header, uint64_t latch,
                                        std::set<uint64_t> &body) {
  body.insert(header);

  if (header == latch)
    return;

  std::stack<uint64_t> worklist;
  worklist.push(latch);
  body.insert(latch);

  while (!worklist.empty()) {
    uint64_t block = worklist.top();
    worklist.pop();

    for (uint64_t pred : predecessors_[block]) {
      if (body.find(pred) == body.end()) {
        body.insert(pred);
        worklist.push(pred);
      }
    }
  }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           QUERY METHODS                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

uint64_t DominatorAnalysis::getImmediateDominator(uint64_t blockId) const {
  auto it = idom_.find(blockId);
  return (it != idom_.end()) ? it->second : 0;
}

bool DominatorAnalysis::dominates(uint64_t dominator,
                                  uint64_t dominated) const {
  auto it = doms_.find(dominated);
  if (it == doms_.end())
    return false;
  return it->second.find(dominator) != it->second.end();
}

bool DominatorAnalysis::strictlyDominates(uint64_t dominator,
                                          uint64_t dominated) const {
  return (dominator != dominated) && dominates(dominator, dominated);
}

const std::set<uint64_t> &
DominatorAnalysis::getDominators(uint64_t blockId) const {
  auto it = doms_.find(blockId);
  return (it != doms_.end()) ? it->second : emptySet_;
}

uint64_t DominatorAnalysis::getImmediatePostDominator(uint64_t blockId) const {
  auto it = ipdom_.find(blockId);
  return (it != ipdom_.end()) ? it->second : 0;
}

bool DominatorAnalysis::postDominates(uint64_t postDom,
                                      uint64_t dominated) const {
  auto it = pdoms_.find(dominated);
  if (it == pdoms_.end())
    return false;
  return it->second.find(postDom) != it->second.end();
}

bool DominatorAnalysis::isLoopHeader(uint64_t blockId) const {
  for (const auto &loop : loops_) {
    if (loop.headerBlock == blockId)
      return true;
  }
  return false;
}

const LoopInfo *DominatorAnalysis::getLoopFor(uint64_t blockId) const {
  auto it = blockToLoop_.find(blockId);
  if (it != blockToLoop_.end() && it->second < loops_.size()) {
    return &loops_[it->second];
  }
  return nullptr;
}

} // namespace ShadPKG::Decompiler::Analysis
