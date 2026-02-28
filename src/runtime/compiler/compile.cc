#include "../../../include/ast/ast.hpp"
#include "../../../include/runtime/compiler/compiler.hpp"
#include "../../../include/runtime/runtime_allocator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mylang {
namespace runtime {

using namespace ast;

StringRef CompileError::format() const
{
    std::ostringstream ss;
    ss << "[line " << line << "] in '" << context << "': " << message;
    return ss.str().data();
}

Chunk* Compiler::compile(std::vector<ast::Stmt*> const& stmts)
{
    if (stmts.empty() || !stmts[0]) {
        error("null AST root", 0);
        return nullptr;
    }

    // Build the top-level chunk
    Chunk* top_chunk = runtime_allocator.allocateObject<Chunk>();
    top_chunk->name = "<main>";

    CompilerState state;
    state.chunk = top_chunk;
    state.funcName = "<main>";
    state.isTopLevel = true;
    state.enclosing = nullptr;
    Current_ = &state;

    for (ast::Stmt const* s : stmts)
        compileStmt(s);

    // Emit implicit RETURN_NIL if the function didn't already return
    if (!isDead_)
        emit(make_ABC(static_cast<uint8_t>(OpCode::RETURN_NIL), 0, 0, 0), stmts.back()->getLine());

    top_chunk->local_count = state.maxReg;
    top_chunk->upvalue_count = static_cast<int>(state.upvalues.size());

    Current_ = nullptr;

    if (had_error())
        return nullptr;

    return top_chunk;
}

// Utilities

void Compiler::error(StringRef const& msg, uint32_t line)
{
    StringRef ctx = Current_ ? Current_->funcName : "<unknown>";
    Errors_.push_back({ msg, line, ctx });
}

} // namespace runtime
} // namespace mylang
