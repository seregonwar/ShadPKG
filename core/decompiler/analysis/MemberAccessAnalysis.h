#pragma once

#include "../ir/AST.h"
#include "TypeSystem.h"
#include <memory>
#include <sstream>
#include <iomanip>

namespace ShadPKG::Decompiler::Analysis {

class MemberAccessAnalysis : public AST::ASTVisitor {
public:
  explicit MemberAccessAnalysis(std::shared_ptr<TypeManager> typeManager)
      : typeManager_(typeManager) {}

  void analyze(std::shared_ptr<AST::FunctionAST> func) {
    if (!func || !func->body)
      return;
    func->body->accept(this);
  }

  // Visitor Implementation
  void visit(AST::ConstantExpr *node) override {}
  void visit(AST::VariableExpr *node) override {}

  void visit(AST::BinaryExpr *node) override {
    node->left = transform(node->left);
    node->right = transform(node->right);
  }

  void visit(AST::UnaryExpr *node) override { 
      node->operand = transform(node->operand); 
  }

  void visit(AST::CallExpr *node) override {
    for (auto &arg : node->arguments)
      arg = transform(arg);
  }

  void visit(AST::MemoryExpr *node) override {}

  void visit(AST::MemberAccessExpr *node) override { node->base->accept(this); }

  void visit(AST::CastExpr *node) override { node->expr = transform(node->expr); }

  void visit(AST::CompoundStatement *node) override {
    for (auto &stmt : node->statements)
      stmt->accept(this);
  }

  void visit(AST::ExpressionStatement *node) override {
    // We need to be able to replace node->expression if it changes.
    // Let's call a helper "transform" that returns the new expression.
    node->expression = transform(node->expression);
  }

  void visit(AST::IfStatement *node) override {
    node->condition = transform(node->condition);
    if (node->thenBranch)
      node->thenBranch->accept(this);
    if (node->elseBranch)
      node->elseBranch->accept(this);
  }

  void visit(AST::WhileStatement *node) override {
    node->condition = transform(node->condition);
    if (node->body)
      node->body->accept(this);
  }

  void visit(AST::DoWhileStatement *node) override {
    node->condition = transform(node->condition);
    if (node->body)
      node->body->accept(this);
  }

  void visit(AST::ForStatement *node) override {} // Unimplemented

  void visit(AST::ReturnStatement *node) override {
    if (node->value)
      node->value = transform(node->value);
  }

  void visit(AST::BreakStatement *node) override {}
  void visit(AST::ContinueStatement *node) override {}
  void visit(AST::GotoStatement *node) override {}
  void visit(AST::LabelStatement *node) override {}
  void visit(AST::CaseStmt *node) override {
    // Switch bodies (CaseStmt) contain a CompoundStatement body
    if (node->body)
      node->body->accept(this);
  }
  void visit(AST::SwitchStmt *node) override {
    node->condition = transform(node->condition);
    for (auto &c : node->cases)
      c->accept(this);
  }

private:
  std::shared_ptr<TypeManager> typeManager_;

  std::shared_ptr<AST::Expression>
  transform(std::shared_ptr<AST::Expression> expr) {
    if (!expr)
      return nullptr;

    // Recurse children first? Or handle this node?
    // Let's handle bottom-up in general, but here top-down might be enough.

    // 1. Check if expr is MemoryExpr (dereference)
    if (auto mem = std::dynamic_pointer_cast<AST::MemoryExpr>(expr)) {
      // Check content of memory access
      // Usually: *(base + offset) or *(base)

      // We need to look inside mem->address (Expression)
      // It is likely a BinaryExpr (ADD) or VariableExpr

      /*
       Case 1: *(ptr) -> ptr->member (at offset 0)
       Case 2: *(ptr + 4) -> ptr->member (at offset 4)
      */

      // Case 1: *base (offset 0) -> mem->offset is null
      // Case 2: base[offset] -> mem->offset is set

      std::shared_ptr<AST::Expression> base = mem->base;
      int offset = 0;

      if (mem->offset) {
        if (auto cnst =
                std::dynamic_pointer_cast<AST::ConstantExpr>(mem->offset)) {
          offset = (int)cnst->intValue;
        } else {
          // Dynamic offset, cannot resolve to static member access easily
          return mem; // Just enable recursion on base? No, return as is.
        }
      }

      if (base) {
        // Infer Struct
        if (auto var = std::dynamic_pointer_cast<AST::VariableExpr>(base)) {
             // DO NOT create structs for RIP-relative access
             if (var->name == "rip") return mem;

             // Create struct for this variable
             std::string structName = "Struct_" + var->name;
             
             auto type = typeManager_->getType(structName);
             std::shared_ptr<StructType> structType;
             if (!type) {
                 structType = typeManager_->createStruct(structName);
             } else {
                 structType = std::dynamic_pointer_cast<StructType>(type);
             }

             if (structType) {
                 // Check if member exists
                 const StructMember* member = structType->getMemberAt(offset);
                 if (!member) {
                     std::stringstream ss;
                     ss << "field_0x" << std::hex << offset;
                     structType->addMember(ss.str(), typeManager_->getType("int"), offset);
                     member = structType->getMemberAt(offset);
                 }
                 
                 // Transform this MemoryExpr into MemberAccessExpr
                 // base->field
                 return std::make_shared<AST::MemberAccessExpr>(
                     base, 
                     member->name, 
                     offset);
             }
        }
      }
    }

    // Recurse for children if not replaced
    // This 'transform' helper is manual recursion.
    // Existing Visitor pattern doesn't support easy rewrite.
    // I'll implement a simple Recursive Rewriter logic here for specific nodes.

    if (auto bin = std::dynamic_pointer_cast<AST::BinaryExpr>(expr)) {
      bin->left = transform(bin->left);
      bin->right = transform(bin->right);
    }
    // ... (other recursive calls)

    return expr;
  }
};

} // namespace ShadPKG::Decompiler::Analysis
