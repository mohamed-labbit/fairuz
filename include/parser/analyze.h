#pragma once


#include "ast.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// Symbol table with type inference
class SymbolTable
{
   public:
    enum class SymbolType { Variable, Function, Class, Module, Unknown };

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
        DataType returnType_ = DataType::Unknown;

        // For type inference
        std::unordered_set<DataType> possibleTypes_;
    };

   private:
    std::unordered_map<std::u16string, Symbol> symbols_;
    SymbolTable* parent_ = nullptr;
    std::vector<std::unique_ptr<SymbolTable>> children_;
    unsigned scopeLevel_ = 0;

   public:
    explicit SymbolTable(SymbolTable* p = nullptr, int level = 0) :
        parent(p),
        scopeLevel(level)
    {
    }

    void define(const std::string& name, Symbol symbol)
    {
        symbol.name = name;
        symbols[name] = std::move(symbol);
    }

    Symbol* lookup(const std::string& name)
    {
        auto it = symbols.find(name);
        if (it != symbols.end())
        {
            return &it->second;
        }
        return parent ? parent->lookup(name) : nullptr;
    }

    Symbol* lookupLocal(const std::string& name)
    {
        auto it = symbols.find(name);
        return it != symbols.end() ? &it->second : nullptr;
    }

    bool isDefined(const std::string& name) const
    {
        if (symbols.count(name))
            return true;
        return parent ? parent->isDefined(name) : false;
    }

    void markUsed(const std::string& name, int line)
    {
        if (auto* sym = lookup(name))
        {
            sym->isUsed = true;
            sym->usageLines.push_back(line);
        }
    }

    SymbolTable* createChild()
    {
        auto child = std::make_unique<SymbolTable>(this, scopeLevel + 1);
        auto* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }

    std::vector<Symbol*> getUnusedSymbols()
    {
        std::vector<Symbol*> unused;
        for (auto& [name, sym] : symbols)
        {
            if (!sym.isUsed && sym.symbolType == SymbolType::Variable)
            {
                unused.push_back(&sym);
            }
        }
        return unused;
    }

    const std::unordered_map<std::string, Symbol>& getSymbols() const { return symbols; }
};

// Control flow graph for advanced analysis
class ControlFlowGraph
{
   public:
    struct BasicBlock
    {
        int id;
        std::vector<StmtPtr*> statements;
        std::vector<int> predecessors;
        std::vector<int> successors;
        bool isReachable = false;

        // Data flow analysis
        std::unordered_set<std::string> defVars;  // Variables defined
        std::unordered_set<std::string> useVars;  // Variables used
        std::unordered_set<std::string> liveIn;  // Live at entry
        std::unordered_set<std::string> liveOut;  // Live at exit
    };

   private:
    std::vector<BasicBlock> blocks;
    int entryBlock = 0;
    int exitBlock = -1;

   public:
    void addBlock(BasicBlock block) { blocks.push_back(std::move(block)); }

    void addEdge(int from, int to)
    {
        if (from >= 0 && from < blocks.size() && to >= 0 && to < blocks.size())
        {
            blocks[from].successors.push_back(to);
            blocks[to].predecessors.push_back(from);
        }
    }

    // Compute reachability
    void computeReachability()
    {
        if (blocks.empty())
            return;

        blocks[entryBlock].isReachable = true;
        std::vector<int> worklist = {entryBlock};

        while (!worklist.empty())
        {
            int blockId = worklist.back();
            worklist.pop_back();

            for (int succ : blocks[blockId].successors)
            {
                if (!blocks[succ].isReachable)
                {
                    blocks[succ].isReachable = true;
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

            for (int i = blocks.size() - 1; i >= 0; --i)
            {
                auto& block = blocks[i];

                // liveOut = union of successors' liveIn
                std::unordered_set<std::string> newLiveOut;
                for (int succ : block.successors)
                {
                    newLiveOut.insert(blocks[succ].liveIn.begin(), blocks[succ].liveIn.end());
                }

                // liveIn = use ∪ (liveOut - def)
                std::unordered_set<std::string> newLiveIn = block.useVars;
                for (const auto& var : newLiveOut)
                {
                    if (!block.defVars.count(var))
                    {
                        newLiveIn.insert(var);
                    }
                }

                if (newLiveIn != block.liveIn || newLiveOut != block.liveOut)
                {
                    block.liveIn = std::move(newLiveIn);
                    block.liveOut = std::move(newLiveOut);
                    changed = true;
                }
            }
        }
    }

    std::vector<int> getUnreachableBlocks() const
    {
        std::vector<int> unreachable;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            if (!blocks[i].isReachable)
            {
                unreachable.push_back(i);
            }
        }
        return unreachable;
    }

    const std::vector<BasicBlock>& getBlocks() const { return blocks; }
};

// Semantic analyzer with type inference and optimization hints
class SemanticAnalyzer
{
   public:
    struct Issue
    {
        enum class Severity { Error, Warning, Info };
        Severity severity;
        std::string message;
        int line;
        std::string suggestion;
    };

   private:
    SymbolTable* currentScope;
    std::unique_ptr<SymbolTable> globalScope;
    std::vector<Issue> issues;
    ControlFlowGraph cfg;

    // Type inference engine
    SymbolTable::DataType inferType(const Expr* expr)
    {
        if (!expr)
            return SymbolTable::DataType::Unknown;

        switch (expr->kind_)
        {
        case Expr::Kind::LITERAL : {
            auto* lit = static_cast<const LiteralExpr*>(expr);
            switch (lit->litType_)
            {
            case LiteralExpr::Type::NUMBER :
                return lit->value_.find('.') != std::string::npos ? SymbolTable::DataType::Float
                                                                  : SymbolTable::DataType::Integer;
            case LiteralExpr::Type::STRING :
                return SymbolTable::DataType::String;
            case LiteralExpr::Type::BOOLEAN :
                return SymbolTable::DataType::Boolean;
            case LiteralExpr::Type::NONE :
                return SymbolTable::DataType::None;
            }
            break;
        }
        case Expr::Kind::NAME : {
            auto* name = static_cast<const NameExpr*>(expr);
            if (auto* sym = currentScope->lookup(name->name_))
            {
                return sym->dataType_;
            }
            break;
        }
        case Expr::Kind::BINARY : {
            auto* bin = static_cast<const BinaryExpr*>(expr);
            auto leftType = inferType(bin->left_.get());
            auto rightType = inferType(bin->right_.get());

            // Type promotion rules
            if (leftType == SymbolTable::DataType::Float || rightType == SymbolTable::DataType::Float)
            {
                return SymbolTable::DataType::Float;
            }
            if (leftType == SymbolTable::DataType::Integer && rightType == SymbolTable::DataType::Integer)
            {
                return SymbolTable::DataType::Integer;
            }

            // String concatenation
            if (bin->op_ == u"+" && leftType == SymbolTable::DataType::String)
            {
                return SymbolTable::DataType::String;
            }

            // Boolean operations
            if (bin->op_ == u"و" || bin->op_ == u"او")
            {
                return SymbolTable::DataType::Boolean;
            }

            break;
        }
        case Expr::Kind::LIST :
            return SymbolTable::DataType::List;
        case Expr::Kind::CALL :
            return SymbolTable::DataType::Any;
        default :
            break;
        }

        return SymbolTable::DataType::Unknown;
    }

    void reportIssue(Issue::Severity sev, const std::string& msg, int line, const std::string& sugg = "")
    {
        issues.push_back({sev, msg, line, sugg});
    }

    void analyzeExpr(const Expr* expr)
    {
        if (!expr)
            return;

        switch (expr->kind_)
        {
        case Expr::Kind::NAME : {
            auto* name = static_cast<const NameExpr*>(expr);
            if (!currentScope->isDefined(name->name_))
            {
                reportIssue(Issue::Severity::Error, u"Undefined variable: " + name->name_, expr->line_,
                  u"Did you forget to initialize it?");
            }
            else
            {
                currentScope->markUsed(name->name_, expr->line_);
            }
            break;
        }
        case Expr::Kind::BINARY : {
            auto* bin = static_cast<const BinaryExpr*>(expr);
            analyzeExpr(bin->left_.get());
            analyzeExpr(bin->right_.get());

            // Type compatibility checking
            auto leftType = inferType(bin->left_.get());
            auto rightType = inferType(bin->right_.get());

            if (leftType != rightType && leftType != SymbolTable::DataType::Unknown
              && rightType != SymbolTable::DataType::Unknown)
            {

                // Check for invalid operations
                if ((bin->op_ == u"-" || bin->op_ == u"*" || bin->op_ == u"/")
                  && (leftType == SymbolTable::DataType::String || rightType == SymbolTable::DataType::String))
                {
                    reportIssue(Issue::Severity::Error, u"Invalid operation on string", expr->line_,
                      u"Strings don't support this operator");
                }
            }

            // Division by zero detection (constant folding)
            if (bin->op_ == u"/" && bin->right_->kind_ == Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<const LiteralExpr*>(bin->right_.get());
                if (lit->value_ == u"0")
                {
                    reportIssue(
                      Issue::Severity::Error, u"Division by zero", expr->line_, u"This will cause a runtime error");
                }
            }
            break;
        }
        case Expr::Kind::UNARY : {
            auto* un = static_cast<const UnaryExpr*>(expr);
            analyzeExpr(un->operand_.get());
            break;
        }
        case Expr::Kind::CALL : {
            auto* call = static_cast<const CallExpr*>(expr);
            analyzeExpr(call->callee_.get());
            for (const auto& arg : call->args_)
            {
                analyzeExpr(arg.get());
            }

            // Check if calling undefined function
            if (call->callee_->kind_ == Expr::Kind::NAME)
            {
                auto* name = static_cast<const NameExpr*>(call->callee_.get());
                if (auto* sym = currentScope->lookup(name->name_))
                {
                    if (sym->symbolType_ != SymbolTable::SymbolType::Function)
                    {
                        reportIssue(Issue::Severity::Error, u"'" + name->name_ + u"' is not callable", expr->line_);
                    }
                }
            }
            break;
        }
        case Expr::Kind::LIST : {
            auto* list = static_cast<const ListExpr*>(expr);
            for (const auto& elem : list->elements_)
            {
                analyzeExpr(elem.get());
            }
            break;
        }
        case Expr::Kind::TERNARY : {
            auto* tern = static_cast<const TernaryExpr*>(expr);
            analyzeExpr(tern->condition_.get());
            analyzeExpr(tern->trueExpr_.get());
            analyzeExpr(tern->falseExpr_.get());
            break;
        }
        default :
            break;
        }
    }

    void analyzeStmt(const Stmt* stmt)
    {
        if (!stmt)
            return;

        switch (stmt->kind_)
        {
        case Stmt::Kind::ASSIGNMENT : {
            auto* assign = static_cast<const AssignmentStmt*>(stmt);
            analyzeExpr(assign->value_.get());

            auto type = inferType(assign->value_.get());
            SymbolTable::Symbol sym;
            sym.symbolType = SymbolTable::SymbolType::Variable;
            sym.dataType = type;
            sym.definitionLine = stmt->line_;

            currentScope->define(assign->target_, sym);
            break;
        }
        case Stmt::Kind::EXPRESSION : {
            auto* exprStmt = static_cast<const ExprStmt*>(stmt);
            analyzeExpr(exprStmt->expression_.get());

            // Warn about unused expression results
            if (exprStmt->expression_->kind_ != Expr::Kind::CALL)
            {
                reportIssue(Issue::Severity::Info, u"Expression result not used", stmt->line_);
            }
            break;
        }
        case Stmt::Kind::IF : {
            auto* ifStmt = static_cast<const IfStmt*>(stmt);
            analyzeExpr(ifStmt->condition_.get());

            // Check for constant conditions
            if (ifStmt->condition_->kind_ == Expr::Kind::LITERAL)
            {
                reportIssue(Issue::Severity::Warning, u"Condition is always constant", stmt->line_,
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
        case Stmt::Kind::WHILE : {
            auto* whileStmt = static_cast<const WhileStmt*>(stmt);
            analyzeExpr(whileStmt->condition_.get());

            // Detect infinite loops
            if (whileStmt->condition_->kind_ == Expr::Kind::LITERAL)
            {
                auto* lit = static_cast<const LiteralExpr*>(whileStmt->condition_.get());
                if (lit->litType_ == LiteralExpr::Type::Boolean && lit->value_ == u"true")
                {
                    reportIssue(
                      Issue::Severity::Warning, u"Infinite loop detected", stmt->line_, u"Add a break condition");
                }
            }

            for (const auto& s : whileStmt->body_)
            {
                analyzeStmt(s.get());
            }
            break;
        }
        case Stmt::Kind::For : {
            auto* forStmt = static_cast<const ForStmt*>(stmt);
            analyzeExpr(forStmt->iter_.get());

            // Create new scope for loop variable
            currentScope = currentScope->createChild();
            SymbolTable::Symbol loopVar;
            loopVar.symbolType_ = SymbolTable::SymbolType::Variable;
            loopVar.dataType_ = SymbolTable::DataType::Any;
            currentScope->define(forStmt->target_, loopVar);

            for (const auto& s : forStmt->body_)
            {
                analyzeStmt(s.get());
            }

            // Check if loop variable is shadowing
            if (currentScope->parent_ && currentScope->parent_->lookupLocal(forStmt->target_))
            {
                reportIssue(Issue::Severity::Warning, "Loop variable shadows outer variable", stmt->line_);
            }

            // Exit loop scope
            currentScope = currentScope->parent_;
            break;
        }
        case Stmt::Kind::FunctionDef : {
            auto* funcDef = static_cast<const FunctionDef*>(stmt);

            SymbolTable::Symbol funcSym;
            funcSym.symbolType_ = SymbolTable::SymbolType::Function;
            funcSym.dataType_ = SymbolTable::DataType::Function;
            funcSym.definitionLine_ = stmt->line_;
            currentScope->define(funcDef->name_, funcSym);

            // Create function scope
            currentScope = currentScope->createChild();

            for (const auto& param : funcDef->params_)
            {
                SymbolTable::Symbol paramSym;
                paramSym.symbolType_ = SymbolTable::SymbolType::Variable;
                paramSym.dataType_ = SymbolTable::DataType::Any;
                currentScope->define(param, paramSym);
            }

            for (const auto& s : funcDef->body)
            {
                analyzeStmt(s.get());
            }

            // Check for missing return statement
            bool hasReturn = false;
            for (const auto& s : funcDef->body_)
            {
                if (s->kind_ == Stmt::Kind::Return)
                {
                    hasReturn = true;
                    break;
                }
            }
            if (!hasReturn)
            {
                reportIssue(Issue::Severity::Info, u"Function may not return a value", stmt->line_);
            }

            // Exit function scope
            currentScope = currentScope->parent_;
            break;
        }
        case Stmt::Kind::Return : {
            auto* ret = static_cast<const ReturnStmt*>(stmt);
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
        globalScope = std::make_unique<SymbolTable>();
        currentScope = globalScope.get();

        // Add built-in functions
        SymbolTable::Symbol printSym;
        printSym.name_ = u"print";
        printSym.symbolType_ = SymbolTable::SymbolType::Function;
        printSym.dataType_ = SymbolTable::DataType::Function;
        globalScope->define(u"print", printSym);
    }

    void analyze(const std::vector<StmtPtr>& statements)
    {
        for (const auto& stmt : statements)
        {
            analyzeStmt(stmt.get());
        }

        // Check for unused variables
        auto unused = globalScope->getUnusedSymbols();
        for (auto* sym : unused)
        {
            reportIssue(Issue::Severity::Warning, u"Unused variable: " + sym->name, sym->definitionLine,
              u"Consider removing if not needed");
        }
    }

    const std::vector<Issue>& getIssues() const { return issues; }

    const SymbolTable* getGlobalScope() const { return globalScope.get(); }

    void printReport() const
    {
        if (issues.empty())
        {
            std::cout << u"✓ No issues found\n";
            return;
        }

        std::cout << u"Found " << issues.size() << u" issue(s):\n\n";
        for (const auto& issue : issues)
        {
            std::string sevStr;
            switch (issue.severity_)
            {
            case Issue::Severity::Error :
                sevStr = u"ERROR";
                break;
            case Issue::Severity::Warning :
                sevStr = u"WARNING";
                break;
            case Issue::Severity::Info :
                sevStr = u"INFO";
                break;
            }

            std::cout << u"[" << sevStr << u"] Line " << issue.line_ << u": " << issue.message_ << u"\n";
            if (!issue.suggestion_.empty())
            {
                std::cout << "  → " << issue.suggestion_ << "\n";
            }
            std::cout << "\n";
        }
    }
};