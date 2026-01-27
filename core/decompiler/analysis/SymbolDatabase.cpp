#include "SymbolDatabase.h"
#include <iomanip>
#include <sstream>

namespace ShadPKG::Decompiler::Analysis {

void SymbolDatabase::addSymbol(uint64_t address, const std::string &name,
                               SymbolType type, SymbolSource source) {
  std::lock_guard<std::mutex> lock(mutex_);

  // If symbol exists and is User-defined, don't overwrite with Auto unless
  // forced? For now, if source is Auto and existing is User, ignore.
  if (symbols_.count(address)) {
    if (symbols_[address].source == SymbolSource::User &&
        source == SymbolSource::Auto) {
      return;
    }
  }

  SymbolInfo info;
  info.address = address;
  info.name = name;
  info.type = type;
  info.source = source;

  if (symbols_.count(address)) {
    // Preserve original name if we are renaming manually?
    // This is a simplification.
    if (symbols_[address].originalName.empty()) {
      info.originalName = symbols_[address].name;
    } else {
      info.originalName = symbols_[address].originalName;
    }
  }

  symbols_[address] = info;
}

void SymbolDatabase::renameSymbol(uint64_t address,
                                  const std::string &newName) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (symbols_.count(address)) {
    symbols_[address].name = newName;
    symbols_[address].source = SymbolSource::User;
  } else {
    // Create new entry if it didn't exist (e.g. renaming an arbitrary address)
    addSymbol(address, newName, SymbolType::Label, SymbolSource::User);
  }
}

std::optional<SymbolInfo> SymbolDatabase::getSymbol(uint64_t address) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = symbols_.find(address);
  if (it != symbols_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string SymbolDatabase::getSymbolName(uint64_t address) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = symbols_.find(address);
  if (it != symbols_.end()) {
    return it->second.name;
  }

  // Fallback naming
  std::stringstream ss;
  ss << "loc_" << std::hex << address;
  return ss.str();
}

void SymbolDatabase::addXRef(uint64_t source, uint64_t target, XRefType type) {
  std::lock_guard<std::mutex> lock(mutex_);

  XRef xref{source, target, type};
  xrefsTo_[target].push_back(xref);
  xrefsFrom_[source].push_back(xref);
}

std::vector<XRef> SymbolDatabase::getXRefsTo(uint64_t target) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (xrefsTo_.count(target)) {
    return xrefsTo_.at(target);
  }
  return {};
}

std::vector<XRef> SymbolDatabase::getXRefsFrom(uint64_t source) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (xrefsFrom_.count(source)) {
    return xrefsFrom_.at(source);
  }
  return {};
}

void SymbolDatabase::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  symbols_.clear();
  xrefsTo_.clear();
  xrefsFrom_.clear();
}

} // namespace ShadPKG::Decompiler::Analysis
