#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {

enum class SymbolSource {
  Auto, // Derived from ELF, analysis, or default naming
  User  // Explicitly renamed by user
};

enum class SymbolType { Function, GlobalVariable, Label, StringLiteral };

struct SymbolInfo {
  uint64_t address;
  std::string name;
  SymbolType type;
  SymbolSource source;

  // Optional: Original name from ELF if renamed?
  std::string originalName;
};

enum class XRefType {
  Call,  // Function call
  Read,  // Data read
  Write, // Data write
  Jump   // Control flow jump
};

struct XRef {
  uint64_t sourceAddress; // Where the reference happens
  uint64_t targetAddress; // What is being referenced
  XRefType type;
};

class SymbolDatabase {
public:
  // Singleton access (Project-scoped ideally, but singleton for now for
  // simplicity) In a real multi-project app, pass this instance around.

  SymbolDatabase() = default;

  // Symbol Management
  void addSymbol(uint64_t address, const std::string &name, SymbolType type,
                 SymbolSource source = SymbolSource::Auto);
  void renameSymbol(uint64_t address,
                    const std::string &newName); // Sets source = User
  std::optional<SymbolInfo> getSymbol(uint64_t address) const;
  std::string getSymbolName(
      uint64_t address) const; // Returns name or "sub_XXXX"/"var_XXXX" fallback

  // Cross-References
  void addXRef(uint64_t source, uint64_t target, XRefType type);
  std::vector<XRef> getXRefsTo(uint64_t target) const;
  std::vector<XRef> getXRefsFrom(uint64_t source) const;

  // Bulk loading (e.g. from ELF analysis)
  void clear();

  const std::map<uint64_t, SymbolInfo> &getSymbols() const { return symbols_; }

private:
  mutable std::mutex mutex_;
  std::map<uint64_t, SymbolInfo> symbols_;
  std::map<uint64_t, std::vector<XRef>> xrefsTo_;   // Who calls Target?
  std::map<uint64_t, std::vector<XRef>> xrefsFrom_; // Who does Source call?
};

} // namespace ShadPKG::Decompiler::Analysis
