#include "../../include/parser/language_server.hpp"


namespace mylang {
namespace parser {

LanguageServer::Hover LanguageServer::getHover(const string_type& source, LanguageServer::Position pos)
{
  Hover hover;
  hover.contents = u"Variable: x\nType: int";
  return hover;
}

LanguageServer::Position LanguageServer::getDefinition(const string_type& source, Position pos) { return {0, 0}; }

std::vector<LanguageServer::Range> LanguageServer::getReferences(const string_type& source, Position pos) { return {}; }

std::unordered_map<string_type, string_type> LanguageServer::rename(const string_type& source, Position pos, const string_type& newName)
{
  return {};
}

/*
std::vector<LanguageServer::CompletionItem> LanguageServer::getCompletions(const string_type& source, Position pos)
{
    std::vector<CompletionItem> items;
    // Parse and get symbol table
    mylang::parser::Parser parser(source);
    auto ast = parser.parse();
    // Analyze and extract symbols
    // Add keywords
    for (const auto& kw : {u"اذا", u"طالما", u"لكل", u"عرف", u"return"})
    {
        CompletionItem item;
        item.label = kw;
        item.kind = 14;  // Keyword
        items.push_back(item);
    }
    return items;
}
*/

}
}