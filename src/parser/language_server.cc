#include "../../include/parser/language_server.hpp"

namespace mylang {
namespace parser {

LanguageServer::Hover LanguageServer::getHover(StringRef const& source, LanguageServer::Position pos)
{
    Hover hover;
    hover.contents = "Variable: x\nType: int";
    return hover;
}

LanguageServer::Position LanguageServer::getDefinition(StringRef const& source, Position pos)
{
    return { 0, 0 };
}

std::vector<LanguageServer::Range> LanguageServer::getReferences(StringRef const& source, Position pos)
{
    return { };
}

std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> LanguageServer::rename(
    StringRef const& source, Position pos, StringRef const& newName)
{
    return { };
}

}
}
