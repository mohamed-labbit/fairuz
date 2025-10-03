#pragma once

#include "entry.h"
#include "lex/token.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


class SymbolTable
{
   public:
    using string_type = std::wstring;
    using Entry       = SymbolTableEntry;

    // Each scope = a map from lexeme to entry
    using Scope = std::unordered_map<string_type, Entry>;

    SymbolTable() {
        // Always start with global scope
        this->scopes_.emplace_back();
    }

    // Enter a new scope (e.g., function, block)
    void enterScope() { this->scopes_.emplace_back(); }

    // Leave current scope
    void leaveScope() {
        if (this->scopes_.size() > 1)
        {
            this->scopes_.pop_back();
        }
    }

    // Insert symbol into current scope
    bool insert(const Entry& entry) {
        auto& currentScope = this->scopes_.back();
        auto  result       = currentScope.emplace(entry.lexeme_, entry);
        return result.second;  // false if already existed
    }

    // Lookup symbol (searches from innermost → outermost)
    std::optional<Entry> lookup(const string_type& name) const {
        for (auto it = this->scopes_.rbegin(); it != this->scopes_.rend(); it += 1)
        {
            auto found = it->find(name);
            if (found != it->end())
            {
                return found->second;
            }
        }

        return std::nullopt;  // not found
    }

    // Check if exists in *current* scope only
    bool existsInCurrentScope(const string_type& name) const {
        const auto& currentScope = this->scopes_.back();
        return currentScope.find(name) != currentScope.end();
    }

    // Current nesting depth
    int scopeLevel() const { return static_cast<int>(this->scopes_.size()) - 1; }

   private:
    std::vector<Scope> scopes_;  // stack of scopes
};