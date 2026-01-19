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
    ss << "    \"message\": \"" << utf8::utf16to8(diag.message) << "\",\n";
    ss << "    \"code\": \"" << utf8::utf16to8(diag.code) << "\"\n";
    ss << "  }";
    if (i + 1 < Diagnostics_.size())
      ss << ",";
    ss << "\n";
  }
  ss << "]\n";
  return ss.str();
}

void DiagnosticEngine::prettyPrint() const
{
  for (const Diagnostic& diag : Diagnostics_)
  {
    StringType sevStr;
    StringType color;

    switch (diag.severity)
    {
    case Severity::NOTE :
      sevStr = u"note";
      color  = Color::CYAN;
      break;
    case Severity::WARNING :
      sevStr = u"warning";
      color  = Color::MAGENTA;
      break;
    case Severity::ERROR :
      sevStr = u"error";
      color  = Color::RED;
      break;
    case Severity::FATAL :
      sevStr = u"fatal";
      color  = Color::RED;
      break;
    }

    std::cout << utf8::utf16to8(color) << utf8::utf16to8(sevStr) << utf8::utf16to8(Color::RESET);
    if (!diag.code.empty())
      std::cout << "[" << utf8::utf16to8(diag.code) << "]";
    std::cout << ": " << utf8::utf16to8(diag.message) << "\n";
    // Show source line
    std::cout << "  --> line " << diag.line << ":" << diag.column << "\n";
    // Extract and show the problematic line
    std::vector<std::string> lines = splitLines(utf8::utf16to8(SourceCode_));
    if (diag.line > 0 && diag.line <= lines.size())
    {
      std::cout << "   |\n";
      std::cout << std::setw(3) << diag.line << " | " << lines[diag.line - 1] << "\n";
      std::cout << "   | " << std::string(diag.column - 1, ' ') << utf8::utf16to8(color) << std::string(std::max(1, diag.length), '^')
                << utf8::utf16to8(Color::RESET) << '\n';
    }
    // Show suggestions
    if (!diag.suggestions.empty())
    {
      std::cout << utf8::utf16to8(Color::MAGENTA) << "Help" << utf8::utf16to8(Color::RESET) << std::endl;
      for (const StringType& sugg : diag.suggestions)
        std::cout << "    • " << utf8::utf16to8(sugg) << "\n";
    }
    // Show notes
    for (const auto& [noteLine, noteMsg] : diag.notes)
    {
      std::cout << utf8::utf16to8(Color::CYAN) << "note:" << utf8::utf16to8(Color::RESET) << std::endl;
      std::cout << "  --> line " << noteLine << "\n";
    }
    std::cout << "\n";
  }
}

}
}