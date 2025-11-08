#pragma once

#include "diagnostic.h"
#include <string>
#include <vector>


namespace diagnostics {

/*
fn emit(d: Diagnostic);
fn emit_simple(file, start_offset, end_offset, severity, code, msg);
fn collect_for_file(file: FileId) -> Vec<Diagnostic>;
fn to_json() -> String;
*/

class DiagnosticsEngine
{
   private:
   public:
    void emit(Diagnostic&);
    void emit_simple(
      std::string, std::size_t, std::size_t, /*, mylang::lex::tok::Token*/ std::string);
    std::vector<Diagnostic> collect_for_file(std::string);
};
}