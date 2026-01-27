#pragma once

#include "../ir/AST.h"
#include <map>
#include <memory>
#include <set>

namespace ShadPKG::Decompiler::Analysis {

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           DATA FLOW ANALYSIS                              ║
// ║                                                                           ║
// ║  Infers types of variables based on usage.                                ║
// ║  - Usage in CMP -> Integer/Boolean                                        ║
// ║  - Usage as Address -> Pointer                                            ║
// ║  - Flow propagation                                                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class DataFlowAnalysis : public AST::ASTVisitor {
public:
  explicit DataFlowAnalysis(std::shared_ptr<AST::FunctionAST> func);

  void analyze();

  // Visitor Overrides
  void visit(AST::ConstantExpr *node) override {}
  void visit(AST::VariableExpr *node) override;
  void visit(AST::BinaryExpr *node) override;
  void visit(AST::UnaryExpr *node) override;
  void visit(AST::CallExpr *node) override;
  void visit(AST::MemoryExpr *node) override;
  void visit(AST::MemberAccessExpr *node) override { node->base->accept(this); }
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
  std::shared_ptr<AST::FunctionAST> func_;

  // Track inferred types
  std::map<std::string, AST::Expression::Type> inferredTypes_;

  void inferType(const std::string &varName, AST::Expression::Type type);
  void applyTypes();
};

} // namespace ShadPKG::Decompiler::Analysis
