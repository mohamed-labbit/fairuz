#pragma once

#include "../../utfcpp/source/utf8.h"
#include "macros.hpp"
#include <iostream>


namespace mylang {
namespace diagnostics {

class Diagnostic
{
 public:
  enum class Severity : int {
    FATAL,
    ERROR,
    WARNING,
  };

  Diagnostic() = default;

  void emit(const string_type& msg, std::optional<Severity> sv = std::nullopt) { emit_error(msg, sv.value_or(Severity::ERROR)); }
  void emit(const std::string& msg, std::optional<Severity> sv = std::nullopt) { emit_error(utf8::utf8to16(msg), sv.value_or(Severity::ERROR)); }

 private:
  string_type msg_;

  void emit_error(const string_type& msg, Severity sv)
  {
    std::cerr << svToStr(sv) << utf8::utf16to8(msg) << std::endl;
    msg_ = msg;
  }

  static std::string svToStr(const Severity sv)
  {
    return sv == Severity::FATAL ? "fatal" : sv == Severity::ERROR ? "error" : sv == Severity::WARNING ? "warning" : "";
  }
};

extern Diagnostic diag_engine;

}
}
