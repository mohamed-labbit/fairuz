#include "../../../include/runtime/vm/vm.hpp"
#include "../../../include/runtime/object/object.hpp"

#include <cmath>
#include <iostream>


namespace mylang {
namespace runtime {

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::define(const std::string& name, bool isParam)
{
  if (symbols_.count(name)) return &symbols_[name];

  Symbol sym;
  sym.name        = name;
  sym.index       = nextIndex_++;
  sym.scope       = parent_ ? SymbolScope::LOCAL : SymbolScope::GLOBAL;
  sym.isParameter = isParam;
  sym.isCaptured  = false;
  sym.isUsed      = false;

  symbols_[name] = sym;
  return &symbols_[name];
}

typename CompilerSymbolTable::Symbol* CompilerSymbolTable::resolve(const std::string& name)
{
  // Check local scope
  auto it = symbols_.find(name);
  if (it != symbols_.end())
  {
    it->second.isUsed = true;
    return &it->second;
  }
  // Check parent scopes
  if (parent_)
  {
    Symbol* parentSym = parent_->resolve(name);
    if (parentSym)
    {
      // Mark as captured for closure
      parentSym->isCaptured = true;
      // Create closure reference
      Symbol closureSym;
      closureSym.name   = name;
      closureSym.index  = freeVars.size();
      closureSym.scope  = SymbolScope::CLOSURE;
      closureSym.isUsed = true;
      freeVars.push_back(name);
      symbols_[name] = closureSym;
      return &symbols_[name];
    }
  }
  return nullptr;
}

const std::vector<std::string>& CompilerSymbolTable::getFreeVars() const { return freeVars; }

std::int32_t CompilerSymbolTable::getLocalCount() const { return nextIndex_; }

std::vector<typename CompilerSymbolTable::Symbol> CompilerSymbolTable::getUnusedSymbols() const
{
  std::vector<Symbol> unused;
  for (const auto& [name, sym] : symbols_)
    if (!sym.isUsed && !sym.isParameter) unused.push_back(sym);
  return unused;
}

std::int32_t ConstantPool::addConstant(const object::Value& val)
{
  if (val.isInt())
  {
    std::int64_t v  = val.asInt();
    auto         it = intConstants_.find(v);
    if (it != intConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    intConstants_[v] = idx;
    return idx;
  }

  if (val.isFloat())
  {
    double v  = val.asFloat();
    auto   it = floatConstants_.find(v);
    if (it != floatConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    floatConstants_[v] = idx;
    return idx;
  }

  if (val.isString())
  {
    const string_type& v  = val.asString();
    auto               it = stringConstants_.find(v);
    if (it != stringConstants_.end()) return it->second;
    std::int32_t idx = constants_.size();
    constants_.push_back(val);
    stringConstants_[v] = idx;
    return idx;
  }

  // Other types - no deduplication
  std::int32_t idx = constants_.size();
  constants_.push_back(val);
  return idx;
}

const std::vector<object::Value>& ConstantPool::getConstants() const { return constants_; }

void ConstantPool::optimize()
{
  // Remove unused constants (requires usage tracking)
}

void JumpResolver::defineLabel(const std::string& name, std::int32_t position) { labels_[name] = position; }

void JumpResolver::addJump(std::int32_t instrIndex, const std::string& target)
{
  auto it = labels_.find(target);
  if (it != labels_.end())
  {
    // TODO:
    // Label already defined - immediate resolution
    // (Would patch instruction here)
  }
  else
  {
    pendingJumps_.push_back({instrIndex, target});
  }
}

void JumpResolver::resolveJumps(std::vector<bytecode::Instruction>& instructions)
{
  for (const PendingJump& jump : pendingJumps_)
  {
    auto it = labels_.find(jump.labelName);
    if (it != labels_.end())
      instructions[jump.instructionIndex].arg = it->second;
    else
      throw std::runtime_error("Undefined label: " + jump.labelName);
  }
}

std::int32_t JumpResolver::getLabel(const std::string& name) const
{
  auto it = labels_.find(name);
  return it != labels_.end() ? it->second : -1;
}

void LoopAnalyzer::detectLoops(const std::vector<bytecode::Instruction>& instructions)
{
  // Detect back-edges (jumps to earlier instructions)
  for (std::size_t i = 0; i < instructions.size(); i++)
  {
    const bytecode::Instruction& instr = instructions[i];
    if ((instr.op == bytecode::OpCode::JUMP_BACKWARD || instr.op == bytecode::OpCode::FOR_ITER) && instr.arg < i)
    {
      Loop loop;
      loop.headerPC     = instr.arg;
      loop.exitPC       = i;
      loop.isInnerLoop  = true;
      loop.nestingLevel = 1;
      // Collect loop body
      for (std::int32_t pc = instr.arg; pc <= i; pc++)
        loop.bodyPCs.push_back(pc);
      loops_.push_back(loop);
    }
  }

  // Calculate nesting levels
  for (Loop& outer : loops_)
    for (const Loop& inner : loops_)
      if (inner.headerPC > outer.headerPC && inner.exitPC < outer.exitPC) outer.isInnerLoop = false;
  // TODO: inner is nested in outer
}

void LoopAnalyzer::findInvariants(const std::vector<bytecode::Instruction>& instructions, const CompilerSymbolTable& symbols)
{
  for (Loop& loop : loops_)
  {
    std::unordered_set<std::int32_t> modifiedVars;
    std::unordered_set<std::int32_t> usedVars;
    for (std::int32_t pc : loop.bodyPCs)
    {
      const bytecode::Instruction& instr = instructions[pc];
      if (instr.op == bytecode::OpCode::STORE_VAR || instr.op == bytecode::OpCode::STORE_FAST) modifiedVars.insert(instr.arg);
      if (instr.op == bytecode::OpCode::LOAD_VAR || instr.op == bytecode::OpCode::LOAD_FAST) usedVars.insert(instr.arg);
    }
    for (std::int32_t var : usedVars)
      if (!modifiedVars.count(var)) loop.invariants.insert(var);
  }
}

const std::vector<typename LoopAnalyzer::Loop>& LoopAnalyzer::getLoops() const { return loops_; }

bool PeepholeOptimizer::matchPattern(const std::vector<bytecode::Instruction>& code, std::size_t pos, const std::vector<bytecode::OpCode>& pattern)
{
  if (pos + pattern.size() > code.size()) return false;
  for (std::size_t i = 0; i < pattern.size(); i++)
    if (code[pos + i].op != pattern[i]) return false;
  return true;
}


void PeepholeOptimizer::optimize(std::vector<bytecode::Instruction>& instructions)
{
  std::int32_t replacements = 0;
  // Pattern 1: LOAD_CONST followed by POP -> remove both
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::LOAD_CONST && instructions[i + 1].op == bytecode::OpCode::POP)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Const-Pop elimination", replacements});
  // Pattern 2: LOAD x, STORE x -> remove (redundant load-store)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    bytecode::Instruction& instr1 = instructions[i];
    bytecode::Instruction& instr2 = instructions[i + 1];

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
  if (replacements > 0) optimizations_.push_back({"Load-Store elimination", replacements});
  // Pattern 3: DUP, POP -> remove both (useless dup)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::DUP && instructions[i + 1].op == bytecode::OpCode::POP)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Dup-Pop elimination", replacements});
  // Pattern 4: JUMP to next instruction -> remove
  replacements = 0;
  for (std::size_t i = 0; i < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::JUMP && instructions[i].arg == i + 1)
    {
      instructions.erase(instructions.begin() + i);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Redundant jump elimination", replacements});
  // Pattern 5: NOT, NOT -> remove both (double negation)
  replacements = 0;
  for (std::size_t i = 0; i + 1 < instructions.size();)
  {
    if (instructions[i].op == bytecode::OpCode::NOT && instructions[i + 1].op == bytecode::OpCode::NOT)
    {
      instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
      replacements++;
      continue;
    }
    i++;
  }
  if (replacements > 0) optimizations_.push_back({"Double negation elimination", replacements});
  // Pattern 6: Use fast opcodes for common operations
  replacements = 0;
  for (bytecode::Instruction& instr : instructions)
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
  if (replacements > 0) optimizations_.push_back({"Fast opcode substitution", replacements});
}

const std::vector<typename PeepholeOptimizer::Optimization>& PeepholeOptimizer::getOptimizations() const { return optimizations_; }

void PeepholeOptimizer::printReport() const
{
  if (optimizations_.empty())
  {
    std::cout << "  No peephole optimizations applied\n";
    return;
  }
  std::cout << "  Peephole Optimizations:\n";
  for (const auto& opt : optimizations_)
    std::cout << "    • " << opt.name << ": " << opt.name << " replacements\n";
}

void BytecodeCompiler::emit(bytecode::OpCode op, std::int32_t arg, std::int32_t line)
{
  bytecode::Instruction instr(op, arg, line);
  unit_.instructions.push_back(instr);
  stats_.instructionsGenerated++;
  // Track stack depth
  updateStackDepth(op);
  // Track line numbers
  if (line > 0) unit_.lineNumbers.push_back(line);
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
  case bytecode::OpCode::DUP : currentStackDepth_++; break;
  case bytecode::OpCode::POP :
  case bytecode::OpCode::STORE_VAR :
  case bytecode::OpCode::STORE_GLOBAL :
  case bytecode::OpCode::STORE_FAST : currentStackDepth_--; break;
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
    currentStackDepth_--;  // Two operands -> one result
    break;
  case bytecode::OpCode::CALL :
    // Function + args -> result
    // TODO: Stack depth change handled separately
    break;
  default : break;
  }

  maxStackDepth_ = std::max(maxStackDepth_, currentStackDepth_);
}

std::int32_t BytecodeCompiler::getCurrentPC() const { return unit_.instructions.size(); }

void BytecodeCompiler::patchJump(std::int32_t jumpIndex) { unit_.instructions[jumpIndex].arg = getCurrentPC(); }

void BytecodeCompiler::enterScope()
{
  CompilerSymbolTable* newScope = new CompilerSymbolTable(currentScope_.get());
  if (currentScope_) scopeStack_.push(currentScope_.release());
  currentScope_.reset(newScope);
}

void BytecodeCompiler::exitScope()
{
  if (!scopeStack_.empty())
  {
    currentScope_.reset(scopeStack_.top());
    scopeStack_.pop();
  }
}

void BytecodeCompiler::compileExpr(const parser::ast::Expr* expr)
{
  if (!expr) return;

  switch (expr->kind)
  {
  case parser::ast::Expr::Kind::LITERAL : {
    const parser::ast::LiteralExpr* lit = static_cast<const parser::ast::LiteralExpr*>(expr);
    object::Value                   val;
    switch (lit->litType)
    {
    case parser::ast::LiteralExpr::Type::NUMBER : {
      if (lit->value.find('.') != std::string::npos)
        val = object::Value(std::stod(utf8::utf16to8(lit->value)));
      else
        val = object::Value(static_cast<std::int64_t>(std::stoll(utf8::utf16to8(lit->value))));
      break;
    }
    case parser::ast::LiteralExpr::Type::STRING : val = object::Value(lit->value); break;
    case parser::ast::LiteralExpr::Type::BOOLEAN : val = object::Value(lit->value == u"true" || lit->value == u"صحيح"); break;
    case parser::ast::LiteralExpr::Type::NONE : val = object::Value(); break;
    }
    std::int32_t idx = constants_.addConstant(val);
    emit(bytecode::OpCode::LOAD_CONST, idx, expr->line);
    break;
  }

  case parser::ast::Expr::Kind::NAME : {
    const parser::ast::NameExpr* name = static_cast<const parser::ast::NameExpr*>(expr);
    CompilerSymbolTable::Symbol* sym  = currentScope_->resolve(utf8::utf16to8(name->name));
    if (!sym) throw std::runtime_error("Undefined variable: " + utf8::utf16to8(name->name));
    switch (sym->scope)
    {
    case CompilerSymbolTable::SymbolScope::GLOBAL : emit(bytecode::OpCode::LOAD_GLOBAL, sym->index, expr->line); break;
    case CompilerSymbolTable::SymbolScope::LOCAL : emit(bytecode::OpCode::LOAD_FAST, sym->index, expr->line); break;
    case CompilerSymbolTable::SymbolScope::CLOSURE : emit(bytecode::OpCode::LOAD_CLOSURE, sym->index, expr->line); break;
    default : emit(bytecode::OpCode::LOAD_VAR, sym->index, expr->line); break;
    }
    break;
  }

  case parser::ast::Expr::Kind::BINARY : {
    const parser::ast::BinaryExpr* bin = static_cast<const parser::ast::BinaryExpr*>(expr);
    // Short-circuit evaluation for logical operators
    if (bin->op == u"و" || bin->op == u"and")
    {
      compileExpr(bin->left.get());
      emit(bytecode::OpCode::DUP, 0, expr->line);
      std::int32_t jumpIfFalse = getCurrentPC();
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
      std::int32_t jumpIfTrue = getCurrentPC();
      emit(bytecode::OpCode::POP_JUMP_IF_TRUE, 0, expr->line);
      emit(bytecode::OpCode::POP, 0, expr->line);
      compileExpr(bin->right.get());
      patchJump(jumpIfTrue);
      break;
    }
    // Regular binary operations
    compileExpr(bin->left.get());
    compileExpr(bin->right.get());
    if (bin->op == u"+") { emit(bytecode::OpCode::ADD, 0, expr->line); }
    else if (bin->op == u"-") { emit(bytecode::OpCode::SUB, 0, expr->line); }
    else if (bin->op == u"*") { emit(bytecode::OpCode::MUL, 0, expr->line); }
    else if (bin->op == u"/") { emit(bytecode::OpCode::DIV, 0, expr->line); }
    else if (bin->op == u"//") { emit(bytecode::OpCode::FLOOR_DIV, 0, expr->line); }
    else if (bin->op == u"%") { emit(bytecode::OpCode::MOD, 0, expr->line); }
    else if (bin->op == u"**") { emit(bytecode::OpCode::POW, 0, expr->line); }
    else if (bin->op == u"==") { emit(bytecode::OpCode::EQ, 0, expr->line); }
    else if (bin->op == u"!=") { emit(bytecode::OpCode::NE, 0, expr->line); }
    else if (bin->op == u"<") { emit(bytecode::OpCode::LT, 0, expr->line); }
    else if (bin->op == u">") { emit(bytecode::OpCode::GT, 0, expr->line); }
    else if (bin->op == u"<=") { emit(bytecode::OpCode::LE, 0, expr->line); }
    else if (bin->op == u">=") { emit(bytecode::OpCode::GE, 0, expr->line); }
    else if (bin->op == u"&") { emit(bytecode::OpCode::BITAND, 0, expr->line); }
    else if (bin->op == u"|") { emit(bytecode::OpCode::BITOR, 0, expr->line); }
    else if (bin->op == u"^") { emit(bytecode::OpCode::BITXOR, 0, expr->line); }
    else if (bin->op == u"<<") { emit(bytecode::OpCode::LSHIFT, 0, expr->line); }
    else if (bin->op == u">>") { emit(bytecode::OpCode::RSHIFT, 0, expr->line); }
    else if (bin->op == u"في" || bin->op == u"in") { emit(bytecode::OpCode::IN, 0, expr->line); }
    break;
  }

  case parser::ast::Expr::Kind::UNARY : {
    const parser::ast::UnaryExpr* un = static_cast<const parser::ast::UnaryExpr*>(expr);
    compileExpr(un->operand.get());
    if (un->op == u"-")
      emit(bytecode::OpCode::NEG, 0, expr->line);
    else if (un->op == u"+")
      emit(bytecode::OpCode::POS, 0, expr->line);
    else if (un->op == u"~")
      emit(bytecode::OpCode::BITNOT, 0, expr->line);
    else if (un->op == u"ليس" || un->op == u"not")
      emit(bytecode::OpCode::NOT, 0, expr->line);
    break;
  }

  case parser::ast::Expr::Kind::CALL : {
    const parser::ast::CallExpr* call = static_cast<const parser::ast::CallExpr*>(expr);
    // Compile arguments first
    for (const parser::ast::ExprPtr& arg : call->args)
      compileExpr(arg.get());
    // Compile callee
    compileExpr(call->callee.get());
    // Emit call instruction
    emit(bytecode::OpCode::CALL, call->args.size(), expr->line);
    // Adjust stack depth
    currentStackDepth_ -= call->args.size();
    break;
  }

  case parser::ast::Expr::Kind::LIST : {
    const parser::ast::ListExpr* list = static_cast<const parser::ast::ListExpr*>(expr);
    // Compile all elements
    for (const parser::ast::ExprPtr& elem : list->elements)
      compileExpr(elem.get());
    // Build list from stack
    emit(bytecode::OpCode::BUILD_LIST, list->elements.size(), expr->line);
    // Adjust stack depth
    currentStackDepth_ -= list->elements.size();
    currentStackDepth_++;
    break;
  }

  case parser::ast::Expr::Kind::TERNARY : {
    const parser::ast::TernaryExpr* tern = static_cast<const parser::ast::TernaryExpr*>(expr);
    // Compile condition
    compileExpr(tern->condition.get());
    std::int32_t jumpIfFalse = getCurrentPC();
    emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);
    // True branch
    compileExpr(tern->trueExpr.get());
    std::int32_t jumpEnd = getCurrentPC();
    emit(bytecode::OpCode::JUMP, 0, expr->line);
    // False branch
    patchJump(jumpIfFalse);
    if (tern->falseExpr) compileExpr(tern->falseExpr.get());
    patchJump(jumpEnd);
    break;
  }

  case parser::ast::Expr::Kind::ASSIGNMENT : {
    const parser::ast::AssignmentExpr* assign = static_cast<const parser::ast::AssignmentExpr*>(expr);
    // Compile value
    compileExpr(assign->value.get());
    // Duplicate for expression result
    emit(bytecode::OpCode::DUP, 0, expr->line);
    // Store
    CompilerSymbolTable::Symbol* sym = currentScope_->define(utf8::utf16to8(assign->target));
    if (sym->scope == CompilerSymbolTable::SymbolScope::GLOBAL)
      emit(bytecode::OpCode::STORE_GLOBAL, sym->index, expr->line);
    else
      emit(bytecode::OpCode::STORE_FAST, sym->index, expr->line);
    break;
  }

  default : throw std::runtime_error("Unsupported expression type");
  }
}

void BytecodeCompiler::compileStmt(const parser::ast::Stmt* stmt)
{
  if (!stmt) return;

  switch (stmt->kind)
  {
  case parser::ast::Stmt::Kind::ASSIGNMENT : {
    const parser::ast::AssignmentStmt* assign = static_cast<const parser::ast::AssignmentStmt*>(stmt);
    // Compile value
    compileExpr(assign->value.get());
    // Store to variable
    CompilerSymbolTable::Symbol* sym = currentScope_->define(utf8::utf16to8(assign->target));
    if (sym->scope == CompilerSymbolTable::SymbolScope::GLOBAL)
      emit(bytecode::OpCode::STORE_GLOBAL, sym->index, stmt->line);
    else
      emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);
    break;
  }
  case parser::ast::Stmt::Kind::EXPRESSION : {
    const parser::ast::ExprStmt* exprStmt = static_cast<const parser::ast::ExprStmt*>(stmt);
    compileExpr(exprStmt->expression.get());
    emit(bytecode::OpCode::POP, 0, stmt->line);  // Discard result
    break;
  }
  case parser::ast::Stmt::Kind::IF : {
    const parser::ast::IfStmt* ifStmt = static_cast<const parser::ast::IfStmt*>(stmt);
    // Compile condition
    compileExpr(ifStmt->condition.get());
    std::int32_t jumpIfFalse = getCurrentPC();
    emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);
    // Then block
    for (const parser::ast::StmtPtr& s : ifStmt->thenBlock)
      compileStmt(s.get());
    if (!ifStmt->elseBlock.empty())
    {
      std::int32_t jumpEnd = getCurrentPC();
      emit(bytecode::OpCode::JUMP, 0, stmt->line);
      patchJump(jumpIfFalse);
      // Else block
      for (const parser::ast::StmtPtr& s : ifStmt->elseBlock)
        compileStmt(s.get());
      patchJump(jumpEnd);
    }
    else
    {
      patchJump(jumpIfFalse);
    }
    break;
  }
  case parser::ast::Stmt::Kind::WHILE : {
    const parser::ast::WhileStmt* whileStmt = static_cast<const parser::ast::WhileStmt*>(stmt);
    std::int32_t                  loopStart = getCurrentPC();
    // Mark as potential hot loop
    emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);
    // Compile condition
    compileExpr(whileStmt->condition.get());
    std::int32_t jumpIfFalse = getCurrentPC();
    emit(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->line);
    // Loop body
    LoopContext ctx;
    ctx.startPC       = loopStart;
    ctx.continueLabel = loopStart;
    ctx.breakLabel    = -1;  // Will be patched
    loopStack_.push(ctx);
    for (const parser::ast::StmtPtr& s : whileStmt->body)
      compileStmt(s.get());
    // Jump back to loop start
    emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);
    patchJump(jumpIfFalse);
    emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);
    // Patch break statements
    if (loopStack_.top().breakLabel != -1) patchJump(loopStack_.top().breakLabel);
    loopStack_.pop();
    stats_.loopsDetected++;
    break;
  }
  case parser::ast::Stmt::Kind::FOR : {
    const parser::ast::ForStmt* forStmt = static_cast<const parser::ast::ForStmt*>(stmt);
    // Compile iterator
    compileExpr(forStmt->iter.get());
    emit(bytecode::OpCode::GET_ITER, 0, stmt->line);
    std::int32_t loopStart = getCurrentPC();
    emit(bytecode::OpCode::HOT_LOOP_START, 0, stmt->line);
    // FOR_ITER gets next item or jumps to end
    std::int32_t forIter = getCurrentPC();
    emit(bytecode::OpCode::FOR_ITER_FAST, 0, stmt->line);
    // Store loop variable
    CompilerSymbolTable::Symbol* sym = currentScope_->define(utf8::utf16to8(forStmt->target));
    emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);
    // Loop body
    LoopContext ctx;
    ctx.startPC       = loopStart;
    ctx.continueLabel = loopStart;
    ctx.breakLabel    = -1;
    loopStack_.push(ctx);
    for (const parser::ast::StmtPtr& s : forStmt->body)
      compileStmt(s.get());
    // Jump back
    emit(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->line);
    // Patch FOR_ITER to jump here when exhausted
    patchJump(forIter);
    emit(bytecode::OpCode::HOT_LOOP_END, 0, stmt->line);
    if (loopStack_.top().breakLabel != -1) patchJump(loopStack_.top().breakLabel);
    loopStack_.pop();
    stats_.loopsDetected++;
    break;
  }
  case parser::ast::Stmt::Kind::FUNCTION_DEF : {
    const parser::ast::FunctionDef* funcDef = static_cast<const parser::ast::FunctionDef*>(stmt);
    // Enter new scope for function
    enterScope();
    // Define parameters
    for (const string_type& param : funcDef->params)
      currentScope_->define(utf8::utf16to8(param), true);
    // Save current compilation state
    std::vector<bytecode::Instruction>&& savedInstructions  = std::move(unit_.instructions);
    ConstantPool                         savedConstants     = constants_;
    std::int32_t                         savedStackDepth    = currentStackDepth_;
    std::int32_t                         savedMaxStackDepth = maxStackDepth_;
    unit_.instructions.clear();
    currentStackDepth_ = 0;
    maxStackDepth_     = 0;
    // Compile function body
    for (const parser::ast::StmtPtr& s : funcDef->body)
      compileStmt(s.get());
    // Implicit return None if no return statement
    if (unit_.instructions.empty() || unit_.instructions.back().op != bytecode::OpCode::RETURN)
    {
      std::int32_t noneIdx = constants_.addConstant(object::Value());
      emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
      emit(bytecode::OpCode::RETURN, 0, stmt->line);
    }
    // Create function object
    std::vector<bytecode::Instruction>&& funcInstructions = std::move(unit_.instructions);
    std::int32_t                         funcStackSize    = maxStackDepth_;
    // Restore compilation state
    unit_.instructions = std::move(savedInstructions);
    constants_         = savedConstants;
    currentStackDepth_ = savedStackDepth;
    maxStackDepth_     = savedMaxStackDepth;
    // Store function object as constant
    object::Value funcObj;  // Would create FunctionObject here
    std::int32_t  funcIdx = constants_.addConstant(funcObj);
    emit(bytecode::OpCode::LOAD_CONST, funcIdx, stmt->line);
    emit(bytecode::OpCode::MAKE_FUNCTION, funcDef->params.size(), stmt->line);
    // Store function
    CompilerSymbolTable::Symbol* sym = currentScope_->define(utf8::utf16to8(funcDef->name));
    emit(bytecode::OpCode::STORE_FAST, sym->index, stmt->line);
    exitScope();
    break;
  }
  case parser::ast::Stmt::Kind::RETURN : {
    const parser::ast::ReturnStmt* ret = static_cast<const parser::ast::ReturnStmt*>(stmt);
    if (ret->value) { compileExpr(ret->value.get()); }
    else
    {
      std::int32_t noneIdx = constants_.addConstant(object::Value());
      emit(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->line);
    }
    emit(bytecode::OpCode::RETURN, 0, stmt->line);
    break;
  }
  case parser::ast::Stmt::Kind::BLOCK : {
    const parser::ast::BlockStmt* block = static_cast<const parser::ast::BlockStmt*>(stmt);
    for (const parser::ast::StmtPtr& s : block->statements)
      compileStmt(s.get());
    break;
  }
  default : throw std::runtime_error("Unsupported statement type");
  }
}


typename BytecodeCompiler::CompilationUnit BytecodeCompiler::compile(const std::vector<parser::ast::StmtPtr>& ast)
{
  // Reset state
  unit_              = CompilationUnit();
  stats_             = Stats();
  currentStackDepth_ = 0;
  maxStackDepth_     = 0;
  // Compile all statements
  for (const parser::ast::StmtPtr& stmt : ast)
    compileStmt(stmt.get());
  // Add HALT at end
  emit(bytecode::OpCode::HALT, 0, 0);
  // Finalize constant pool
  unit_.constants          = constants_.getConstants();
  stats_.constantsPoolSize = unit_.constants.size();
  // Resolve all jumps
  jumps_.resolveJumps(unit_.instructions);
  // Detect loops for optimization
  loopAnalyzer_.detectLoops(unit_.instructions);
  loopAnalyzer_.findInvariants(unit_.instructions, *currentScope_);
  // Apply peephole optimizations
  peephole_.optimize(unit_.instructions);
  stats_.peepholeOptimizations = peephole_.getOptimizations().size();
  // Set metadata
  unit_.numLocals = currentScope_->getLocalCount();
  unit_.stackSize = maxStackDepth_;
  // Report unused variables
  std::vector<CompilerSymbolTable::Symbol> unused = currentScope_->getUnusedSymbols();
  if (!unused.empty()) std::cout << "[Compiler] Warning: " << unused.size() << " unused variables detected\n";
  return unit_;
}

void BytecodeCompiler::disassemble(const CompilationUnit& unit, std::ostream& out) const
{
  out << "=== Bytecode Disassembly ===\n\n";
  out << "Constants Pool (" << unit.constants.size() << " entries):\n";
  for (std::size_t i = 0; i < unit.constants.size(); i++)
    out << "  [" << i << "] " << unit.constants[i].repr() << "\n";
  out << "\nCode (" << unit.instructions.size() << " instructions):\n";
  out << "Stack size: " << unit.stackSize << "\n";
  out << "Locals: " << unit.numLocals << "\n\n";

  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    out << std::setw(6) << i << "  ";
    // Line number
    if (instr.lineNumber > 0)
      out << std::setw(4) << instr.lineNumber << "  ";
    else
      out << "      ";
    // Opcode name
    out << std::left << std::setw(20) << opcodeToString(instr.op);
    // Argument
    if (needsArg(instr.op))
    {
      out << std::setw(6) << instr.arg;
      // Show constant value for LOAD_CONST
      if (instr.op == bytecode::OpCode::LOAD_CONST && instr.arg < unit.constants.size()) out << "  ; " << unit.constants[instr.arg].repr();
    }
    out << "\n";
  }

  out << "\n=== Compilation Statistics ===\n";
  out << "Instructions generated: " << stats_.instructionsGenerated << "\n";
  out << "Constants in pool: " << stats_.constantsPoolSize << "\n";
  out << "Loops detected: " << stats_.loopsDetected << "\n";
  out << "Peephole optimizations: " << stats_.peepholeOptimizations << "\n";
  // Loop analysis
  const std::vector<LoopAnalyzer::Loop>& loops = loopAnalyzer_.getLoops();
  if (!loops.empty())
  {
    out << "\nLoop Analysis:\n";
    for (std::size_t i = 0; i < loops.size(); i++)
    {
      const LoopAnalyzer::Loop& loop = loops[i];
      out << "  Loop " << i << ": PC " << loop.headerPC << " -> " << loop.exitPC << " (nesting: " << loop.nestingLevel << ")\n";
      out << "    Invariants: " << loop.invariants.size() << " variables\n";
    }
  }
  // Peephole report
  out << "\n";
  peephole_.printReport();
}


void BytecodeCompiler::optimizationReport(std::ostream& out) const
{
  out << "\n╔═══════════════════════════════════════╗\n";
  out << "║    Bytecode Optimization Report       ║\n";
  out << "╚═══════════════════════════════════════╝\n\n";
  out << "Code Size:\n";
  out << "  Instructions: " << stats_.instructionsGenerated << "\n";
  out << "  Constants: " << stats_.constantsPoolSize << "\n";
  out << "  Stack size: " << maxStackDepth_ << "\n\n";
  out << "Loop Optimizations:\n";
  out << "  Loops detected: " << stats_.loopsDetected << "\n";
  const std::vector<LoopAnalyzer::Loop>& loops           = loopAnalyzer_.getLoops();
  std::int32_t                           totalInvariants = 0;
  for (const LoopAnalyzer::Loop& loop : loops)
    totalInvariants += loop.invariants.size();
  out << "  Hoistable invariants: " << totalInvariants << "\n\n";
  out << "Peephole Optimizations:\n";
  if (stats_.peepholeOptimizations > 0)
    peephole_.printReport();
  else
    out << "  None applied\n";
}

std::string BytecodeCompiler::opcodeToString(bytecode::OpCode op) const
{
  switch (op)
  {
  case bytecode::OpCode::LOAD_CONST : return "LOAD_CONST";
  case bytecode::OpCode::LOAD_VAR : return "LOAD_VAR";
  case bytecode::OpCode::LOAD_GLOBAL : return "LOAD_GLOBAL";
  case bytecode::OpCode::LOAD_FAST : return "LOAD_FAST";
  case bytecode::OpCode::STORE_VAR : return "STORE_VAR";
  case bytecode::OpCode::STORE_GLOBAL : return "STORE_GLOBAL";
  case bytecode::OpCode::STORE_FAST : return "STORE_FAST";
  case bytecode::OpCode::POP : return "POP";
  case bytecode::OpCode::DUP : return "DUP";
  case bytecode::OpCode::SWAP : return "SWAP";
  case bytecode::OpCode::ROT_THREE : return "ROT_THREE";
  case bytecode::OpCode::ADD : return "ADD";
  case bytecode::OpCode::ADD_FAST : return "ADD_FAST";
  case bytecode::OpCode::SUB : return "SUB";
  case bytecode::OpCode::SUB_FAST : return "SUB_FAST";
  case bytecode::OpCode::MUL : return "MUL";
  case bytecode::OpCode::MUL_FAST : return "MUL_FAST";
  case bytecode::OpCode::DIV : return "DIV";
  case bytecode::OpCode::FLOOR_DIV : return "FLOOR_DIV";
  case bytecode::OpCode::MOD : return "MOD";
  case bytecode::OpCode::POW : return "POW";
  case bytecode::OpCode::NEG : return "NEG";
  case bytecode::OpCode::POS : return "POS";
  case bytecode::OpCode::BITAND : return "BITAND";
  case bytecode::OpCode::BITOR : return "BITOR";
  case bytecode::OpCode::BITXOR : return "BITXOR";
  case bytecode::OpCode::BITNOT : return "BITNOT";
  case bytecode::OpCode::LSHIFT : return "LSHIFT";
  case bytecode::OpCode::RSHIFT : return "RSHIFT";
  case bytecode::OpCode::EQ : return "EQ";
  case bytecode::OpCode::NE : return "NE";
  case bytecode::OpCode::LT : return "LT";
  case bytecode::OpCode::GT : return "GT";
  case bytecode::OpCode::LE : return "LE";
  case bytecode::OpCode::GE : return "GE";
  case bytecode::OpCode::IN : return "IN";
  case bytecode::OpCode::NOT_IN : return "NOT_IN";
  case bytecode::OpCode::IS : return "IS";
  case bytecode::OpCode::IS_NOT : return "IS_NOT";
  case bytecode::OpCode::AND : return "AND";
  case bytecode::OpCode::OR : return "OR";
  case bytecode::OpCode::NOT : return "NOT";
  case bytecode::OpCode::JUMP : return "JUMP";
  case bytecode::OpCode::JUMP_FORWARD : return "JUMP_FORWARD";
  case bytecode::OpCode::JUMP_BACKWARD : return "JUMP_BACKWARD";
  case bytecode::OpCode::JUMP_IF_FALSE : return "JUMP_IF_FALSE";
  case bytecode::OpCode::JUMP_IF_TRUE : return "JUMP_IF_TRUE";
  case bytecode::OpCode::POP_JUMP_IF_FALSE : return "POP_JUMP_IF_FALSE";
  case bytecode::OpCode::POP_JUMP_IF_TRUE : return "POP_JUMP_IF_TRUE";
  case bytecode::OpCode::FOR_ITER : return "FOR_ITER";
  case bytecode::OpCode::FOR_ITER_FAST : return "FOR_ITER_FAST";
  case bytecode::OpCode::CALL : return "CALL";
  case bytecode::OpCode::CALL_FAST : return "CALL_FAST";
  case bytecode::OpCode::RETURN : return "RETURN";
  case bytecode::OpCode::YIELD : return "YIELD";
  case bytecode::OpCode::BUILD_LIST : return "BUILD_LIST";
  case bytecode::OpCode::BUILD_DICT : return "BUILD_DICT";
  case bytecode::OpCode::BUILD_TUPLE : return "BUILD_TUPLE";
  case bytecode::OpCode::BUILD_SET : return "BUILD_SET";
  case bytecode::OpCode::UNPACK_SEQUENCE : return "UNPACK_SEQUENCE";
  case bytecode::OpCode::GET_ITEM : return "GET_ITEM";
  case bytecode::OpCode::SET_ITEM : return "SET_ITEM";
  case bytecode::OpCode::GET_ITER : return "GET_ITER";
  case bytecode::OpCode::MAKE_FUNCTION : return "MAKE_FUNCTION";
  case bytecode::OpCode::LOAD_CLOSURE : return "LOAD_CLOSURE";
  case bytecode::OpCode::PRINT : return "PRINT";
  case bytecode::OpCode::NOP : return "NOP";
  case bytecode::OpCode::HALT : return "HALT";
  case bytecode::OpCode::HOT_LOOP_START : return "HOT_LOOP_START";
  case bytecode::OpCode::HOT_LOOP_END : return "HOT_LOOP_END";
  default : return "UNKNOWN";
  }
}

bool BytecodeCompiler::needsArg(bytecode::OpCode op) const
{
  return op != bytecode::OpCode::POP && op != bytecode::OpCode::DUP && op != bytecode::OpCode::ADD && op != bytecode::OpCode::SUB
      && op != bytecode::OpCode::MUL && op != bytecode::OpCode::DIV && op != bytecode::OpCode::NEG && op != bytecode::OpCode::NOT
      && op != bytecode::OpCode::RETURN && op != bytecode::OpCode::HALT && op != bytecode::OpCode::NOP;
}

void BytecodeOptimizer::optimize(std::vector<bytecode::Instruction>& code, std::int32_t maxIterations)
{
  bool         changed   = true;
  std::int32_t iteration = 0;
  while (changed && iteration < maxIterations)
  {
    changed = false;
    for (OptimizationPass& pass : passes_)
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
  for (const OptimizationPass& pass : passes_)
    if (pass.applicationsCount > 0) out << "  • " << pass.name << ": " << pass.applicationsCount << " applications\n";
}

bool BytecodeOptimizer::isJumpTarget(const std::vector<bytecode::Instruction>& code, std::size_t pos)
{
  // Check if any instruction jumps to this position
  for (const bytecode::Instruction& instr : code)
    if (isJumpOp(instr.op) && instr.arg == pos) return true;
  return false;
}

bool BytecodeOptimizer::isJumpOp(bytecode::OpCode op)
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
      || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
      || op == bytecode::OpCode::POP_JUMP_IF_TRUE;
}

bool BytecodeOptimizer::isBinaryOp(bytecode::OpCode op)
{
  return op == bytecode::OpCode::ADD || op == bytecode::OpCode::SUB || op == bytecode::OpCode::MUL || op == bytecode::OpCode::DIV
      || op == bytecode::OpCode::MOD || op == bytecode::OpCode::POW;
}

bool BytecodeVerifier::verify(const BytecodeCompiler::CompilationUnit& unit)
{
  errors_.clear();
  // Check 1: Valid jump targets
  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    if (isJumpInstruction(instr.op))
      if (instr.arg < 0 || instr.arg >= unit.instructions.size())
        errors_.push_back({static_cast<std::int32_t>(i), "Jump target out of bounds: " + std::to_string(instr.arg)});
  }
  // Check 2: Stack balance
  std::vector<std::int32_t> stackDepths(unit.instructions.size(), -1);
  verifyStackDepth(unit, 0, 0, stackDepths);
  // Check 3: Constant pool bounds
  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    if (instr.op == bytecode::OpCode::LOAD_CONST)
      if (instr.arg < 0 || instr.arg >= unit.constants.size())
        errors_.push_back({static_cast<std::int32_t>(i), "Constant index out of bounds: " + std::to_string(instr.arg)});
  }
  // Check 4: All paths return (for functions)
  // Would implement dataflow analysis here
  return errors_.empty();
}

const std::vector<typename BytecodeVerifier::VerificationError>& BytecodeVerifier::getErrors() const { return errors_; }

void BytecodeVerifier::printErrors(std::ostream& out) const
{
  if (errors_.empty())
  {
    out << "✓ Bytecode verification passed\n";
    return;
  }
  out << "✗ Bytecode verification failed with " << errors_.size() << " error(s):\n";
  for (const VerificationError& err : errors_)
    out << "  PC " << err.pc << ": " << err.message << "\n";
}

void BytecodeVerifier::verifyStackDepth(const BytecodeCompiler::CompilationUnit& unit,
                                        std::int32_t                             pc,
                                        std::int32_t                             depth,
                                        std::vector<std::int32_t>&               depths)
{
  if (pc >= unit.instructions.size()) return;
  // Already visited with same or greater depth
  if (depths[pc] >= depth) return;
  depths[pc]                            = depth;
  const bytecode::Instruction& instr    = unit.instructions[pc];
  std::int32_t                 newDepth = depth + getStackEffect(instr.op, instr.arg);
  if (newDepth < 0)
  {
    errors_.push_back({pc, "Stack underflow"});
    return;
  }
  if (newDepth > unit.stackSize)
    errors_.push_back({pc, "Stack overflow (depth " + std::to_string(newDepth) + " > " + std::to_string(unit.stackSize) + ")"});
  // Follow control flow
  if (instr.op == bytecode::OpCode::RETURN || instr.op == bytecode::OpCode::HALT) return;
  if (isJumpInstruction(instr.op))
  {
    verifyStackDepth(unit, instr.arg, newDepth, depths);
    if (!isUnconditionalJump(instr.op)) verifyStackDepth(unit, pc + 1, newDepth, depths);
  }
  else
  {
    verifyStackDepth(unit, pc + 1, newDepth, depths);
  }
}

std::int32_t BytecodeVerifier::getStackEffect(bytecode::OpCode op, std::int32_t arg) const
{
  switch (op)
  {
  case bytecode::OpCode::LOAD_CONST :
  case bytecode::OpCode::LOAD_VAR :
  case bytecode::OpCode::LOAD_FAST :
  case bytecode::OpCode::LOAD_GLOBAL :
  case bytecode::OpCode::DUP : return 1;
  case bytecode::OpCode::POP :
  case bytecode::OpCode::STORE_VAR :
  case bytecode::OpCode::STORE_FAST :
  case bytecode::OpCode::STORE_GLOBAL : return -1;
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
  case bytecode::OpCode::GE : return -1;  // 2 operands -> 1 result
  case bytecode::OpCode::BUILD_LIST :
  case bytecode::OpCode::BUILD_TUPLE :
  case bytecode::OpCode::BUILD_SET : return 1 - arg;  // n items -> 1 collection
  case bytecode::OpCode::CALL : return -arg;          // func + n args -> result
  default : return 0;
  }
}

bool BytecodeVerifier::isJumpInstruction(bytecode::OpCode op) const
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD
      || op == bytecode::OpCode::JUMP_IF_FALSE || op == bytecode::OpCode::JUMP_IF_TRUE || op == bytecode::OpCode::POP_JUMP_IF_FALSE
      || op == bytecode::OpCode::POP_JUMP_IF_TRUE || op == bytecode::OpCode::FOR_ITER;
}

bool BytecodeVerifier::isUnconditionalJump(bytecode::OpCode op) const
{
  return op == bytecode::OpCode::JUMP || op == bytecode::OpCode::JUMP_FORWARD || op == bytecode::OpCode::JUMP_BACKWARD;
}

void VirtualMachine::push(const object::Value& val)
{
  if (stack_.size() >= 10000) throw std::runtime_error("Stack overflow");
  stack_.push_back(val);
}

object::Value VirtualMachine::pop()
{
  if (stack_.empty()) throw std::runtime_error("Stack underflow");
  object::Value val = stack_.back();
  stack_.pop_back();
  return val;
}

object::Value& VirtualMachine::top()
{
  if (stack_.empty()) throw std::runtime_error("Stack empty");
  return stack_.back();
}

object::Value& VirtualMachine::peek(std::int32_t offset)
{
  if (stack_.size() < offset + 1) throw std::runtime_error("Stack underflow in peek");
  return stack_[stack_.size() - 1 - offset];
}

// Fast integer operations (avoid type checking overhead)
object::Value VirtualMachine::fastAdd(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() + b.asInt());
  return a + b;
}

object::Value VirtualMachine::fastSub(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() - b.asInt());
  return a - b;
}

object::Value VirtualMachine::fastMul(const object::Value& a, const object::Value& b)
{
  if (a.isInt() && b.isInt()) return object::Value(a.asInt() * b.asInt());
  return a * b;
}

void VirtualMachine::registerNativeFunctions()
{
  // print
  nativeFunctions["print"] = [](const std::vector<object::Value>& args) {
    for (std::size_t i = 0; i < args.size(); i++)
    {
      std::cout << args[i].repr();
      if (i + 1 < args.size()) std::cout << " ";
    }
    std::cout << "\n";
    return object::Value();
  };
  // len
  nativeFunctions["len"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("len() takes 1 argument");
    if (args[0].isList()) return object::Value(static_cast<std::int64_t>(args[0].asList().size()));
    if (args[0].isString()) return object::Value(static_cast<std::int64_t>(args[0].asString().size()));
    throw std::runtime_error("len() requires list or string");
  };
  // range
  nativeFunctions["range"] = [](const std::vector<object::Value>& args) {
    if (args.empty() || args.size() > 3) throw std::runtime_error("range() takes 1-3 arguments");

    std::int64_t start = 0, stop, step = 1;
    if (args.size() == 1) { stop = args[0].toInt(); }
    else if (args.size() == 2)
    {
      start = args[0].toInt();
      stop  = args[1].toInt();
    }
    else
    {
      start = args[0].toInt();
      stop  = args[1].toInt();
      step  = args[2].toInt();
    }

    std::vector<object::Value> result;
    for (std::int64_t i = start; (step > 0) ? (i < stop) : (i > stop); i += step)
      result.push_back(object::Value(i));
    return object::Value(result);
  };
  // type
  nativeFunctions["type"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("type() takes 1 argument");
    string_type typeName;
    switch (args[0].getType())
    {
    case object::Value::Type::NONE : typeName = u"<class 'NoneType'>"; break;
    case object::Value::Type::INT : typeName = u"<class 'int'>"; break;
    case object::Value::Type::FLOAT : typeName = u"<class 'float'>"; break;
    case object::Value::Type::STRING : typeName = u"<class 'str'>"; break;
    case object::Value::Type::BOOL : typeName = u"<class 'bool'>"; break;
    case object::Value::Type::LIST : typeName = u"<class 'list'>"; break;
    case object::Value::Type::DICT : typeName = u"<class 'dict'>"; break;
    default : typeName = u"<class 'object'>"; break;
    }
    return object::Value(typeName);
  };
  // str
  nativeFunctions["str"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("str() takes 1 argument");
    return object::Value(args[0].asString());
  };
  // int
  nativeFunctions["int"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("int() takes 1 argument");
    return object::Value(args[0].toInt());
  };
  // float
  nativeFunctions["float"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("float() takes 1 argument");
    return object::Value(args[0].toFloat());
  };
  // sum
  nativeFunctions["sum"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) throw std::runtime_error("sum() takes at least 1 argument");
    if (!args[0].isList()) throw std::runtime_error("sum() requires iterable");
    object::Value total = args.size() > 1 ? args[1] : object::Value(static_cast<std::int64_t>(0));
    for (const object::Value& item : args[0].asList())
      total = total + item;
    return total;
  };
  // abs
  nativeFunctions["abs"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("abs() takes 1 argument");
    if (args[0].isInt()) return object::Value(std::abs(args[0].asInt()));
    return object::Value(std::abs(args[0].toFloat()));
  };
  // min
  nativeFunctions["min"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) throw std::runtime_error("min() requires at least 1 argument");
    if (args.size() == 1 && args[0].isList())
    {
      const std::vector<object::Value>& list = args[0].asList();
      if (list.empty()) throw std::runtime_error("min() arg is empty sequence");
      object::Value minVal = list[0];
      for (std::size_t i = 1; i < list.size(); i++)
        if (list[i] < minVal) minVal = list[i];
      return minVal;
    }
    object::Value minVal = args[0];
    for (std::size_t i = 1; i < args.size(); i++)
      if (args[i] < minVal) minVal = args[i];
    return minVal;
  };
  // max
  nativeFunctions["max"] = [](const std::vector<object::Value>& args) {
    if (args.empty()) throw std::runtime_error("max() requires at least 1 argument");
    if (args.size() == 1 && args[0].isList())
    {
      const std::vector<object::Value>& list = args[0].asList();
      if (list.empty()) throw std::runtime_error("max() arg is empty sequence");
      object::Value maxVal = list[0];
      for (std::size_t i = 1; i < list.size(); i++)
        if (list[i] > maxVal) maxVal = list[i];
      return maxVal;
    }
    object::Value maxVal = args[0];
    for (std::size_t i = 1; i < args.size(); i++)
      if (args[i] > maxVal) maxVal = args[i];
    return maxVal;
  };
  // sorted
  nativeFunctions["sorted"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("sorted() takes 1 argument");
    if (!args[0].isList()) throw std::runtime_error("sorted() requires list");
    std::vector<object::Value> result = args[0].asList();
    std::sort(result.begin(), result.end());
    return object::Value(result);
  };
  // reversed
  nativeFunctions["reversed"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("reversed() takes 1 argument");
    if (!args[0].isList()) throw std::runtime_error("reversed() requires list");
    std::vector<object::Value> result = args[0].asList();
    std::reverse(result.begin(), result.end());
    return object::Value(result);
  };
  // enumerate
  nativeFunctions["enumerate"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("enumerate() takes 1 argument");
    if (!args[0].isList()) throw std::runtime_error("enumerate() requires list");
    std::vector<object::Value>        result;
    const std::vector<object::Value>& list = args[0].asList();
    for (std::size_t i = 0; i < list.size(); i++)
    {
      std::vector<object::Value> pair = {object::Value(static_cast<std::int64_t>(i)), list[i]};
      result.push_back(object::Value(pair));
    }
    return object::Value(result);
  };
  // zip
  nativeFunctions["zip"] = [](const std::vector<object::Value>& args) {
    if (args.size() < 2) throw std::runtime_error("zip() requires at least 2 arguments");
    std::size_t minLen = SIZE_MAX;
    for (const object::Value& arg : args)
    {
      if (!arg.isList()) throw std::runtime_error("zip() requires lists");
      minLen = std::min(minLen, arg.asList().size());
    }
    std::vector<object::Value> result;
    for (std::size_t i = 0; i < minLen; i++)
    {
      std::vector<object::Value> tuple;
      for (const object::Value& arg : args)
        tuple.push_back(arg.asList()[i]);
      result.push_back(object::Value(tuple));
    }
    return object::Value(result);
  };
  // all
  nativeFunctions["all"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("all() takes 1 argument");
    if (!args[0].isList()) throw std::runtime_error("all() requires list");
    for (const object::Value& item : args[0].asList())
      if (!item.toBool()) return object::Value(false);
    return object::Value(true);
  };
  // any
  nativeFunctions["any"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("any() takes 1 argument");
    if (!args[0].isList()) throw std::runtime_error("any() requires list");
    for (const object::Value& item : args[0].asList())
      if (item.toBool()) return object::Value(true);
    return object::Value(false);
  };
  // map, filter
  nativeFunctions["map"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 2) throw std::runtime_error("map() takes 2 arguments");
    // Would need to handle function calls properly
    return object::Value();
  };
  // sqrt
  nativeFunctions["sqrt"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("sqrt() takes 1 argument");
    return object::Value(std::sqrt(args[0].toFloat()));
  };
  // pow
  nativeFunctions["pow"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 2) throw std::runtime_error("pow() takes 2 arguments");
    return object::Value(std::pow(args[0].toFloat(), args[1].toFloat()));
  };
  // round
  nativeFunctions["round"] = [](const std::vector<object::Value>& args) {
    if (args.size() != 1) throw std::runtime_error("round() takes 1 argument");
    return object::Value(static_cast<std::int64_t>(std::round(args[0].toFloat())));
  };
}

void VirtualMachine::execute(const BytecodeCompiler::CompilationUnit& code)
{
  globals_.resize(code.numCellVars);
  stack_.clear();
  stack_.reserve(1000);  // Pre-allocate for performance
  ip_            = 0;
  auto startTime = std::chrono::high_resolution_clock::now();
  // Main execution loop with computed goto (if supported)
  while (ip_ < code.instructions.size())
  {
    const bytecode::Instruction& instr = code.instructions[ip_];
    stats_.instructionsExecuted++;
    // JIT hot spot detection
    if (stats_.instructionsExecuted % 100 == 0) jit_.recordExecution(ip_);
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
  auto endTime         = std::chrono::high_resolution_clock::now();
  stats_.executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
}

void VirtualMachine::executeInstruction(const bytecode::Instruction& instr, const BytecodeCompiler::CompilationUnit& code)
{
  switch (instr.op)
  {
  case bytecode::OpCode::LOAD_CONST : push(code.constants[instr.arg]); break;
  case bytecode::OpCode::LOAD_VAR :
    if (instr.arg >= globals_.size()) throw std::runtime_error("Variable index out of range");
    push(globals_[instr.arg]);
    break;
  case bytecode::OpCode::LOAD_GLOBAL : push(globals_[instr.arg]); break;
  case bytecode::OpCode::STORE_VAR :
    if (instr.arg >= globals_.size()) globals_.resize(instr.arg + 1);
    globals_[instr.arg] = pop();
    break;
  case bytecode::OpCode::STORE_GLOBAL : globals_[instr.arg] = pop(); break;
  case bytecode::OpCode::POP : pop(); break;
  case bytecode::OpCode::DUP : push(top()); break;
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
    push(object::Value(static_cast<std::int64_t>(a.toFloat() / b.toFloat())));
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
      for (const object::Value& item : b.asList())
        if (item == a)
        {
          found = true;
          break;
        }
      push(object::Value(found));
    }
    else if (b.isString()) { push(object::Value(b.asString().find(a.toString()) != std::string::npos)); }
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
    if (!cond.toBool()) ip_ = instr.arg - 1;
    break;
  }
  case bytecode::OpCode::JUMP_IF_TRUE : {
    object::Value cond = pop();
    if (cond.toBool()) ip_ = instr.arg - 1;
    break;
  }
  case bytecode::OpCode::POP_JUMP_IF_FALSE : {
    if (!top().toBool()) ip_ = instr.arg - 1;
    break;
  }
  case bytecode::OpCode::FOR_ITER : {
    object::Value& iterator = top();
    if (iterator.hasNext()) { push(iterator.next()); }
    else
    {
      pop();  // Remove exhausted iterator
      ip_ = instr.arg - 1;
    }
    break;
  }
  // Function calls
  case bytecode::OpCode::CALL : {
    std::int32_t               numArgs = instr.arg;
    std::vector<object::Value> args;
    for (std::int32_t i = 0; i < numArgs; i++)
      args.insert(args.begin(), pop());
    object::Value func = pop();
    // Check for native function
    if (func.isString())
    {
      std::string funcName = utf8::utf16to8(func.asString());
      auto        it       = nativeFunctions.find(funcName);
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
  case bytecode::OpCode::RETURN : return;  // Exit function
  case bytecode::OpCode::YIELD :
    // Generator support
    throw std::runtime_error("Generators not yet implemented");
  // Collections
  case bytecode::OpCode::BUILD_LIST : {
    std::vector<object::Value> elements;
    for (std::int32_t i = 0; i < instr.arg; i++)
      elements.insert(elements.begin(), pop());
    push(object::Value(elements));
    break;
  }
  case bytecode::OpCode::BUILD_DICT : {
    std::unordered_map<string_type, object::Value> dict;
    for (std::int32_t i = 0; i < instr.arg; i++)
    {
      object::Value val    = pop();
      object::Value key    = pop();
      dict[key.asString()] = val;
    }
    std::shared_ptr<std::unordered_map<string_type, object::Value>> dictPtr = std::make_shared<std::unordered_map<string_type, object::Value>>(dict);
    object::Value                                                   result;
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
    if (!seq.isList()) throw std::runtime_error("Cannot unpack non-sequence");
    const std::vector<object::Value>& list = seq.asList();
    if (list.size() != instr.arg) throw std::runtime_error("Unpack size mismatch");
    for (const object::Value& item : list)
      push(item);
    break;
  }
  case bytecode::OpCode::GET_ITEM : {
    object::Value key = pop();
    object::Value obj = pop();
    push(obj.getItem(key));
    break;
  }
  case bytecode::OpCode::SET_ITEM : {
    object::Value  val = pop();
    object::Value  key = pop();
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
    for (std::int32_t i = 0; i < instr.arg; i++)
      args.insert(args.begin(), pop());
    for (std::size_t i = 0; i < args.size(); i++)
    {
      std::cout << args[i].repr();
      if (i + 1 < args.size()) std::cout << " ";
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
  case bytecode::OpCode::HALT : return;
  default : throw std::runtime_error("Unknown opcode: " + std::to_string(static_cast<std::int32_t>(instr.op)));
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
    std::cout << "Instructions/second:    " << static_cast<std::int64_t>(ips) << "\n";
  }

  std::cout << "\nStack size:            " << stack_.size() << "\n";
  std::cout << "Global variables:       " << globals_.size() << "\n";
}

}
}