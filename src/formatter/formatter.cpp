/**
 * slang - a simple scripting language.
 *
 * source formatter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "formatter/formatter.h"

namespace slang::formatter
{

namespace
{

static bool is_word_token(const slang::token& tok)
{
    return tok.type == token_type::identifier
           || tok.type == token_type::macro_identifier
           || tok.type == token_type::macro_name
           || tok.type == token_type::int_literal
           || tok.type == token_type::fp_literal
           || tok.type == token_type::str_literal;
}

static bool is_tight_after(const std::string& s)
{
    return s == "(" || s == "[" || s == "{" || s == "." || s == "::";
}

static bool is_tight_before(const std::string& s)
{
    return s == ")" || s == "]" || s == "}" || s == "," || s == ";" || s == "." || s == "::" || s == "...";
}

static bool is_keyword_space_before_paren(const slang::token& prev)
{
    static const std::set<std::string> keywords = {
      "if", "while", "return", "fn", "macro"};
    return prev.type == token_type::identifier && keywords.contains(prev.s);
}

static bool is_wrap_operator(const std::string& s)
{
    static const std::set<std::string> ops = {
      "+", "-", "*", "/", "%", "&", "|", "^",
      "&&", "||", "<<", ">>",
      "==", "!=", "<", "<=", ">", ">=",
      "=",
      "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};
    return ops.contains(s);
}

static bool needs_space(const slang::token& prev, const slang::token& curr)
{
    if(is_tight_after(prev.s) || is_tight_before(curr.s))
    {
        return false;
    }

    if(curr.s == "[")
    {
        // Keep attachment for `type[]`, `arr[i]`, and `#[...]`,
        // but preserve spacing in assignments like `= [ ... ]`.
        if(prev.s == "#")
        {
            return false;
        }
        return prev.s == "=";
    }

    if(curr.s == "(")
    {
        if(is_keyword_space_before_paren(prev))
        {
            return true;
        }

        // Function/macro calls and indexing chains.
        if(prev.type == token_type::identifier
           || prev.type == token_type::macro_identifier
           || prev.type == token_type::macro_name
           || prev.s == ")"
           || prev.s == "]")
        {
            return false;
        }

        // Grouping after operators such as `+ (` or `= (`.
        return true;
    }

    if((curr.s == "++" || curr.s == "--")
       && (is_word_token(prev) || prev.s == ")" || prev.s == "]"))
    {
        return false;
    }

    if(is_word_token(prev) && is_word_token(curr))
    {
        return true;
    }

    if(is_word_token(prev) && curr.s == "{")
    {
        return true;
    }

    if((prev.s == ")" || prev.s == "]") && is_word_token(curr))
    {
        return true;
    }

    return true;
}

static std::string read_file(const std::filesystem::path& file)
{
    std::ifstream in{file, std::ios::in | std::ios::binary};
    if(!in)
    {
        throw std::runtime_error("Failed to open file for reading: " + file.string());
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void write_file(const std::filesystem::path& file, const std::string& text)
{
    std::ofstream out{file, std::ios::out | std::ios::binary | std::ios::trunc};
    if(!out)
    {
        throw std::runtime_error("Failed to open file for writing: " + file.string());
    }

    out << text;
}

static std::size_t current_line_length(const std::string& out)
{
    const auto pos = out.find_last_of('\n');
    if(pos == std::string::npos)
    {
        return out.size();
    }

    return out.size() - pos - 1;
}

}    // namespace

source_formatter::source_formatter(options opts)
: opts{opts}
{
}

std::string source_formatter::format_text(const std::string& source) const
{
    if(opts.validate_syntax)
    {
        lexer parser_lexer{source};
        parser p;
        p.parse(parser_lexer);
    }

    lexer l{source};
    std::vector<token> tokens;
    while(auto t = l.next())
    {
        tokens.emplace_back(std::move(*t));
    }

    std::string out;
    std::optional<token> prev;

    std::size_t indent_level = 0;
    bool line_start = true;
    bool in_directive = false;

    auto append_indent = [&]() -> void
    {
        out.append(indent_level * opts.indent_size, ' ');
    };

    auto append_newline = [&]() -> void
    {
        if(!out.empty() && out.back() != '\n')
        {
            out.push_back('\n');
        }
        line_start = true;
    };

    for(std::size_t i = 0; i < tokens.size(); ++i)
    {
        const auto& tok = tokens[i];
        const token* next_tok = (i + 1 < tokens.size()) ? &tokens[i + 1] : nullptr;

        for(const auto& comment: tok.leading_comments)
        {
            if(!line_start)
            {
                append_newline();
            }

            if(line_start)
            {
                append_indent();
                line_start = false;
            }

            out += comment.text;
            append_newline();
        }

        const bool token_starts_line = line_start;

        if(tok.s == "}")
        {
            if(indent_level > 0)
            {
                --indent_level;
            }

            if(!line_start)
            {
                append_newline();
            }
        }

        // Always render directives (`#[...]`) as standalone lines.
        if(tok.s == "#" && next_tok && next_tok->s == "[")
        {
            if(!token_starts_line && !line_start && !out.empty() && out.back() != '\n')
            {
                append_newline();
            }
            in_directive = true;
        }

        if(line_start)
        {
            append_indent();
            line_start = false;
        }

        bool insert_space = !token_starts_line && prev.has_value() && needs_space(*prev, tok);
        const std::size_t projected_len =
          current_line_length(out)
          + (insert_space ? 1 : 0)
          + tok.s.size();

        bool break_after_token = false;
        if(opts.max_line_length > 0
           && !token_starts_line
           && projected_len > opts.max_line_length)
        {
            const bool can_defer_break_to_operator =
              next_tok
              && is_word_token(tok)
              && is_wrap_operator(next_tok->s)
              && tok.trailing_comments.empty();

            const bool can_break_after_operator =
              is_wrap_operator(tok.s)
              && next_tok
              && is_word_token(*next_tok)
              && tok.trailing_comments.empty();

            if(can_break_after_operator)
            {
                break_after_token = true;
            }
            else if(!can_defer_break_to_operator)
            {
                append_newline();
                append_indent();
                line_start = false;
                insert_space = false;
            }
        }

        if(insert_space && !line_start)
        {
            out.push_back(' ');
        }

        out += tok.s;

        for(const auto& comment: tok.trailing_comments)
        {
            if(!line_start)
            {
                out.push_back(' ');
            }

            out += comment.text;

            if(!comment.is_block)
            {
                append_newline();
            }
        }

        if(tok.s == "{")
        {
            ++indent_level;
            append_newline();
        }
        else if(tok.s == ";")
        {
            append_newline();
        }
        else if(in_directive && tok.s == "]")
        {
            append_newline();
            in_directive = false;
        }
        else if(tok.s == "}")
        {
            if(next_tok
               && next_tok->s != ";"
               && next_tok->s != "else")
            {
                append_newline();
            }
        }
        else if(break_after_token)
        {
            append_newline();
        }
        prev = tok;
    }

    // Trim trailing spaces and normalize final newline policy.
    while(!out.empty() && (out.back() == ' ' || out.back() == '\t' || out.back() == '\n' || out.back() == '\r'))
    {
        out.pop_back();
    }

    if(opts.ensure_trailing_newline)
    {
        out.push_back('\n');
    }

    return out;
}

std::string source_formatter::format_file(const std::filesystem::path& file) const
{
    return format_text(read_file(file));
}

bool source_formatter::format_file_in_place(const std::filesystem::path& file) const
{
    const auto original = read_file(file);
    const auto formatted = format_text(original);

    if(original == formatted)
    {
        return false;
    }

    write_file(file, formatted);
    return true;
}

}    // namespace slang::formatter
