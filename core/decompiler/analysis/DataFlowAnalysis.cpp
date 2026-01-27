#include "DataFlowAnalysis.h"
#include <iostream>

namespace ShadPKG::Decompiler::Analysis {

DataFlowAnalysis::DataFlowAnalysis(std::shared_ptr<AST::FunctionAST> func)
    : func_(func) {}

void DataFlowAnalysis::analyze() {
  if (!func_ || !func_->body)
    return;

  // Pass 1: Collect constraints/usages
  func_->body->accept(this);

  // Pass 2: Apply inferred types to locals
  applyTypes();
}

void DataFlowAnalysis::inferType(const std::string &varName,
                                 AST::Expression::Type type) {
  // Determine priority: Pointer > Int64 > Int32 > Int8
  // Simple overwrite for now
  if (inferredTypes_.find(varName) == inferredTypes_.end()) {
    inferredTypes_[varName] = type;
  } else {
    // Upgrade type logic
    auto current = inferredTypes_[varName];
    if (current == AST::Expression::Type::Unknown)
      inferredTypes_[varName] = type;
    // else if (type == AST::Expression::Type::Pointer && current !=
    // AST::Expression::Type::Pointer) ...
  }
}

void DataFlowAnalysis::applyTypes() {
  for (auto &local : func_->locals) {
    if (inferredTypes_.count(local.name)) {
      local.type = inferredTypes_[local.name];
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Visitor Implementation
// ═══════════════════════════════════════════════════════════════════════════

void DataFlowAnalysis::visit(AST::VariableExpr *node) {
  // Base usage - nothing specific unless context provided
}

void DataFlowAnalysis::visit(AST::BinaryExpr *node) {
  node->left->accept(this);
  node->right->accept(this);

  // Inference rules
  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(node->left)) {
    if (node->op == AST::BinaryExpr::Op::Assign) {
      // Assigning pointer?
      // If right is a call to malloc? (Ideally Semantic passed here)
    }

    // CMP / Logic -> Int/Bool context
    if (node->op == AST::BinaryExpr::Op::Eq ||
        node->op == AST::BinaryExpr::Op::Gt) {
      // Usually integers
    }
  }
}

void DataFlowAnalysis::visit(AST::UnaryExpr *node) {
  node->operand->accept(this);

  if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(node->operand)) {
    if (node->op == AST::UnaryExpr::Op::Deref) {
      inferType(var->name, AST::Expression::Type::Pointer);
    }
  }
}

void DataFlowAnalysis::visit(AST::CallExpr *node) {
  for (auto &arg : node->arguments) {
    arg->accept(this);
  }
  // known function signatures would help here
}

void DataFlowAnalysis::visit(AST::MemoryExpr *node) {
  // If we have MemoryExpr( Base + Offset )
  // Base is likely a pointer
}

void DataFlowAnalysis::visit(AST::CastExpr *node) {
  if (node->expr)
    node->expr->accept(this);
}

// Control flow traversal
void DataFlowAnalysis::visit(AST::CompoundStatement *node) {
  for (const auto &s : node->statements)
    s->accept(this);
}
void DataFlowAnalysis::visit(AST::ExpressionStatement *node) {
  node->expression->accept(this);
}
void DataFlowAnalysis::visit(AST::IfStatement *node) {
  node->condition->accept(this);
  if (node->thenBranch)
    node->thenBranch->accept(this);
  if (node->elseBranch)
    node->elseBranch->accept(this);
}
void DataFlowAnalysis::visit(AST::WhileStatement *node) {
  node->condition->accept(this);
  if (node->body)
    node->body->accept(this);
}
void DataFlowAnalysis::visit(AST::DoWhileStatement *node) {
  node->condition->accept(this);
  if (node->body)
    node->body->accept(this);
}
void DataFlowAnalysis::visit(AST::ForStatement *node) {}
void DataFlowAnalysis::visit(AST::ReturnStatement *node) {
  if (node->value)
    node->value->accept(this);
}
void DataFlowAnalysis::visit(AST::BreakStatement *node) {}
void DataFlowAnalysis::visit(AST::ContinueStatement *node) {}
void DataFlowAnalysis::visit(AST::GotoStatement *node) {}
void DataFlowAnalysis::visit(AST::LabelStatement *node) {}

void DataFlowAnalysis::visit(AST::CaseStmt *node) {
  if (node->body)
    node->body->accept(this);
}

void DataFlowAnalysis::visit(AST::SwitchStmt *node) {
  if (node->condition)
    node->condition->accept(this);
  for (auto &cse : node->cases) {
    cse->accept(this);
  }
}

} // namespace ShadPKG::Decompiler::Analysis