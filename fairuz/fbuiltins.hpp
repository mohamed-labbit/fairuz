#ifndef FA_BUILTINS_HPP
#define FA_BUILTINS_HPP

#include "fobject.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace fairuz::runtime {

struct BuiltinDefinition {
    std::string_view name;
    NativeFn fn;
    int arity;
};

// The registry is immutable and built at compile time. Each VM lazily creates
// its own GC-managed callable values when a builtin is first looked up.
struct BuiltinsList {
    explicit BuiltinsList(VM& vm);

    static std::span<BuiltinDefinition const> definitions();
    Value const* find(StringRef const& name) const;
    std::span<Value const> values() const { return m_values; }

private:
    VM& m_vm;
    mutable std::vector<Value> m_values;
};

} // namespace fairuz::runtime

#endif // FA_BUILTINS_HPP
