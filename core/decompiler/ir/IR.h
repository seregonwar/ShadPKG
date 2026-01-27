#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {
class Type;
}

namespace ShadPKG::Decompiler::IR {

enum class OpCode {
  NOP,
  MOV,
  ADD,
  SUB,
  MUL,
  DIV,
  AND,
  OR,
  XOR,
  SHL,
  SHR,
  CMP,
  LEA,
  PUSH,
  POP,
  MOVSX,
  MOVZX,
  BSWAP,
  FISTTP,
  LEAVE,
  INT,
  JMP,
  JE,
  JNE,
  JG,
  JL,
  CALL,
  RET,
  // High-level
  PHI,
  LOAD,
  STORE
};

struct Operand {
  enum class Type { Register, Immediate, Memory, Variable };
  Type type;
  uint64_t value;   // For Immediate or Register ID
  std::string name; // For Variable

  // Extended Info
  std::string regName;     // Name of register if type == Register
  uint64_t memBase = 0;    // Register ID
  std::string memBaseName; // Name of base register
  int64_t memDisp = 0;     // Displacement
};

struct Instruction {
  uint64_t address;
  OpCode opcode;
  std::vector<Operand> operands;
  std::string disassembly; // Original assembly for reference
};

struct BasicBlock {
  uint64_t id;
  uint64_t startAddress;
  uint64_t endAddress;
  std::vector<Instruction> instructions;
  std::vector<uint64_t> successors;
  std::vector<uint64_t> predecessors;

  // Switch Metadata: Value -> Target Block ID
  std::map<int64_t, uint64_t> switchMap;
};

struct LocalVariable {
  std::string name;
  int stackOffset;
  int size;
  std::shared_ptr<Analysis::Type> complexType;
};

struct Function {
  enum class Category { Unknown, GameLogic, Physics, System, Library };

  std::string name;
  uint64_t address;
  std::vector<std::shared_ptr<BasicBlock>> basicBlocks;
  std::string signature;
  Category category = Category::Unknown;
  std::vector<LocalVariable> locals;
};

} // namespace ShadPKG::Decompiler::IR
