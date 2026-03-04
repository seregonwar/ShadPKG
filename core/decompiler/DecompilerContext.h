#pragma once

#include "analysis/DataFlowAnalysis.h"
#include "analysis/DominatorAnalysis.h"
#include "analysis/StructuralAnalysis.h"
#include "analysis/SymbolAnalysis.h"
#include "codegen/CppEmitter.h"
#include "ir/AST.h"
#include "ir/IR.h"
#include "lifter/VariableAnalysis.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "analysis/SymbolDatabase.h"
#include "analysis/TypeSystem.h"

namespace ShadPKG::Decompiler {

namespace IR {
class Function;
}

class DecompilerContext {
public:
  static DecompilerContext &Get() {
    static DecompilerContext instance;
    return instance;
  }

  ~DecompilerContext() = default;

  void LoadBinary(const std::vector<uint8_t> &data, uint64_t baseAddress);
  bool LoadELF(const std::vector<uint8_t> &data);
  void Analyze();
  std::string GenerateCode();
  std::string GenerateStructuredCode();
  void ExportProject(const std::string &outPath);

  // Project Persistence
  bool SaveProject(const std::string &path);
  bool LoadProject(const std::string &path);

  // Safe memory access helpers
  uint64_t ReadPointer(uint64_t address) const;
  int32_t ReadInt32(uint64_t address) const;

  uint64_t GetBaseAddress() const { return baseAddress_; }
  uint64_t GetEntryPoint() const { return entryPoint_; }
  void SetEntryPoint(uint64_t ep) { entryPoint_ = ep; }
  
  uint64_t GetInitArrayAddr() const { return initArrayAddr_; }
  uint64_t GetInitArraySize() const { return initArraySize_; }
  uint64_t GetFiniArrayAddr() const { return finiArrayAddr_; }
  uint64_t GetFiniArraySize() const { return finiArraySize_; }

  const std::vector<std::shared_ptr<IR::Function>> &GetFunctions() const {
    return functions_;
  }
  const std::vector<uint8_t> &GetRawData() const { return rawData_; }

  // Helper to find a function by address
  std::shared_ptr<IR::Function> GetFunctionAt(uint64_t address);
  std::shared_ptr<Analysis::SymbolDatabase> GetSymbolDatabase() {
    return symbolDatabase_;
  }

  // Get Code Tokens for a function (populates after GenerateStructuredCode)
  const std::vector<Codegen::CppEmitter::Token> &
  GetFunctionTokens(uint64_t address) const;

private:
  void AnalyzeFunction(uint64_t startAddress,
                       std::set<uint64_t> &visitedGlobal);

  std::vector<uint8_t> rawData_;
  uint64_t baseAddress_ = 0;
  uint64_t entryPoint_ = 0;
  uint64_t elfOffset_ = 0;
  uint64_t initArrayAddr_ = 0;
  uint64_t initArraySize_ = 0;
  uint64_t finiArrayAddr_ = 0;
  uint64_t finiArraySize_ = 0;
  std::vector<std::shared_ptr<IR::Function>> functions_;
  std::shared_ptr<Analysis::SymbolDatabase> symbolDatabase_;
  std::shared_ptr<Analysis::TypeManager> typeManager_ =
      std::make_shared<Analysis::TypeManager>();

  // Persistence for user overrides: FunctionAddr -> (StackOffset -> Type)
  std::map<uint64_t, std::map<int, std::shared_ptr<Analysis::Type>>>
      userVarTypes_;

public:
  std::shared_ptr<Analysis::TypeManager> GetTypeManager() {
    return typeManager_;
  }

  void SetUserVarType(uint64_t funcAddr, int stackOffset,
                      std::shared_ptr<Analysis::Type> type) {
    userVarTypes_[funcAddr][stackOffset] = type;
  }

  std::shared_ptr<Analysis::Type> GetUserVarType(uint64_t funcAddr,
                                                 int stackOffset) {
    if (userVarTypes_.count(funcAddr) &&
        userVarTypes_[funcAddr].count(stackOffset)) {
      return userVarTypes_[funcAddr][stackOffset];
    }
    return nullptr;
  }

private:
  std::map<uint64_t, std::vector<Codegen::CppEmitter::Token>> functionTokens_;
  std::map<uint64_t, int> functionParamCounts_;

public:
  int GetFunctionParamCount(uint64_t address) const {
    auto it = functionParamCounts_.find(address);
    if (it != functionParamCounts_.end())
      return it->second;
    return 0; // Default or unknown
  }
  void SetFunctionParamCount(uint64_t address, int count) {
    functionParamCounts_[address] = count;
  }

  // ┌─────────────────────────────────────────────────────────────────────────┐
  // │  Progress Tracking for GUI                                              │
  // └─────────────────────────────────────────────────────────────────────────┘
public:
  struct AnalysisProgress {
    int prologuesFound = 0;    // Candidates from prologue scan
    int functionsAnalyzed = 0; // Functions fully analyzed
    std::string currentPhase;  // "scanning", "analyzing", "complete"
    bool isComplete = false;
  };

  const AnalysisProgress &GetProgress() const { return progress_; }

private:
  mutable AnalysisProgress progress_;

  // Status tracking
  bool isAnalyzed_ = false;

  struct Segment {
    uint64_t virtualAddress;
    uint64_t fileOffset;
    uint64_t size;    // p_filesz: bytes present in file
    uint64_t memSize; // p_memsz:  bytes to map in memory (>= size, BSS zero-pad)
  };
  std::vector<Segment> segments_;

  bool VirtualAddressToFileOffset(uint64_t va, uint64_t &offset) const;
};

} // namespace ShadPKG::Decompiler
