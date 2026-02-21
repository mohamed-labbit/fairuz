#include "../../../include/runtime/compiler/compiler.hpp"
#include "../../../include/IR/codegen.hpp"
#include "../../../include/IR/env.hpp"
#include "../../../include/ast/ast.hpp"

namespace mylang {
namespace runtime {

using namespace ast;

void Compiler::compile(ASTNode const* node)
{
    if (!node)
        return;

    IR::Environment environment;
    IR::CodeGenerator evaluator(&environment);

    ASTNode::NodeType node_type = node->getNodeType();

    if (node_type == ASTNode::NodeType::STATEMENT) [[likely]] {
        Stmt const* statement = static_cast<Stmt const*>(node);
        if (!statement) {
            diagnostic::engine.emit("static cast failed!");
            return;
        }

        Stmt::Kind stmt_kind = statement->getKind();

        switch (stmt_kind) {
        case Stmt::Kind::ASSIGNMENT:
            break;
        case Stmt::Kind::BLOCK:
            break;
        case Stmt::Kind::EXPR:
            break;
        case Stmt::Kind::FOR:
            break;
        case Stmt::Kind::FUNC:
            break;
        case Stmt::Kind::IF:
            break;
        case Stmt::Kind::RETURN:
            break;
        case Stmt::Kind::WHILE:
            break;
        case Stmt::Kind::INVALID:
            break;
        default:
            break;
        }

    } else /*node_type == ASTNode::NodeType::EXPRESSION*/
    {
        Expr const* expression = static_cast<Expr const*>(node);
        if (!expression) {
            diagnostic::engine.emit("static cast failed!");
            return;
        }

        Expr::Kind expr_kind = expression->getKind();

        switch (expr_kind) {
        case Expr::Kind::ASSIGNMENT:
            break;
        case Expr::Kind::BINARY: {
            BinaryExpr const* binary_expr = static_cast<BinaryExpr const*>(expression);
            if (!binary_expr) {
                diagnostic::engine.emit("static cast failed!");
                return;
            }

            compile(binary_expr->getLeft());
            compile(binary_expr->getRight());
        } break;
        case Expr::Kind::CALL:
            break;
        case Expr::Kind::LIST:
            break;
        case Expr::Kind::LITERAL: {
            LiteralExpr const* literal_expr = static_cast<LiteralExpr const*>(expression);
            if (!literal_expr) {
                diagnostic::engine.emit("static cast failed!");
                return;
            }

            IR::Value value = evaluator.eval(literal_expr);

            if (value.isInt() || value.isFloat())
            {
                emitConstant(value);
            }
        } break;
        case Expr::Kind::NAME:
            break;
        case Expr::Kind::UNARY:
            break;
        case Expr::Kind::INVALID:
            break;
        default:
            break;
        }
    }
}

}
}
