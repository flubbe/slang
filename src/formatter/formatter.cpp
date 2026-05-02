/**
 * slang - a simple scripting language.
 *
 * source formatter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <optional>
#include <set>
#include <span>
#include <stdexcept>

#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "filemanager.h"
#include "formatter/formatter.h"

namespace slang::formatter
{

namespace
{

/*
 * token helpers.
 */

static bool is_word_token(
  const slang::token& tok)
{
    return tok.type == token_type::identifier
           || tok.type == token_type::macro_identifier
           || tok.type == token_type::macro_name
           || tok.type == token_type::int_literal
           || tok.type == token_type::fp_literal
           || tok.type == token_type::str_literal;
}

static std::string escape_string_literal(
  const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('"');

    for(char c: s)
    {
        switch(c)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\f': out += "\\f"; break;
        case '\v': out += "\\v"; break;
        default: out.push_back(c); break;
        }
    }

    out.push_back('"');
    return out;
}

/*
 * formatting / rule helpers.
 */

static std::string render_token_text(
  const slang::token& tok)
{
    if(tok.type == token_type::str_literal
       && tok.value.has_value())
    {
        return escape_string_literal(
          std::get<std::string>(*tok.value));
    }

    return tok.s;
}

static bool is_tight_after(
  const std::string& s)
{
    return s == "(" || s == "[" || s == "{" || s == "." || s == "::";
}

static bool is_tight_before(
  const std::string& s)
{
    return s == ")" || s == "]" || s == "}" || s == "," || s == ";" || s == "." || s == "::" || s == "...";
}

static bool is_keyword_space_before_paren(
  const slang::token& prev)
{
    static const std::set<std::string> keywords = {
      "return", "fn", "macro"};
    return prev.type == token_type::identifier && keywords.contains(prev.s);
}

static bool is_wrap_operator(
  const std::string& s)
{
    static const std::set<std::string> ops = {
      "+", "-", "*", "/", "%", "&", "|", "^",
      "&&", "||", "<<", ">>",
      "==", "!=", "<", "<=", ">", ">=",
      "=",
      "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};
    return ops.contains(s);
}

static bool is_unary_prefix_operator(
  const std::string& s)
{
    static const std::set<std::string> ops = {
      "+", "-", "!", "~", "++", "--"};
    return ops.contains(s);
}

static bool starts_expression(
  const std::optional<slang::token>& tok)
{
    if(!tok.has_value())
    {
        return true;
    }

    const auto& s = tok->s;
    if(s == "(" || s == "[" || s == "{" || s == "," || s == ";" || s == ":")
    {
        return true;
    }

    if(is_wrap_operator(s))
    {
        return true;
    }

    return tok->type == token_type::identifier && s == "return";
}

enum class brace_context
{
    other,
    struct_members
};

enum class bracket_context
{
    other,
    array_multiline
};

static bool is_struct_member_block_open(
  const std::optional<slang::token>& prev_prev,
  const std::optional<slang::token>& prev)
{
    if(!prev.has_value()
       || prev->type != token_type::identifier)
    {
        return false;
    }

    if(!prev_prev.has_value())
    {
        return false;
    }

    const auto& s = prev_prev->s;
    return s == "struct"
           || s == "="
           || s == "return"
           || s == ","
           || s == "("
           || s == "["
           || s == "{"
           || s == ":"
           || s == "::";
}

static bool is_array_literal_open(
  const std::optional<slang::token>& prev_prev,
  const std::optional<slang::token>& prev)
{
    if(!prev.has_value())
    {
        return false;
    }

    if(prev->s == "#")
    {
        return false;
    }

    if(prev->s == "="
       || prev->s == ","
       || prev->s == "("
       || prev->s == "["
       || prev->s == "{"
       || prev->s == "return")
    {
        return true;
    }

    return is_wrap_operator(prev->s) && starts_expression(prev_prev);
}

static bool needs_space(
  const std::optional<slang::token>& prev_prev,
  const slang::token& prev,
  const slang::token& curr)
{
    // Keep unary prefix operators attached to their operand: `-120`, `!flag`, `++i`.
    if(is_unary_prefix_operator(prev.s)
       && starts_expression(prev_prev))
    {
        return false;
    }

    if(is_tight_after(prev.s)
       || is_tight_before(curr.s))
    {
        return false;
    }

    if(curr.s == ":")
    {
        return false;
    }

    if(curr.s == "[")
    {
        // Keep attachment for `type[]`, `arr[i]`, and `#[...]`,
        // but preserve spacing in assignments like `= [ ... ]`
        // and member initializers like `name: [ ... ]`.
        if(prev.s == "#")
        {
            return false;
        }
        return prev.s == "=" || prev.s == ":";
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

/*
 * archive helpers.
 */

/**
 * Read a text archive/file via the serialization API.
 *
 * @param ar The archive to read from.
 * @return The file contents.
 */
static std::string read_archive(
  slang::archive& ar)
{
    if(!ar.is_reading())
    {
        throw std::runtime_error{
          "read_archive: Archive not readable."};
    }

    const auto size = ar.size();
    ar.seek(0);

    std::string content(size, '\0');
    if(size > 0)
    {
        ar.serialize(std::as_writable_bytes(std::span(content)));
    }

    return content;
}

/**
 * Write a text archive/file via the serialization API.
 *
 * @param ar The archive to write the string to.
 * @param text The text to write.
 */
static void write_archive(
  slang::archive& ar,
  const std::string& text)
{
    if(!ar.is_writing())
    {
        throw std::runtime_error{
          "write_archive: Archive not writable."};
    }

    ar.seek(0);

    if(text.empty())
    {
        return;
    }

    std::string writable{text};
    ar.serialize(std::as_writable_bytes(std::span(writable)));
}

/**
 * Read a file using the file manager.
 *
 * @param file_mgr The file manager to use.
 * @param path The file path.
 * @return Returns the file contents.
 */
static std::string read_file(
  slang::file_manager& file_mgr,
  const std::filesystem::path& path)
{
    auto ar = file_mgr.open(
      path,
      slang::file_manager::open_mode::read);
    return read_archive(*ar);
}

/**
 * Write a file using the file manager.
 *
 * @param file_mgr The file manager to use.
 * @param path Path of the file to write to.
 * @param text The text to write.
 */
static void write_file(
  slang::file_manager& file_mgr,
  const std::filesystem::path& path,
  const std::string& text)
{
    auto ar = file_mgr.open(
      path,
      slang::file_manager::open_mode::write);
    write_archive(*ar, text);
}

/** Return the length of the current line. */
static std::size_t current_line_length(
  const std::string& out)
{
    const auto pos = out.find_last_of('\n');
    if(pos == std::string::npos)
    {
        return out.size();
    }

    return out.size() - pos - 1;
}

}    // namespace

source_formatter::source_formatter(
  file_manager& file_mgr,
  options opts)
: file_mgr{file_mgr}
, opts{opts}
{
}

std::pair<std::string, bool>
  source_formatter::format_text(
    const std::string& source) const
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
    std::optional<token> prev_prev;
    std::optional<token> prev;
    std::vector<brace_context> brace_stack;
    std::vector<bracket_context> bracket_stack;
    std::size_t paren_depth = 0;
    std::size_t extra_indent_level = 0;

    std::size_t indent_level = 0;
    bool line_start = true;
    bool in_directive = false;

    auto append_indent = [&]() -> void
    {
        out.append((indent_level + extra_indent_level) * opts.indent_size, ' ');
    };

    auto append_newline = [&]() -> void
    {
        if(!out.empty() && out.back() != '\n')
        {
            out.push_back('\n');
        }
        line_start = true;
    };

    auto append_blank_line = [&]() -> void
    {
        if(out.empty())
        {
            return;
        }

        if(out.size() >= 2 && out[out.size() - 1] == '\n' && out[out.size() - 2] == '\n')
        {
            line_start = true;
            return;
        }

        if(out.back() != '\n')
        {
            out.push_back('\n');
        }
        out.push_back('\n');
        line_start = true;
    };

    auto inline_array_length_from =
      [&](std::size_t start_index) -> std::optional<std::pair<std::size_t, std::size_t>>
    {
        if(start_index >= tokens.size() || tokens[start_index].s != "[")
        {
            return std::nullopt;
        }

        std::size_t depth = 0;
        std::size_t length = 0;
        std::optional<token> local_prev_prev;
        std::optional<token> local_prev;

        for(std::size_t j = start_index; j < tokens.size(); ++j)
        {
            const auto& t = tokens[j];

            if(j > start_index
               && local_prev.has_value()
               && needs_space(local_prev_prev, *local_prev, t))
            {
                ++length;
            }

            length += render_token_text(t).size();

            if(t.s == "[")
            {
                ++depth;
            }
            else if(t.s == "]")
            {
                if(depth == 0)
                {
                    return std::nullopt;
                }

                --depth;
                if(depth == 0)
                {
                    return std::make_pair(length, j);
                }
            }

            local_prev_prev = local_prev;
            local_prev = t;
        }

        return std::nullopt;
    };

    for(std::size_t i = 0; i < tokens.size(); ++i)
    {
        const auto& tok = tokens[i];
        const token* next_tok = (i + 1 < tokens.size()) ? &tokens[i + 1] : nullptr;
        bool ended_top_level_decl = false;
        const bool closing_multiline_array =
          tok.s == "]"
          && !bracket_stack.empty()
          && bracket_stack.back() == bracket_context::array_multiline;

        if(tok.has_blank_line_before)
        {
            append_blank_line();
        }

        for(const auto& comment: tok.leading_comments)
        {
            if(comment.has_blank_line_before)
            {
                append_blank_line();
            }

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
        else if(closing_multiline_array)
        {
            if(extra_indent_level > 0)
            {
                --extra_indent_level;
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

        bool insert_space = !token_starts_line
                            && prev.has_value()
                            && needs_space(prev_prev, *prev, tok);
        const std::string token_text = render_token_text(tok);
        const std::size_t projected_len =
          current_line_length(out)
          + (insert_space ? 1 : 0)
          + token_text.size();

        bool break_after_token = false;
        bool expand_array_multiline = false;

        if(opts.max_line_length > 0
           && tok.s == "["
           && is_array_literal_open(prev_prev, prev))
        {
            const auto inline_array_length = inline_array_length_from(i);
            if(inline_array_length.has_value())
            {
                const auto [array_len, end_index] = *inline_array_length;
                const std::size_t projected_array_len =
                  current_line_length(out)
                  + (insert_space ? 1 : 0)
                  + array_len;

                std::size_t projected_total_len = projected_array_len;
                if(end_index + 1 < tokens.size())
                {
                    const auto& end_tok = tokens[end_index];
                    const auto& next_after_array = tokens[end_index + 1];

                    std::optional<token> array_prev;
                    if(end_index > i)
                    {
                        array_prev = tokens[end_index - 1];
                    }

                    if(needs_space(array_prev, end_tok, next_after_array))
                    {
                        ++projected_total_len;
                    }

                    projected_total_len += render_token_text(next_after_array).size();
                }

                expand_array_multiline = projected_total_len > opts.max_line_length;
            }
        }

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
            else if(!can_defer_break_to_operator
                    && !expand_array_multiline)
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

        out += token_text;

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
        else if(tok.s == "(")
        {
            ++paren_depth;
        }
        else if(tok.s == "[")
        {
            if(expand_array_multiline
               && next_tok
               && next_tok->s != "]")
            {
                bracket_stack.emplace_back(bracket_context::array_multiline);
                ++extra_indent_level;
                append_newline();
            }
            else
            {
                bracket_stack.emplace_back(bracket_context::other);
            }
        }
        else if(tok.s == ";")
        {
            append_newline();
            if(indent_level == 0 && !in_directive)
            {
                ended_top_level_decl = true;
            }
        }
        else if(in_directive && tok.s == "]")
        {
            append_newline();
            in_directive = false;
        }
        else if(tok.s == "}")
        {
            if(!brace_stack.empty())
            {
                brace_stack.pop_back();
            }

            if(next_tok
               && next_tok->s != ";"
               && next_tok->s != "else")
            {
                append_newline();
            }

            if(indent_level == 0 && !in_directive && (!next_tok || next_tok->s != ";"))
            {
                ended_top_level_decl = true;
            }
        }
        else if(tok.s == ")")
        {
            if(paren_depth > 0)
            {
                --paren_depth;
            }
        }
        else if(tok.s == "]")
        {
            if(!bracket_stack.empty())
            {
                bracket_stack.pop_back();
            }
        }
        else if(break_after_token)
        {
            append_newline();
        }
        else if(tok.s == ","
                && !brace_stack.empty()
                && brace_stack.back() == brace_context::struct_members)
        {
            if(bracket_stack.empty() && paren_depth == 0)
            {
                append_newline();
            }
        }
        else if(tok.s == ","
                && !bracket_stack.empty()
                && bracket_stack.back() == bracket_context::array_multiline)
        {
            append_newline();
        }

        if(ended_top_level_decl && next_tok)
        {
            append_blank_line();
        }

        if(tok.s == "{")
        {
            brace_stack.emplace_back(
              is_struct_member_block_open(prev_prev, prev)
                ? brace_context::struct_members
                : brace_context::other);
        }

        prev_prev = prev;
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

    return std::make_pair(
      out,
      out != source);
}

std::pair<std::string, bool>
  source_formatter::format_file(
    const std::filesystem::path& file) const
{
    return format_text(read_file(file_mgr, file));
}

bool source_formatter::format_file_in_place(const std::filesystem::path& file) const
{
    const auto original = read_file(file_mgr, file);
    const auto [formatted, changed] = format_text(original);

    if(!changed)
    {
        return false;
    }

    write_file(file_mgr, file, formatted);
    return true;
}

}    // namespace slang::formatter
