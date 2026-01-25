#include "../../include/parser/language_server.hpp"


namespace mylang {
namespace parser {

LanguageServer::Hover LanguageServer::getHover(const StringRef& source, LanguageServer::Position pos)
{
  Hover hover;
  hover.contents = u"Variable: x\nType: int";
  return hover;
}

LanguageServer::Position LanguageServer::getDefinition(const StringRef& source, Position pos) { return {0, 0}; }

std::vector<LanguageServer::Range> LanguageServer::getReferences(const StringRef& source, Position pos) { return {}; }

std::unordered_map<StringRef, StringRef> LanguageServer::rename(const StringRef& source, Position pos, const StringRef& newName) { return {}; }

}
}