#include "../../../include/runtime/compiler/compiler.hpp"

namespace mylang {
namespace runtime {

Reg Compiler::allocReg()
{
    Reg r = Current_->allocReg();
    if (r >= MAX_REGISTERS) {
        error("too many registers in function '" + Current_->funcName + "'", 0);
        return 0;
    }

    return r;
}

void Compiler::freeReg() { Current_->freeReg(); }
void Compiler::freeRegsTo(Reg mark) { Current_->freeRegsTo(mark); }
Reg Compiler::ensureDst(Reg* dst) { return dst ? *dst : allocReg(); }
Reg Compiler::errorReg() { return 0; }

}

}
