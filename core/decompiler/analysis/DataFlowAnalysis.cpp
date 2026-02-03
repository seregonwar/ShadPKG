#include "DataFlowAnalysis.h"
#include <iostream>
#include <functional>

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
  
  // Pass 3: Track variable usage for dead code elimination
  trackVariableUsage();
  
  // Pass 4: Eliminate dead code (assignments to unused variables)
  eliminateDeadCode();
}

void DataFlowAnalysis::inferType(const std::string &varName,
                                 AST::Expression::Type type) {
  // Type priority hierarchy:
  // Pointer > Int64 > Int32 > Int16 > Int8 > Unknown
  // Once a type is inferred, only upgrade to higher priority types
  
  if (inferredTypes_.find(varName) == inferredTypes_.end()) {
    inferredTypes_[varName] = type;
    return;
  }
  
  auto current = inferredTypes_[varName];
  
  // If current is already Pointer, don't downgrade
  if (current == AST::Expression::Type::Pointer) {
    return;
  }
  
  // Upgrade to Pointer if needed
  if (type == AST::Expression::Type::Pointer) {
    inferredTypes_[varName] = type;
    return;
  }
  
  // For integer types, upgrade to larger sizes
  if (current == AST::Expression::Type::Unknown) {
    inferredTypes_[varName] = type;
  } else if (type == AST::Expression::Type::Int64 && 
             (current == AST::Expression::Type::Int32 ||
              current == AST::Expression::Type::Int16 ||
              current == AST::Expression::Type::Int8)) {
    inferredTypes_[varName] = type;
  } else if (type == AST::Expression::Type::Int32 && 
             (current == AST::Expression::Type::Int16 ||
              current == AST::Expression::Type::Int8)) {
    inferredTypes_[varName] = type;
  } else if (type == AST::Expression::Type::Int16 && 
             current == AST::Expression::Type::Int8) {
    inferredTypes_[varName] = type;
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

void DataFlowAnalysis::trackVariableUsage() {
  if (!func_ || !func_->body)
    return;

  // Forward declaration for mutual recursion
  std::function<void(const std::shared_ptr<AST::Expression>&)> trackExpr;
  std::function<void(const std::shared_ptr<AST::Statement>&)> trackStmt;

  trackExpr = [&](const std::shared_ptr<AST::Expression> &expr) {
    if (!expr) return;
    
    if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(expr)) {
      usedVariables_.insert(var->name);
    } else if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
      // For assignments, only track RHS as usage
      if (bin->op != AST::BinaryExpr::Op::Assign) {
        trackExpr(bin->left);
      }
      trackExpr(bin->right);
    } else if (auto unary = std::dynamic_pointer_cast<AST::UnaryExpr>(expr)) {
      trackExpr(unary->operand);
    } else if (auto call = std::dynamic_pointer_cast<AST::CallExpr>(expr)) {
      for (const auto &arg : call->arguments) {
        trackExpr(arg);
      }
    } else if (auto cast = std::dynamic_pointer_cast<AST::CastExpr>(expr)) {
      trackExpr(cast->expr);
    }
  };

  trackStmt = [&](const std::shared_ptr<AST::Statement> &stmt) {
    if (!stmt) return;
    
    if (auto compound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
      for (const auto &s : compound->statements) {
        trackStmt(s);
      }
    } else if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(stmt)) {
      // Track assignments
      if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(exprStmt->expression)) {
        if (bin->op == AST::BinaryExpr::Op::Assign) {
          if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(bin->left)) {
            assignedVariables_.insert(var->name);
          }
        }
      }
      trackExpr(exprStmt->expression);
    } else if (auto ifStmt = std::dynamic_pointer_cast<AST::IfStatement>(stmt)) {
      trackExpr(ifStmt->condition);
      trackStmt(ifStmt->thenBranch);
      trackStmt(ifStmt->elseBranch);
    } else if (auto whileStmt = std::dynamic_pointer_cast<AST::WhileStatement>(stmt)) {
      trackExpr(whileStmt->condition);
      trackStmt(whileStmt->body);
    } else if (auto doWhile = std::dynamic_pointer_cast<AST::DoWhileStatement>(stmt)) {
      trackExpr(doWhile->condition);
      trackStmt(doWhile->body);
    } else if (auto forStmt = std::dynamic_pointer_cast<AST::ForStatement>(stmt)) {
      trackExpr(forStmt->init);
      trackExpr(forStmt->condition);
      trackExpr(forStmt->step);
      trackStmt(forStmt->body);
    } else if (auto ret = std::dynamic_pointer_cast<AST::ReturnStatement>(stmt)) {
      trackExpr(ret->value);
    }
  };

  trackStmt(func_->body);
}

void DataFlowAnalysis::eliminateDeadCode() {
  if (!func_ || !func_->body)
    return;

  // Recursively eliminate dead assignments from compound statements
  std::function<void(std::shared_ptr<AST::CompoundStatement>&)> eliminateFromCompound;
  
  eliminateFromCompound = [&](std::shared_ptr<AST::CompoundStatement> &compound) {
    if (!compound) return;
    
    std::vector<std::shared_ptr<AST::Statement>> filtered;
    
    for (const auto &stmt : compound->statements) {
      bool shouldKeep = true;
      
      // Check if this is a dead assignment
      if (auto exprStmt = std::dynamic_pointer_cast<AST::ExpressionStatement>(stmt)) {
        if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(exprStmt->expression)) {
          if (bin->op == AST::BinaryExpr::Op::Assign) {
            if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(bin->left)) {
              // If variable is assigned but never used, it's dead code
              if (usedVariables_.find(var->name) == usedVariables_.end()) {
                shouldKeep = false;
              }
            }
          }
        }
      }
      
      // Recursively process nested compound statements
      if (auto nestedCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(stmt)) {
        eliminateFromCompound(nestedCompound);
      } else if (auto ifStmt = std::dynamic_pointer_cast<AST::IfStatement>(stmt)) {
        if (auto thenCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(ifStmt->thenBranch)) {
          eliminateFromCompound(thenCompound);
        }
        if (auto elseCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(ifStmt->elseBranch)) {
          eliminateFromCompound(elseCompound);
        }
      } else if (auto whileStmt = std::dynamic_pointer_cast<AST::WhileStatement>(stmt)) {
        if (auto bodyCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(whileStmt->body)) {
          eliminateFromCompound(bodyCompound);
        }
      } else if (auto doWhile = std::dynamic_pointer_cast<AST::DoWhileStatement>(stmt)) {
        if (auto bodyCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(doWhile->body)) {
          eliminateFromCompound(bodyCompound);
        }
      } else if (auto forStmt = std::dynamic_pointer_cast<AST::ForStatement>(stmt)) {
        if (auto bodyCompound = std::dynamic_pointer_cast<AST::CompoundStatement>(forStmt->body)) {
          eliminateFromCompound(bodyCompound);
        }
      }
      
      if (shouldKeep) {
        filtered.push_back(stmt);
      }
    }
    
    compound->statements = filtered;
  };
  
  eliminateFromCompound(func_->body);
}

} // namespace ShadPKG::Decompiler::Analysis