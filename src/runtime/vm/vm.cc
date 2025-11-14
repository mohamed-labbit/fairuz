#include "../../../include/runtime/vm/vm.hpp"
#include "../../../include/runtime/object/object.hpp"

#include <iostream>


namespace mylang {
namespace runtime {

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::define(const std::string& name, bool isParam)
{
    if (symbols.count(name))
    {
        return &symbols[name];
    }

    Symbol sym;
    sym.name = name;
    sym.index = nextIndex++;
    sym.scope = parent ? SymbolScope::Local : SymbolScope::Global;
    sym.isParameter = isParam;
    sym.isCaptured = false;
    sym.isUsed = false;

    symbols[name] = sym;
    return &symbols[name];
}

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::resolve(const std::string& name)
{
    // Check local scope
    auto it = symbols.find(name);
    if (it != symbols.end())
    {
        it->second.isUsed = true;
        return &it->second;
    }

    // Check parent scopes
    if (parent)
    {
        Symbol* parentSym = parent->resolve(name);
        if (parentSym)
        {
            // Mark as captured for closure
            parentSym->isCaptured = true;

            // Create closure reference
            Symbol closureSym;
            closureSym.name = name;
            closureSym.index = freeVars.size();
            closureSym.scope = SymbolScope::Closure;
            closureSym.isUsed = true;

            freeVars.push_back(name);
            symbols[name] = closureSym;
            return &symbols[name];
        }
    }

    return nullptr;
}

const std::vector<std::string>& CompilerSymbolTable::getFreeVars() const { return freeVars; }

int CompilerSymbolTable::getLocalCount() const { return nextIndex; }

std::vector<typename CompilerSymbolTable::Symbol> CompilerSymbolTable::getUnusedSymbols() const
{
    std::vector<Symbol> unused;
    for (const auto& [name, sym] : symbols)
    {
        if (!sym.isUsed && !sym.isParameter)
        {
            unused.push_back(sym);
        }
    }
    return unused;
}

int ConstantPool::addConstant(const object::Value& val)
{
    if (val.isInt())
    {
        long long v = val.asInt();
        auto it = intConstants.find(v);
        if (it != intConstants.end())
        {
            return it->second;
        }

        int idx = constants.size();
        constants.push_back(val);
        intConstants[v] = idx;
        return idx;
    }

    if (val.isFloat())
    {
        double v = val.asFloat();
        auto it = floatConstants.find(v);
        if (it != floatConstants.end())
        {
            return it->second;
        }

        int idx = constants.size();
        constants.push_back(val);
        floatConstants[v] = idx;
        return idx;
    }

    if (val.isString())
    {
        const std::u16string& v = val.asString();
        auto it = stringConstants.find(v);
        if (it != stringConstants.end())
        {
            return it->second;
        }

        int idx = constants.size();
        constants.push_back(val);
        stringConstants[v] = idx;
        return idx;
    }

    // Other types - no deduplication
    int idx = constants.size();
    constants.push_back(val);
    return idx;
}

const std::vector<object::Value>& ConstantPool::getConstants() const { return constants; }

void ConstantPool::optimize()
{
    // Remove unused constants (requires usage tracking)
}

void JumpResolver::defineLabel(const std::string& name, int position) { labels[name] = position; }

void JumpResolver::addJump(int instrIndex, const std::string& target)
{
    auto it = labels.find(target);
    if (it != labels.end())
    {
        // Label already defined - immediate resolution
        // (Would patch instruction here)
    }
    else
    {
        pendingJumps.push_back({instrIndex, target});
    }
}

void JumpResolver::resolveJumps(std::vector<bytecode::Instruction>& instructions)
{
    for (const auto& jump : pendingJumps)
    {
        auto it = labels.find(jump.labelName);
        if (it != labels.end())
        {
            instructions[jump.instructionIndex].arg = it->second;
        }
        else
        {
            throw std::runtime_error("Undefined label: " + jump.labelName);
        }
    }
}

int JumpResolver::getLabel(const std::string& name) const
{
    auto it = labels.find(name);
    return it != labels.end() ? it->second : -1;
}

void LoopAnalyzer::detectLoops(const std::vector<bytecode::Instruction>& instructions)
{
    // Detect back-edges (jumps to earlier instructions)
    for (size_t i = 0; i < instructions.size(); i++)
    {
        const auto& instr = instructions[i];
        if ((instr.op == bytecode::OpCode::JUMP_BACKWARD || instr.op == bytecode::OpCode::FOR_ITER) && instr.arg < i)
        {

            Loop loop;
            loop.headerPC = instr.arg;
            loop.exitPC = i;
            loop.isInnerLoop = true;
            loop.nestingLevel = 1;

            // Collect loop body
            for (int pc = instr.arg; pc <= i; pc++)
            {
                loop.bodyPCs.push_back(pc);
            }

            loops.push_back(loop);
        }
    }

    // Calculate nesting levels
    for (auto& outer : loops)
    {
        for (const auto& inner : loops)
        {
            if (inner.headerPC > outer.headerPC && inner.exitPC < outer.exitPC)
            {
                outer.isInnerLoop = false;
                // inner is nested in outer
            }
        }
    }
}

void LoopAnalyzer::findInvariants(
  const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols)
{
    for (auto& loop : loops)
    {
        // Variables that are:
        // 1. Defined outside loop
        // 2. Not modified inside loop
        // 3. Used inside loop
        // Can be hoisted out

        std::unordered_set<int> modifiedVars;
        std::unordered_set<int> usedVars;

        for (int pc : loop.bodyPCs)
        {
            const auto& instr = instructions[pc];
            if (instr.op == bytecode::OpCode::STORE_VAR || instr.op == bytecode::OpCode::STORE_FAST)
            {
                modifiedVars.insert(instr.arg);
            }
            if (instr.op == bytecode::OpCode::LOAD_VAR || instr.op == bytecode::OpCode::LOAD_FAST)
            {
                usedVars.insert(instr.arg);
            }
        }

        for (int var : usedVars)
        {
            if (!modifiedVars.count(var))
            {
                loop.invariants.insert(var);
            }
        }
    }
}

const std::vector<typename LoopAnalyzer::Loop>& LoopAnalyzer::getLoops() const { return loops; }

bool PeepholeOptimizer::matchPattern(
  const std::vector<bytecode::Instruction>& code, size_t pos, const std::vector<bytecode::OpCode>& pattern)
{
    if (pos + pattern.size() > code.size())
    {
        return false;
    }

    for (size_t i = 0; i < pattern.size(); i++)
    {
        if (code[pos + i].op != pattern[i])
        {
            return false;
        }
    }
    return true;
}


void PeepholeOptimizer::optimize(std::vector<bytecode::Instruction>& instructions)
{
    int replacements = 0;

    // Pattern 1: LOAD_CONST followed by POP -> remove both
    for (size_t i = 0; i + 1 < instructions.size();)
    {
        if (instructions[i].op == bytecode::OpCode::LOAD_CONST && instructions[i + 1].op == bytecode::OpCode::POP)
        {
            instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
            replacements++;
            continue;
        }
        i++;
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Const-Pop elimination", replacements});
    }

    // Pattern 2: LOAD x, STORE x -> remove (redundant load-store)
    replacements = 0;
    for (size_t i = 0; i + 1 < instructions.size();)
    {
        auto& instr1 = instructions[i];
        auto& instr2 = instructions[i + 1];

        if ((instr1.op == bytecode::OpCode::LOAD_VAR && instr2.op == bytecode::OpCode::STORE_VAR)
          || (instr1.op == bytecode::OpCode::LOAD_FAST && instr2.op == bytecode::OpCode::STORE_FAST))
        {
            if (instr1.arg == instr2.arg)
            {
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                replacements++;
                continue;
            }
        }
        i++;
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Load-Store elimination", replacements});
    }

    // Pattern 3: DUP, POP -> remove both (useless dup)
    replacements = 0;
    for (size_t i = 0; i + 1 < instructions.size();)
    {
        if (instructions[i].op == bytecode::OpCode::DUP && instructions[i + 1].op == bytecode::OpCode::POP)
        {
            instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
            replacements++;
            continue;
        }
        i++;
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Dup-Pop elimination", replacements});
    }

    // Pattern 4: JUMP to next instruction -> remove
    replacements = 0;
    for (size_t i = 0; i < instructions.size();)
    {
        if (instructions[i].op == bytecode::OpCode::JUMP && instructions[i].arg == i + 1)
        {
            instructions.erase(instructions.begin() + i);
            replacements++;
            continue;
        }
        i++;
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Redundant jump elimination", replacements});
    }

    // Pattern 5: NOT, NOT -> remove both (double negation)
    replacements = 0;
    for (size_t i = 0; i + 1 < instructions.size();)
    {
        if (instructions[i].op == bytecode::OpCode::NOT && instructions[i + 1].op == bytecode::OpCode::NOT)
        {
            instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
            replacements++;
            continue;
        }
        i++;
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Double negation elimination", replacements});
    }

    // Pattern 6: Use fast opcodes for common operations
    replacements = 0;
    for (auto& instr : instructions)
    {
        if (instr.op == bytecode::OpCode::ADD)
        {
            instr.op = bytecode::OpCode::ADD_FAST;
            replacements++;
        }
        else if (instr.op == bytecode::OpCode::SUB)
        {
            instr.op = bytecode::OpCode::SUB_FAST;
            replacements++;
        }
        else if (instr.op == bytecode::OpCode::MUL)
        {
            instr.op = bytecode::OpCode::MUL_FAST;
            replacements++;
        }
    }
    if (replacements > 0)
    {
        optimizations.push_back({"Fast opcode substitution", replacements});
    }
}

const std::vector<typename PeepholeOptimizer::Optimization>& PeepholeOptimizer::getOptimizations() const
{
    return optimizations;
}

void PeepholeOptimizer::printReport() const
{
    if (optimizations.empty())
    {
        std::cout << "  No peephole optimizations applied\n";
        return;
    }

    std::cout << "  Peephole Optimizations:\n";
    for (const auto& opt : optimizations)
    {
        std::cout << "    • " << opt.name << ": " << opt.name << " replacements\n";
    }
}

void BytecodeCompiler::emit(bytecode::OpCode op, int arg, int line)
{
    bytecode::Instruction instr(op, arg, line);
    unit.instructions.push_back(instr);
    stats.instructionsGenerated++;

    // Track stack depth
    updateStackDepth(op);

    // Track line numbers
    if (line > 0)
    {
        unit.lineNumbers.push_back(line);
    }
}


void BytecodeCompiler::updateStackDepth(bytecode::OpCode op)
{
    // Update stack depth based on operation
    switch (op)
    {
    case bytecode::OpCode::LOAD_CONST :
    case bytecode::OpCode::LOAD_VAR :
    case bytecode::OpCode::LOAD_GLOBAL :
    case bytecode::OpCode::LOAD_FAST :
    case bytecode::OpCode::DUP :
        currentStackDepth++;
        break;

    case bytecode::OpCode::POP :
    case bytecode::OpCode::STORE_VAR :
    case bytecode::OpCode::STORE_GLOBAL :
    case bytecode::OpCode::STORE_FAST :
        currentStackDepth--;
        break;

    case bytecode::OpCode::ADD :
    case bytecode::OpCode::SUB :
    case bytecode::OpCode::MUL :
    case bytecode::OpCode::DIV :
    case bytecode::OpCode::MOD :
    case bytecode::OpCode::POW :
    case bytecode::OpCode::EQ :
    case bytecode::OpCode::NE :
    case bytecode::OpCode::LT :
    case bytecode::OpCode::GT :
    case bytecode::OpCode::LE :
    case bytecode::OpCode::GE :
    case bytecode::OpCode::AND :
    case bytecode::OpCode::OR :
        currentStackDepth--;  // Two operands -> one result
        break;

    case bytecode::OpCode::CALL :
        // Function + args -> result
        // Stack depth change handled separately
        break;

    default :
        break;
    }

    maxStackDepth = std::max(maxStackDepth, currentStackDepth);
}

int BytecodeCompiler::getCurrentPC() const { return unit.instructions.size(); }

void BytecodeCompiler::patchJump(int jumpIndex) { unit.instructions[jumpIndex].arg = getCurrentPC(); }

void BytecodeCompiler::enterScope()
{
    auto* newScope = new CompilerSymbolTable(currentScope.get());
    if (currentScope)
    {
        scopeStack.push(currentScope.release());
    }
    currentScope.reset(newScope);
}

void BytecodeCompiler::exitScope()
{
    if (!scopeStack.empty())
    {
        currentScope.reset(scopeStack.top());
        scopeStack.pop();
    }
}

void BytecodeCompiler::compileExpr(const parser::ast::Expr* expr)
{
    if (!expr)
        return;

    switch (expr->kind)
    {
    case parser::ast::Expr::Kind::LITERAL : {
        auto* lit = static_cast<const parser::ast::LiteralExpr*>(expr);
        object::Value val;

        switch (lit->litType)
        {
        case parser::ast::LiteralExpr::Type::NUMBER : {
            if (lit->value.find('.') != std::string::npos)
            {
                val = object::Value(std::stod(utf8::utf16to8(lit->value)));
            }
            else
            {
                val = object::Value(std::stoll(utf8::utf16to8(lit->value)));
            }
            break;
        }
        case parser::ast::LiteralExpr::Type::STRING :
            val = object::Value(lit->value);
            break;
        case parser::ast::LiteralExpr::Type::BOOLEAN :
            val = object::Value(lit->value == u"true" || lit->value == u"صحيح");
            break;
        case parser::ast::LiteralExpr::Type::NONE :
            val = object::Value();
            break;
        }

        int idx = constants.addConstant(val);
        emit(bytecode::OpCode::LOAD_CONST, idx, expr->line);
        break;
    }

    case parser::ast::Expr::Kind::NAME : {
        auto* name = static_cast<const parser::ast::NameExpr*>(expr);
        auto* sym = currentScope->resolve(utf8::utf16to8(name->name));

        if (!sym)
        {
            throw std::runtime_error("Undefined variable: " + utf8::utf16to8(name->name));
        }

        switch (sym->scope)
        {
        case CompilerSymbolTable::SymbolScope::Global :
            emit(bytecode::OpCode::LOAD_GLOBAL, sym->index, expr->line);
            break;
        case CompilerSymbolTable::SymbolScope::Local :
            emit(bytecode::OpCode::LOAD_FAST, sym->index, expr->line);
            break;
        case CompilerSymbolTable::SymbolScope::Closure :
            emit(bytecode::OpCode::LOAD_CLOSURE, sym->index, expr->line);
            break;
        default :
            emit(bytecode::OpCode::LOAD_VAR, sym->index, expr->line);
            break;
        }
        break;
    }

    case parser::ast::Expr::Kind::BINARY : {
        auto* bin = static_cast<const parser::ast::BinaryExpr*>(expr);

        // Short-circuit evaluation for logical operators
        if (bin->op == u"و" || bin->op == u"and")
        {
            compileExpr(bin->left.get());
            emit(bytecode::OpCode::DUP, 0, expr->line);
            int jumpIfFalse = getCurrentPC();
            emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);
            emit(bytecode::OpCode::POP, 0, expr->line);
            compileExpr(bin->right.get());
            patchJump(jumpIfFalse);
            break;
        }

        if (bin->op == u"او" || bin->op == u"or")
        {
            compileExpr(bin->left.get());
            emit(bytecode::OpCode::DUP, 0, expr->line);
            int jumpIfTrue = getCurrentPC();
            emit(bytecode::OpCode::POP_JUMP_IF_TRUE, 0, expr->line);
            emit(bytecode::OpCode::POP, 0, expr->line);
            compileExpr(bin->right.get());
            patchJump(jumpIfTrue);
            break;
        }

        // Regular binary operations
        compileExpr(bin->left.get());
        compileExpr(bin->right.get());

        if (bin->op == u"+")
        {
            emit(bytecode::OpCode::ADD, 0, expr->line);
        }
        else if (bin->op == u"-")
        {
            emit(bytecode::OpCode::SUB, 0, expr->line);
        }
        else if (bin->op == u"*")
        {
            emit(bytecode::OpCode::MUL, 0, expr->line);
        }
        else if (bin->op == u"/")
        {
            emit(bytecode::OpCode::DIV, 0, expr->line);
        }
        else if (bin->op == u"//")
        {
            emit(bytecode::OpCode::FLOOR_DIV, 0, expr->line);
        }
        else if (bin->op == u"%")
        {
            emit(bytecode::OpCode::MOD, 0, expr->line);
        }
        else if (bin->op == u"**")
        {
            emit(bytecode::OpCode::POW, 0, expr->line);
        }
        else if (bin->op == u"==")
        {
            emit(bytecode::OpCode::EQ, 0, expr->line);
        }
        else if (bin->op == u"!=")
        {
            emit(bytecode::OpCode::NE, 0, expr->line);
        }
        else if (bin->op == u"<")
        {
            emit(bytecode::OpCode::LT, 0, expr->line);
        }
        else if (bin->op == u">")
        {
            emit(bytecode::OpCode::GT, 0, expr->line);
        }
        else if (bin->op == u"<=")
        {
            emit(bytecode::OpCode::LE, 0, expr->line);
        }
        else if (bin->op == u">=")
        {
            emit(bytecode::OpCode::GE, 0, expr->line);
        }
        else if (bin->op == u"&")
        {
            emit(bytecode::OpCode::BITAND, 0, expr->line);
        }
        else if (bin->op == u"|")
        {
            emit(bytecode::OpCode::BITOR, 0, expr->line);
        }
        else if (bin->op == u"^")
        {
            emit(bytecode::OpCode::BITXOR, 0, expr->line);
        }
        else if (bin->op == u"<<")
        {
            emit(bytecode::OpCode::LSHIFT, 0, expr->line);
        }
        else if (bin->op == u">>")
        {
            emit(bytecode::OpCode::RSHIFT, 0, expr->line);
        }
        else if (bin->op == u"في" || bin->op == u"in")
        {
            emit(bytecode::OpCode::IN, 0, expr->line);
        }
        break;
    }

    case parser::ast::Expr::Kind::UNARY : {
        auto* un = static_cast<const parser::ast::UnaryExpr*>(expr);
        compileExpr(un->operand.get());

        if (un->op == u"-")
        {
            emit(bytecode::OpCode::NEG, 0, expr->line);
        }
        else if (un->op == u"+")
        {
            emit(bytecode::OpCode::POS, 0, expr->line);
        }
        else if (un->op == u"~")
        {
            emit(bytecode::OpCode::BITNOT, 0, expr->line);
        }
        else if (un->op == u"ليس" || un->op == u"not")
        {
            emit(bytecode::OpCode::NOT, 0, expr->line);
        }
        break;
    }

    case parser::ast::Expr::Kind::CALL : {
        auto* call = static_cast<const parser::ast::CallExpr*>(expr);

        // Compile arguments first
        for (const auto& arg : call->args)
        {
            compileExpr(arg.get());
        }

        // Compile callee
        compileExpr(call->callee.get());

        // Emit call instruction
        emit(bytecode::OpCode::CALL, call->args.size(), expr->line);

        // Adjust stack depth
        currentStackDepth -= call->args.size();
        break;
    }

    case parser::ast::Expr::Kind::LIST : {
        auto* list = static_cast<const parser::ast::ListExpr*>(expr);

        // Compile all elements
        for (const auto& elem : list->elements)
        {
            compileExpr(elem.get());
        }

        // Build list from stack
        emit(bytecode::OpCode::BUILD_LIST, list->elements.size(), expr->line);

        // Adjust stack depth
        currentStackDepth -= list->elements.size();
        currentStackDepth++;
        break;
    }

    case parser::ast::Expr::Kind::TERNARY : {
        auto* tern = static_cast<const parser::ast::TernaryExpr*>(expr);

        // Compile condition
        compileExpr(tern->condition.get());

        int jumpIfFalse = getCurrentPC();
        emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);

        // True branch
        compileExpr(tern->trueExpr.get());
        int jumpEnd = getCurrentPC();
        emit(bytecode::OpCode::JUMP, 0, expr->line);

        // False branch
        patchJump(jumpIfFalse);
        if (tern->falseExpr)
        {
            compileExpr(tern->falseExpr.get());
        }

        patchJump(jumpEnd);
        break;
    }

    case parser::ast::Expr::Kind::ASSIGNMENT : {
        auto* assign = static_cast<const parser::ast::AssignmentExpr*>(expr);

        // Compile value
        compileExpr(assign->value.get());

        // Duplicate for expression result
        emit(bytecode::OpCode::DUP, 0, expr->line);

        // Store
        auto* sym = currentScope->define(utf8::utf16to8(assign->target));
        if (sym->scope == CompilerSymbolTable::SymbolScope::Global)
        {
            emit(bytecode::OpCode::STORE_GLOBAL, sym->index, expr->line);
        }
        else
        {
            emit(bytecode::OpCode::STORE_FAST, sym->index, expr->line);
        }
        break;
    }

    default :
        throw std::runtime_error("Unsupported expression type");
    }
}

void BytecodeCompiler::compileStmt(const parser::ast::Stmt* stmt)
{
    if (!stmt)
    {
        return;
    }

    switch (stmt->kind)
    {
    case parser::ast::Stmt::Kind::ASSIGNMENT : {
        auto* assign = static_cast<const parser::ast::AssignmentStmt*>(stmt);

        // Compile value
        compileExpr(assign->value.get());

        // Store to variable
        auto* sym = currentScope->define(utf8::utf16to8(assign->target));

        if (sym->scope == CompilerSymbolTable::SymbolScope::Global)
        {
            emit(bytecode::OpCode::STORE_GLOBAL, sym->index, stmt->line);
        }
        else
        {
            emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);
        }
        break;
    }

    case parser::ast::Stmt::Kind::EXPRESSION : {
        auto* exprStmt = static_cast<const parser::ast::ExprStmt*>(stmt);
        compileExpr(exprStmt->expression.get());
        emit(bytecode::OpCode::POP, 0, stmt->line);  // Discard result
        break;
    }

    case parser::ast::Stmt::Kind::IF : {
        auto* ifStmt = static_cast<const parser::ast::IfStmt*>(stmt);

        // Compile condition
        compileExpr(ifStmt->condition.get());

        int jumpIfFalse = getCurrentPC();
        emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);

        // Then block
        for (const auto& s : ifStmt->thenBlock)
        {
            compileStmt(s.get());
        }

        if (!ifStmt->elseBlock.empty())
        {
            int jumpEnd = getCurrentPC();
            emit(bytecode::OpCode::JUMP, 0, stmt->line);

            patchJump(jumpIfFalse);

            // Else block
            for (const auto& s : ifStmt->elseBlock)
            {
                compileStmt(s.get());
            }

            patchJump(jumpEnd);
        }
        else
        {
            patchJump(jumpIfFalse);
        }
        break;
    }

    case parser::ast::Stmt::Kind::WHILE : {
        auto* whileStmt = static_cast<const parser::ast::WhileStmt*>(stmt);

        int loopStart = getCurrentPC();

        // Mark as potential hot loop
        emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);

        // Compile condition
        compileExpr(whileStmt->condition.get());

        int jumpIfFalse = getCurrentPC();
        emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);

        // Loop body
        LoopContext ctx;
        ctx.startPC = loopStart;
        ctx.continueLabel = loopStart;
        ctx.breakLabel = -1;  // Will be patched
        loopStack.push(ctx);

        for (const auto& s : whileStmt->body)
        {
            compileStmt(s.get());
        }

        // Jump back to loop start
        emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);

        patchJump(jumpIfFalse);
        emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);

        // Patch break statements
        if (loopStack.top().breakLabel != -1)
        {
            patchJump(loopStack.top().breakLabel);
        }
        loopStack.pop();

        stats.loopsDetected++;
        break;
    }

    case parser::ast::Stmt::Kind::FOR : {
        auto* forStmt = static_cast<const parser::ast::ForStmt*>(stmt);

        // Compile iterator
        compileExpr(forStmt->iter.get());
        emit(bytecode::OpCode::GET_ITER, 0, stmt->line);

        int loopStart = getCurrentPC();
        emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);

        // FOR_ITER gets next item or jumps to end
        int forIter = getCurrentPC();
        emit(bytecode::OpCode::FOR_ITER_FAST, 0, stmt->line);

        // Store loop variable
        auto* sym = currentScope->define(utf8::utf16to8(forStmt->target));
        emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);

        // Loop body
        LoopContext ctx;
        ctx.startPC = loopStart;
        ctx.continueLabel = loopStart;
        ctx.breakLabel = -1;
        loopStack.push(ctx);

        for (const auto& s : forStmt->body)
        {
            compileStmt(s.get());
        }

        // Jump back
        emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);

        // Patch FOR_ITER to jump here when exhausted
        patchJump(forIter);
        emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);

        if (loopStack.top().breakLabel != -1)
        {
            patchJump(loopStack.top().breakLabel);
        }
        loopStack.pop();

        stats.loopsDetected++;
        break;
    }

    case parser::ast::Stmt::Kind::FUNCTION_DEF : {
        auto* funcDef = static_cast<const parser::ast::FunctionDef*>(stmt);

        // Enter new scope for function
        enterScope();

        // Define parameters
        for (const auto& param : funcDef->params)
        {
            currentScope->define(utf8::utf16to8(param), true);
        }

        // Save current compilation state
        auto savedInstructions = std::move(unit.instructions);
        auto savedConstants = constants;
        int savedStackDepth = currentStackDepth;
        int savedMaxStackDepth = maxStackDepth;

        unit.instructions.clear();
        currentStackDepth = 0;
        maxStackDepth = 0;

        // Compile function body
        for (const auto& s : funcDef->body)
        {
            compileStmt(s.get());
        }

        // Implicit return None if no return statement
        if (unit.instructions.empty() || unit.instructions.back().op != bytecode::OpCode::RETURN)
        {
            int noneIdx = constants.addConstant(object::Value());
            emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
            emit(bytecode::OpCode::RETURN, 0, stmt->line);
        }

        // Create function object
        auto funcInstructions = std::move(unit.instructions);
        int funcStackSize = maxStackDepth;

        // Restore compilation state
        unit.instructions = std::move(savedInstructions);
        constants = savedConstants;
        currentStackDepth = savedStackDepth;
        maxStackDepth = savedMaxStackDepth;

        // Store function object as constant
        object::Value funcObj;  // Would create FunctionObject here
        int funcIdx = constants.addConstant(funcObj);

        emit(bytecode::OpCode::LOAD_CONST, funcIdx, stmt->line);
        emit(bytecode::OpCode::MAKE_FUNCTION, funcDef->params.size(), stmt->line);

        // Store function
        auto* sym = currentScope->define(utf8::utf16to8(funcDef->name));
        emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);

        exitScope();
        break;
    }

    case parser::ast::Stmt::Kind::RETURN : {
        auto* ret = static_cast<const parser::ast::ReturnStmt*>(stmt);

        if (ret->value)
        {
            compileExpr(ret->value.get());
        }
        else
        {
            int noneIdx = constants.addConstant(object::Value());
            emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
        }

        emit(bytecode::OpCode::RETURN, 0, stmt->line);
        break;
    }

    case parser::ast::Stmt::Kind::BLOCK : {
        auto* block = static_cast<const parser::ast::BlockStmt*>(stmt);
        for (const auto& s : block->statements)
        {
            compileStmt(s.get());
        }
        break;
    }

    default :
        throw std::runtime_error("Unsupported statement type");
    }
}


typename BytecodeCompiler::CompilationUnit BytecodeCompiler::compile(const std::vector<parser::ast::StmtPtr>& ast)
{
    // Reset state
    unit = CompilationUnit();
    stats = Stats();
    currentStackDepth = 0;
    maxStackDepth = 0;

    // Compile all statements
    for (const auto& stmt : ast)
    {
        compileStmt(stmt.get());
    }

    // Add HALT at end
    emit(bytecode::OpCode::HALT, 0, 0);

    // Finalize constant pool
    unit.constants = constants.getConstants();
    stats.constantsPoolSize = unit.constants.size();

    // Resolve all jumps
    jumps.resolveJumps(unit.instructions);

    // Detect loops for optimization
    loopAnalyzer.detectLoops(unit.instructions);
    loopAnalyzer.findInvariants(unit.instructions, *currentScope);

    // Apply peephole optimizations
    peephole.optimize(unit.instructions);
    stats.peepholeOptimizations = peephole.getOptimizations().size();

    // Set metadata
    unit.numLocals = currentScope->getLocalCount();
    unit.stackSize = maxStackDepth;

    // Report unused variables
    auto unused = currentScope->getUnusedSymbols();
    if (!unused.empty())
    {
        std::cout << "[Compiler] Warning: " << unused.size() << " unused variables detected\n";
    }

    return unit;
}

void BytecodeCompiler::disassemble(const CompilationUnit& unit, std::ostream& out) const
{
    out << "=== Bytecode Disassembly ===\n\n";
    out << "Constants Pool (" << unit.constants.size() << " entries):\n";
    for (size_t i = 0; i < unit.constants.size(); i++)
    {
        out << "  [" << i << "] " << unit.constants[i].toString() << "\n";
    }

    out << "\nCode (" << unit.instructions.size() << " instructions):\n";
    out << "Stack size: " << unit.stackSize << "\n";
    out << "Locals: " << unit.numLocals << "\n\n";

    for (size_t i = 0; i < unit.instructions.size(); i++)
    {
        const auto& instr = unit.instructions[i];

        out << std::setw(6) << i << "  ";

        // Line number
        if (instr.lineNumber > 0)
        {
            out << std::setw(4) << instr.lineNumber << "  ";
        }
        else
        {
            out << "      ";
        }

        // Opcode name
        out << std::left << std::setw(20) << opcodeToString(instr.op);

        // Argument
        if (needsArg(instr.op))
        {
            out << std::setw(6) << instr.arg;

            // Show constant value for LOAD_CONST
            if (instr.op == bytecode::OpCode::LOAD_CONST && instr.arg < unit.constants.size())
            {
                out << "  ; " << unit.constants[instr.arg].toString();
            }
        }

        out << "\n";
    }

    out << "\n=== Compilation Statistics ===\n";
    out << "Instructions generated: " << stats.instructionsGenerated << "\n";
    out << "Constants in pool: " << stats.constantsPoolSize << "\n";
    out << "Loops detected: " << stats.loopsDetected << "\n";
    out << "Peephole optimizations: " << stats.peepholeOptimizations << "\n";

    // Loop analysis
    const auto& loops = loopAnalyzer.getLoops();
    if (!loops.empty())
    {
        out << "\nLoop Analysis:\n";
        for (size_t i = 0; i < loops.size(); i++)
        {
            const auto& loop = loops[i];
            out << "  Loop " << i << ": PC " << loop.headerPC << " -> " << loop.exitPC
                << " (nesting: " << loop.nestingLevel << ")\n";
            out << "    Invariants: " << loop.invariants.size() << " variables\n";
        }
    }

    // Peephole report
    out << "\n";
    peephole.printReport();
}


void BytecodeCompiler::optimizationReport(std::ostream& out) const
{
    out << "\n╔═══════════════════════════════════════╗\n";
    out << "║    Bytecode Optimization Report       ║\n";
    out << "╚═══════════════════════════════════════╝\n\n";

    out << "Code Size:\n";
    out << "  Instructions: " << stats.instructionsGenerated << "\n";
    out << "  Constants: " << stats.constantsPoolSize << "\n";
    out << "  Stack size: " << maxStackDepth << "\n\n";

    out << "Loop Optimizations:\n";
    out << "  Loops detected: " << stats.loopsDetected << "\n";
    const auto& loops = loopAnalyzer.getLoops();
    int totalInvariants = 0;
    for (const auto& loop : loops)
    {
        totalInvariants += loop.invariants.size();
    }
    out << "  Hoistable invariants: " << totalInvariants << "\n\n";

    out << "Peephole Optimizations:\n";
    if (stats.peepholeOptimizations > 0)
    {
        peephole.printReport();
    }
    else
    {
        out << "  None applied\n";
    }
}

std::string BytecodeCompiler::opcodeToString(bytecode::OpCode op) const
{
    switch (op)
    {
    case bytecode::OpCode::LOAD_CONST :
        return "LOAD_CONST";
    case bytecode::OpCode::LOAD_VAR :
        return "LOAD_VAR";
    case bytecode::OpCode::LOAD_GLOBAL :
        return "LOAD_GLOBAL";
    case bytecode::OpCode::LOAD_FAST :
        return "LOAD_FAST";
    case bytecode::OpCode::STORE_VAR :
        return "STORE_VAR";
    case bytecode::OpCode::STORE_GLOBAL :
        return "STORE_GLOBAL";
    case bytecode::OpCode::STORE_FAST :
        return "STORE_FAST";
    case bytecode::OpCode::POP :
        return "POP";
    case bytecode::OpCode::DUP :
        return "DUP";
    case bytecode::OpCode::SWAP :
        return "SWAP";
    case bytecode::OpCode::ROT_THREE :
        return "ROT_THREE";
    case bytecode::OpCode::ADD :
        return "ADD";
    case bytecode::OpCode::ADD_FAST :
        return "ADD_FAST";
    case bytecode::OpCode::SUB :
        return "SUB";
    case bytecode::OpCode::SUB_FAST :
        return "SUB_FAST";
    case bytecode::OpCode::MUL :
        return "MUL";
    case bytecode::OpCode::MUL_FAST :
        return "MUL_FAST";
    case bytecode::OpCode::DIV :
        return "DIV";
    case bytecode::OpCode::FLOOR_DIV :
        return "FLOOR_DIV";
    case bytecode::OpCode::MOD :
        return "MOD";
    case bytecode::OpCode::POW :
        return "POW";
    case bytecode::OpCode::NEG :
        return "NEG";
    case bytecode::OpCode::POS :
        return "POS";
    case bytecode::OpCode::BITAND :
        return "BITAND";
    case bytecode::OpCode::BITOR :
        return "BITOR";
    case bytecode::OpCode::BITXOR :
        return "BITXOR";
    case bytecode::OpCode::BITNOT :
        return "BITNOT";
    case bytecode::OpCode::LSHIFT :
        return "LSHIFT";
    case bytecode::OpCode::RSHIFT :
        return "RSHIFT";
    case bytecode::OpCode::EQ :
        return "EQ";
    case bytecode::OpCode::NE :
        return "NE";
    case bytecode::OpCode::LT :
        return "LT";
    case bytecode::OpCode::GT :
        return "GT";
    case bytecode::OpCode::LE :
        return "LE";
    case bytecode::OpCode::GE :
        return "GE";
    case bytecode::OpCode::IN :
        return "IN";
    case bytecode::OpCode::NOT_IN :
        return "NOT_IN";
    case bytecode::OpCode::IS :
        return "IS";
    case bytecode::OpCode::IS_NOT :
        return "IS_NOT";
    case bytecode::OpCode::AND :
        return "AND";
    case bytecode::OpCode::OR :
        return "OR";
    case bytecode::OpCode::NOT :
        return "NOT";
    case bytecode::OpCode::JUMP :
        return "JUMP";
    case bytecode::OpCode::JUMP_FORWARD :
        return "JUMP_FORWARD";
    case bytecode::OpCode::JUMP_BACKWARD :
        return "JUMP_BACKWARD";
    case bytecode::OpCode::JUMP_IF_FALSE :
        return "JUMP_IF_FALSE";
    case bytecode::OpCode::JUMP_IF_TRUE :
        return "JUMP_IF_TRUE";
    case bytecode::OpCode::POP_JUMP_IF_FALSE :
        return "POP_JUMP_IF_FALSE";
    case bytecode::OpCode::POP_JUMP_IF_TRUE :
        return "POP_JUMP_IF_TRUE";
    case bytecode::OpCode::FOR_ITER :
        return "FOR_ITER";
    case bytecode::OpCode::FOR_ITER_FAST :
        return "FOR_ITER_FAST";
    case bytecode::OpCode::CALL :
        return "CALL";
    case bytecode::OpCode::CALL_FAST :
        return "CALL_FAST";
    case bytecode::OpCode::RETURN :
        return "RETURN";
    case bytecode::OpCode::YIELD :
        return "YIELD";
    case bytecode::OpCode::BUILD_LIST :
        return "BUILD_LIST";
    case bytecode::OpCode::BUILD_DICT :
        return "BUILD_DICT";
    case bytecode::OpCode::BUILD_TUPLE :
        return "BUILD_TUPLE";
    case bytecode::OpCode::BUILD_SET :
        return "BUILD_SET";
    case bytecode::OpCode::UNPACK_SEQUENCE :
        return "UNPACK_SEQUENCE";
    case bytecode::OpCode::GET_ITEM :
        return "GET_ITEM";
    case bytecode::OpCode::SET_ITEM :
        return "SET_ITEM";
    case bytecode::OpCode::GET_ITER :
        return "GET_ITER";
    case bytecode::OpCode::MAKE_FUNCTION :
        return "MAKE_FUNCTION";
    case bytecode::OpCode::LOAD_CLOSURE :
        return "LOAD_CLOSURE";
    case bytecode::OpCode::PRINT :
        return "PRINT";
    case bytecode::OpCode::NOP :
        return "NOP";
    case bytecode::OpCode::HALT :
        return "HALT";
    case bytecode::OpCode::HOT_LOOP_START :
        return "HOT_LOOP_START";
    case bytecode::OpCode::HOT_LOOP_END :
        return "HOT_LOOP_END";
    default :
        return "UNKNOWN";
    }
}

bool BytecodeCompiler::needsArg(bytecode::OpCode op) const
{
    return op != bytecode::OpCode::POP && op != bytecode::OpCode::DUP && op != bytecode::OpCode::ADD
      && op != bytecode::OpCode::SUB && op != bytecode::OpCode::MUL && op != bytecode::OpCode::DIV
      && op != bytecode::OpCode::NEG && op != bytecode::OpCode::NOT && op != bytecode::OpCode::RETURN
      && op != bytecode::OpCode::HALT && op != bytecode::OpCode::NOP;
}

void BytecodeOptimizer::optimize(std::vector<bytecode::Instruction>& code, int maxIterations)
{
    bool changed = true;
    int iteration = 0;

    while (changed && iteration < maxIterations)
    {
        changed = false;

        for (auto& pass : passes)
        {
            if (pass.apply(code))
            {
                pass.applicationsCount++;
                changed = true;
            }
        }

        iteration++;
    }
}

void BytecodeOptimizer::printReport(std::ostream& out) const
{
    out << "\nBytecode Optimizer Report:\n";
    for (const auto& pass : passes)
    {
        if (pass.applicationsCount > 0)
        {
            out << "  • " << pass.name << ": " << pass.applicationsCount << " applications\n";
        }
    }
}

bool BytecodeOptimizer::isJumpTarget(const std::vector<bytecode::Instruction>& code, size_t pos)
{
    // Check if any instruction jumps to this position
    for (const auto& instr : code)
    {
        if (isJumpOp(instr.op) && instr.arg == pos)
        {
            return true;
        }
    }
    return false;
}

bool BytecodeOptimizer::isJumpOp(bytecode::OpCode op)
{
    return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
      || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE
      || op == bytecode::OpCode::POP_JUMP_IF_FALSE || op == bytecode::OpCode::POP_JUMP_IF_TRUE;
}

bool BytecodeOptimizer::isBinaryOp(bytecode::OpCode op)
{
    return op == bytecode::OpCode::ADD || op == bytecode::OpCode::SUB || op == bytecode::OpCode::MUL
      || op == bytecode::OpCode::DIV || op == bytecode::OpCode::MOD || op == bytecode::OpCode::POW;
}

bool BytecodeVerifier::verify(const BytecodeCompiler::CompilationUnit& unit)
{
    errors.clear();

    // Check 1: Valid jump targets
    for (size_t i = 0; i < unit.instructions.size(); i++)
    {
        const auto& instr = unit.instructions[i];

        if (isJumpInstruction(instr.op))
        {
            if (instr.arg < 0 || instr.arg >= unit.instructions.size())
            {
                errors.push_back({static_cast<int>(i), "Jump target out of bounds: " + std::to_string(instr.arg)});
            }
        }
    }

    // Check 2: Stack balance
    std::vector<int> stackDepths(unit.instructions.size(), -1);
    verifyStackDepth(unit, 0, 0, stackDepths);

    // Check 3: Constant pool bounds
    for (size_t i = 0; i < unit.instructions.size(); i++)
    {
        const auto& instr = unit.instructions[i];

        if (instr.op == bytecode::OpCode::LOAD_CONST)
        {
            if (instr.arg < 0 || instr.arg >= unit.constants.size())
            {
                errors.push_back({static_cast<int>(i), "Constant index out of bounds: " + std::to_string(instr.arg)});
            }
        }
    }

    // Check 4: All paths return (for functions)
    // Would implement dataflow analysis here

    return errors.empty();
}

const std::vector<typename BytecodeVerifier::VerificationError>& BytecodeVerifier::getErrors() const { return errors; }

void BytecodeVerifier::printErrors(std::ostream& out) const
{
    if (errors.empty())
    {
        out << "✓ Bytecode verification passed\n";
        return;
    }

    out << "✗ Bytecode verification failed with " << errors.size() << " error(s):\n";
    for (const auto& err : errors)
    {
        out << "  PC " << err.pc << ": " << err.message << "\n";
    }
}

void BytecodeVerifier::verifyStackDepth(
  const BytecodeCompiler::CompilationUnit& unit, int pc, int depth, std::vector<int>& depths)
{
    if (pc >= unit.instructions.size())
    {
        return;
    }

    // Already visited with same or greater depth
    if (depths[pc] >= depth)
    {
        return;
    }

    depths[pc] = depth;

    const auto& instr = unit.instructions[pc];
    int newDepth = depth + getStackEffect(instr.op, instr.arg);

    if (newDepth < 0)
    {
        errors.push_back({pc, "Stack underflow"});
        return;
    }

    if (newDepth > unit.stackSize)
    {
        errors.push_back(
          {pc, "Stack overflow (depth " + std::to_string(newDepth) + " > " + std::to_string(unit.stackSize) + ")"});
    }

    // Follow control flow
    if (instr.op == bytecode::OpCode::RETURN || instr.op == bytecode::OpCode::HALT)
    {
        return;
    }

    if (isJumpInstruction(instr.op))
    {
        verifyStackDepth(unit, instr.arg, newDepth, depths);
        if (!isUnconditionalJump(instr.op))
        {
            verifyStackDepth(unit, pc + 1, newDepth, depths);
        }
    }
    else
    {
        verifyStackDepth(unit, pc + 1, newDepth, depths);
    }
}

int BytecodeVerifier::getStackEffect(bytecode::OpCode op, int arg) const
{
    switch (op)
    {
    case bytecode::OpCode::LOAD_CONST :
    case bytecode::OpCode::LOAD_VAR :
    case bytecode::OpCode::LOAD_FAST :
    case bytecode::OpCode::LOAD_GLOBAL :
    case bytecode::OpCode::DUP :
        return 1;

    case bytecode::OpCode::POP :
    case bytecode::OpCode::STORE_VAR :
    case bytecode::OpCode::STORE_FAST :
    case bytecode::OpCode::STORE_GLOBAL :
        return -1;

    case bytecode::OpCode::ADD :
    case bytecode::OpCode::SUB :
    case bytecode::OpCode::MUL :
    case bytecode::OpCode::DIV :
    case bytecode::OpCode::MOD :
    case bytecode::OpCode::POW :
    case bytecode::OpCode::EQ :
    case bytecode::OpCode::NE :
    case bytecode::OpCode::LT :
    case bytecode::OpCode::GT :
    case bytecode::OpCode::LE :
    case bytecode::OpCode::GE :
        return -1;  // 2 operands -> 1 result

    case bytecode::OpCode::BUILD_LIST :
    case bytecode::OpCode::BUILD_TUPLE :
    case bytecode::OpCode::BUILD_SET :
        return 1 - arg;  // n items -> 1 collection

    case bytecode::OpCode::CALL :
        return -arg;  // func + n args -> result

    default :
        return 0;
    }
}

bool BytecodeVerifier::isJumpInstruction(bytecode::OpCode op) const
{
    return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
      || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE
      || op == bytecode::OpCode::POP_JUMP_IF_FALSE || op == bytecode::OpCode::POP_JUMP_IF_TRUE
      || op == bytecode::OpCode::FOR_ITER;
}

bool BytecodeVerifier::isUnconditionalJump(bytecode::OpCode op) const
{
    return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD
      || op == bytecode::OpCode::JUMP_BACKWARD;
}

void VirtualMachine::push(const object::Value& val)
{
    if (stack_.size() >= 10000)
    {
        throw std::runtime_error("Stack overflow");
    }
    stack_.push_back(val);
}

object::Value VirtualMachine::pop()
{
    if (stack_.empty())
    {
        throw std::runtime_error("Stack underflow");
    }
    object::Value val = stack_.back();
    stack_.pop_back();
    return val;
}

object::Value& VirtualMachine::top()
{
    if (stack_.empty())
    {
        throw std::runtime_error("Stack empty");
    }
    return stack_.back();
}

object::Value& VirtualMachine::peek(int offset)
{
    if (stack_.size() < offset + 1)
    {
        throw std::runtime_error("Stack underflow in peek");
    }
    return stack_[stack_.size() - 1 - offset];
}

// Fast integer operations (avoid type checking overhead)
object::Value VirtualMachine::fastAdd(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() + b.asInt());
    }
    return a + b;
}

object::Value VirtualMachine::fastSub(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() - b.asInt());
    }
    return a - b;
}

object::Value VirtualMachine::fastMul(const object::Value& a, const object::Value& b)
{
    if (a.isInt() && b.isInt())
    {
        return object::Value(a.asInt() * b.asInt());
    }
    return a * b;
}

void VirtualMachine::registerNativeFunctions()
{
    // print
    nativeFunctions["print"] = [](const std::vector<object::Value>& args) {
        for (size_t i = 0; i < args.size(); i++)
        {
            std::cout << args[i].toString();
            if (i + 1 < args.size())
            {
                std::cout << " ";
            }
        }
        std::cout << "\n";
        return object::Value();
    };

    // len
    nativeFunctions["len"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("len() takes 1 argument");
        }
        if (args[0].isList())
        {
            return object::Value(static_cast<long long>(args[0].asList().size()));
        }
        if (args[0].isString())
        {
            return object::Value(static_cast<long long>(args[0].asString().size()));
        }
        throw std::runtime_error("len() requires list or string");
    };

    // range
    nativeFunctions["range"] = [](const std::vector<object::Value>& args) {
        if (args.empty() || args.size() > 3)
        {
            throw std::runtime_error("range() takes 1-3 arguments");
        }

        long long start = 0, stop, step = 1;
        if (args.size() == 1)
        {
            stop = args[0].toInt();
        }
        else if (args.size() == 2)
        {
            start = args[0].toInt();
            stop = args[1].toInt();
        }
        else
        {
            start = args[0].toInt();
            stop = args[1].toInt();
            step = args[2].toInt();
        }

        std::vector<object::Value> result;
        for (long long i = start; (step > 0) ? (i < stop) : (i > stop); i += step)
        {
            result.push_back(object::Value(i));
        }
        return object::Value(result);
    };

    // type
    nativeFunctions["type"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("type() takes 1 argument");
        }

        std::u16string typeName;
        switch (args[0].getType())
        {
        case object::Value::Type::NONE :
            typeName = u"<class 'NoneType'>";
            break;
        case object::Value::Type::INT :
            typeName = u"<class 'int'>";
            break;
        case object::Value::Type::FLOAT :
            typeName = u"<class 'float'>";
            break;
        case object::Value::Type::STRING :
            typeName = u"<class 'str'>";
            break;
        case object::Value::Type::BOOL :
            typeName = u"<class 'bool'>";
            break;
        case object::Value::Type::LIST :
            typeName = u"<class 'list'>";
            break;
        case object::Value::Type::DICT :
            typeName = u"<class 'dict'>";
            break;
        default :
            typeName = u"<class 'object'>";
            break;
        }
        return object::Value(typeName);
    };

    // str
    nativeFunctions["str"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("str() takes 1 argument");
        }
        return object::Value(args[0].asString());
    };

    // int
    nativeFunctions["int"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("int() takes 1 argument");
        }
        return object::Value(args[0].toInt());
    };

    // float
    nativeFunctions["float"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("float() takes 1 argument");
        }
        return object::Value(args[0].toFloat());
    };

    // sum
    nativeFunctions["sum"] = [](const std::vector<object::Value>& args) {
        if (args.empty())
        {
            throw std::runtime_error("sum() takes at least 1 argument");
        }

        if (!args[0].isList())
        {
            throw std::runtime_error("sum() requires iterable");
        }

        object::Value total = args.size() > 1 ? args[1] : object::Value(0LL);
        for (const auto& item : args[0].asList())
        {
            total = total + item;
        }
        return total;
    };

    // abs
    nativeFunctions["abs"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("abs() takes 1 argument");
        }
        if (args[0].isInt())
        {
            return object::Value(std::abs(args[0].asInt()));
        }
        return object::Value(std::abs(args[0].toFloat()));
    };

    // min, max
    nativeFunctions["min"] = [](const std::vector<object::Value>& args) {
        if (args.empty())
        {
            throw std::runtime_error("min() requires at least 1 argument");
        }
        if (args.size() == 1 && args[0].isList())
        {
            const auto& list = args[0].asList();
            if (list.empty())
            {
                throw std::runtime_error("min() arg is empty sequence");
            }
            object::Value minVal = list[0];
            for (size_t i = 1; i < list.size(); i++)
            {
                if (list[i] < minVal)
                {
                    minVal = list[i];
                }
            }
            return minVal;
        }
        object::Value minVal = args[0];
        for (size_t i = 1; i < args.size(); i++)
        {
            if (args[i] < minVal)
            {
                minVal = args[i];
            }
        }
        return minVal;
    };

    nativeFunctions["max"] = [](const std::vector<object::Value>& args) {
        if (args.empty())
        {
            throw std::runtime_error("max() requires at least 1 argument");
        }
        if (args.size() == 1 && args[0].isList())
        {
            const auto& list = args[0].asList();
            if (list.empty())
            {
                throw std::runtime_error("max() arg is empty sequence");
            }
            object::Value maxVal = list[0];
            for (size_t i = 1; i < list.size(); i++)
            {
                if (list[i] > maxVal)
                {
                    maxVal = list[i];
                }
            }
            return maxVal;
        }
        object::Value maxVal = args[0];
        for (size_t i = 1; i < args.size(); i++)
        {
            if (args[i] > maxVal)
            {
                maxVal = args[i];
            }
        }
        return maxVal;
    };

    // sorted
    nativeFunctions["sorted"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("sorted() takes 1 argument");
        }
        if (!args[0].isList())
        {
            throw std::runtime_error("sorted() requires list");
        }

        auto result = args[0].asList();
        std::sort(result.begin(), result.end());
        return object::Value(result);
    };

    // reversed
    nativeFunctions["reversed"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("reversed() takes 1 argument");
        }
        if (!args[0].isList())
        {
            throw std::runtime_error("reversed() requires list");
        }

        auto result = args[0].asList();
        std::reverse(result.begin(), result.end());
        return object::Value(result);
    };

    // enumerate
    nativeFunctions["enumerate"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("enumerate() takes 1 argument");
        }
        if (!args[0].isList())
        {
            throw std::runtime_error("enumerate() requires list");
        }

        std::vector<object::Value> result;
        const auto& list = args[0].asList();
        for (size_t i = 0; i < list.size(); i++)
        {
            std::vector<object::Value> pair = {object::Value(static_cast<long long>(i)), list[i]};
            result.push_back(object::Value(pair));
        }
        return object::Value(result);
    };

    // zip
    nativeFunctions["zip"] = [](const std::vector<object::Value>& args) {
        if (args.size() < 2)
        {
            throw std::runtime_error("zip() requires at least 2 arguments");
        }

        size_t minLen = SIZE_MAX;
        for (const auto& arg : args)
        {
            if (!arg.isList())
            {
                throw std::runtime_error("zip() requires lists");
            }
            minLen = std::min(minLen, arg.asList().size());
        }

        std::vector<object::Value> result;
        for (size_t i = 0; i < minLen; i++)
        {
            std::vector<object::Value> tuple;
            for (const auto& arg : args)
            {
                tuple.push_back(arg.asList()[i]);
            }
            result.push_back(object::Value(tuple));
        }
        return object::Value(result);
    };

    // all, any
    nativeFunctions["all"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("all() takes 1 argument");
        }

        if (!args[0].isList())
        {
            throw std::runtime_error("all() requires list");
        }

        for (const auto& item : args[0].asList())
        {
            if (!item.toBool())
            {
                return object::Value(false);
            }
        }
        return object::Value(true);
    };

    nativeFunctions["any"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("any() takes 1 argument");
        }
        if (!args[0].isList())
        {
            throw std::runtime_error("any() requires list");
        }

        for (const auto& item : args[0].asList())
        {
            if (item.toBool())
            {
                return object::Value(true);
            }
        }
        return object::Value(false);
    };

    // map, filter
    nativeFunctions["map"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 2)
        {
            throw std::runtime_error("map() takes 2 arguments");
        }
        // Would need to handle function calls properly
        return object::Value();
    };

    // Math functions
    nativeFunctions["sqrt"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("sqrt() takes 1 argument");
        }
        return object::Value(std::sqrt(args[0].toFloat()));
    };

    nativeFunctions["pow"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 2)
        {
            throw std::runtime_error("pow() takes 2 arguments");
        }
        return object::Value(std::pow(args[0].toFloat(), args[1].toFloat()));
    };

    nativeFunctions["round"] = [](const std::vector<object::Value>& args) {
        if (args.size() != 1)
        {
            throw std::runtime_error("round() takes 1 argument");
        }
        return object::Value(static_cast<long long>(std::round(args[0].toFloat())));
    };
}

void VirtualMachine::execute(const BytecodeCompiler::CompiledCode& code)
{
    globals_.resize(code.numVariables);
    stack_.clear();
    stack_.reserve(1000);  // Pre-allocate for performance
    ip_ = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Main execution loop with computed goto (if supported)
    while (ip_ < code.instructions.size())
    {
        const auto& instr = code.instructions[ip_];
        stats_.instructionsExecuted++;

        // JIT hot spot detection
        if (stats_.instructionsExecuted % 100 == 0)
        {
            jit_.recordExecution(ip_);
        }

        // Dispatch instruction
        try
        {
            executeInstruction(instr, code);
        } catch (const std::exception& e)
        {
            std::cerr << "Runtime error at line " << instr.lineNumber << ": " << e.what() << "\n";
            throw;
        }

        ip_++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    stats_.executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void VirtualMachine::executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompiledCode& code)
{
    switch (instr.op)
    {
    case bytecode::OpCode::LOAD_CONST :
        push(code.constants[instr.arg]);
        break;

    case bytecode::OpCode::LOAD_VAR :
        if (instr.arg >= globals_.size())
        {
            throw std::runtime_error("Variable index out of range");
        }
        push(globals_[instr.arg]);
        break;

    case bytecode::OpCode::LOAD_GLOBAL :
        push(globals_[instr.arg]);
        break;

    case bytecode::OpCode::STORE_VAR :
        if (instr.arg >= globals_.size())
        {
            globals_.resize(instr.arg + 1);
        }
        globals_[instr.arg] = pop();
        break;

    case bytecode::OpCode::STORE_GLOBAL :
        globals_[instr.arg] = pop();
        break;

    case bytecode::OpCode::POP :
        pop();
        break;

    case bytecode::OpCode::DUP :
        push(top());
        break;

    case bytecode::OpCode::SWAP : {
        object::Value a = pop();
        object::Value b = pop();
        push(a);
        push(b);
        break;
    }

    case bytecode::OpCode::ROT_THREE : {
        object::Value a = pop();
        object::Value b = pop();
        object::Value c = pop();
        push(a);
        push(c);
        push(b);
        break;
    }

    // Arithmetic operations (with fast path)
    case bytecode::OpCode::ADD : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastAdd(a, b));
        break;
    }

    case bytecode::OpCode::SUB : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastSub(a, b));
        break;
    }

    case bytecode::OpCode::MUL : {
        object::Value b = pop();
        object::Value a = pop();
        push(fastMul(a, b));
        break;
    }

    case bytecode::OpCode::DIV : {
        object::Value b = pop();
        object::Value a = pop();
        push(a / b);
        break;
    }

    case bytecode::OpCode::FLOOR_DIV : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(static_cast<long long>(a.toFloat() / b.toFloat())));
        break;
    }

    case bytecode::OpCode::MOD : {
        object::Value b = pop();
        object::Value a = pop();
        push(a % b);
        break;
    }

    case bytecode::OpCode::POW : {
        object::Value b = pop();
        object::Value a = pop();
        push(a.pow(b));
        break;
    }

    case bytecode::OpCode::NEG : {
        object::Value a = pop();
        push(-a);
        break;
    }

    case bytecode::OpCode::POS :
        // Unary + is a no-op
        break;

    // Bitwise operations
    case bytecode::OpCode::BITAND : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() & b.toInt()));
        break;
    }

    case bytecode::OpCode::BITOR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() | b.toInt()));
        break;
    }

    case bytecode::OpCode::BITXOR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() ^ b.toInt()));
        break;
    }

    case bytecode::OpCode::BITNOT : {
        object::Value a = pop();
        push(object::Value(~a.toInt()));
        break;
    }

    case bytecode::OpCode::LSHIFT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() << b.toInt()));
        break;
    }

    case bytecode::OpCode::RSHIFT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toInt() >> b.toInt()));
        break;
    }

    // Comparison operations
    case bytecode::OpCode::EQ : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a == b));
        break;
    }

    case bytecode::OpCode::NE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a != b));
        break;
    }

    case bytecode::OpCode::LT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a < b));
        break;
    }

    case bytecode::OpCode::GT : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a > b));
        break;
    }

    case bytecode::OpCode::LE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a <= b));
        break;
    }

    case bytecode::OpCode::GE : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a >= b));
        break;
    }

    case bytecode::OpCode::IN : {
        object::Value b = pop();  // Container
        object::Value a = pop();  // Item
        if (b.isList())
        {
            bool found = false;
            for (const auto& item : b.asList())
            {
                if (item == a)
                {
                    found = true;
                    break;
                }
            }
            push(object::Value(found));
        }
        else if (b.isString())
        {
            push(object::Value(b.asString().find(utf8::utf8to16(a.toString())) != std::string::npos));
        }
        else
        {
            throw std::runtime_error("'in' requires list or string");
        }
        break;
    }

    case bytecode::OpCode::NOT_IN : {
        object::Value b = pop();
        object::Value a = pop();
        stack_.push_back(a);
        stack_.push_back(b);
        executeInstruction({bytecode::OpCode::IN}, code);
        object::Value result = pop();
        push(!result);
        break;
    }

    // Logical operations
    case bytecode::OpCode::AND : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toBool() && b.toBool()));
        break;
    }

    case bytecode::OpCode::OR : {
        object::Value b = pop();
        object::Value a = pop();
        push(object::Value(a.toBool() || b.toBool()));
        break;
    }

    case bytecode::OpCode::NOT : {
        object::Value a = pop();
        push(!a);
        break;
    }

    // Control flow
    case bytecode::OpCode::JUMP :
        ip_ = instr.arg - 1;  // -1 because ip++ at end of loop
        break;

    case bytecode::OpCode::JUMP_IF_FALSE : {
        object::Value cond = pop();
        if (!cond.toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::JUMP_IF_TRUE : {
        object::Value cond = pop();
        if (cond.toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::POP_JUMP_IF_FALSE : {
        if (!top().toBool())
        {
            ip_ = instr.arg - 1;
        }
        break;
    }

    case bytecode::OpCode::FOR_ITER : {
        object::Value& iterator = top();
        if (iterator.hasNext())
        {
            push(iterator.next());
        }
        else
        {
            pop();  // Remove exhausted iterator
            ip_ = instr.arg - 1;
        }
        break;
    }

    // Function calls
    case bytecode::OpCode::CALL : {
        int numArgs = instr.arg;
        std::vector<object::Value> args;
        for (int i = 0; i < numArgs; i++)
        {
            args.insert(args.begin(), pop());
        }

        object::Value func = pop();

        // Check for native function
        if (func.isString())
        {
            std::string funcName = utf8::utf16to8(func.asString());
            auto it = nativeFunctions.find(funcName);
            if (it != nativeFunctions.end())
            {
                stats_.functionsCalled++;
                push(it->second(args));
            }
            else
            {
                throw std::runtime_error("Unknown function: " + funcName);
            }
        }
        else if (func.isFunction())
        {
            // User-defined function (would need call frame management)
            stats_.functionsCalled++;
            throw std::runtime_error("User functions not yet implemented");
        }
        else
        {
            throw std::runtime_error("Object is not callable");
        }
        break;
    }

    case bytecode::OpCode::RETURN :
        return;  // Exit function

    case bytecode::OpCode::YIELD :
        // Generator support
        throw std::runtime_error("Generators not yet implemented");

    // Collections
    case bytecode::OpCode::BUILD_LIST : {
        std::vector<object::Value> elements;
        for (int i = 0; i < instr.arg; i++)
        {
            elements.insert(elements.begin(), pop());
        }
        push(object::Value(elements));
        break;
    }

    case bytecode::OpCode::BUILD_DICT : {
        std::unordered_map<std::u16string, object::Value> dict;
        for (int i = 0; i < instr.arg; i++)
        {
            object::Value val = pop();
            object::Value key = pop();
            dict[key.toString()] = val;
        }
        auto dictPtr = std::make_shared<std::unordered_map<std::string, object::Value>>(dict);
        object::Value result;
        result.setType(object::Value::Type::DICT);
        result.setData(dictPtr);
        push(result);
        break;
    }

    case bytecode::OpCode::BUILD_TUPLE :
        // Similar to BUILD_LIST but immutable
        executeInstruction({bytecode::OpCode::BUILD_LIST, instr.arg}, code);
        break;

    case bytecode::OpCode::UNPACK_SEQUENCE : {
        object::Value seq = pop();
        if (!seq.isList())
        {
            throw std::runtime_error("Cannot unpack non-sequence");
        }
        const auto& list = seq.asList();
        if (list.size() != instr.arg)
        {
            throw std::runtime_error("Unpack size mismatch");
        }
        for (const auto& item : list)
        {
            push(item);
        }
        break;
    }

    case bytecode::OpCode::GET_ITEM : {
        object::Value key = pop();
        object::Value obj = pop();
        push(obj.getItem(key));
        break;
    }

    case bytecode::OpCode::SET_ITEM : {
        object::Value val = pop();
        object::Value key = pop();
        object::Value& obj = top();
        obj.setItem(key, val);
        break;
    }

    case bytecode::OpCode::GET_ITER : {
        object::Value obj = pop();
        push(obj.getIterator());
        break;
    }

    // Special operations
    case bytecode::OpCode::PRINT : {
        std::vector<object::Value> args;
        for (int i = 0; i < instr.arg; i++)
        {
            args.insert(args.begin(), pop());
        }
        for (size_t i = 0; i < args.size(); i++)
        {
            std::cout << args[i].toString();
            if (i + 1 < args.size())
            {
                std::cout << " ";
            }
        }
        std::cout << "\n";
        push(object::Value());  // print returns None
        break;
    }

    case bytecode::OpCode::FORMAT : {
        // String formatting
        object::Value formatStr = pop();
        // Would implement f-string or % formatting
        push(formatStr);
        break;
    }

    case bytecode::OpCode::NOP :
        // No operation
        break;

    case bytecode::OpCode::HALT :
        return;

    default :
        throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<int>(instr.op)));
    }
}

void VirtualMachine::printStatistics() const
{
    std::cout << "\n╔═══════════════════════════════════════╗\n";
    std::cout << "║     VM Execution Statistics           ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n\n";

    std::cout << "Instructions executed:  " << stats_.instructionsExecuted << "\n";
    std::cout << "Functions called:       " << stats_.functionsCalled << "\n";
    std::cout << "GC collections:         " << stats_.gcCollections << "\n";
    std::cout << "JIT compilations:       " << stats_.jitCompilations << "\n";
    std::cout << "Execution time:         " << stats_.executionTime.count() / 1000.0 << " ms\n";

    if (stats_.instructionsExecuted > 0)
    {
        double ips = stats_.instructionsExecuted / (stats_.executionTime.count() / 1000000.0);
        std::cout << "Instructions/second:    " << static_cast<long long>(ips) << "\n";
    }

    std::cout << "\nStack size:            " << stack_.size() << "\n";
    std::cout << "Global variables:       " << globals_.size() << "\n";
}

}
}
