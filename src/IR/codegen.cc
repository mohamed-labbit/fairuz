#include "../../include/IR/codegen.hpp"

namespace mylang {
namespace IR {

// Exception used to propagate return values up the call stack
struct ReturnException {
    Value value;
    explicit ReturnException(Value v)
        : value(std::move(v))
    {
    }
};

// Exception used to propagate break out of loops
struct BreakException { };

// Exception used to propagate continue in loops
struct ContinueException { };

Value CodeGenerator::eval(ast::ASTNode const* node)
{
    if (!node)
        return Value();

    ast::ASTNode::NodeType type = node->getNodeType();

    switch (type) {
    case ast::ASTNode::NodeType::EXPRESSION: {
        ast::Expr const* expr = dynamic_cast<ast::Expr const*>(node);
        if (!expr)
            return Value();

        ast::Expr::Kind kind = expr->getKind();
        switch (kind) {
        case ast::Expr::Kind::ASSIGNMENT: {
            ast::AssignmentExpr const* assignment_expr = dynamic_cast<ast::AssignmentExpr const*>(expr);
            if (!assignment_expr)
                return Value();

            ast::NameExpr const* target = dynamic_cast<ast::NameExpr const*>(assignment_expr->getTarget());
            if (!target)
                throw std::runtime_error("Invalid assignment target");

            StringRef const& target_name = target->getValue();
            Value value = eval(assignment_expr->getValue());

            if (Env_->exists(target_name))
                Env_->assign(target_name, value);
            else
                Env_->define(target_name, value);

            return value;
        }

        case ast::Expr::Kind::BINARY: {
            ast::BinaryExpr const* binary_expr = dynamic_cast<ast::BinaryExpr const*>(expr);
            if (!binary_expr)
                return Value();

            Value lhs = eval(binary_expr->getLeft());
            Value rhs = eval(binary_expr->getRight());

            ast::BinaryExpr::Op op = binary_expr->getOperator();

            switch (op) {
            case ast::BinaryOp::OP_ADD: return lhs + rhs;
            case ast::BinaryOp::OP_SUB: return lhs - rhs;
            case ast::BinaryOp::OP_MUL: return lhs * rhs;
            case ast::BinaryOp::OP_DIV: return lhs / rhs;
            case ast::BinaryOp::OP_EQ: return lhs == rhs;
            case ast::BinaryOp::OP_GT: return lhs > rhs;
            case ast::BinaryOp::OP_GTE: return lhs >= rhs;
            case ast::BinaryOp::OP_LT: return lhs < rhs;
            case ast::BinaryOp::OP_LTE: return lhs <= rhs;
            case ast::BinaryOp::OP_MOD: return lhs % rhs;
            default:
                throw std::runtime_error("Unknown binary operator");
            }
        }

        case ast::Expr::Kind::CALL: {
            ast::CallExpr const* call_expr = dynamic_cast<ast::CallExpr const*>(expr);
            if (!call_expr)
                return Value();

            ast::NameExpr* name_expr = dynamic_cast<ast::NameExpr*>(call_expr->getCallee());
            if (!name_expr)
                throw std::runtime_error("Invalid function call");

            StringRef func_name = name_expr->getValue();

            std::vector<Value> args;
            for (ast::Expr const* arg_expr : call_expr->getArgs())
                args.push_back(eval(arg_expr));

            return callUserFunction(func_name, args);
        }

        case ast::Expr::Kind::LIST: {
            ast::ListExpr const* list_expr = dynamic_cast<ast::ListExpr const*>(expr);
            if (!list_expr)
                return Value();

            // FIX: empty list should return an empty list Value, not None
            if (list_expr->isEmpty())
                return Value::makeList({});

            std::vector<Value> evaluated_elems;
            for (ast::Expr const* elem : list_expr->getElements())
                evaluated_elems.push_back(eval(elem));

            return Value::makeList(evaluated_elems);
        }

        case ast::Expr::Kind::LITERAL: {
            ast::LiteralExpr const* literal_expr = dynamic_cast<ast::LiteralExpr const*>(expr);
            if (!literal_expr)
                return Value();

            if (literal_expr->isBoolean())
                return literal_expr->getBool();

            if (literal_expr->isNumeric())
                return literal_expr->toNumber();

            if (literal_expr->isString())
                return literal_expr->getStr();

            return Value();
        }

        case ast::Expr::Kind::NAME: {
            ast::NameExpr const* name_expr = dynamic_cast<ast::NameExpr const*>(expr);

            if (!name_expr)
                return Value();

            StringRef name = name_expr->getValue();

            if (!Env_->exists(name))
                throw std::runtime_error(
                    "Undefined variable: " + std::string(name.data()));

            return Env_->get(name);
        }

        case ast::Expr::Kind::UNARY: {
            ast::UnaryExpr const* unary_expr = dynamic_cast<ast::UnaryExpr const*>(expr);
            if (!unary_expr)
                return Value();

            Value operand = eval(unary_expr->getOperand());
            ast::UnaryExpr::Op op = unary_expr->getOperator();

            switch (op) {
            case ast::UnaryOp::OP_NEG:
                return -operand;
            case ast::UnaryOp::OP_NOT:
                return !operand;
            default:
                throw std::runtime_error("Unknown unary operator");
            }
        }

        case ast::Expr::Kind::INVALID:
            throw std::runtime_error("Invalid expression");

        default:
            throw std::runtime_error("Unknown expression kind");
        }
    }

    case ast::ASTNode::NodeType::STATEMENT: {
        ast::Stmt const* statement = static_cast<ast::Stmt const*>(node);
        if (!statement) {
            diagnostic::engine.emit("static cast failed!");
            return Value();
        }

        ast::Stmt::Kind kind = statement->getKind();

        switch (kind) {
        case ast::Stmt::Kind::ASSIGNMENT: {
            ast::AssignmentStmt const* assign_stmt = static_cast<ast::AssignmentStmt const*>(statement);

            Value value = eval(assign_stmt->getValue());
            StringRef const name = static_cast<ast::NameExpr const*>(assign_stmt->getTarget())->getValue();

            if (Env_->exists(name))
                Env_->assign(name, value);
            else
                Env_->define(name, value);

            return Value();
        }

        case ast::Stmt::Kind::BLOCK: {
            ast::BlockStmt const* block_stmt = static_cast<ast::BlockStmt const*>(statement);

            std::vector<ast::Stmt*> statements = block_stmt->getStatements();
            if (statements.empty())
                return Value();

            // FIX: A block returns the value of its last statement (or propagates
            // ReturnException / BreakException / ContinueException upward).
            // We do NOT wrap everything in a list — that was semantically wrong.
            Value result;
            for (ast::Stmt const* s : statements)
                result = eval(s); // ReturnException will propagate naturally

            return result;
        }

        case ast::Stmt::Kind::EXPR: {
            ast::ExprStmt const* expr_stmt = static_cast<ast::ExprStmt const*>(statement);
            if (!expr_stmt) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }
            return eval(expr_stmt->getExpr());
        }

        case ast::Stmt::Kind::FOR: {
            /*
            // FIX: Implement for-each loop properly.
            ast::ForStmt const* for_stmt = static_cast<ast::ForStmt const*>(statement);
            if (!for_stmt) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }

            // Evaluate the iterable
            Value iterable = eval(for_stmt->getIterable());
            if (!iterable.isList())
            throw std::runtime_error("For loop requires an iterable (list)");

            StringRef loop_var = for_stmt->getVariable()->getValue();

            Value result;
            for (Value const& item : *iterable.asList()) {
                // Create a new scope for each iteration
                Environment* loop_env = new Environment(Env_);
                loop_env->define(loop_var, item);

                Environment* prev = Env_;
                Env_ = loop_env;

                try {
                    result = eval(for_stmt->getBlock());
                } catch (BreakException const&) {
                    Env_ = prev;
                    freeEnvironment(loop_env);
                    return result;
                    } catch (ContinueException const&) {
                        // continue to next iteration
                    } catch (...) {
                        Env_ = prev;
                        freeEnvironment(loop_env);
                        throw;
                    }

                    Env_ = prev;
                    freeEnvironment(loop_env);
                }

                return result;
            */
        }

        case ast::Stmt::Kind::FUNC: {
            ast::FunctionDef const* func_def = static_cast<ast::FunctionDef const*>(statement);
            if (!func_def) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }

            StringRef const func_name = func_def->getName()->getValue();
            Value::Function func;
            func.name = func_name;
            // FIX: capture the closure at definition time (correct), but do NOT
            // pre-define parameters into the closure — parameters belong only in
            // the per-call environment created in callUserFunction.
            func.closure = Env_;
            func.body = func_def->getBody();

            ast::ListExpr* param_list = func_def->getParameterList();

            // FIX: store the evaluated parameter name list as a Value
            std::vector<Value> p_vals;
            p_vals.reserve(param_list->size());
            for (ast::Expr* p : param_list->getElements())
                p_vals.push_back(Value(static_cast<ast::NameExpr*>(p)->getValue()));

            func.params = param_list
                ? object_allocator.allocateObject(Value::makeList(p_vals))
                : nullptr;

            Value funcValue = Value::makeFunction(func);
            Env_->define(func_name, funcValue);

            return Value();
        }

        case ast::Stmt::Kind::IF: {
            ast::IfStmt const* if_stmt = static_cast<ast::IfStmt const*>(statement);
            if (!if_stmt) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }

            Value condition = eval(if_stmt->getCondition());

            if (condition.toBool())
                return eval(if_stmt->getThenBlock());
            else if (if_stmt->getElseBlock())
                return eval(if_stmt->getElseBlock());

            return Value();
        }

        case ast::Stmt::Kind::RETURN: {
            ast::ReturnStmt const* return_stmt = static_cast<ast::ReturnStmt const*>(statement);
            if (!return_stmt) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }

            // FIX: evaluate the return expression and throw ReturnException so
            // it unwinds through any nested eval() calls back to callUserFunction.
            Value retval = return_stmt->getValue()
                ? eval(return_stmt->getValue())
                : Value();

            throw ReturnException(std::move(retval));
        }

        case ast::Stmt::Kind::WHILE: {
            ast::WhileStmt const* while_stmt = static_cast<ast::WhileStmt const*>(statement);
            if (!while_stmt) {
                diagnostic::engine.emit("static cast failed");
                return Value();
            }

            Value result;

            while (eval(while_stmt->getCondition()).toBool()) {
                try {
                    result = eval(while_stmt->getBlock());
                } catch (BreakException const&) {
                    break;
                } catch (ContinueException const&) {
                    continue;
                }
            }

            return result;
        }

        case ast::Stmt::Kind::INVALID:
            return Value();

        default:
            throw std::runtime_error("Unknown statement kind");
        }
    }

    default:
        throw std::runtime_error("Unknown AST node type");
    }
}

Value CodeGenerator::callUserFunction(StringRef const& func_name, std::vector<Value> const& args)
{
    if (!Env_->exists(func_name)) {
        diagnostic::engine.emit("function value not found for function name : " + std::string(func_name.data()));
        return Value();
    }

    Value func_value = Env_->get(func_name);
    Value::Function* func = func_value.asFunction();
    if (!func)
        throw std::runtime_error("'" + std::string(func_name.data()) + "' is not a function");

    Value* params = func->params;
    std::size_t params_size = 0;

    if (params && !params->isNone()) {
        if (params->isList())
            params_size = params->asList()->size();
        else
            params_size = 1;
    }

    if (args.size() != params_size)
        throw std::runtime_error(
            "Expected " + std::to_string(params_size) + " arguments but got " + std::to_string(args.size()));

    // FIX: always create the call environment as a child of the *closure*,
    // not of the current Env_. This gives correct lexical scoping.
    Environment* func_env = new Environment(func->closure);

    // Bind arguments to parameter names
    if (params_size > 0) {
        if (params_size == 1) {
            // FIX: single param — get the name from the list element, not from
            // toString() on the whole list Value.
            StringRef param_name = params->isList()
                ? (*params->asList())[0].toString()
                : params->toString();
            func_env->define(param_name, args[0]);
        } else {
            auto const& param_list = *params->asList();
            for (std::size_t i = 0; i < args.size(); ++i)
                func_env->define(param_list[i].toString(), args[i]);
        }
    }

    Environment* prev = Env_;
    Env_ = func_env;

    Value result;

    try {
        result = eval(func->body);
    } catch (ReturnException& ret) {
        // FIX: catch ReturnException and use its value as the function result
        result = std::move(ret.value);
    } catch (...) {
        Env_ = prev;
        freeEnvironment(func_env);
        throw;
    }

    Env_ = prev;
    freeEnvironment(func_env);
    return result;
}

} // namespace IR
} // namespace mylang
