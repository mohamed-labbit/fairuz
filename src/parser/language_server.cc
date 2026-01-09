#include "../../include/parser/language_server.hpp"


namespace mylang {
namespace parser {

LanguageServer::Hover LanguageServer::getHover(const StringType& source, LanguageServer::Position pos)
{
  Hover hover;
  hover.contents = u"Variable: x\nType: int";
  return hover;
}

LanguageServer::Position LanguageServer::getDefinition(const StringType& source, Position pos) { return {0, 0}; }

std::vector<LanguageServer::Range> LanguageServer::getReferences(const StringType& source, Position pos) { return {}; }

std::unordered_map<StringType, StringType> LanguageServer::rename(const StringType& source, Position pos, const StringType& newName) { return {}; }

}
}