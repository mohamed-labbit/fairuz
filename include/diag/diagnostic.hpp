#pragma once

#include "../../utfcpp/source/utf8.h"
#include "../macros.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>


namespace mylang {
namespace diagnostic {

class DiagnosticEngine
{
 public:
  enum class Severity : int { NOTE, FATAL, ERROR, WARNING };

  struct Diagnostic
  {
    Severity severity;
    std::int32_t line, column;
    std::int32_t length;
    StringType message;
    StringType code;  // Error code like E0001
    std::vector<StringType> suggestions;
    std::vector<std::pair<std::int32_t, StringType>> notes;  // Additional context
  };

  DiagnosticEngine() = default;

  void emit(const StringType& msg, Severity sv = Severity::ERROR) { emitError(utf8::utf16to8(msg), sv); }
  void emit(const std::string& msg, Severity sv = Severity::ERROR) { emitError(msg, sv); }

  [[noreturn]] void panic(const StringType& msg) { _panic(utf8::utf16to8(msg)); }
  [[noreturn]] void panic(const std::string& msg) { _panic(msg); }

  void setSource(const StringType& source) { sourceCode_ = source; }

  void report(Severity sev, std::int32_t line, std::int32_t col, std::int32_t len, const StringType& msg, const StringType& code = u"")
  {
    Diagnostic diag;
    diag.severity = sev;
    diag.line = line;
    diag.column = col;
    diag.length = len;
    diag.message = msg;
    diag.code = code;
    diagnostics_.push_back(diag);
  }

  void addSuggestion(const StringType& suggestion)
  {
    if (!diagnostics_.empty()) diagnostics_.back().suggestions.push_back(suggestion);
  }

  void addNote(std::int32_t line, const StringType& note)
  {
    if (!diagnostics_.empty()) diagnostics_.back().notes.push_back({line, note});
  }

  std::string toJSON() const
  {
    std::stringstream ss;
    ss << "[\n";
    for (std::size_t i = 0; i < diagnostics_.size(); i++)
    {
      const Diagnostic& diag = diagnostics_[i];
      ss << "  {\n";
      ss << "    \"severity\": " << static_cast<std::int32_t>(diag.severity) << ",\n";
      ss << "    \"line\": " << diag.line << ",\n";
      ss << "    \"column\": " << diag.column << ",\n";
      ss << "    \"message\": \"" << utf8::utf16to8(diag.message) << "\",\n";
      ss << "    \"code\": \"" << utf8::utf16to8(diag.code) << "\"\n";
      ss << "  }";
      if (i + 1 < diagnostics_.size()) ss << ",";
      ss << "\n";
    }
    ss << "]\n";
    return ss.str();
  }

  // Beautiful terminal output with colors
  void prettyPrint() const
  {
    for (const Diagnostic& diag : diagnostics_)
    {
      StringType sevStr;
      StringType color;

      switch (diag.severity)
      {
      case Severity::NOTE :
        sevStr = u"note";
        color = Color::CYAN;
        break;
      case Severity::WARNING :
        sevStr = u"warning";
        color = Color::MAGENTA;
        break;
      case Severity::ERROR :
        sevStr = u"error";
        color = Color::RED;
        break;
      case Severity::FATAL :
        sevStr = u"fatal";
        color = Color::RED;
        break;
      }

      std::cout << utf8::utf16to8(color) << utf8::utf16to8(sevStr) << utf8::utf16to8(Color::RESET);
      if (!diag.code.empty()) std::cout << "[" << utf8::utf16to8(diag.code) << "]";
      std::cout << ": " << utf8::utf16to8(diag.message) << "\n";
      // Show source line
      std::cout << "  --> line " << diag.line << ":" << diag.column << "\n";
      // Extract and show the problematic line
      std::vector<std::string> lines = splitLines(utf8::utf16to8(sourceCode_));
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
        for (const StringType& sugg : diag.suggestions) std::cout << "    • " << utf8::utf16to8(sugg) << "\n";
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

 private:
  std::vector<Diagnostic> diagnostics_;
  StringType sourceCode_;

  void emitError(const std::string& msg, Severity sv)
  {
    std::cerr << svToStr(sv) << ": " << msg << std::endl;
    if (sv == Severity::FATAL) throw std::runtime_error("");
  }

  [[noreturn]] void _panic(const std::string& msg) const
  {
    std::cerr << "fatal: Program panic: " << msg << std::endl;
    std::terminate();  // Or throw std::runtime_error(msg);
  }

  static std::string svToStr(const Severity sv)
  {
    switch (sv)
    {
    case Severity::NOTE : return utf8::utf16to8(Color::BOLD) + utf8::utf16to8(Color::CYAN) + "note" + utf8::utf16to8(Color::RESET);
    case Severity::FATAL : return utf8::utf16to8(Color::BOLD) + utf8::utf16to8(Color::RED) + "fatal" + utf8::utf16to8(Color::RESET);
    case Severity::ERROR : return utf8::utf16to8(Color::BOLD) + utf8::utf16to8(Color::RED) + "error" + utf8::utf16to8(Color::RESET);
    case Severity::WARNING : return utf8::utf16to8(Color::BOLD) + utf8::utf16to8(Color::YELLOW) + "warning" + utf8::utf16to8(Color::RESET);
    default : return utf8::utf16to8(Color::BOLD) + "unknown" + utf8::utf16to8(Color::RESET);
    }
  }

  std::vector<std::string> splitLines(const std::string& text) const
  {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
  }
};

inline DiagnosticEngine engine;

}
}