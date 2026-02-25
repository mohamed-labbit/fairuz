#pragma once

#include "../ast/ast.hpp"
#include "../macros.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace mylang {
namespace parser {

class TypeSystem {
public:
    enum class BaseType {
        INT,
        FLOAT,
        STRING,
        BOOL,
        NONE,
        ANY,
        LIST,
        DICT,
        TUPLE,
        SET,
        FUNCTION,
        CLASS
    };

    struct Type {
        BaseType base;
        std::vector<std::shared_ptr<Type>> TypeParams;                                              // For generics: List[Int]
        std::unordered_map<StringRef, std::shared_ptr<Type>, StringRefHash, StringRefEqual> fields; // For classes

        // Function signature
        std::vector<std::shared_ptr<Type>> ParamTypes;
        std::shared_ptr<Type> ReturnType;

        bool operator==(Type const& other) const;
        StringRef toString() const;
    };

    // Hindley-Milner type inference
    class TypeInference {
    private:
        int32_t FreshVarCounter_ { 0 };
        std::unordered_map<StringRef, std::shared_ptr<Type>, StringRefHash, StringRefEqual> Substitutions_;

        std::shared_ptr<Type> freshTypeVar();
        void unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2);

    public:
        std::shared_ptr<Type> inferExpr(ast::Expr const* expr);
    };
};

}
}
