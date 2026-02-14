#pragma once

#include "../token.hpp"
#include "entry.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mylang {
namespace lex {

class SymbolTable {
public:
    using Entry = SymbolTableEntry;

    // Each scope = a map from lexeme to entry
    using Scope = std::unordered_map<StringRef, Entry, StringRefHash, StringRefEqual>;

    SymbolTable()
    {
        // Always start with global scope
        this->Scopes_.emplace_back();
    }

    // Enter a new scope (e.g., function, block)
    void enterScope() { this->Scopes_.emplace_back(); }

    // Leave current scope
    void leaveScope()
    {
        if (this->Scopes_.size() > 1)
            this->Scopes_.pop_back();
    }

    // Insert symbol into current scope
    // Returns false if symbol already exists in current scope
    // Returns true if successfully inserted
    bool insert(StringRef const& lexeme, SymbolType st)
    {
        Scope& current_scope = this->Scopes_.back();

        // Check if symbol already exists in current scope
        if (current_scope.find(lexeme) != current_scope.end())
            return false;

        // Insert into current scope
        Entry e(st, static_cast<SizeType>(scopeLevel()));
        current_scope[lexeme] = e;
        return true;
    }

    // Lookup symbol (searches from innermost → outermost)
    std::optional<Entry> lookup(StringRef const& name) const
    {
        for (auto it = this->Scopes_.rbegin(); it != this->Scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second;
        }
        return std::nullopt; // not found
    }

    // Check if a symbol is visible in the current scope chain
    bool isInScope(StringRef const& name) const { return lookup(name).has_value(); }

    // Check if a symbol exists at a specific scope level
    bool isInScope(StringRef const& name, std::optional<SizeType> _scope) const
    {
        // Default: check if symbol is visible anywhere in scope chain
        if (_scope == std::nullopt)
            return lookup(name).has_value();

        // Check if symbol exists at specific scope level
        SizeType scope_level = _scope.value();
        if (scope_level >= this->Scopes_.size())
            return false;

        Scope const& scope = this->Scopes_[scope_level];
        return scope.find(name) != scope.end();
    }

    // Current nesting depth (0 = global scope)
    std::int32_t scopeLevel() const { return static_cast<std::int32_t>(this->Scopes_.size()) - 1; }

    // Get all symbols in current scope
    std::vector<Entry> getCurrentScopeSymbols() const
    {
        std::vector<Entry> symbols;
        if (!this->Scopes_.empty()) {
            Scope const& current = this->Scopes_.back();
            symbols.reserve(current.size());
            std::transform(current.begin(), current.end(), std::back_inserter(symbols), [](auto const& pair) { return pair.second; });
        }
        return symbols;
    }

    // Get all visible symbols (from all scopes in the chain)
    // Respects shadowing - inner scope symbols hide outer ones with same name
    std::vector<Entry> getAllVisibleSymbols() const
    {
        std::vector<Entry> symbols;
        std::unordered_set<StringRef, StringRefHash, StringRefEqual> seen;

        // Iterate from innermost to outermost
        for (auto it = this->Scopes_.rbegin(); it != this->Scopes_.rend(); ++it) {
            for (auto const& pair : *it) { // Only add if not shadowed by inner scope
                if (seen.find(pair.first) == seen.end()) {
                    symbols.push_back(pair.second);
                    seen.insert(pair.first);
                }
            }
        }

        return symbols;
    }

    // Get all symbols at a specific scope level
    std::vector<Entry> getSymbolsAtLevel(SizeType level) const
    {
        std::vector<Entry> symbols;
        if (level < this->Scopes_.size()) {
            Scope const& scope = this->Scopes_[level];
            symbols.reserve(scope.size());
            std::transform(scope.begin(), scope.end(), std::back_inserter(symbols), [](auto const& pair) { return pair.second; });
        }
        return symbols;
    }

private:
    std::vector<Scope> Scopes_; // Stack of scopes (each scope is a map)
}; // class SymbolTable

}; // namespace lex
}; // namespace mylang
