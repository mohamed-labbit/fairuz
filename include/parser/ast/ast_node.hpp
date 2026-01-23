#pragma once

#include <utility>


namespace mylang {
namespace parser {
namespace ast {

class ASTNode
{
 public:
  enum NodeType { EXPRESSION, STATEMENT };

 private:
  SizeType Line_{};
  SizeType Column_{};

  NodeType NodeType_;

 public:
  ASTNode()               = default;
  ASTNode(const ASTNode&) = delete;
  ASTNode(ASTNode&&)      = delete;

  ASTNode& operator=(const ASTNode&) = delete;
  ASTNode& operator=(ASTNode&&)      = delete;

  NodeType getNodeType() const { return NodeType_; }

  void setNodeType(const NodeType t) { NodeType_ = t; }

  SizeType getLine() const { return Line_; }

  SizeType getColumn() const { return Column_; }

  void setLine(SizeType l) { Line_ = l; }

  void setColumn(SizeType c) { Column_ = c; }

  virtual ~ASTNode() = default;
};

}  // namespace ast
}  // namespace parser
}  // namespace mylang