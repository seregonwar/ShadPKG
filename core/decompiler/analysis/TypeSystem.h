#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           TYPE SYSTEM CORE                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class Type;

class Type {
public:
  enum class Kind { Primitive, Pointer, Struct, Array, Function };

  virtual ~Type() = default;

  Kind getKind() const { return kind_; }
  virtual std::string toString() const = 0;
  virtual int getSize() const = 0; // Size in bytes

protected:
  Type(Kind kind) : kind_(kind) {}
  Kind kind_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PrimitiveType: int, float, void, etc.
// ─────────────────────────────────────────────────────────────────────────────

class PrimitiveType : public Type {
public:
  enum class Specific {
    Void,
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double
  };

  PrimitiveType(Specific spec) : Type(Kind::Primitive), specific_(spec) {}

  std::string toString() const override {
    switch (specific_) {
    case Specific::Void:
      return "void";
    case Specific::Bool:
      return "bool";
    case Specific::Int8:
      return "int8_t";
    case Specific::UInt8:
      return "uint8_t";
    case Specific::Int16:
      return "int16_t";
    case Specific::UInt16:
      return "uint16_t";
    case Specific::Int32:
      return "int32_t";
    case Specific::UInt32:
      return "uint32_t";
    case Specific::Int64:
      return "int64_t";
    case Specific::UInt64:
      return "uint64_t";
    case Specific::Float:
      return "float";
    case Specific::Double:
      return "double";
    }
    return "unknown";
  }

  int getSize() const override {
    switch (specific_) {
    case Specific::Void:
      return 0;
    case Specific::Bool:
      return 1;
    case Specific::Int8:
    case Specific::UInt8:
      return 1;
    case Specific::Int16:
    case Specific::UInt16:
      return 2;
    case Specific::Int32:
    case Specific::UInt32:
    case Specific::Float:
      return 4;
    case Specific::Int64:
    case Specific::UInt64:
    case Specific::Double:
      return 8;
    }
    return 0;
  }

  Specific getSpecific() const { return specific_; }

private:
  Specific specific_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PointerType: T*
// ─────────────────────────────────────────────────────────────────────────────

class PointerType : public Type {
public:
  PointerType(std::shared_ptr<Type> pointee)
      : Type(Kind::Pointer), pointee_(pointee) {}

  std::string toString() const override { return pointee_->toString() + "*"; }

  int getSize() const override { return 8; } // 64-bit pointers

  std::shared_ptr<Type> getPointee() const { return pointee_; }

private:
  std::shared_ptr<Type> pointee_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  StructType: User-defined structures
// ─────────────────────────────────────────────────────────────────────────────

struct StructMember {
  std::string name;
  std::shared_ptr<Type> type;
  int offset;
};

class StructType : public Type {
public:
  StructType(const std::string &name) : Type(Kind::Struct), name_(name) {}

  void addMember(const std::string &name, std::shared_ptr<Type> type,
                 int offset) {
    members_.push_back({name, type, offset});
    // Keep sorted by offset?
  }

  std::string toString() const override {
    return name_; // Just the name (e.g., "Player")
  }

  int getSize() const override {
    if (members_.empty())
      return 0;
    // Simple size calculation: end of last member
    const auto &last = members_.back();
    return last.offset + last.type->getSize();
    // Real struct size might have padding/alignment, but this suffices for now
  }

  void setName(const std::string &name) { name_ = name; }

  const std::vector<StructMember> &getMembers() const { return members_; }

  const StructMember *getMemberAt(int offset) const {
    for (const auto &member : members_) {
      if (member.offset == offset)
        return &member;
    }
    return nullptr;
  }

private:
  std::string name_;
  std::vector<StructMember> members_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  TypeManager: Manages known types
// ─────────────────────────────────────────────────────────────────────────────

class TypeManager {
public:
  TypeManager() {
    // Register basic primitives
    registerType(
        "void", std::make_shared<PrimitiveType>(PrimitiveType::Specific::Void));
    registerType(
        "int", std::make_shared<PrimitiveType>(PrimitiveType::Specific::Int32));
    // ... add others as needed
  }

  void registerType(const std::string &name, std::shared_ptr<Type> type) {
    namedTypes_[name] = type;
  }

  std::shared_ptr<Type> getType(const std::string &name) const {
    auto it = namedTypes_.find(name);
    if (it != namedTypes_.end())
      return it->second;
    return nullptr;
  }

  std::shared_ptr<StructType> createStruct(const std::string &name) {
    auto s = std::make_shared<StructType>(name);
    registerType(name, s);
    return s;
  }

  const std::map<std::string, std::shared_ptr<Type>> &getAllTypes() const {
    return namedTypes_;
  }

  void renameType(const std::string &oldName, const std::string &newName) {
    auto it = namedTypes_.find(oldName);
    if (it != namedTypes_.end()) {
      auto type = it->second;
      namedTypes_.erase(it);
      namedTypes_[newName] = type;
      // Update internal name if StructType
      if (auto s = std::dynamic_pointer_cast<StructType>(type)) {
        s->setName(newName);
      }
    }
  }

private:
  std::map<std::string, std::shared_ptr<Type>> namedTypes_;
};

} // namespace ShadPKG::Decompiler::Analysis
