#pragma once

#include "../../utfcpp/source/utf8.h"
#include "../macros.hpp"

#include <exception>
#include <vector>


namespace mylang {
namespace diagnostic {

class DiagnosticEngine
{
 public:
  enum class Severity : int { NOTE, FATAL, ERROR, WARNING };

  struct Diagnostic
  {
    Severity                                         severity;
    std::int32_t                                     line, column;
    std::int32_t                                     length;
    StringType                                       message;
    StringType                                       code;  // Error code like E0001
    std::vector<StringType>                          suggestions;
    std::vector<std::pair<std::int32_t, StringType>> notes;  // Additional context
  };

  DiagnosticEngine() = default;

  void emit(const StringType& msg, Severity sv = Severity::ERROR) { emitError(utf8::utf16to8(msg), sv); }
  void emit(const std::string& msg, Severity sv = Severity::ERROR) { emitError(msg, sv); }

  [[noreturn]] void panic(const StringType& msg) { _panic(utf8::utf16to8(msg)); }
  [[noreturn]] void panic(const std::string& msg) { _panic(msg); }

  void setSource(const StringType& source) { SourceCode_ = source; }

  void report(Severity sev, std::int32_t line, std::int32_t col, std::int32_t len, const StringType& msg, const StringType& code = u"")
  {
    Diagnostics_.push_back({sev, line, col, len, msg, code});
  }

  void addSuggestion(const StringType& suggestion)
  {
    if (!Diagnostics_.empty()) Diagnostics_.back().suggestions.push_back(suggestion);
  }

  void addNote(std::int32_t line, const StringType& note)
  {
    if (!Diagnostics_.empty()) Diagnostics_.back().notes.push_back({line, note});
  }

  std::string toJSON() const;

  // Beautiful terminal output with colors
  void prettyPrint() const;

 private:
  std::vector<Diagnostic> Diagnostics_;
  StringType              SourceCode_;

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
    std::stringstream        ss(text);
    std::string              line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
  }
};

inline DiagnosticEngine engine;

}
}