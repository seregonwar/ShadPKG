#include "AST.h"
#include "../analysis/TypeSystem.h"

namespace ShadPKG::Decompiler::AST {

std::string LocalVariable::getTypeName() const {
  if (complexType) {
    return complexType->toString();
  }

  // Fallback to legacy enum
  switch (type) {
  case Expression::Type::Int8:
    return "int8_t";
  case Expression::Type::Int16:
    return "int16_t";
  case Expression::Type::Int32:
    return "int32_t";
  case Expression::Type::Int64:
    return "int64_t";
  case Expression::Type::UInt8:
    return "uint8_t";
  case Expression::Type::UInt16:
    return "uint16_t";
  case Expression::Type::UInt32:
    return "uint32_t";
  case Expression::Type::UInt64:
    return "uint64_t";
  case Expression::Type::Float:
    return "float";
  case Expression::Type::Double:
    return "double";
  case Expression::Type::Pointer:
    return "void*";
  case Expression::Type::Boolean:
    return "bool";
  default:
    return "int32_t";
  }
}

} // namespace ShadPKG::Decompiler::AST
