#pragma once


#include "ast.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include "../../utfcpp/source/utf8.h"


namespace mylang {
namespace parser {
namespace symbols {

// Symbol table with type inference
class SymbolTable
{
   public:
    enum class SymbolType { VARIABLE, FUNCTION, CLASS, MODULE, UNKNOWN };

    enum class DataType { INTEGER, FLOAT, STRING, BOOLEAN, LIST, DICT, TUPLE, NONE, FUNCTION, ANY, UNKNOWN };

    struct Symbol
    {
        std::u16string name_;
        SymbolType symbolType_;
        DataType dataType_;
        bool isConstant_ = false;
        bool isGlobal_ = false;
        bool isUsed_ = false;
        int definitionLine_ = 0;
        std::vector<int> usageLines_;

        // For functions
        std::vector<DataType> paramTypes_;
        DataType returnType_ = DataType::UNKNOWN;

        // For type inference
        std::unordered_set<DataType> possibleTypes_;
    };

    SymbolTable* parent_ = nullptr;
   
   private:
    std::unordered_map<std::u16string, Symbol> symbols_;
    std::vector<std::unique_ptr<SymbolTable>> children_;
    unsigned scopeLevel_ = 0;

   public:
    explicit SymbolTable(SymbolTable* p = nullptr, int level = 0) :
        parent_(p),
        scopeLevel_(level)
    {
    }

    void define(const std::u16string& name, Symbol symbol)
    {
        symbol.name_ = name;
        symbols_[name] = std::move(symbol);
    }

    Symbol* lookup(const std::u16string& name)
    {
        auto it = symbols_.find(name);
        if (it != symbols_.end())
        {
            return &it->second;
        }
        return parent_ ? parent_->lookup(name) : nullptr;
    }

    Symbol* lookupLocal(const std::u16string& name)
    {
        auto it = symbols_.find(name);
        return it != symbols_.end() ? &it->second : nullptr;
    }

    bool isDefined(const std::u16string& name) const
    {
        if (symbols_.count(name))
        {
            return true;
        }
        return parent_ ? parent_->isDefined(name) : false;
    }

    void markUsed(const std::u16string& name, int line)
    {
        if (auto* sym = lookup(name))
        {
            sym->isUsed_ = true;
            sym->usageLines_.push_back(line);
        }
    }

    SymbolTable* createChild()
    {
        auto child = std::make_unique<SymbolTable>(this, scopeLevel_ + 1);
        auto* ptr = child.get();
        children_.push_back(std::move(child));
        return ptr;
    }

    std::vector<Symbol*> getUnusedSymbols()
    {
        std::vector<Symbol*> unused;
        for (auto& [name, sym] : symbols_)
        {
            if (!sym.isUsed_ && sym.symbolType_ == SymbolType::VARIABLE)
            {
                unused.push_back(&sym);
            }
        }
        return unused;
    }

    const std::unordered_map<std::u16string, Symbol>& getSymbols() const { return symbols_; }
};

// Control flow graph for advanced analysis
class ControlFlowGraph
{
   public:
    struct BasicBlock
    {
        int id_;
        std::vector<ast::StmtPtr*> statements_;
        std::vector<int> predecessors_;
        std::vector<int> successors_;
        bool isReachable_ = false;

        // Data flow analysis
        std::unordered_set<std::u16string> defVars_;  // Variables defined
        std::unordered_set<std::u16string> useVars_;  // Variables used
        std::unordered_set<std::u16string> liveIn_;  // Live at entry
        std::unordered_set<std::u16string> liveOut_;  // Live at exit
    };

   private:
    std::vector<BasicBlock> blocks_;
    int entryBlock_ = 0;
    int exitBlock_ = -1;

   public:
    void addBlock(BasicBlock block) { blocks_.push_back(std::move(block)); }

    void addEdge(int from, int to)
    {
        if (from >= 0 && from < blocks_.size() && to >= 0 && to < blocks_.size())
        {
            blocks_[from].successors_.push_back(to);
            blocks_[to].predecessors_.push_back(from);
        }
    }

    // Compute reachability
    void computeReachability()
    {
        if (blocks_.empty())
        {
            return;
        }

        blocks_[entryBlock_].isReachable_ = true;
        std::vector<int> worklist = {entryBlock_};

        while (!worklist.empty())
        {
            int blockId = worklist.back();
            worklist.pop_back();

            for (int succ : blocks_[blockId].successors_)
            {
                if (!blocks_[succ].isReachable_)
                {
                    blocks_[succ].isReachable_ = true;
                    worklist.push_back(succ);
                }
            }
        }
    }

    // Liveness analysis for dead code detection
    void computeLiveness()
    {
        bool changed = true;
        while (changed)
        {
            changed = false;

            for (int i = blocks_.size() - 1; i >= 0; --i)
            {
                auto& block = blocks_[i];

                // liveOut = union of successors' liveIn
                std::unordered_set<std::u16string> newLiveOut;
                for (int succ : block.successors_)
                {
                    newLiveOut.insert(blocks_[succ].liveIn_.begin(), blocks_[succ].liveIn_.end());
                }

                // liveIn = use ∪ (liveOut - def)
                std::unordered_set<std::u16string> newLiveIn = block.useVars_;
                for (const std::u16string& var : newLiveOut)
                {
                    if (!block.defVars_.count(var))
                    {
                        newLiveIn.insert(var);
                    }
                }

                if (newLiveIn != block.liveIn_ || newLiveOut != block.liveOut_)
                {
                    block.liveIn_ = std::move(newLiveIn);
                    block.liveOut_ = std::move(newLiveOut);
                    changed = true;
                }
            }
        }
    }

    std::vector<int> getUnreachableBlocks() const
    {
        std::vector<int> unreachable;
        for (size_t i = 0; i < blocks_.size(); ++i)
        {
            if (!blocks_[i].isReachable_)
            {
                unreachable.push_back(i);
            }
        }
        return unreachable;
    }

    const std::vector<BasicBlock>& getBlocks() const { return blocks_; }
};

// Semantic analyzer with type inference and optimization hints
class SemanticAnalyzer
{
   public:
    struct Issue
    {
        enum class Severity { ERROR, WARNING, INFO };
        Severity severity_;
        std::u16string message_;
        int line_;
        std::u16string suggestion_;
    };

   private:
    SymbolTable* currentScope_;
    std::unique_ptr<SymbolTable> globalScope_;
    std::vector<Issue> issues_;
    ControlFlowGraph cfg_;

    // Type inference engine
    SymbolTable::DataType inferType(const ast::Expr* expr)
    {
        if (!expr)
        {
            return SymbolTable::DataType::UNKNOWN;
        }

        switch (expr->kind_)
        {
        case ast::Expr::Kind::LITERAL : {
            auto* lit = static_cast<const ast::LiteralExpr*>(expr);
            switch (lit->litType_)
            {
            case ast::LiteralExpr::Type::NUMBER :
                return lit->value_.find('.') != std::string::npos ? SymbolTable::DataType::FLOAT
                                                                  : SymbolTable::DataType::INTEGER;
            case ast::LiteralExpr::Type::STRING :
                return SymbolTable::DataType::STRING;
            case ast::LiteralExpr::Type::BOOLEAN :
                return SymbolTable::DataType::BOOLEAN;
            case ast::LiteralExpr::Type::NONE :
                return SymbolTable::DataType::NONE;
            }
            break;
        }
        case ast::Expr::Kind::NAME : {
            auto* name = static_cast<const ast::NameExpr*>(expr);
            if (auto* sym = currentScope_->lookup(name->name_))
            {
                return sym->dataType_;
            }
            break;
        }
        case ast::Expr::Kind::BINARY : {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            auto leftType = inferType(bin->left_.get());
            auto rightType = inferType(bin->right_.get());

            // Type promotion rules
            if (leftType == SymbolTable::DataType::FLOAT || rightType == SymbolTable::DataType::FLOAT)
            {
                return SymbolTable::DataType::FLOAT;
            }

            if (leftType == SymbolTable::DataType::INTEGER && rightType == SymbolTable::DataType::INTEGER)
            {
                return SymbolTable::DataType::INTEGER;
            }

            // String concatenation
            if (bin->op_ == u"+" && leftType == SymbolTable::DataType::STRING)
            {
                return SymbolTable::DataType::STRING;
            }

            // Boolean operations
            if (bin->op_ == u"و" || bin->op_ == u"او")
            {
                return SymbolTable::DataType::BOOLEAN;
            }

            break;
        }
        case ast::Expr::Kind::LIST :
            return SymbolTable::DataType::LIST;
        case ast::Expr::Kind::CALL :
            return SymbolTable::DataType::ANY;
        default :
            break;
        }

        return SymbolTable::DataType::UNKNOWN;
    }

    void reportIssue(Issue::Severity sev, const std::u16string& msg, int line, const std::u16string& sugg = u"")
    {
        issues_.push_back({sev, msg, line, sugg});
    }

    void analyzeExpr(const ast::Expr* expr)
    {
        if (!expr)
        {
            return;
        }

        switch (expr->kind_)
        {
        case ast::Expr::Kind::NAME : {
            auto* name = static_cast<const ast::NameExpr*>(expr);
            if (!currentScope_->isDefined(name->name_))
            {
                reportIssue(Issue::Severity::ERROR, u"Undefined variable: " + name->name_, expr->line_,
                  u"Did you forget to initialize it?");
            }
            else
            {
                currentScope_->markUsed(name->name_, expr->line_);
            }
            break;
        }
        case ast::Expr::Kind::BINARY : {
            auto* bin = static_cast<const ast::BinaryExpr*>(expr);
            analyzeExpr(bin->left_.get());
            analyzeExpr(bin->right_.get());

            // Type compatibility checking
            auto leftType = inferType(bin->left_.get());
            auto rightType = inferType(bin->right_.get());

            if (leftType != rightType && leftType != SymbolTable::DataType::UNKNOWN
              && rightType != SymbolTable::DataType::UNKNOWN)
            {

                // Check for invalid operations
                if ((bin->op_ == u"-" || bin->op_ == u"*" || bin->op_ == u"/")
                  && (leftType == SymbolTable::DataType::STRING || rightType == SymbolTable::DataType::STRING))
                {
                    reportIssue(Issue::Severity::ERROR, u"Invalid operation on string", expr->line_,
                      u"Strings don't support this operator");
                }
            }

            // Division by zero detection (constant folding)
            if (bin->op_ == u"/" && bin->right_->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<const ast::LiteralExpr*>(bin->right_.get());
                if (lit->value_ == u"0")
                {
                    reportIssue(
                      Issue::Severity::ERROR, u"Division by zero", expr->line_, u"This will cause a runtime error");
                }
            }
            break;
        }
        case ast::Expr::Kind::UNARY : {
            auto* un = static_cast<const ast::UnaryExpr*>(expr);
            analyzeExpr(un->operand_.get());
            break;
        }
        case ast::Expr::Kind::CALL : {
            auto* call = static_cast<const ast::CallExpr*>(expr);
            analyzeExpr(call->callee_.get());
            for (const auto& arg : call->args_)
            {
                analyzeExpr(arg.get());
            }

            // Check if calling undefined function
            if (call->callee_->kind_ == ast::Expr::Kind::NAME)
            {
                auto* name = static_cast<const ast::NameExpr*>(call->callee_.get());
                if (auto* sym = currentScope_->lookup(name->name_))
                {
                    if (sym->symbolType_ != SymbolTable::SymbolType::FUNCTION)
                    {
                        reportIssue(Issue::Severity::ERROR, u"'" + name->name_ + u"' is not callable", expr->line_);
                    }
                }
            }
            break;
        }
        case ast::Expr::Kind::LIST : {
            auto* list = static_cast<const ast::ListExpr*>(expr);
            for (const auto& elem : list->elements_)
            {
                analyzeExpr(elem.get());
            }
            break;
        }
        case ast::Expr::Kind::TERNARY : {
            auto* tern = static_cast<const ast::TernaryExpr*>(expr);
            analyzeExpr(tern->condition_.get());
            analyzeExpr(tern->trueExpr_.get());
            analyzeExpr(tern->falseExpr_.get());
            break;
        }
        default :
            break;
        }
    }

    void analyzeStmt(const ast::Stmt* stmt)
    {
        if (!stmt)
        {
            return;
        }

        switch (stmt->kind_)
        {
        case ast::Stmt::Kind::ASSIGNMENT : {
            auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
            analyzeExpr(assign->value_.get());

            auto type = inferType(assign->value_.get());
            SymbolTable::Symbol sym;
            sym.symbolType_ = SymbolTable::SymbolType::VARIABLE;
            sym.dataType_ = type;
            sym.definitionLine_ = stmt->line_;

            currentScope_->define(assign->target_, sym);
            break;
        }
        case ast::Stmt::Kind::EXPRESSION : {
            auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
            analyzeExpr(exprStmt->expression_.get());

            // Warn about unused expression results
            if (exprStmt->expression_->kind_ != ast::Expr::Kind::CALL)
            {
                reportIssue(Issue::Severity::INFO, u"Expression result not used", stmt->line_);
            }
            break;
        }
        case ast::Stmt::Kind::IF : {
            auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
            analyzeExpr(ifStmt->condition_.get());

            // Check for constant conditions
            if (ifStmt->condition_->kind_ == ast::Expr::Kind::LITERAL)
            {
                reportIssue(Issue::Severity::WARNING, u"Condition is always constant", stmt->line_,
                  u"Consider removing if statement");
            }

            for (const auto& s : ifStmt->thenBlock_)
            {
                analyzeStmt(s.get());
            }

            for (const auto& s : ifStmt->elseBlock_)
            {
                analyzeStmt(s.get());
            }

            break;
        }
        case ast::Stmt::Kind::WHILE : {
            auto* whileStmt = static_cast<const ast::WhileStmt*>(stmt);
            analyzeExpr(whileStmt->condition_.get());

            // Detect infinite loops
            if (whileStmt->condition_->kind_ == ast::Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<const ast::LiteralExpr*>(whileStmt->condition_.get());
                if (lit->litType_ == ast::LiteralExpr::Type::BOOLEAN && lit->value_ == u"true")
                {
                    reportIssue(
                      Issue::Severity::WARNING, u"Infinite loop detected", stmt->line_, u"Add a break condition");
                }
            }

            for (const auto& s : whileStmt->body_)
            {
                analyzeStmt(s.get());
            }
            break;
        }
        case ast::Stmt::Kind::FOR : {
            auto* forStmt = static_cast<const ast::ForStmt*>(stmt);
            analyzeExpr(forStmt->iter_.get());

            // Create new scope for loop variable
            currentScope_ = currentScope_->createChild();
            SymbolTable::Symbol loopVar;
            loopVar.symbolType_ = SymbolTable::SymbolType::VARIABLE;
            loopVar.dataType_ = SymbolTable::DataType::ANY;
            currentScope_->define(forStmt->target_, loopVar);

            for (const auto& s : forStmt->body_)
            {
                analyzeStmt(s.get());
            }

            // Check if loop variable is shadowing
            if (currentScope_->parent_ && currentScope_->parent_->lookupLocal(forStmt->target_))
            {
                reportIssue(Issue::Severity::WARNING, u"Loop variable shadows outer variable", stmt->line_);
            }

            // Exit loop scope
            currentScope_ = currentScope_->parent_;
            break;
        }
        case ast::Stmt::Kind::FUNCTION_DEF : {
            auto* funcDef = static_cast<const ast::FunctionDef*>(stmt);

            SymbolTable::Symbol funcSym;
            funcSym.symbolType_ = SymbolTable::SymbolType::FUNCTION;
            funcSym.dataType_ = SymbolTable::DataType::FUNCTION;
            funcSym.definitionLine_ = stmt->line_;
            currentScope_->define(funcDef->name_, funcSym);

            // Create function scope
            currentScope_ = currentScope_->createChild();

            for (const auto& param : funcDef->params_)
            {
                SymbolTable::Symbol paramSym;
                paramSym.symbolType_ = SymbolTable::SymbolType::VARIABLE;
                paramSym.dataType_ = SymbolTable::DataType::ANY;
                currentScope_->define(param, paramSym);
            }

            for (const auto& s : funcDef->body_)
            {
                analyzeStmt(s.get());
            }

            // Check for missing return statement
            bool hasReturn = false;
            for (const auto& s : funcDef->body_)
            {
                if (s->kind_ == ast::Stmt::Kind::RETURN)
                {
                    hasReturn = true;
                    break;
                }
            }
            if (!hasReturn)
            {
                reportIssue(Issue::Severity::INFO, u"Function may not return a value", stmt->line_);
            }

            // Exit function scope
            currentScope_ = currentScope_->parent_;
            break;
        }
        case ast::Stmt::Kind::RETURN : {
            auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
            analyzeExpr(ret->value_.get());
            break;
        }
        default :
            break;
        }
    }

   public:
    SemanticAnalyzer()
    {
        globalScope_ = std::make_unique<SymbolTable>();
        currentScope_ = globalScope_.get();

        // Add built-in functions
        SymbolTable::Symbol printSym;
        printSym.name_ = u"print";
        printSym.symbolType_ = SymbolTable::SymbolType::FUNCTION;
        printSym.dataType_ = SymbolTable::DataType::FUNCTION;
        globalScope_->define(u"print", printSym);
    }

    void analyze(const std::vector<ast::StmtPtr>& statements_)
    {
        for (const auto& stmt : statements_)
        {
            analyzeStmt(stmt.get());
        }

        // Check for unused variables
        auto unused = globalScope_->getUnusedSymbols();
        for (auto* sym : unused)
        {
            reportIssue(Issue::Severity::WARNING, u"Unused variable: " + sym->name_, sym->definitionLine_,
              u"Consider removing if not needed");
        }
    }

    const std::vector<Issue>& getIssues() const { return issues_; }

    const SymbolTable* getGlobalScope() const { return globalScope_.get(); }

    void printReport() const
    {
        if (issues_.empty())
        {
            std::cout << u"✓ No issues found\n";
            return;
        }

        std::cout << u"Found " << issues_.size() << u" issue(s):\n\n";
        for (const auto& issue : issues_)
        {
            std::u16string sevStr;
            switch (issue.severity_)
            {
            case Issue::Severity::ERROR :
                sevStr = u"ERROR";
                break;
            case Issue::Severity::WARNING :
                sevStr = u"WARNING";
                break;
            case Issue::Severity::INFO :
                sevStr = u"INFO";
                break;
            }

            std::cout << u"[" << utf8::utf16to8(sevStr) << u"] Line " << issue.line_ << u": " << utf8::utf16to8(issue.message_) << u"\n";
            if (!issue.suggestion_.empty())
            {
                std::cout << "  → " << utf8::utf16to8(issue.suggestion_) << "\n";
            }
            std::cout << "\n";
        }
    }
};

}
}
}