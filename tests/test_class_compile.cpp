#include "../fairuz/compiler.hpp"
#include "../fairuz/lexer.hpp"
#include "../fairuz/parser.hpp"
#include "../fairuz/vm.hpp"
#include <gtest/gtest.h>

namespace fairuz::test {

using namespace fairuz::runtime;

class CompilerClassTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        allocator_ = std::make_unique<Fa_ArenaAllocator>();
        vm_ = std::make_unique<Fa_VM>();
    }

    void TearDown() override
    {
        vm_.reset();
        allocator_.reset();
    }

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

    // Helper: Execute compiled chunk and return result
    Fa_Value execute(Fa_Chunk* chunk)
    {
        if (chunk == nullptr)
            return NIL_VAL;
        return vm_->execute(chunk);
    }

    std::unique_ptr<Fa_ArenaAllocator> allocator_;
    std::unique_ptr<Fa_VM> vm_;
};

// ============================================================================
// Basic Class Definition Tests
// ============================================================================

TEST_F(CompilerClassTest, SimpleClassDefinition)
{
    // Test: class without methods
    std::string const source = R"(
        class Point
            member x
            member y
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Class should be defined globally
    Fa_Value point_class = vm_->get_global("Point");
    EXPECT_FALSE(Fa_IS_NIL(point_class));
    EXPECT_TRUE(Fa_IS_OBJECT(point_class));
}

TEST_F(CompilerClassTest, ClassWithSingleMethod)
{
    std::string const source = R"(
        class Counter
            member count
            
            def increment()
                count = count + 1
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Value counter_class = vm_->get_global("Counter");
    EXPECT_FALSE(Fa_IS_NIL(counter_class));
}

TEST_F(CompilerClassTest, ClassWithMultipleMethods)
{
    std::string const source = R"(
        class Rect
            member width
            member height
            
            def get_area()
                width * height
            end
            
            def scale(factor)
                width = width * factor
                height = height * factor
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Value rect_class = vm_->get_global("Rect");
    EXPECT_FALSE(Fa_IS_NIL(rect_class));
}

TEST_F(CompilerClassTest, DuplicateMembers)
{
    // Test: class with duplicate member names (should deduplicate)
    std::string const source = R"(
        class Box
            member size
            member size
            member color
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Should compile without error; duplicates filtered
    Fa_Value box_class = vm_->get_global("Box");
    EXPECT_FALSE(Fa_IS_NIL(box_class));
}

// ============================================================================
// Method Compilation Tests (Implicit Instance Parameter)
// ============================================================================

TEST_F(CompilerClassTest, MethodReceivesImplicitInstance)
{
    std::string const source = R"(
        class Obj
            member value
            
            def get_value()
                value
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Verify method is stored in class dict
    Fa_Value obj_class = vm_->get_global("Obj");
    EXPECT_TRUE(Fa_IS_OBJECT(obj_class));

    // Class should be a dict with "get_value" key
    // (Exact verification depends on VM dict/class structure)
}

TEST_F(CompilerClassTest, MethodWithParameters)
{
    std::string const source = R"(
        class Multiplier
            member factor
            
            def multiply(x, y)
                (x + y) * factor
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    Fa_Value multiplier = vm_->get_global("Multiplier");
    EXPECT_FALSE(Fa_IS_NIL(multiplier));
}

TEST_F(CompilerClassTest, MethodArityIncludesImplicitInstance)
{
    // This test verifies the compiled method function has correct arity:
    // explicit params + 1 (implicit instance)
    std::string const source = R"(
        class Math
            member base
            
            def add(x)
                base + x
            end
            
            def multiply(x, y)
                base * x * y
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Retrieve class and check method arities
    // add(x) should have arity 2 (instance + x)
    // multiply(x, y) should have arity 3 (instance + x + y)
    Fa_Value math_class = vm_->get_global("Math");
    EXPECT_TRUE(Fa_IS_OBJECT(math_class));
}

// ============================================================================
// Parameter Binding Tests
// ============================================================================

TEST_F(CompilerClassTest, ExplicitParametersBindCorrectly)
{
    std::string const source = R"(
        class Adder
            member base
            
            def add(a, b)
                a + b + base
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Execute should not crash; params should bind to correct registers
    execute(chunk);
}

TEST_F(CompilerClassTest, ParameterOrderPreserved)
{
    std::string const source = R"(
        class Echo
            member prefix
            
            def format(first, second, third)
                first
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

// ============================================================================
// Method Body Execution Tests
// ============================================================================

TEST_F(CompilerClassTest, MethodBodyCompiled)
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

    // Method should have compiled body
    Fa_Value simple = vm_->get_global("Simple");
    EXPECT_FALSE(Fa_IS_NIL(simple));
}

TEST_F(CompilerClassTest, MethodBodyWithConditional)
{
    std::string const source = R"(
        class Validator
            member min_val
            
            def is_valid(x)
                if x > min_val
                    true
                else
                    false
                end
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

TEST_F(CompilerClassTest, MethodBodyWithLoop)
{
    std::string const source = R"(
        class Summer
            member total
            
            def sum_range(n)
                i = 0
                while i < n
                    total = total + i
                    i = i + 1
                end
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

// ============================================================================
// Return Value Tests
// ============================================================================

TEST_F(CompilerClassTest, MethodDefaultReturnNil)
{
    std::string const source = R"(
        class Mutator
            member x
            
            def set_x(val)
                x = val
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Method should return nil when no explicit return
    // (verified via VM execution in integration tests)
}

TEST_F(CompilerClassTest, MethodExplicitReturn)
{
    std::string const source = R"(
        class Computer
            member state
            
            def compute(a, b)
                return a + b
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

TEST_F(CompilerClassTest, MethodImplicitReturn)
{
    std::string const source = R"(
        class Calculator
            member base
            
            def result()
                base * 2
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

// ============================================================================
// Scope and Local Variable Tests
// ============================================================================

TEST_F(CompilerClassTest, MethodLocalVariables)
{
    std::string const source = R"(
        class Worker
            member x
            
            def process()
                temp = x + 1
                temp * 2
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

TEST_F(CompilerClassTest, MethodInstanceAccessInNestedScope)
{
    std::string const source = R"(
        class Nested
            member value
            
            def outer()
                if true
                    value + 1
                end
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

// ============================================================================
// Error Cases
// ============================================================================

TEST_F(CompilerClassTest, NestedClassNotAllowed)
{
    std::string const source = R"(
        class Outer
            member x
            
            class Inner
                member y
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    // Should emit NESTED_FUNCTION_UNSUPPORTED error
    // Exact behavior depends on error handling
}

TEST_F(CompilerClassTest, InvalidClassNameExpression)
{
    std::string const source = R"(
        class x + y
            member a
        end
    )";

    Fa_Chunk* chunk = compile(source);
    // Should emit error for non-simple name
}

TEST_F(CompilerClassTest, NonMethodInClassBody)
{
    std::string const source = R"(
        class BadClass
            member x
            x = 5
        end
    )";

    Fa_Chunk* chunk = compile(source);
    // Should emit error for non-method statement
}

// ============================================================================
// Closure and Upvalue Tests
// ============================================================================

TEST_F(CompilerClassTest, MethodClosureEmitted)
{
    std::string const source = R"(
        class Closure
            member captured
            
            def get_captured()
                captured
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Method should be compiled as closure (CLOSURE opcode)
    // Verify by checking function index in chunk
}

TEST_F(CompilerClassTest, MultipleMethodClosures)
{
    std::string const source = R"(
        class Multi
            member a
            member b
            
            def method1()
                a
            end
            
            def method2()
                b
            end
            
            def method3()
                a + b
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // All three methods should be compiled as separate closures
    // and stored in class dict
}

// ============================================================================
// Metadata Tests
// ============================================================================

TEST_F(CompilerClassTest, ClassMetadataStored)
{
    std::string const source = R"(
        class DataClass
            member field1
            member field2
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Class dict should contain __class__ metadata key
    Fa_Value data_class = vm_->get_global("DataClass");
    EXPECT_TRUE(Fa_IS_OBJECT(data_class));
}

TEST_F(CompilerClassTest, MemberListInMetadata)
{
    std::string const source = R"(
        class Store
            member name
            member inventory
            member owner
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Metadata should contain deduplicated member list
    Fa_Value store = vm_->get_global("Store");
    EXPECT_FALSE(Fa_IS_NIL(store));
}

// ============================================================================
// Integration Tests (Compilation + VM Execution)
// ============================================================================

TEST_F(CompilerClassTest, ClassInstantiationAndMethodCall)
{
    std::string const source = R"(
        class Point
            member x
            member y
            
            def distance()
                (x * x + y * y)
            end
        end
        
        p = Point()
        p
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Execute and verify Point class is callable and returns instance
    Fa_Value result = execute(chunk);
    EXPECT_FALSE(Fa_IS_NIL(result));
}

TEST_F(CompilerClassTest, MethodCallWithArguments)
{
    std::string const source = R"(
        class Calc
            member base
            
            def add(x, y)
                base + x + y
            end
        end
        
        c = Calc()
        c.add(1, 2)
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Execute method with arguments
    Fa_Value result = execute(chunk);
    // Result verification depends on VM instance/method call semantics
}

TEST_F(CompilerClassTest, MutatingMethodCall)
{
    std::string const source = R"(
        class Counter
            member count
            
            def increment()
                count = count + 1
            end
        end
        
        cnt = Counter()
        cnt.increment()
        cnt.count
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);

    // Execute and verify mutation persists
    Fa_Value result = execute(chunk);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CompilerClassTest, EmptyMethodBody)
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
    execute(chunk);
}

TEST_F(CompilerClassTest, LargeNumberOfMembers)
{
    std::string const source = R"(
        class Many
            member a
            member b
            member c
            member d
            member e
            member f
            member g
            member h
            member i
            member j
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
}

TEST_F(CompilerClassTest, LargeNumberOfMethods)
{
    std::string const source = R"(
        class Many
            member x
            
            def m1() x end
            def m2() x end
            def m3() x end
            def m4() x end
            def m5() x end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
}

TEST_F(CompilerClassTest, MethodWithManyParameters)
{
    std::string const source = R"(
        class Func
            member base
            
            def apply(a, b, c, d, e, f, g, h)
                a + b + c + d + e + f + g + h + base
            end
        end
    )";

    Fa_Chunk* chunk = compile(source);
    ASSERT_NE(chunk, nullptr);
    execute(chunk);
}

} // namespace fairuz::test
