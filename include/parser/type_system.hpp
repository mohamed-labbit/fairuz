#pragma once


#include "../macros.hpp"
#include "ast/ast.hpp"

#include <memory>
#include <unordered_map>
#include <vector>


namespace mylang {
namespace parser {

class TypeSystem
{
 public:
  enum class BaseType { Int, Float, String, Bool, None, Any, List, Dict, Tuple, Set, Function, Class };

  struct Type
  {
    BaseType base;
    std::vector<std::shared_ptr<Type>> typeParams;                  // For generics: List[Int]
    std::unordered_map<string_type, std::shared_ptr<Type>> fields;  // For classes

    // Function signature
    std::vector<std::shared_ptr<Type>> paramTypes;
    std::shared_ptr<Type> returnType;

    bool operator==(const Type& other) const;
    string_type toString() const;
  };

  // Hindley-Milner type inference
  class TypeInference
  {
   private:
    std::int32_t freshVarCounter = 0;
    std::unordered_map<string_type, std::shared_ptr<Type>> substitutions;

    std::shared_ptr<Type> freshTypeVar();
    void unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2);

   public:
    std::shared_ptr<Type> inferExpr(const ast::Expr* expr);
  };
};

}
}