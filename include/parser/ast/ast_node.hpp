#pragma once

#include <utility>

namespace mylang {
namespace parser {
namespace ast {

class ASTNode
{
 private:
  std::size_t Line_{};
  std::size_t Column_{};

 public:
  ASTNode()               = default;
  ASTNode(const ASTNode&) = delete;
  ASTNode(ASTNode&&)      = delete;

  ASTNode& operator=(const ASTNode&) = delete;
  ASTNode& operator=(ASTNode&&)      = delete;

  std::size_t getLine() const { return Line_; }
  std::size_t getColumn() const { return Column_; }

  void setLine(std::size_t l) { Line_ = l; }
  void setColumn(std::size_t c) { Column_ = c; }

  virtual ~ASTNode() = default;
};

}
}
}