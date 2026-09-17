#include "fformatter.hpp"

#include "flexer.hpp"
#include "fparser.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fairuz {
namespace {

using Kind = tok::TokenType;

bool is_open(Kind kind)
{
    return kind == Kind::LPAREN || kind == Kind::LBRACKET || kind == Kind::LBRACE;
}

bool is_close(Kind kind)
{
    return kind == Kind::RPAREN || kind == Kind::RBRACKET || kind == Kind::RBRACE;
}

bool ends_expression(Kind kind)
{
    return is_close(kind) || kind == Kind::IDENTIFIER || kind == Kind::NAME
        || kind == Kind::STRING || kind == Kind::INTEGER || kind == Kind::DECIMAL
        || kind == Kind::HEX || kind == Kind::OCTAL || kind == Kind::BINARY
        || kind == Kind::KW_THIS || kind == Kind::KW_TRUE || kind == Kind::KW_FALSE
        || kind == Kind::KW_NIL;
}

bool is_symbolic_unary(Kind kind)
{
    return kind == Kind::OP_PLUS || kind == Kind::OP_MINUS || kind == Kind::OP_BITNOT;
}

bool needs_space(Kind previous, Kind current, bool previous_unary)
{
    if (current == Kind::COMMA || current == Kind::COLON || current == Kind::DOT || is_close(current))
        return false;
    if (previous == Kind::DOT || is_open(previous))
        return false;
    if ((current == Kind::LPAREN || current == Kind::LBRACKET) && ends_expression(previous))
        return false;
    if (previous_unary && !is_symbolic_unary(current))
        return false;
    return true;
}

size_t indentation(std::string_view line)
{
    size_t width = 0;
    for (char ch : line) {
        if (ch == ' ')
            ++width;
        else if (ch == '\t')
            width += 4 - width % 4;
        else if (ch == '\f')
            width = 0;
        else
            break;
    }
    return width;
}

struct Line {
    size_t start;
    std::string_view text;
    std::vector<TokenPtr> tokens;
    size_t depth { 0 };
};

// String lexer values are decoded. Preserve their source spelling, including
// quote style and escapes, instead of inventing new literals from AST values.
size_t token_end(TokenPtr token, std::string_view source)
{
    size_t start = token->location().offset;
    if (token->type() != Kind::STRING)
        return start + token->lexeme().len();
    char quote = source.at(start);
    for (size_t i = start + 1; i < source.size(); ++i) {
        if (source[i] == '\\')
            ++i;
        else if (source[i] == quote)
            return i + 1;
    }
    throw std::runtime_error("Cannot format an unterminated string");
}

Array<TokenPtr> tokenize(lex::FileManager& file)
{
    lex::Lexer lexer(&file);
    return lexer.tokenize();
}

// Retain logical statement boundaries and indentation, but ignore blank lines
// and the optional final newline. Comma spellings are equivalent in Fairuz.
std::vector<TokenPtr> significant(Array<TokenPtr> const& tokens)
{
    std::vector<TokenPtr> result;
    bool has_code = false;
    for (auto token : tokens) {
        Kind kind = token->type();
        if (kind == Kind::BEGINMARKER || kind == Kind::ENDMARKER)
            continue;
        if (kind == Kind::NEWLINE) {
            if (has_code)
                result.push_back(token);
            has_code = false;
        } else if (kind == Kind::DEDENT || kind == Kind::INDENT) {
            // A final newline can occur before EOF's implicit dedents.
            if (!result.empty() && result.back()->type() == Kind::NEWLINE)
                result.pop_back();
            result.push_back(token);
            has_code = false;
        } else {
            result.push_back(token);
            has_code = true;
        }
    }
    if (!result.empty() && result.back()->type() == Kind::NEWLINE)
        result.pop_back();
    return result;
}

} // namespace

StringRef Formatter::format(StringRef const& source)
{
    lex::FileManager input;
    input.buffer() = source;
    auto tokens = tokenize(input);
    if (diagnostic::has_errors())
        throw std::runtime_error("Cannot format invalid source");

    std::string_view text(source.empty() ? "" : source.data(), source.len());
    std::vector<Line> lines;
    for (size_t start = 0; start < text.size();) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        size_t content_end = end;
        if (content_end > start && text[content_end - 1] == '\r')
            --content_end;
        lines.push_back({ start, text.substr(start, content_end - start), { }, 0 });
        start = end + 1;
    }

    size_t depth = 0;
    for (auto token : tokens) {
        Kind kind = token->type();
        if (kind == Kind::INDENT) {
            ++depth;
            continue;
        }
        if (kind == Kind::DEDENT) {
            if (depth)
                --depth;
            continue;
        }
        if (kind == Kind::BEGINMARKER || kind == Kind::ENDMARKER || kind == Kind::NEWLINE)
            continue;
        size_t line = token->line() - 1;
        if (line >= lines.size())
            throw std::runtime_error("Invalid formatter source location");
        lines[line].tokens.push_back(token);
        lines[line].depth = depth;
    }

    struct Bracket {
        size_t indent;
    };
    std::vector<Bracket> brackets;
    std::vector<size_t> source_indents { 0 };
    std::string output;
    std::string_view eol = text.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
    Kind previous = Kind::NEWLINE;
    bool previous_unary = false;

    for (auto const& line : lines) {
        size_t indent = line.depth * 4;
        if (!brackets.empty()) {
            indent = brackets.back().indent + 4;
            size_t closing = 0;
            for (auto token : line.tokens) {
                if (!is_close(token->type()) || closing == brackets.size())
                    break;
                indent = brackets[brackets.size() - 1 - closing++].indent;
            }
        } else if (!line.tokens.empty()) {
            source_indents.resize(line.depth + 1, indentation(line.text));
            source_indents.back() = indentation(line.text);
            previous = Kind::NEWLINE;
            previous_unary = false;
        } else {
            // Comments do not cause lexer INDENT/DEDENT tokens. Locate their
            // indentation in the active source block, including a new body
            // whose first line is a comment.
            size_t width = indentation(line.text);
            size_t level = 0;
            while (level + 1 < source_indents.size() && source_indents[level + 1] <= width)
                ++level;
            if (width > source_indents[level])
                ++level;
            indent = level * 4;
        }

        std::string rendered;
        size_t consumed = line.start;
        for (auto token : line.tokens) {
            Kind kind = token->type();
            bool unary = is_symbolic_unary(kind) && !ends_expression(previous);
            if (!rendered.empty() && needs_space(previous, kind, previous_unary))
                rendered += ' ';
            size_t start = token->location().offset;
            consumed = token_end(token, text);
            if (kind == Kind::COMMA)
                rendered += "،";
            else
                rendered.append(text.substr(start, consumed - start));
            if (is_open(kind))
                brackets.push_back({ indent });
            else if (is_close(kind) && !brackets.empty())
                brackets.pop_back();
            previous = kind;
            previous_unary = unary;
        }

        // Everything after the last token is whitespace or a comment. Hashes
        // inside strings are already consumed with the raw string token.
        size_t comment = line.text.find('#', consumed - line.start);
        if (comment != std::string_view::npos) {
            if (!rendered.empty())
                rendered += "  ";
            rendered.append(line.text.substr(comment));
        }
        while (!rendered.empty() && (rendered.back() == ' ' || rendered.back() == '\t'))
            rendered.pop_back();
        if (!rendered.empty()) {
            output.append(indent, ' ');
            output += rendered;
        }
        output += eol;
    }

    // Exactly one final newline for nonempty documents; empty input stays empty.
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();
    if (!output.empty())
        output += eol;

    lex::FileManager formatted;
    formatted.buffer() = StringRef(output.c_str());
    auto before = significant(tokens);
    auto after = significant(tokenize(formatted));
    if (before.size() != after.size())
        throw std::runtime_error("Formatting changed the token stream; input left unchanged");
    for (size_t i = 0; i < before.size(); ++i) {
        Kind kind = before[i]->type();
        if (kind != after[i]->type()
            || (kind != Kind::COMMA && kind != Kind::NEWLINE && before[i]->lexeme() != after[i]->lexeme()))
            throw std::runtime_error("Formatting changed a token; input left unchanged");
    }
    parser::Parser parser(&formatted);
    (void)parser.parse_program();
    if (diagnostic::has_errors())
        throw std::runtime_error("Formatted output is invalid; input left unchanged");
    return formatted.buffer();
}

} // namespace fairuz
