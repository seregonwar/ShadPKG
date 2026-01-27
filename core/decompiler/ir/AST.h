#pragma once

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ShadPKG::Decompiler::Analysis {
class Type;
}

namespace ShadPKG::Decompiler::AST {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           FORWARD DECLARATIONS                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class ASTVisitor;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              BASE AST NODE                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class ASTNode {
public:
  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor *visitor) = 0;

  uint64_t sourceAddress = 0; // Original instruction address for debugging
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           EXPRESSION HIERARCHY                            ║
// ║                                                                           ║
// ║  Expression                                                               ║
// ║  ├── ConstantExpr (42, 0xFF, "string")                                    ║
// ║  ├── VariableExpr (var_4, rax)                                            ║
// ║  ├── BinaryExpr   (a + b, a > b, a && b)                                  ║
// ║  ├── UnaryExpr    (!a, -a, ~a)                                            ║
// ║  ├── CallExpr     (func(args...))                                         ║
// ║  └── MemoryExpr   (*ptr, arr[i])                                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class Expression : public ASTNode {
public:
  enum class Type {
    Unknown,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    Pointer,
    Boolean
  };

  Type resultType = Type::Unknown;

  virtual std::string toString() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ConstantExpr: Literal values (42, 0xFF, 3.14)
// ─────────────────────────────────────────────────────────────────────────────

class ConstantExpr : public Expression {
public:
  enum class Kind { Integer, Float, String };

  Kind kind = Kind::Integer;
  int64_t intValue = 0;
  double floatValue = 0.0;
  std::string strValue;
  bool isHex = false;

  ConstantExpr(int64_t val, bool hex = false)
      : kind(Kind::Integer), intValue(val), isHex(hex) {
    resultType = Type::Int64;
  }

  ConstantExpr(double val) : kind(Kind::Float), floatValue(val) {
    resultType = Type::Double;
  }

  ConstantExpr(const std::string &val) : kind(Kind::String), strValue(val) {
    resultType = Type::Unknown; // String literal type? Pointer usually.
  }

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override {
    std::stringstream ss;
    if (kind == Kind::Integer) {
      if (isHex)
        ss << "0x" << std::hex << intValue;
      else
        ss << std::dec << intValue;
    } else if (kind == Kind::Float) {
      ss << floatValue;
    } else {
      ss << "\"" << strValue << "\"";
    }
    return ss.str();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  VariableExpr: Named variables (var_4, param_0, rax)
// ─────────────────────────────────────────────────────────────────────────────

class VariableExpr : public Expression {
public:
  std::string name;
  bool isParameter = false;
  int stackOffset = 0; // Original [rbp - offset] for reference

  explicit VariableExpr(const std::string &n) : name(n) {}

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override { return name; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  BinaryExpr: Two-operand expressions (a + b, a > b)
// ─────────────────────────────────────────────────────────────────────────────

class BinaryExpr : public Expression {
public:
  enum class Op {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    // Bitwise
    And,
    Or,
    Xor,
    Shl,
    Shr,
    // Comparison
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    // Logical
    LogicalAnd,
    LogicalOr,
    // Assignment
    Assign
  };

  Op op;
  std::shared_ptr<Expression> left;
  std::shared_ptr<Expression> right;

  BinaryExpr(Op o, std::shared_ptr<Expression> l, std::shared_ptr<Expression> r)
      : op(o), left(std::move(l)), right(std::move(r)) {}

  void accept(ASTVisitor *visitor) override;

  static std::string opToString(Op o) {
    switch (o) {
    case Op::Add:
      return "+";
    case Op::Sub:
      return "-";
    case Op::Mul:
      return "*";
    case Op::Div:
      return "/";
    case Op::Mod:
      return "%";
    case Op::And:
      return "&";
    case Op::Or:
      return "|";
    case Op::Xor:
      return "^";
    case Op::Shl:
      return "<<";
    case Op::Shr:
      return ">>";
    case Op::Eq:
      return "==";
    case Op::Ne:
      return "!=";
    case Op::Lt:
      return "<";
    case Op::Le:
      return "<=";
    case Op::Gt:
      return ">";
    case Op::Ge:
      return ">=";
    case Op::LogicalAnd:
      return "&&";
    case Op::LogicalOr:
      return "||";
    case Op::Assign:
      return "=";
    }
    return "?";
  }

  std::string toString() const override {
    return "(" + left->toString() + " " + opToString(op) + " " +
           right->toString() + ")";
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  UnaryExpr: Single-operand expressions (!a, -a, ~a)
// ─────────────────────────────────────────────────────────────────────────────

class UnaryExpr : public Expression {
public:
  enum class Op {
    Negate,     // -a
    Not,        // !a
    BitwiseNot, // ~a
    Deref,      // *a
    AddressOf   // &a
  };

  Op op;
  std::shared_ptr<Expression> operand;

  UnaryExpr(Op o, std::shared_ptr<Expression> expr)
      : op(o), operand(std::move(expr)) {}

  void accept(ASTVisitor *visitor) override;

  static std::string opToString(Op o) {
    switch (o) {
    case Op::Negate:
      return "-";
    case Op::Not:
      return "!";
    case Op::BitwiseNot:
      return "~";
    case Op::Deref:
      return "*";
    case Op::AddressOf:
      return "&";
    }
    return "?";
  }

  std::string toString() const override {
    return opToString(op) + operand->toString();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CallExpr: Function calls (func(a, b, c))
// ─────────────────────────────────────────────────────────────────────────────

class CallExpr : public Expression {
public:
  std::string functionName;
  uint64_t targetAddress = 0;
  std::vector<std::shared_ptr<Expression>> arguments;

  explicit CallExpr(const std::string &name) : functionName(name) {}
  explicit CallExpr(uint64_t addr) : targetAddress(addr) {
    std::stringstream ss;
    ss << "sub_" << std::hex << addr;
    functionName = ss.str();
  }

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override {
    std::string result = functionName + "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
      if (i > 0)
        result += ", ";
      result += arguments[i]->toString();
    }
    return result + ")";
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MemoryExpr: Memory access (*ptr, base[offset])
// ─────────────────────────────────────────────────────────────────────────────

class MemoryExpr : public Expression {
public:
  std::shared_ptr<Expression> base;
  std::shared_ptr<Expression> offset; // nullptr if just *base
  int scale = 1; // For array indexing: base + offset * scale

  explicit MemoryExpr(std::shared_ptr<Expression> b) : base(std::move(b)) {}

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override {
    if (offset) {
      return base->toString() + "[" + offset->toString() + "]";
    }
    return "*" + base->toString();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  MemberAccessExpr: Struct member access (base->member)
// ─────────────────────────────────────────────────────────────────────────────

class MemberAccessExpr : public Expression {
public:
  std::shared_ptr<Expression> base; // The pointer or struct
  std::string memberName;
  int offset; // Debug info: the offset being accessed

  MemberAccessExpr(std::shared_ptr<Expression> b, const std::string &m, int off)
      : base(std::move(b)), memberName(m), offset(off) {}

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override {
    // Assuming pointer access (->) mostly for this task.
    // If base is not a pointer, it should be dot (.).
    // Logic can be refined in Codegen.
    return base->toString() + "->" + memberName;
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  CastExpr: Explicit Type Cast ( (int64_t)var )
// ─────────────────────────────────────────────────────────────────────────────

class CastExpr : public Expression {
public:
  std::shared_ptr<Expression> expr;
  std::string targetType;

  CastExpr(std::shared_ptr<Expression> e, const std::string &type)
      : expr(std::move(e)), targetType(type) {}

  void accept(ASTVisitor *visitor) override;

  std::string toString() const override {
    return "(" + targetType + ")" + expr->toString();
  }
};

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║                          STATEMENT HIERARCHY                             ║
// ╠══════════════════════════════════════════════════════════════════════════╣
// ║ Statement                                                                ║
// ║ ├── CompoundStatement      { stmt1; stmt2; ... }                         ║
// ║ ├── ExpressionStatement    expr;                                         ║
// ║ ├── IfStatement            if (cond) { } else { }                        ║
// ║ ├── WhileStatement         while (cond) { }                              ║
// ║ ├── DoWhileStatement       do { } while (cond);                          ║
// ║ ├── ForStatement           for (init; cond; step) { }                    ║
// ║ ├── ReturnStatement        return expr;                                  ║
// ║ ├── BreakStatement         break;                                        ║
// ║ ├── ContinueStatement      continue;                                     ║
// ║ ├── GotoStatement          goto label;  (fallback for irreducible CFG)   ║
// ║ └── LabelStatement         label:                                        ║
// ╚══════════════════════════════════════════════════════════════════════════╝

class Statement : public ASTNode {
public:
  std::string comment; // Optional inline comment
};

// ─────────────────────────────────────────────────────────────────────────────
//  CompoundStatement: Block of statements { ... }
// ─────────────────────────────────────────────────────────────────────────────

class CompoundStatement : public Statement {
public:
  std::vector<std::shared_ptr<Statement>> statements;

  void accept(ASTVisitor *visitor) override;

  void addStatement(std::shared_ptr<Statement> stmt) {
    statements.push_back(std::move(stmt));
  }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ExpressionStatement: Expression as statement (x = 5;)
// ─────────────────────────────────────────────────────────────────────────────

class ExpressionStatement : public Statement {
public:
  std::shared_ptr<Expression> expression;

  explicit ExpressionStatement(std::shared_ptr<Expression> expr)
      : expression(std::move(expr)) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  IfStatement: if (condition) then-branch [else else-branch]
// ─────────────────────────────────────────────────────────────────────────────

class IfStatement : public Statement {
public:
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> thenBranch;
  std::shared_ptr<Statement> elseBranch; // nullptr if no else

  IfStatement(std::shared_ptr<Expression> cond,
              std::shared_ptr<Statement> thenStmt,
              std::shared_ptr<Statement> elseStmt = nullptr)
      : condition(std::move(cond)), thenBranch(std::move(thenStmt)),
        elseBranch(std::move(elseStmt)) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  WhileStatement: while (condition) { body }
// ─────────────────────────────────────────────────────────────────────────────

class WhileStatement : public Statement {
public:
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> body;

  WhileStatement(std::shared_ptr<Expression> cond, std::shared_ptr<Statement> b)
      : condition(std::move(cond)), body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  DoWhileStatement: do { body } while (condition);
// ─────────────────────────────────────────────────────────────────────────────

class DoWhileStatement : public Statement {
public:
  std::shared_ptr<Statement> body;
  std::shared_ptr<Expression> condition;

  DoWhileStatement(std::shared_ptr<Statement> b,
                   std::shared_ptr<Expression> cond)
      : body(std::move(b)), condition(std::move(cond)) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ForStatement: for (init; cond; step) { body }
// ─────────────────────────────────────────────────────────────────────────────

class ForStatement : public Statement {
public:
  std::shared_ptr<Expression> init;
  std::shared_ptr<Expression> condition;
  std::shared_ptr<Expression> step;
  std::shared_ptr<Statement> body;

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ReturnStatement: return [expr];
// ─────────────────────────────────────────────────────────────────────────────

class ReturnStatement : public Statement {
public:
  std::shared_ptr<Expression> value; // nullptr for void return

  ReturnStatement() = default;
  explicit ReturnStatement(std::shared_ptr<Expression> v)
      : value(std::move(v)) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  BreakStatement: break;
// ─────────────────────────────────────────────────────────────────────────────

class BreakStatement : public Statement {
public:
  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ContinueStatement: continue;
// ─────────────────────────────────────────────────────────────────────────────

class ContinueStatement : public Statement {
public:
  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  GotoStatement: goto label; (fallback for irreducible control flow)
// ─────────────────────────────────────────────────────────────────────────────

class GotoStatement : public Statement {
public:
  std::string label;
  uint64_t targetAddress = 0;

  explicit GotoStatement(const std::string &lbl) : label(lbl) {}
  explicit GotoStatement(uint64_t addr) : targetAddress(addr) {
    std::stringstream ss;
    ss << "loc_" << std::hex << addr;
    label = ss.str();
  }

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  LabelStatement: label:
// ─────────────────────────────────────────────────────────────────────────────

class LabelStatement : public Statement {
public:
  std::string name;
  uint64_t address = 0;

  explicit LabelStatement(const std::string &n) : name(n) {}
  explicit LabelStatement(uint64_t addr) : address(addr) {
    std::stringstream ss;
    ss << "loc_" << std::hex << addr;
    name = ss.str();
  }

  void accept(ASTVisitor *visitor) override;
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              LOCAL VARIABLE                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct LocalVariable {
  std::string name;
  Expression::Type type =
      Expression::Type::Int32; // Legacy enum for simple cases
  // NEW: Advanced Type System Integration
  std::shared_ptr<ShadPKG::Decompiler::Analysis::Type> complexType;

  int stackOffset = 0; // Original [rbp - offset]
  int size = 4;        // Size in bytes
  bool isParameter = false;
  int paramIndex = -1; // If parameter, which one (0, 1, 2...)

  // If complexType is set, use it. Otherwise fallback to enum.
  std::string getTypeName() const;
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              FUNCTION AST                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class FunctionAST {
public:
  std::string name;
  uint64_t address = 0;
  std::string returnType = "void";

  std::vector<LocalVariable> parameters;
  std::vector<LocalVariable> locals;
  std::shared_ptr<CompoundStatement> body;

  FunctionAST() : body(std::make_shared<CompoundStatement>()) {}
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              AST VISITOR                                  ║
// ║                                                                           ║
// ║  Visitor pattern for traversing AST without modifying node classes.       ║
// ║  Subclass and override visit() methods to implement code generation,      ║
// ║  optimization passes, or analysis.                                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ─────────────────────────────────────────────────────────────────────────────
//  CaseStmt: case val1: case val2: ... body
// ─────────────────────────────────────────────────────────────────────────────

class CaseStmt : public Statement {
public:
  std::vector<int64_t> values; // Multiple values for stacked cases
  bool isDefault = false;
  std::shared_ptr<CompoundStatement> body;

  CaseStmt() : body(std::make_shared<CompoundStatement>()) {}

  void accept(ASTVisitor *visitor) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  SwitchStmt: switch (expr) { cases... }
// ─────────────────────────────────────────────────────────────────────────────

class SwitchStmt : public Statement {
public:
  std::shared_ptr<Expression> condition;
  std::vector<std::shared_ptr<CaseStmt>> cases;

  explicit SwitchStmt(std::shared_ptr<Expression> cond)
      : condition(std::move(cond)) {}

  void accept(ASTVisitor *visitor) override;
};

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  // Expressions
  virtual void visit(ConstantExpr *node) = 0;
  virtual void visit(VariableExpr *node) = 0;
  virtual void visit(BinaryExpr *node) = 0;
  virtual void visit(UnaryExpr *node) = 0;
  virtual void visit(CallExpr *node) = 0;
  virtual void visit(MemoryExpr *node) = 0;
  virtual void visit(MemberAccessExpr *node) = 0;
  virtual void visit(CastExpr *node) = 0; // NEW

  // Statements
  virtual void visit(CompoundStatement *node) = 0;
  virtual void visit(ExpressionStatement *node) = 0;
  virtual void visit(IfStatement *node) = 0;
  virtual void visit(WhileStatement *node) = 0;
  virtual void visit(DoWhileStatement *node) = 0;
  virtual void visit(ForStatement *node) = 0;
  virtual void visit(ReturnStatement *node) = 0;
  virtual void visit(BreakStatement *node) = 0;
  virtual void visit(ContinueStatement *node) = 0;
  virtual void visit(GotoStatement *node) = 0;
  virtual void visit(LabelStatement *node) = 0;
  virtual void visit(CaseStmt *node) = 0;   // NEW
  virtual void visit(SwitchStmt *node) = 0; // NEW
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                        VISITOR ACCEPT IMPLEMENTATIONS                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

inline void ConstantExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void VariableExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void BinaryExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void UnaryExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void CallExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void MemoryExpr::accept(ASTVisitor *v) { v->visit(this); }
inline void MemberAccessExpr::accept(ASTVisitor *v) { v->visit(this); } // NEW
inline void CastExpr::accept(ASTVisitor *v) { v->visit(this); } // NEW
inline void CompoundStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void ExpressionStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void IfStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void WhileStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void DoWhileStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void ForStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void ReturnStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void BreakStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void ContinueStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void GotoStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void LabelStatement::accept(ASTVisitor *v) { v->visit(this); }
inline void CaseStmt::accept(ASTVisitor *v) { v->visit(this); }
inline void SwitchStmt::accept(ASTVisitor *v) { v->visit(this); }

} // namespace ShadPKG::Decompiler::AST
