#include "../../include/parser/parallel.hpp"
#include "../../include/parser/parser.hpp"

#include <future>


namespace mylang {
namespace parser {

/*
std::vector<ast::Stmt*> ParallelParser::parseParallel(const std::vector<mylang::tok::Token>& tokens, std::int32_t threadCount)
{
  // Split tokens by top-level definitions
  std::vector<std::vector<mylang::tok::Token>> chunks = splitIntoChunks(tokens);
  std::vector<std::future<ast::Stmt*>> futures;
  for (auto& chunk : chunks)
  {
    futures.push_back(std::async(std::launch::async, [chunk]() {
      mylang::parser::Parser parser(chunk);
      auto stmts = parser.parse();
      return stmts.empty() ? nullptr : std::move(stmts[0]);
    }));
  }
  std::vector<ast::Stmt*> result;
  for (auto& future : futures)
  if (auto stmt = future.get()) result.push_back(std::move(stmt));
  return result;
}
*/

std::vector<std::vector<tok::Token>> ParallelParser::splitIntoChunks(const std::vector<tok::Token>& tokens)
{
  std::vector<std::vector<tok::Token>> chunks;
  std::vector<tok::Token>              current;
  for (const tok::Token& tok : tokens)
  {
    current.push_back(tok);
    if (tok.type() == tok::TokenType::KW_FN && current.size() > 1)
    {
      chunks.push_back(current);
      current.clear();
      current.push_back(tok);
    }
  }
  if (!current.empty())
    chunks.push_back(current);
  return chunks;
}

}
}