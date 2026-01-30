#pragma once

#include "../ir/AST.h"
#include <set>
#include <sstream>
#include <string>

namespace ShadPKG::Decompiler::Codegen {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           CPP EMITTER                                     ║
// ║                                                                           ║
// ║  AST Visitor that generates formatted C++ code strings.                   ║
// ║  Handles indentation, comments, and syntax structure.                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class CppEmitter : public AST::ASTVisitor {
public:
  CppEmitter() = default;

  enum class TokenType {
    Text,
    Keyword,
    Identifier,
    Function,
    Global,
    Number,
    String,
    Comment,
    Type
  };

  struct Token {
    std::string text;
    TokenType type;
    uint64_t relatedAddress = 0; // if clickable
  };

  // Generate code for a complete function
  std::string generate(const std::shared_ptr<AST::FunctionAST> &func);

  const std::vector<Token> &getTokens() const { return tokens_; }

  // Visitor Methods
  void visit(AST::ConstantExpr *node) override;
  void visit(AST::VariableExpr *node) override;
  void visit(AST::BinaryExpr *node) override;
  void visit(AST::UnaryExpr *node) override;
  void visit(AST::CallExpr *node) override;
  void visit(AST::MemoryExpr *node) override;
  void visit(AST::MemberAccessExpr *node) override;
  void visit(AST::CastExpr *node) override;

  void visit(AST::CompoundStatement *node) override;
  void visit(AST::ExpressionStatement *node) override;
  void visit(AST::IfStatement *node) override;
  void visit(AST::WhileStatement *node) override;
  void visit(AST::DoWhileStatement *node) override;
  void visit(AST::ForStatement *node) override;
  void visit(AST::ReturnStatement *node) override;
  void visit(AST::BreakStatement *node) override;
  void visit(AST::ContinueStatement *node) override;
  void visit(AST::GotoStatement *node) override;
  void visit(AST::LabelStatement *node) override;
  void visit(AST::CaseStmt *node) override;
  void visit(AST::SwitchStmt *node) override;

private:
  std::stringstream ss_; // Legacy/Build string
  std::vector<Token> tokens_;
  int indentLevel_ = 0;
  std::shared_ptr<AST::FunctionAST> currentFunc_;
  std::set<std::string> usedRegs_; // Track reg_ variables to declare

  void indent();
  void emit(const std::string &str, TokenType type = TokenType::Text,
            uint64_t addr = 0);
  void emitLine(const std::string &str = "", TokenType type = TokenType::Text);
  void collectUsedRegs(const std::shared_ptr<AST::Statement> &stmt);
  void collectUsedRegsExpr(const std::shared_ptr<AST::Expression> &expr);

  std::string inferTypeFromInstruction(uint64_t address);
};

} // namespace ShadPKG::Decompiler::Codegen
