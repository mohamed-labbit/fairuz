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
    BaseType                                              base;
    std::vector<std::shared_ptr<Type>>                    TypeParams;  // For generics: List[Int]
    std::unordered_map<StringType, std::shared_ptr<Type>> fields;      // For classes

    // Function signature
    std::vector<std::shared_ptr<Type>> ParamTypes;
    std::shared_ptr<Type>              ReturnType;

    bool       operator==(const Type& other) const;
    StringType toString() const;
  };

  // Hindley-Milner type inference
  class TypeInference
  {
   private:
    std::int32_t                                          FreshVarCounter_{0};
    std::unordered_map<StringType, std::shared_ptr<Type>> Substitutions_;

    std::shared_ptr<Type> freshTypeVar();
    void                  unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2);

   public:
    std::shared_ptr<Type> inferExpr(const ast::Expr* expr);
  };
};

}
}