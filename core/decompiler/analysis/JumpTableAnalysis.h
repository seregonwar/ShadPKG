#pragma once

#include "../ir/IR.h"
#include <memory>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {

class JumpTableAnalysis {
public:
  explicit JumpTableAnalysis(std::shared_ptr<IR::Function> func);
  void analyze();

private:
  std::shared_ptr<IR::Function> func_;

  // Helper to read memory from context
  uint64_t readPointer(uint64_t address);
  int32_t readInt32(uint64_t address);
};

} // namespace ShadPKG::Decompiler::Analysis
