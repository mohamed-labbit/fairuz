#include "../../include/diag/diagnostic.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>


namespace mylang {
namespace diagnostic {

std::string DiagnosticEngine::toJSON() const
{
  std::stringstream ss;
  ss << "[\n";
  for (SizeType i = 0; i < Diagnostics_.size(); i++)
  {
    const Diagnostic& diag = Diagnostics_[i];
    ss << "  {\n";
    ss << "    \"severity: " << static_cast<std::int32_t>(diag.severity) << ",\n";
    ss << "    \"line\": " << diag.line << ",\n";
    ss << "    \"column\": " << diag.column << ",\n";
    ss << "    \"message\": \"" << diag.message << "\",\n";
    ss << "    \"code\": \"" << diag.code << "\"\n";
    ss << "  }";
    if (i + 1 < Diagnostics_.size())
    {
      ss << ",";
    }
    ss << "\n";
  }
  ss << "]\n";
  return ss.str();
}

void DiagnosticEngine::prettyPrint() const
{
  for (const Diagnostic& diag : Diagnostics_)
  {
    std::string sevStr;
    std::string color;

    switch (diag.severity)
    {
    case Severity::NOTE :
      sevStr = "note";
      color  = Color::CYAN;
      break;
    case Severity::WARNING :
      sevStr = "warning";
      color  = Color::MAGENTA;
      break;
    case Severity::ERROR :
      sevStr = "error";
      color  = Color::RED;
      break;
    case Severity::FATAL :
      sevStr = "fatal";
      color  = Color::RED;
      break;
    }

    std::cout << color << sevStr << Color::RESET;
    if (!diag.code.empty())
    {
      std::cout << "[" << diag.code << "]";
    }
    std::cout << ": " << diag.message << "\n";
    // Show source line
    std::cout << "  --> line " << diag.line << ":" << diag.column << "\n";
    // Extract and show the problematic line
    /*
    std::vector<std::string> lines = splitLines(SourceCode_);
    if (diag.line > 0 && diag.line <= lines.size())
    {
      std::cout << "   |\n";
      std::cout << std::setw(3) << diag.line << " | " << lines[diag.line - 1] << "\n";
      std::cout << "   | " << std::string(diag.column - 1, ' ') << color << std::string(std::max(1, diag.length), '^')
      << Color::RESET << '\n';
    }
    */
    // Show suggestions
    if (!diag.suggestions.empty())
    {
      std::cout << Color::MAGENTA << "Help" << Color::RESET << std::endl;
      for (const std::string& sugg : diag.suggestions)
      {
        std::cout << "    • " << sugg << "\n";
      }
    }
    // Show notes
    for (const auto& [noteLine, noteMsg] : diag.notes)
    {
      std::cout << Color::CYAN << "note:" << Color::RESET << std::endl;
      std::cout << "  --> line " << noteLine << "\n";
    }
    std::cout << "\n";
  }
}

}
}