#include "../../../include/runtime/vm/btcompiler.hpp"
#include "../../../include/diag/diagnostic.hpp"

#include <iomanip>
#include <iostream>


namespace mylang {
namespace runtime {

void BytecodeCompiler::emitInstruction(bytecode::OpCode op, std::int32_t arg, std::int32_t line)
{
  bytecode::Instruction instr(op, arg, line);
  Unit_.instructions.push_back(instr);
  Stats_.InstructionsGenerated++;
  // Track stack depth
  updateStackDepth(op);
  // Track line numbers
  if (line > 0) Unit_.LineNumbers.push_back(line);
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
  case bytecode::OpCode::DUP : CurrentStackDepth_++; break;
  case bytecode::OpCode::POP :
  case bytecode::OpCode::STORE_VAR :
  case bytecode::OpCode::STORE_GLOBAL :
  case bytecode::OpCode::STORE_FAST : CurrentStackDepth_--; break;
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
  case bytecode::OpCode::OR : CurrentStackDepth_--; break;
  case bytecode::OpCode::CALL :
    // Function + args -> result
    /// TODO:: Stack depth change handled separately
    break;
  default : break;
  }

  MaxStackDepth_ = std::max(MaxStackDepth_, CurrentStackDepth_);
}

std::int32_t BytecodeCompiler::getCurrentPC() const { return Unit_.instructions.size(); }

void BytecodeCompiler::patchJump(std::int32_t jumpIndex) { Unit_.instructions[jumpIndex].arg = getCurrentPC(); }

void BytecodeCompiler::enterScope()
{
  CompilerSymbolTable* newScope = new CompilerSymbolTable(CurrentScope_.get());
  if (CurrentScope_) ScopeStack_.push(CurrentScope_.release());
  CurrentScope_.reset(newScope);
}

void BytecodeCompiler::exitScope()
{
  if (!ScopeStack_.empty())
  {
    CurrentScope_.reset(ScopeStack_.top());
    ScopeStack_.pop();
  }
}

void BytecodeCompiler::compileExpr(const parser::ast::Expr* expr)
{
  if (!expr) return;

  switch (expr->getKind())
  {
  case parser::ast::Expr::Kind::LITERAL : {
    const parser::ast::LiteralExpr* lit = static_cast<const parser::ast::LiteralExpr*>(expr);
    object::Value                   val;
    switch (lit->getType())
    {
    case parser::ast::LiteralExpr::Type::NUMBER : {
      if (lit->getValue().find('.') != std::string::npos)
        val = object::Value(std::stod(utf8::utf16to8(lit->getValue())));
      else
        val = object::Value(static_cast<std::int64_t>(std::stoll(utf8::utf16to8(lit->getValue()))));
      break;
    }
    case parser::ast::LiteralExpr::Type::STRING : val = object::Value(lit->getValue()); break;
    case parser::ast::LiteralExpr::Type::BOOLEAN : val = object::Value(lit->getValue() == u"true" || lit->getValue() == u"صحيح"); break;
    case parser::ast::LiteralExpr::Type::NONE : val = object::Value(); break;
    }
    std::int32_t idx = Constants_.addConstant(val);
    emitInstruction(bytecode::OpCode::LOAD_CONST, idx, expr->getLine());
    break;
  }

  case parser::ast::Expr::Kind::NAME : {
    const parser::ast::NameExpr* name = static_cast<const parser::ast::NameExpr*>(expr);
    CompilerSymbolTable::Symbol* sym  = CurrentScope_->resolve(utf8::utf16to8(name->getValue()));
    if (!sym) diagnostic::engine.panic("Undefined variable: " + utf8::utf16to8(name->getValue()));
    switch (sym->scope)
    {
    case CompilerSymbolTable::SymbolScope::GLOBAL : emitInstruction(bytecode::OpCode::LOAD_GLOBAL, sym->index, expr->getLine()); break;
    case CompilerSymbolTable::SymbolScope::LOCAL : emitInstruction(bytecode::OpCode::LOAD_FAST, sym->index, expr->getLine()); break;
    case CompilerSymbolTable::SymbolScope::CLOSURE : emitInstruction(bytecode::OpCode::LOAD_CLOSURE, sym->index, expr->getLine()); break;
    default : emitInstruction(bytecode::OpCode::LOAD_VAR, sym->index, expr->getLine()); break;
    }
    break;
  }

  case parser::ast::Expr::Kind::BINARY : {
    const parser::ast::BinaryExpr* bin = static_cast<const parser::ast::BinaryExpr*>(expr);
    // Short-circuit evaluation for logical operators
    if (bin->getOperator() == lex::tok::TokenType::KW_AND)
    {
      compileExpr(bin->getLeft());
      emitInstruction(bytecode::OpCode::DUP, 0, expr->getLine());
      std::int32_t jumpIfFalse = getCurrentPC();
      emitInstruction(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->getLine());
      emitInstruction(bytecode::OpCode::POP, 0, expr->getLine());
      compileExpr(bin->getRight());
      patchJump(jumpIfFalse);
      break;
    }
    if (bin->getOperator() == lex::tok::TokenType::KW_OR)
    {
      compileExpr(bin->getLeft());
      emitInstruction(bytecode::OpCode::DUP, 0, expr->getLine());
      std::int32_t jumpIfTrue = getCurrentPC();
      emitInstruction(bytecode::OpCode::POP_JUMP_IF_TRUE, 0, expr->getLine());
      emitInstruction(bytecode::OpCode::POP, 0, expr->getLine());
      compileExpr(bin->getRight());
      patchJump(jumpIfTrue);
      break;
    }
    // Regular binary operations
    compileExpr(bin->getLeft());
    compileExpr(bin->getRight());
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS) { emitInstruction(bytecode::OpCode::ADD, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_MINUS) { emitInstruction(bytecode::OpCode::SUB, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_STAR) { emitInstruction(bytecode::OpCode::MUL, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_SLASH) { emitInstruction(bytecode::OpCode::DIV, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_PERCENT) { emitInstruction(bytecode::OpCode::MOD, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_POWER) { emitInstruction(bytecode::OpCode::POW, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_EQ) { emitInstruction(bytecode::OpCode::EQ, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_NEQ) { emitInstruction(bytecode::OpCode::NE, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_LT) { emitInstruction(bytecode::OpCode::LT, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_GT) { emitInstruction(bytecode::OpCode::GT, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_LTE) { emitInstruction(bytecode::OpCode::LE, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_GTE) { emitInstruction(bytecode::OpCode::GE, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_BITAND) { emitInstruction(bytecode::OpCode::BITAND, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_BITOR) { emitInstruction(bytecode::OpCode::BITOR, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_BITXOR) { emitInstruction(bytecode::OpCode::BITXOR, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_LSHIFT) { emitInstruction(bytecode::OpCode::LSHIFT, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_RSHIFT) { emitInstruction(bytecode::OpCode::RSHIFT, 0, expr->getLine()); }
    else if (bin->getOperator() == lex::tok::TokenType::KW_IN) { emitInstruction(bytecode::OpCode::IN, 0, expr->getLine()); }
    break;
  }

  case parser::ast::Expr::Kind::UNARY : {
    const parser::ast::UnaryExpr* un = static_cast<const parser::ast::UnaryExpr*>(expr);
    compileExpr(un->getOperand());
    if (un->getOperator() == lex::tok::TokenType::OP_MINUS) { emitInstruction(bytecode::OpCode::NEG, 0, expr->getLine()); }
    else if (un->getOperator() == lex::tok::TokenType::OP_PLUS) { emitInstruction(bytecode::OpCode::POS, 0, expr->getLine()); }
    else if (un->getOperator() == lex::tok::TokenType::OP_BITNOT) { emitInstruction(bytecode::OpCode::BITNOT, 0, expr->getLine()); }
    else if (un->getOperator() == lex::tok::TokenType::KW_NOT) { emitInstruction(bytecode::OpCode::NOT, 0, expr->getLine()); }
    break;
  }

  case parser::ast::Expr::Kind::CALL : {
    const parser::ast::CallExpr* call = static_cast<const parser::ast::CallExpr*>(expr);
    // Compile arguments first
    for (const parser::ast::Expr* arg : call->getArgs()) compileExpr(arg);
    // Compile callee
    compileExpr(call->getCallee());
    // Emit call instruction
    emitInstruction(bytecode::OpCode::CALL, call->getArgs().size(), expr->getLine());
    // Adjust stack depth
    CurrentStackDepth_ -= call->getArgs().size();
    break;
  }

  case parser::ast::Expr::Kind::LIST : {
    const parser::ast::ListExpr* list = static_cast<const parser::ast::ListExpr*>(expr);
    // Compile all elements
    for (const parser::ast::Expr* elem : list->getElements()) compileExpr(elem);
    // Build list from stack
    emitInstruction(bytecode::OpCode::BUILD_LIST, list->getElements().size(), expr->getLine());
    // Adjust stack depth
    CurrentStackDepth_ -= list->getElements().size();
    CurrentStackDepth_++;
    break;
  }

    /*
  case parser::ast::Expr::Kind::TERNARY : {
    const parser::ast::TernaryExpr* tern = static_cast<const parser::ast::TernaryExpr*>(expr);
    // Compile condition
    compileExpr(tern->condition.get());
    std::int32_t jumpIfFalse = getCurrentPC();
    emitInstruction(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, expr->line);
    // True branch
    compileExpr(tern->trueExpr.get());
    std::int32_t jumpEnd = getCurrentPC();
    emitInstruction(bytecode::OpCode::JUMP, 0, expr->line);
    // False branch
    patchJump(jumpIfFalse);
    if (tern->falseExpr) compileExpr(tern->falseExpr.get());
    patchJump(jumpEnd);
    break;
  }
  */

  case parser::ast::Expr::Kind::ASSIGNMENT : {
    const parser::ast::AssignmentExpr* assign = static_cast<const parser::ast::AssignmentExpr*>(expr);
    // Compile value
    compileExpr(assign->getValue());
    // Duplicate for expression result
    emitInstruction(bytecode::OpCode::DUP, 0, expr->getLine());
    // Store
    CompilerSymbolTable::Symbol* sym = CurrentScope_->define(utf8::utf16to8(assign->getTarget()->getValue()));
    if (sym->scope == CompilerSymbolTable::SymbolScope::GLOBAL)
      emitInstruction(bytecode::OpCode::STORE_GLOBAL, sym->index, expr->getLine());
    else
      emitInstruction(bytecode::OpCode::STORE_FAST, sym->index, expr->getLine());
    break;
  }

  default : diagnostic::engine.panic("Unsupported expression type");
  }
}

void BytecodeCompiler::compileStmt(const parser::ast::Stmt* stmt)
{
  if (!stmt) return;

  switch (stmt->getKind())
  {
  case parser::ast::Stmt::Kind::ASSIGNMENT : {
    const parser::ast::AssignmentStmt* assign = static_cast<const parser::ast::AssignmentStmt*>(stmt);
    // Compile value
    compileExpr(assign->getValue());
    // Store to variable
    parser::ast::Expr* target = assign->getTarget();
    assert(target);
    StringType target_name = u"";
    /// TODO: check other type of target expressions
    if (target->getKind() == parser::ast::Expr::Kind::NAME) target_name = dynamic_cast<parser::ast::NameExpr*>(target)->getValue();
    CompilerSymbolTable::Symbol* sym = CurrentScope_->define(utf8::utf16to8(target_name));
    if (sym->scope == CompilerSymbolTable::SymbolScope::GLOBAL)
      emitInstruction(bytecode::OpCode::STORE_GLOBAL, sym->index, stmt->getLine());
    else
      emitInstruction(bytecode::OpCode::STORE_FAST, sym->index, stmt->getLine());
    break;
  }

  case parser::ast::Stmt::Kind::EXPR : {
    const parser::ast::ExprStmt* exprStmt = static_cast<const parser::ast::ExprStmt*>(stmt);
    compileExpr(exprStmt->getExpr());
    emitInstruction(bytecode::OpCode::POP, 0, stmt->getLine());  // Discard result
    break;
  }

  case parser::ast::Stmt::Kind::IF : {
    const parser::ast::IfStmt* ifStmt = static_cast<const parser::ast::IfStmt*>(stmt);
    // Compile condition
    compileExpr(ifStmt->getCondition());
    std::int32_t jumpIfFalse = getCurrentPC();
    emitInstruction(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->getLine());
    // Then block
    for (const parser::ast::Stmt* s : ifStmt->getThenBlock()->getStatements()) compileStmt(s);
    if (!ifStmt->getElseBlock()->isEmpty())
    {
      std::int32_t jumpEnd = getCurrentPC();
      emitInstruction(bytecode::OpCode::JUMP, 0, stmt->getLine());
      patchJump(jumpIfFalse);
      // Else block
      for (const parser::ast::Stmt* s : ifStmt->getElseBlock()->getStatements()) compileStmt(s);
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
    emitInstruction(bytecode::OpCode::HOT_LOOP_START, 0, stmt->getLine());
    // Compile condition
    compileExpr(whileStmt->getCondition());
    std::int32_t jumpIfFalse = getCurrentPC();
    emitInstruction(bytecode::OpCode::POP_JUMP_IF_FALSE, 0, stmt->getLine());
    // Loop body
    LoopContext ctx;
    ctx.StartPC       = loopStart;
    ctx.ContinueLabel = loopStart;
    ctx.BreakLabel    = -1;  // Will be patched
    LoopStack_.push(ctx);
    for (const parser::ast::Stmt* s : whileStmt->getBlock()->getStatements()) compileStmt(s);
    // Jump back to loop start
    emitInstruction(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->getLine());
    patchJump(jumpIfFalse);
    emitInstruction(bytecode::OpCode::HOT_LOOP_END, 0, stmt->getLine());
    // Patch break statements
    if (LoopStack_.top().BreakLabel != -1) patchJump(LoopStack_.top().BreakLabel);
    LoopStack_.pop();
    Stats_.LoopsDetected++;
    break;
  }

  case parser::ast::Stmt::Kind::FOR : {
    const parser::ast::ForStmt* forStmt = static_cast<const parser::ast::ForStmt*>(stmt);
    // Compile iterator
    compileExpr(forStmt->getIter());
    emitInstruction(bytecode::OpCode::GET_ITER, 0, stmt->getLine());
    std::int32_t loopStart = getCurrentPC();
    emitInstruction(bytecode::OpCode::HOT_LOOP_START, 0, stmt->getLine());
    // FOR_ITER gets next item or jumps to end
    std::int32_t forIter = getCurrentPC();
    emitInstruction(bytecode::OpCode::FOR_ITER_FAST, 0, stmt->getLine());
    // Store loop variable
    CompilerSymbolTable::Symbol* sym = CurrentScope_->define(utf8::utf16to8(forStmt->getTarget()->getValue()));
    emitInstruction(bytecode::OpCode::STORE_FAST, sym->index, stmt->getLine());
    // Loop body
    LoopContext ctx;
    ctx.StartPC       = loopStart;
    ctx.ContinueLabel = loopStart;
    ctx.BreakLabel    = -1;
    LoopStack_.push(ctx);
    for (const parser::ast::Stmt* s : forStmt->getBlock()->getStatements()) compileStmt(s);
    // Jump back
    emitInstruction(bytecode::OpCode::JUMP_BACKWARD, loopStart, stmt->getLine());
    // Patch FOR_ITER to jump here when exhausted
    patchJump(forIter);
    emitInstruction(bytecode::OpCode::HOT_LOOP_END, 0, stmt->getLine());
    if (LoopStack_.top().BreakLabel != -1) patchJump(LoopStack_.top().BreakLabel);
    LoopStack_.pop();
    Stats_.LoopsDetected++;
    break;
  }

  case parser::ast::Stmt::Kind::FUNC : {
    const parser::ast::FunctionDef* funcDef = static_cast<const parser::ast::FunctionDef*>(stmt);
    // Enter new scope for function
    enterScope();
    // Define parameters
    for (const parser::ast::Expr* param : funcDef->getParameters())
      CurrentScope_->define(utf8::utf16to8(static_cast<const parser::ast::NameExpr*>(param)->getValue()), true);
    // Save current compilation state
    std::vector<bytecode::Instruction>&& savedInstructions  = std::move(Unit_.instructions);
    ConstantPool                         savedConstants     = Constants_;
    std::int32_t                         savedStackDepth    = CurrentStackDepth_;
    std::int32_t                         savedMaxStackDepth = MaxStackDepth_;
    Unit_.instructions.clear();
    CurrentStackDepth_ = 0;
    MaxStackDepth_     = 0;
    // Compile function body
    for (const parser::ast::Stmt* s : funcDef->getBody()->getStatements()) compileStmt(s);
    // Implicit return None if no return statement
    if (Unit_.instructions.empty() || Unit_.instructions.back().op != bytecode::OpCode::RETURN)
    {
      std::int32_t noneIdx = Constants_.addConstant(object::Value());
      emitInstruction(bytecode::OpCode::LOAD_CONST, noneIdx, stmt->getLine());
      emitInstruction(bytecode::OpCode::RETURN, 0, stmt->getLine());
    }
    // Create function object
    std::vector<bytecode::Instruction>&& funcInstructions = std::move(Unit_.instructions);
    std::int32_t                         funcStackSize    = MaxStackDepth_;
    // Restore compilation state
    Unit_.instructions = std::move(savedInstructions);
    Constants_         = savedConstants;
    CurrentStackDepth_ = savedStackDepth;
    MaxStackDepth_     = savedMaxStackDepth;
    // Store function object as constant
    object::Value funcObj;  // Would create FunctionObject here
    std::int32_t  funcIdx = Constants_.addConstant(funcObj);
    emitInstruction(bytecode::OpCode::LOAD_CONST, funcIdx, stmt->getLine());
    emitInstruction(bytecode::OpCode::MAKE_FUNCTION, funcDef->getParameters().size(), stmt->getLine());
    // Store function
    CompilerSymbolTable::Symbol* sym = CurrentScope_->define(utf8::utf16to8(funcDef->getName()->getValue()));
    emitInstruction(bytecode::OpCode::STORE_FAST, sym->index, stmt->getLine());
    exitScope();
    break;
  }

  case parser::ast::Stmt::Kind::RETURN : {
    const parser::ast::ReturnStmt* ret = static_cast<const parser::ast::ReturnStmt*>(stmt);
    if (ret->getValue())
      compileExpr(ret->getValue());
    else
      emitInstruction(bytecode::OpCode::LOAD_CONST, Constants_.addConstant(object::Value()), stmt->getLine());
    emitInstruction(bytecode::OpCode::RETURN, 0, stmt->getLine());
    break;
  }

  case parser::ast::Stmt::Kind::BLOCK : {
    const parser::ast::BlockStmt* block = static_cast<const parser::ast::BlockStmt*>(stmt);
    for (const parser::ast::Stmt* s : block->getStatements()) compileStmt(s);
    break;
  }

  default : diagnostic::engine.panic("Unsupported statement type");
  }
}

typename BytecodeCompiler::CompilationUnit BytecodeCompiler::compile(const std::vector<parser::ast::Stmt*>& ast)
{
  // Reset state
  Unit_              = CompilationUnit();
  Stats_             = Stats();
  CurrentStackDepth_ = MaxStackDepth_ = 0;
  // Compile all statements
  for (const parser::ast::Stmt* stmt : ast) compileStmt(stmt);
  // Add HALT at end
  emitInstruction(bytecode::OpCode::HALT, 0, 0);
  // Finalize constant pool
  Unit_.constants          = Constants_.getConstants();
  Stats_.ConstantsPoolSize = Unit_.constants.size();
  // Resolve all jumps
  Jumps_.resolveJumps(Unit_.instructions);
  // Detect loops for optimization
  LoopAnalyzer_.detectLoops(Unit_.instructions);
  LoopAnalyzer_.findInvariants(Unit_.instructions, *CurrentScope_);
  // Apply peephole optimizations
  Peephole_.optimize(Unit_.instructions);
  Stats_.PeepholeOptimizations = Peephole_.getOptimizations().size();
  // Set metadata
  Unit_.NumLocals = CurrentScope_->getLocalCount();
  Unit_.StackSize = MaxStackDepth_;
  // Report unused variables
  std::vector<CompilerSymbolTable::Symbol> unused = CurrentScope_->getUnusedSymbols();
  if (!unused.empty()) std::cout << "[Compiler] Warning: " << unused.size() << " unused variables detected\n";
  return Unit_;
}

void BytecodeCompiler::disassemble(const CompilationUnit& unit, std::ostream& out) const
{
  out << "=== Bytecode Disassembly ===\n\n";
  out << "Constants Pool (" << unit.constants.size() << " entries):\n";
  for (std::size_t i = 0; i < unit.constants.size(); i++) out << "  [" << i << "] " << unit.constants[i].repr() << "\n";
  out << "\nCode (" << unit.instructions.size() << " instructions):\n";
  out << "Stack size: " << unit.StackSize << "\n";
  out << "Locals: " << unit.NumLocals << "\n\n";

  for (std::size_t i = 0; i < unit.instructions.size(); i++)
  {
    const bytecode::Instruction& instr = unit.instructions[i];
    out << std::setw(6) << i << "  ";
    // Line number
    if (instr.LineNumber > 0)
      out << std::setw(4) << instr.LineNumber << "  ";
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
  out << "Instructions generated: " << Stats_.InstructionsGenerated << "\n";
  out << "Constants in pool: " << Stats_.ConstantsPoolSize << "\n";
  out << "Loops detected: " << Stats_.LoopsDetected << "\n";
  out << "Peephole optimizations: " << Stats_.PeepholeOptimizations << "\n";
  // Loop analysis
  const std::vector<LoopAnalyzer::Loop>& loops = LoopAnalyzer_.getLoops();
  if (!loops.empty())
  {
    out << "\nLoop Analysis:\n";
    for (std::size_t i = 0; i < loops.size(); i++)
    {
      const LoopAnalyzer::Loop& loop = loops[i];
      out << "  Loop " << i << ": PC " << loop.HeaderPC << " -> " << loop.ExitPC << " (nesting: " << loop.NestingLevel << ")\n";
      out << "    Invariants: " << loop.invariants.size() << " variables\n";
    }
  }
  // Peephole report
  out << "\n";
  Peephole_.printReport();
}

void BytecodeCompiler::optimizationReport(std::ostream& out) const
{
  out << "\n╔═══════════════════════════════════════╗\n";
  out << "║    Bytecode Optimization Report       ║\n";
  out << "╚═══════════════════════════════════════╝\n\n";
  out << "Code Size:\n";
  out << "  Instructions: " << Stats_.InstructionsGenerated << "\n";
  out << "  Constants: " << Stats_.ConstantsPoolSize << "\n";
  out << "  Stack size: " << MaxStackDepth_ << "\n\n";
  out << "Loop Optimizations:\n";
  out << "  Loops detected: " << Stats_.LoopsDetected << "\n";
  const std::vector<LoopAnalyzer::Loop>& loops           = LoopAnalyzer_.getLoops();
  std::int32_t                           totalInvariants = 0;
  for (const LoopAnalyzer::Loop& loop : loops) totalInvariants += loop.invariants.size();
  out << "  Hoistable invariants: " << totalInvariants << "\n\n";
  out << "Peephole Optimizations:\n";
  if (Stats_.PeepholeOptimizations > 0)
    Peephole_.printReport();
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

}
}