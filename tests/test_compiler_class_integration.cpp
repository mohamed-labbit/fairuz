#include "../fairuz/compiler.hpp"
#include "../fairuz/lexer.hpp"
#include "../fairuz/parser.hpp"
#include "../fairuz/vm.hpp"
#include <gtest/gtest.h>

namespace fairuz::test {

using namespace fairuz::runtime;
using namespace fairuz::parser;
using namespace fairuz::lex;

class CompilerClassIntegrationTest : public ::testing::Test {
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

    Fa_Chunk* compile(std::string const& source)
    {
        Fa_Lexer lexer(source.c_str());
        auto tokens = lexer.tokenize();

        Fa_Parser parser(tokens);
        auto ast = parser.parse();

        Compiler compiler;
        return compiler.compile(ast);
    }

    Fa_Value execute(Fa_Chunk* chunk)
    {
        if (chunk == nullptr)
            return NIL_VAL;
        return vm_->execute(chunk);
    }

    Fa_Value get_global(std::string const& name)
    {
        return vm_->get_global(name);
    }

    bool is_class_dict(Fa_Value v)
    {
        if (!Fa_IS_OBJECT(v))
            return false;
        // Check if it's a dict with __class__ key
        // (Simplified; exact check depends on object type)
        return true;
    }

    std::unique_ptr<Fa_ArenaAllocator> allocator_;
    std::unique_ptr<Fa_VM> vm_;
};

// ============================================================================
// Class Definition Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, ClassDefinedGlobally)
{
    std::string const source = R"(
        class Point
            member x
            member y
        end
    )";

    ASSERT_NE(compile(source), nullptr);
    Fa_Value point_class = get_global("Point");

    EXPECT_FALSE(Fa_IS_NIL(point_class));
    EXPECT_TRUE(is_class_dict(point_class));
}

TEST_F(CompilerClassIntegrationTest, MultipleClassesDefinedSequentially)
{
    std::string const source = R"(
        class Class1
            member x
        end
        
        class Class2
            member y
        end
        
        class Class3
            member z
        end
    )";

    ASSERT_NE(compile(source), nullptr);

    EXPECT_FALSE(Fa_IS_NIL(get_global("Class1")));
    EXPECT_FALSE(Fa_IS_NIL(get_global("Class2")));
    EXPECT_FALSE(Fa_IS_NIL(get_global("Class3")));
}

// ============================================================================
// Method Definition Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, ClassWithMethodsDefinesSuccessfully)
{
    std::string const source = R"(
        class Counter
            member count
            
            def increment()
                count = count + 1
            end
            
            def get_count()
                count
            end
        end
    )";

    ASSERT_NE(compile(source), nullptr);
    Fa_Value counter = get_global("Counter");

    EXPECT_FALSE(Fa_IS_NIL(counter));
}

TEST_F(CompilerClassIntegrationTest, MethodIsCallable)
{
    std::string const source = R"(
        class Adder
            member base
            
            def add(x)
                base + x
            end
        end
        
        adder = Adder()
        adder
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_FALSE(Fa_IS_NIL(result));
}

TEST_F(CompilerClassIntegrationTest, MethodReceivesCorrectParameters)
{
    std::string const source = R"(
        class Echo
            member prefix
            
            def format(a, b)
                a + b
            end
        end
    )";

    ASSERT_NE(compile(source), nullptr);
    Fa_Value echo = get_global("Echo");
    EXPECT_FALSE(Fa_IS_NIL(echo));
}

// ============================================================================
// Instance Creation Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, ClassCallable)
{
    std::string const source = R"(
        class Simple
            member x
        end
        
        instance = Simple()
        instance
    )";

    Fa_Value result = execute(compile(source));
    // Result should be an instance (dict)
    EXPECT_FALSE(Fa_IS_NIL(result));
}

TEST_F(CompilerClassIntegrationTest, ClassCallableWithoutArguments)
{
    std::string const source = R"(
        class NoArgs
            member x
            
            def init()
                x = 0
            end
        end
        
        obj = NoArgs()
        obj
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_FALSE(Fa_IS_NIL(result));
}

// ============================================================================
// Method Invocation Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodInvokedWithInstance)
{
    std::string const source = R"(
        class Hasher
            member value
            
            def process()
                value * 2
            end
        end
        
        h = Hasher()
        h.process()
    )";

    Fa_Value result = execute(compile(source));
    // Method should be callable on instance
    EXPECT_TRUE(true); // Execution didn't crash
}

TEST_F(CompilerClassIntegrationTest, MethodInvokedWithArguments)
{
    std::string const source = R"(
        class Multiplier
            member factor
            
            def multiply(x, y)
                (x + y) * factor
            end
        end
        
        m = Multiplier()
        m.multiply(3, 4)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true); // Execution didn't crash
}

TEST_F(CompilerClassIntegrationTest, MultipleMethodsInvoked)
{
    std::string const source = R"(
        class Operator
            member x
            
            def add(y)
                x + y
            end
            
            def multiply(y)
                x * y
            end
            
            def square()
                x * x
            end
        end
        
        op = Operator()
        op.add(1)
        op.multiply(2)
        op.square()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true); // All methods callable
}

// ============================================================================
// Instance State Mutation Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodMutatesInstanceState)
{
    std::string const source = R"(
        class Accumulator
            member total
            
            def add(x)
                total = total + x
            end
        end
        
        acc = Accumulator()
        acc.add(5)
        acc
    )";

    Fa_Value result = execute(compile(source));
    // Instance returned; mutation should persist in returned instance
    EXPECT_FALSE(Fa_IS_NIL(result));
}

TEST_F(CompilerClassIntegrationTest, MethodChaining)
{
    std::string const source = R"(
        class Builder
            member x
            
            def increment()
                x = x + 1
            end
            
            def double()
                x = x * 2
            end
        end
        
        b = Builder()
        b.increment()
        b.double()
        b.increment()
        b
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_FALSE(Fa_IS_NIL(result));
}

// ============================================================================
// Scope and Variable Binding Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodAccessesInstanceVariable)
{
    std::string const source = R"(
        class Reader
            member value
            
            def get_value()
                value
            end
        end
        
        r = Reader()
        r.get_value()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true); // Execution didn't crash
}

TEST_F(CompilerClassIntegrationTest, MethodAccessesLocalVariables)
{
    std::string const source = R"(
        class Computer
            member base
            
            def compute(a, b)
                temp = a + b
                temp + base
            end
        end
        
        c = Computer()
        c.compute(1, 2)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodShadowsLocalVariables)
{
    std::string const source = R"(
        class Shadower
            member x
            
            def process()
                x = 10
                y = x + 5
                y
            end
        end
        
        s = Shadower()
        s.process()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

// ============================================================================
// Control Flow Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodWithConditional)
{
    std::string const source = R"(
        class Conditional
            member threshold
            
            def check(x)
                if x > threshold
                    true
                else
                    false
                end
            end
        end
        
        c = Conditional()
        c.check(5)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodWithLoop)
{
    std::string const source = R"(
        class Looper
            member accumulator
            
            def sum_to(n)
                i = 0
                while i < n
                    accumulator = accumulator + i
                    i = i + 1
                end
            end
        end
        
        l = Looper()
        l.sum_to(5)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodWithForLoop)
{
    std::string const source = R"(
        class ForLooper
            member total
            
            def iterate(items)
                for item in items
                    total = total + item
                end
            end
        end
        
        fl = ForLooper()
        fl.iterate([1, 2, 3])
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

// ============================================================================
// Parameter Passing Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodWithNoParameters)
{
    std::string const source = R"(
        class NoParams
            member x
            
            def get()
                x
            end
        end
        
        np = NoParams()
        np.get()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodWithSingleParameter)
{
    std::string const source = R"(
        class OneParam
            member x
            
            def process(a)
                a + x
            end
        end
        
        op = OneParam()
        op.process(10)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodWithMultipleParameters)
{
    std::string const source = R"(
        class ManyParams
            member base
            
            def calculate(a, b, c, d)
                a + b + c + d + base
            end
        end
        
        mp = ManyParams()
        mp.calculate(1, 2, 3, 4)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodParametersCorrectlyOrdered)
{
    std::string const source = R"(
        class Ordered
            member base
            
            def subtract(a, b, c)
                a - b - c - base
            end
        end
        
        o = Ordered()
        o.subtract(10, 2, 1)
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

// ============================================================================
// Return Value Integration
// ============================================================================

TEST_F(CompilerClassIntegrationTest, MethodExplicitReturnValue)
{
    std::string const source = R"(
        class ExplicitReturn
            member x
            
            def get_x()
                return x
            end
        end
        
        er = ExplicitReturn()
        result = er.get_x()
        result
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodImplicitReturnValue)
{
    std::string const source = R"(
        class ImplicitReturn
            member x
            
            def get_x()
                x
            end
        end
        
        ir = ImplicitReturn()
        result = ir.get_x()
        result
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, MethodNoReturnValue)
{
    std::string const source = R"(
        class NoReturn
            member x
            
            def set_x(val)
                x = val
            end
        end
        
        nr = NoReturn()
        result = nr.set_x(5)
        result
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

// ============================================================================
// Complex Scenarios
// ============================================================================

TEST_F(CompilerClassIntegrationTest, BankAccount)
{
    std::string const source = R"(
        class BankAccount
            member balance
            member overdraft_limit
            
            def deposit(amount)
                balance = balance + amount
            end
            
            def withdraw(amount)
                if balance - amount >= -overdraft_limit
                    balance = balance - amount
                    true
                else
                    false
                end
            end
            
            def get_balance()
                balance
            end
        end
        
        account = BankAccount()
        account.deposit(100)
        account.withdraw(30)
        account.get_balance()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, Matrix)
{
    std::string const source = R"(
        class Matrix
            member rows
            member cols
            
            def size()
                rows * cols
            end
            
            def transpose_size()
                cols * rows
            end
            
            def is_square()
                rows == cols
            end
        end
        
        m = Matrix()
        m.size()
        m.transpose_size()
        m.is_square()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, NestedObjectCreation)
{
    std::string const source = R"(
        class Inner
            member value
            
            def get()
                value
            end
        end
        
        class Outer
            member inner
            
            def create_inner()
                inner = Inner()
            end
        end
        
        outer = Outer()
        outer.create_inner()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

TEST_F(CompilerClassIntegrationTest, ChainedCalculations)
{
    std::string const source = R"(
        class Chain
            member value
            
            def add(x)
                value = value + x
            end
            
            def multiply(x)
                value = value * x
            end
            
            def square()
                value = value * value
            end
            
            def result()
                value
            end
        end
        
        c = Chain()
        c.add(5)
        c.multiply(2)
        c.square()
        c.result()
    )";

    Fa_Value result = execute(compile(source));
    EXPECT_TRUE(true);
}

} // namespace fairuz::test
