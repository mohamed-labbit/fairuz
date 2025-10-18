#pragma once

#include "entry.h"
#include "lex/token.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace lex {


class SymbolTable
{
   public:
    using string_type = std::u16string;
    using Entry       = SymbolTableEntry;
    using size_type   = std::size_t;

    // Each scope = a map from lexeme to entry
    using Scope = std::unordered_map<string_type, Entry>;

    SymbolTable()
    {
        // Always start with global scope
        this->scopes_.emplace_back();
    }

    // Enter a new scope (e.g., function, block)
    void enterScope() { this->scopes_.emplace_back(); }

    // Leave current scope
    void leaveScope()
    {
        if (this->scopes_.size() > 1)
        {
            this->scopes_.pop_back();
        }
    }

    // Insert symbol into current scope
    // Returns false if symbol already exists in current scope
    // Returns true if successfully inserted
    bool insert(const string_type& lexeme, SymbolType st)
    {
        auto& current_scope = this->scopes_.back();

        // Check if symbol already exists in current scope
        if (current_scope.find(lexeme) != current_scope.end())
        {
            return false;
        }

        // Insert into current scope
        Entry e(st, static_cast<size_type>(scopeLevel()));
        current_scope[lexeme] = e;
        return true;
    }
    
    // Lookup symbol (searches from innermost → outermost)
    std::optional<Entry> lookup(const string_type& name) const
    {
        for (auto it = this->scopes_.rbegin(); it != this->scopes_.rend(); ++it)
        {
            auto found = it->find(name);
            if (found != it->end())
            {
                return found->second;
            }
        }

        return std::nullopt;  // not found
    }

    // Check if a symbol is visible in the current scope chain
    bool is_in_scope(const string_type& name) const { return lookup(name).has_value(); }

    // Check if a symbol exists at a specific scope level
    bool is_in_scope(const string_type& name, std::optional<size_type> _scope) const
    {
        // Default: check if symbol is visible anywhere in scope chain
        if (_scope == std::nullopt)
        {
            return lookup(name).has_value();
        }

        // Check if symbol exists at specific scope level
        size_type scope_level = _scope.value();
        if (scope_level >= this->scopes_.size())
        {
            return false;
        }

        const auto& scope = this->scopes_[scope_level];
        return scope.find(name) != scope.end();
    }

    // Current nesting depth (0 = global scope)
    int scopeLevel() const { return static_cast<int>(this->scopes_.size()) - 1; }

    // Get all symbols in current scope
    std::vector<Entry> getCurrentScopeSymbols() const
    {
        std::vector<Entry> symbols;
        if (!this->scopes_.empty())
        {
            const auto& current = this->scopes_.back();
            symbols.reserve(current.size());
            for (const auto& pair : current)
            {
                symbols.push_back(pair.second);
            }
        }
        return symbols;
    }

    // Get all visible symbols (from all scopes in the chain)
    // Respects shadowing - inner scope symbols hide outer ones with same name
    std::vector<Entry> getAllVisibleSymbols() const
    {
        std::vector<Entry>              symbols;
        std::unordered_set<string_type> seen;

        // Iterate from innermost to outermost
        for (auto it = this->scopes_.rbegin(); it != this->scopes_.rend(); ++it)
        {
            for (const auto& pair : *it)
            {
                // Only add if not shadowed by inner scope
                if (seen.find(pair.first) == seen.end())
                {
                    symbols.push_back(pair.second);
                    seen.insert(pair.first);
                }
            }
        }

        return symbols;
    }

    // Get all symbols at a specific scope level
    std::vector<Entry> getSymbolsAtLevel(size_type level) const
    {
        std::vector<Entry> symbols;
        if (level < this->scopes_.size())
        {
            const auto& scope = this->scopes_[level];
            symbols.reserve(scope.size());
            for (const auto& pair : scope)
            {
                symbols.push_back(pair.second);
            }
        }
        return symbols;
    }

   private:
    std::vector<Scope> scopes_;  // Stack of scopes (each scope is a map)
};

};  // namespace lex
};  // namespace mylang