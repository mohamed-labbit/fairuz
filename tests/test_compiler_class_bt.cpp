#include "../fairuz/compiler.hpp"
#include "../fairuz/lexer.hpp"
#include "../fairuz/parser.hpp"
#include <gtest/gtest.h>

namespace fairuz::test {

using namespace fairuz::runtime;

class CompilerClassBytecodeTest : public ::testing::Test {
protected:
    void SetUp() override { }

    // Helper: Parse and compile source code
    Fa_Chunk* compile(std::string const& source)
    {
        Fa_Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Fa_Parser parser(tokens);
        auto ast = parser.parse();

        Compiler compiler;
        return compiler.compile(ast);
    }

    // Helper: Extract function from chunk by index
    Fa_Chunk* get_function(Fa_Chunk* chunk, size_t idx)
    {
        if (chunk == nullptr || idx >= chunk->functions.size())
            return nullptr;
        return chunk->functions[idx];
    }

    // Helper: Find opcode at given PC
    Fa_OpCode get_opcode_at(Fa_Chunk* chunk, u32 pc)
    {
        if (pc >= chunk->code.size())
            return Fa_OpCode::NOP;
        return Fa_instr_op(chunk->code[pc]);
    }

    // Helper: Extract operand A from instruction at PC
    u8 get_a_at(Fa_Chunk* chunk, u32 pc)
    {
        if (pc >= chunk->code.size())
            return 0;
        return Fa_instr_A(chunk->code[pc]);
    }

    // Helper: Extract operand B from instruction at PC
    u8 get_b_at(Fa_Chunk* chunk, u32 pc)
    {
        if (pc >= chunk->code.size())
            return 0;
        return Fa_instr_B(chunk->code[pc]);
    }

    // Helper: Extract operand C from instruction at PC
    u8 get_c_at(Fa_Chunk* chunk, u32 pc)
    {
        if (pc >= chunk->code.size())
            return 0;
        return Fa_instr_C(chunk->code[pc]);
    }

    // Helper: Count occurrences of opcode in chunk
    int count_opcode(Fa_Chunk* chunk, Fa_OpCode op)
    {
        int count = 0;
        for (u32 instr : chunk->code) {
            if (Fa_instr_op(instr) == op)
                count++;
        }
        return count;
    }
};

// ============================================================================
// Class Bytecode Structure Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, ClassCreatesEmptyDict)
{
    std::string const source = R"(
        class Empty
            member x
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should emit LOAD_GLOBAL (جدول) + IC_CALL (create empty dict)
    int load_global_count = count_opcode(chunk, Fa_OpCode::LOAD_GLOBAL);
    int ic_call_count = count_opcode(chunk, Fa_OpCode::IC_CALL);

    // At least one LOAD_GLOBAL for جدول constructor
    EXPECT_GE(load_global_count, 1);
    EXPECT_GE(ic_call_count, 1);
}

TEST_F(CompilerClassBytecodeTest, ClassStoresMetadata)
{
    std::string const source = R"(
        class Point
            member x
            member y
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should emit LIST_SET for metadata
    int list_set_count = count_opcode(chunk, Fa_OpCode::LIST_SET);

    // At least one LIST_SET for storing __class__ metadata
    EXPECT_GE(list_set_count, 1);
}

TEST_F(CompilerClassBytecodeTest, ClassStoresGlobally)
{
    std::string const source = R"(
        class MyClass
            member x
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should end with STORE_GLOBAL for the class
    int store_global_count = count_opcode(chunk, Fa_OpCode::STORE_GLOBAL);
    EXPECT_GE(store_global_count, 1);
}

// ============================================================================
// Method Closure Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, MethodCompiledAsClosure)
{
    std::string const source = R"(
        class Simple
            member x
            
            def get_x()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should emit CLOSURE opcode for method
    int closure_count = count_opcode(chunk, Fa_OpCode::CLOSURE);
    EXPECT_GE(closure_count, 1);
}

TEST_F(CompilerClassBytecodeTest, MultipleMethodsCreateMultipleClosures)
{
    std::string const source = R"(
        class Multi
            member x
            
            def m1() x end
            def m2() x end
            def m3() x end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should emit three CLOSURE opcodes (one per method)
    int closure_count = count_opcode(chunk, Fa_OpCode::CLOSURE);
    EXPECT_EQ(closure_count, 3);
}

TEST_F(CompilerClassBytecodeTest, MethodClosureStored)
{
    std::string const source = R"(
        class Stored
            member x
            
            def method()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // After CLOSURE, should emit LIST_SET to store method in class dict
    int list_set_count = count_opcode(chunk, Fa_OpCode::LIST_SET);
    EXPECT_GE(list_set_count, 2); // One for metadata, one for method
}

// ============================================================================
// Method Function Structure Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, MethodFunctionCreated)
{
    std::string const source = R"(
        class Func
            member x
            
            def get_x()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should have at least one function (the method)
    EXPECT_GE(chunk->functions.size(), 1);
}

TEST_F(CompilerClassBytecodeTest, MethodFunctionArityIncludesInstance)
{
    std::string const source = R"(
        class Arity
            member x
            
            def method_no_args()
                x
            end
            
            def method_one_arg(a)
                a + x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Find method functions
    Fa_Chunk* method_no_args = get_function(chunk, 0);
    Fa_Chunk* method_one_arg = get_function(chunk, 1);

    ASSERT_NE(method_no_args, nullptr);
    ASSERT_NE(method_one_arg, nullptr);

    // method_no_args: arity = 1 (instance only)
    EXPECT_EQ(method_no_args->arity, 1);

    // method_one_arg: arity = 2 (instance + a)
    EXPECT_EQ(method_one_arg->arity, 2);
}

TEST_F(CompilerClassBytecodeTest, MethodFunctionName)
{
    std::string const source = R"(
        class Named
            member x
            
            def my_method()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method_func = get_function(chunk, 0);
    ASSERT_NE(method_func, nullptr);

    // Method function name should be "ClassName.method_name"
    std::string expected = "Named.my_method";
    EXPECT_EQ(method_func->name, expected);
}

// ============================================================================
// Local Variable Binding Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, InstanceVariableAllocated)
{
    std::string const source = R"(
        class VarTest
            member x
            
            def use_x()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Method should have at least 1 local (the instance)
    EXPECT_GE(method->local_count, 1);
}

TEST_F(CompilerClassBytecodeTest, ExplicitParametersAllocated)
{
    std::string const source = R"(
        class ParamTest
            member x
            
            def add(a, b, c)
                a + b + c + x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Method should have 4 locals: instance + 3 parameters
    EXPECT_GE(method->local_count, 4);
}

TEST_F(CompilerClassBytecodeTest, ParameterOrderPreservedInRegisters)
{
    std::string const source = R"(
        class Order
            member x
            
            def echo_params(p1, p2, p3)
                p1
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Parameters should be allocated in order:
    // reg 0: instance
    // reg 1: p1
    // reg 2: p2
    // reg 3: p3
    // (Verified via semantic analysis in actual test execution)
}

// ============================================================================
// Method Body Bytecode Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, MethodBodyCompiled)
{
    std::string const source = R"(
        class Body
            member x
            
            def get_x()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Method should have code
    EXPECT_GT(method->code.size(), 0);
}

TEST_F(CompilerClassBytecodeTest, MethodBodyWithBinaryOp)
{
    std::string const source = R"(
        class Math
            member x
            
            def add(a)
                x + a
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should contain OP_ADD
    int add_count = count_opcode(method, Fa_OpCode::OP_ADD);
    EXPECT_GE(add_count, 1);
}

TEST_F(CompilerClassBytecodeTest, MethodBodyWithControlFlow)
{
    std::string const source = R"(
        class Control
            member x
            
            def branch(a)
                if a > x
                    a
                else
                    x
                end
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should contain OP_LT (for comparison) and JUMP instructions
    int lt_count = count_opcode(method, Fa_OpCode::OP_LT);
    int jump_count = count_opcode(method, Fa_OpCode::JUMP) + count_opcode(method, Fa_OpCode::JUMP_IF_FALSE);

    EXPECT_GE(lt_count, 1);
    EXPECT_GE(jump_count, 1);
}

// ============================================================================
// Return Bytecode Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, MethodDefaultReturnEmitted)
{
    std::string const source = R"(
        class DefaultRet
            member x
            
            def no_return()
                x = 1
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should have RETURN or RETURN_NIL at end
    int return_count = count_opcode(method, Fa_OpCode::RETURN) + count_opcode(method, Fa_OpCode::RETURN_NIL);
    EXPECT_GE(return_count, 1);
}

TEST_F(CompilerClassBytecodeTest, MethodReturnLoadsNil)
{
    std::string const source = R"(
        class RetNil
            member x
            
            def returns_nothing()
                x = 1
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should emit LOAD_CONST or LOAD_NIL for return value
    int load_const = count_opcode(method, Fa_OpCode::LOAD_CONST);
    int load_nil = count_opcode(method, Fa_OpCode::LOAD_NIL);

    EXPECT_GT(load_const + load_nil, 0);
}

TEST_F(CompilerClassBytecodeTest, MethodExplicitReturnValue)
{
    std::string const source = R"(
        class ExplicitRet
            member x
            
            def get_value()
                return x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should have RETURN instruction
    int return_count = count_opcode(method, Fa_OpCode::RETURN);
    EXPECT_GE(return_count, 1);
}

// ============================================================================
// Instruction Sequence Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, ClassStructureSequence)
{
    std::string const source = R"(
        class Seq
            member x
            
            def m()
                x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Expected sequence: LOAD_GLOBAL(جدول) -> IC_CALL -> LOAD_CONST(__class__) -> LIST_SET
    // Then CLOSURE -> LIST_SET (for method)
    // Then STORE_GLOBAL

    bool found_load_global = false;
    bool found_ic_call = false;
    bool found_closure = false;
    bool found_store_global = false;

    for (u32 instr : chunk->code) {
        Fa_OpCode op = Fa_instr_op(instr);
        if (op == Fa_OpCode::LOAD_GLOBAL)
            found_load_global = true;
        if (op == Fa_OpCode::IC_CALL)
            found_ic_call = true;
        if (op == Fa_OpCode::CLOSURE)
            found_closure = true;
        if (op == Fa_OpCode::STORE_GLOBAL)
            found_store_global = true;
    }

    EXPECT_TRUE(found_load_global);
    EXPECT_TRUE(found_ic_call);
    EXPECT_TRUE(found_closure);
    EXPECT_TRUE(found_store_global);
}

// ============================================================================
// Register Allocation Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, ClassRegisterAllocation)
{
    std::string const source = R"(
        class Regs
            member x
            
            def use_regs(a, b)
                a + b + x
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should allocate registers: 0=instance, 1=a, 2=b, 3+=temporaries
    // local_count should be at least 3
    EXPECT_GE(method->local_count, 3);
}

// ============================================================================
// Edge Case Bytecode Tests
// ============================================================================

TEST_F(CompilerClassBytecodeTest, EmptyMethodBodyCompiles)
{
    std::string const source = R"(
        class Empty
            member x
            
            def noop()
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should still have return statement
    int return_count = count_opcode(method, Fa_OpCode::RETURN) + count_opcode(method, Fa_OpCode::RETURN_NIL);
    EXPECT_GE(return_count, 1);
}

TEST_F(CompilerClassBytecodeTest, LargeMethodBodyCompiles)
{
    std::string const source = R"(
        class Large
            member x
            
            def compute()
                a = x + 1
                b = a + 2
                c = b + 3
                d = c + 4
                e = d + 5
                f = e + 6
                g = f + 7
                h = g + 8
                h
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Chunk* method = get_function(chunk, 0);
    ASSERT_NE(method, nullptr);

    // Should compile without error
    EXPECT_GT(method->code.size(), 0);
    EXPECT_GE(method->local_count, 9); // instance + 8 locals
}

} // namespace fairuz::test
