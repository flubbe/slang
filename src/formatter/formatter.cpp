/**
 * slang - a simple scripting language.
 *
 * source formatter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <array>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "filemanager.h"
#include "formatter/formatter.h"

namespace
{

using namespace slang;

/*
 * token helpers.
 */

/** Return true when the token behaves like a word for spacing rules. */
bool is_word_token(
  const slang::token& tok)
{
    return tok.type == token_type::identifier
           || tok.type == token_type::macro_identifier
           || tok.type == token_type::macro_name
           || tok.type == token_type::int_literal
           || tok.type == token_type::fp_literal
           || tok.type == token_type::str_literal;
}

/** Visit escaped output bytes for a raw string value. */
template<typename Fn>
void for_each_escaped_char(
  const std::string& s,
  Fn&& fn)
{
    for(char c: s)
    {
        switch(c)
        {
        case '\\':
        case '"':
            fn('\\');
            fn(c);
            break;
        case '\n':
            fn('\\');
            fn('n');
            break;
        case '\r':
            fn('\\');
            fn('r');
            break;
        case '\t':
            fn('\\');
            fn('t');
            break;
        case '\f':
            fn('\\');
            fn('f');
            break;
        case '\v':
            fn('\\');
            fn('v');
            break;
        default:
            fn(c);
            break;
        }
    }
}

/** Escape a raw string value into a quoted source-level string literal. */
std::string escape_string_literal(
  const std::string& s)
{
    std::size_t escaped_size = 0;
    for_each_escaped_char(
      s,
      [&](char)
      { ++escaped_size; });

    std::string out;
    out.reserve(escaped_size + 2);
    out.push_back('"');

    for_each_escaped_char(
      s,
      [&](char escaped_c)
      { out.push_back(escaped_c); });

    out.push_back('"');
    return out;
}

/*
 * formatting / rule helpers.
 */

/** Return canonical rendered token length without allocating temporary strings. */
std::size_t rendered_token_size(
  const slang::token& tok)
{
    if(tok.type == token_type::str_literal
       && tok.value.has_value())
    {
        std::size_t escaped_size = 0;
        for_each_escaped_char(
          std::get<std::string>(*tok.value),
          [&](char)
          { ++escaped_size; });

        return escaped_size + 2;
    }

    return tok.s.size();
}

/** Append canonical rendered token text directly into an output buffer. */
void append_rendered_token(
  std::string& out,
  const slang::token& tok)
{
    if(tok.type == token_type::str_literal
       && tok.value.has_value())
    {
        out += escape_string_literal(
          std::get<std::string>(*tok.value));
        return;
    }

    out += tok.s;
}

/**
 * Return true when the token text is contained in the given array of string views.
 *
 * @tparam N The size of the array.
 * @param values The array of string views to check against.
 * @param s The string to check for containment in the array.
 * @return True if the string is contained in the array, false otherwise.
 */
template<std::size_t N>
bool contains_string(
  const std::array<std::string_view, N>& values,
  std::string_view s)
{
    return std::find(values.begin(), values.end(), s) != values.end();
}

/** Return true when no space should be inserted after this token text. */
bool is_tight_after(
  const std::string& s)
{
    static const std::array<std::string_view, 5> tight_after = {
      "(", "[", "{", ".", "::"};
    return contains_string(tight_after, s);
}

/** Return true when no space should be inserted before this token text. */
bool is_tight_before(
  const std::string& s)
{
    static const std::array<std::string_view, 8> tight_before = {
      ")", "]", "}", ",", ";", ".", "::", "..."};
    return contains_string(tight_before, s);
}

/** Return true when keywords like `return` require a space before `(`. */
bool is_keyword_space_before_paren(
  const slang::token& prev)
{
    static const std::array<std::string_view, 3> keywords = {
      "return", "fn", "macro"};
    return prev.type == token_type::identifier
           && contains_string(keywords, prev.s);
}

/** Return true when the token text is a binary operator that can wrap lines. */
bool is_wrap_operator(
  const std::string& s)
{
    static const std::array<std::string_view, 29> ops = {
      "+", "-", "*", "/", "%", "&", "|", "^",
      "&&", "||", "<<", ">>",
      "==", "!=", "<", "<=", ">", ">=",
      "=",
      "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};
    return contains_string(ops, s);
}

/** Return true when the token text is a unary prefix operator. */
bool is_unary_prefix_operator(
  const std::string& s)
{
    static const std::array<std::string_view, 6> ops = {
      "+", "-", "!", "~", "++", "--"};
    return contains_string(ops, s);
}

/** Return true when the previous token position can begin a new expression. */
bool starts_expression(
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

enum class brace_context : std::uint8_t
{
    other,
    struct_members
};

enum class bracket_context : std::uint8_t
{
    other,
    array_multiline
};

/** Return true when `{` opens a struct-member block context. */
bool is_struct_member_block_open(
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

/** Return true when `[` likely starts an array literal context. */
bool is_array_literal_open(
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

/** Return true when a space should be emitted between adjacent tokens. */
bool needs_space(
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
std::string read_archive(
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
void write_archive(
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
std::string read_file(
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
void write_file(
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
std::size_t current_line_length(
  const std::string& out)
{
    const auto pos = out.find_last_of('\n');
    if(pos == std::string::npos)
    {
        return out.size();
    }

    return out.size() - pos - 1;
}

/** Validate syntax with the parser when formatter options require it. */
void validate_syntax_if_enabled(
  const slang::formatter::options& opts,
  const std::string& source)
{
    if(!opts.validate_syntax)
    {
        return;
    }

    lexer parser_lexer{source};
    parser parser;
    parser.parse(parser_lexer);
}

/** Lex source text into formatter input tokens. */
std::vector<token> lex_tokens(
  const std::string& source)
{
    lexer token_lexer{source};
    std::vector<token> tokens;
    while(auto t = token_lexer.next())
    {
        tokens.emplace_back(std::move(*t));
    }
    return tokens;
}

/** Return inline rendered length and matching closing index for an array literal. */
std::optional<std::pair<std::size_t, std::size_t>> inline_array_length_from(
  const std::vector<token>& tokens,
  std::size_t start_index)
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

        length += rendered_token_size(t);

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
}

/** Formatter state. */
struct format_state
{
    /** Formatter options shared by all rendering operations. */
    const slang::formatter::options& opts;    // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    std::string out;                            /**< Accumulated formatted output text. */
    std::optional<token> prev_prev;             /**< Token two positions behind the current token. */
    std::optional<token> prev;                  /**< Previous token emitted to the output stream. */
    std::vector<brace_context> brace_stack;     /**< Stack tracking brace-delimited formatting contexts. */
    std::vector<bracket_context> bracket_stack; /**< Stack tracking bracket-delimited formatting contexts. */
    std::size_t paren_depth = 0;                /**< Parenthesis nesting depth for argument and expression layout. */
    std::size_t extra_indent_level = 0;         /**< Additional indentation used for multiline array layouts. */
    std::size_t indent_level = 0;               /**< Base indentation depth from brace nesting. */
    bool line_start = true;                     /**< Whether the next token/comment starts a new logical line. */
    bool in_directive = false;                  /**< Whether we are currently rendering a `#[...]` directive. */

    /** Construct formatter state bound to immutable formatter options. */
    explicit format_state(
      const slang::formatter::options& opts)
    : opts{opts}
    {
    }

    /** Append indentation for the current logical line depth. */
    void append_indent()
    {
        // Indentation is driven by `indent_level`; brace/bracket stacks are used
        // for contextual rules and may transiently differ during token processing.
        out.append((indent_level + extra_indent_level) * opts.indent_size, ' ');
    }

    /** Append a newline unless one is already present at the end. */
    void append_newline()
    {
        if(!out.empty() && out.back() != '\n')
        {
            out.push_back('\n');
        }
        line_start = true;
    }

    /** Ensure exactly one blank line boundary at the current position. */
    void append_blank_line()
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
    }
};

/** Plan for wrapping and spacing decisions for a single token. */
struct wrap_plan
{
    bool insert_space = false;                  /**< Whether to insert a space before the token. */
    bool break_after_token = false;             /**< Whether to break the line after the token. */
    bool expand_array_multiline = false;        /**< Whether to expand an array literal across multiple lines. */
    bool force_line_break_before_token = false; /**< Whether to force a line break before the token. */
};

/** Compute wrap/spacing decisions for one token at the current output position. */
wrap_plan compute_wrap_plan(
  const format_state& state,
  const std::vector<token>& tokens,
  std::size_t i,
  const token& tok,
  const token* next_tok,
  bool token_starts_line)
{
    wrap_plan plan;
    plan.insert_space = !token_starts_line
                        && state.prev.has_value()
                        && needs_space(state.prev_prev, *state.prev, tok);

    const std::size_t projected_len =
      current_line_length(state.out)
      + (plan.insert_space ? 1 : 0)
      + rendered_token_size(tok);

    if(state.opts.max_line_length > 0
       && tok.s == "["
       && is_array_literal_open(state.prev_prev, state.prev))
    {
        const auto inline_array_length = inline_array_length_from(tokens, i);
        if(inline_array_length.has_value())
        {
            const auto [array_len, end_index] = *inline_array_length;
            const std::size_t projected_array_len =
              current_line_length(state.out)
              + (plan.insert_space ? 1 : 0)
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

                projected_total_len += rendered_token_size(next_after_array);
            }

            plan.expand_array_multiline = projected_total_len > state.opts.max_line_length;
        }
    }

    if(state.opts.max_line_length > 0
       && !token_starts_line
       && projected_len > state.opts.max_line_length)
    {
        const bool can_defer_break_to_operator =
          next_tok != nullptr
          && is_word_token(tok)
          && is_wrap_operator(next_tok->s)
          && tok.trailing_comments.empty();

        const bool can_break_after_operator =
          is_wrap_operator(tok.s)
          && next_tok != nullptr
          && is_word_token(*next_tok)
          && tok.trailing_comments.empty();

        if(can_break_after_operator)
        {
            plan.break_after_token = true;
        }
        else if(!can_defer_break_to_operator
                && !plan.expand_array_multiline)
        {
            plan.force_line_break_before_token = true;
            plan.insert_space = false;
        }
    }

    return plan;
}

/** Emit leading blank lines/comments and prepare token indentation context. */
bool emit_leading_for_token(
  format_state& state,
  const token& tok,
  const token* next_tok)
{
    if(tok.has_blank_line_before)
    {
        state.append_blank_line();
    }

    for(const auto& comment: tok.leading_comments)
    {
        if(comment.has_blank_line_before)
        {
            state.append_blank_line();
        }

        if(!state.line_start)
        {
            state.append_newline();
        }

        if(state.line_start)
        {
            state.append_indent();
            state.line_start = false;
        }

        state.out += comment.text;
        state.append_newline();
    }

    const bool token_starts_line = state.line_start;
    if(tok.s == "#" && next_tok && next_tok->s == "[")
    {
        if(!token_starts_line && !state.line_start && !state.out.empty() && state.out.back() != '\n')
        {
            state.append_newline();
        }
        state.in_directive = true;
    }

    return token_starts_line;
}

/** Emit the token text and trailing comments, applying wrap plan decisions. */
void emit_token_core(
  format_state& state,
  const token& tok,
  const wrap_plan& plan)
{
    if(plan.force_line_break_before_token)
    {
        state.append_newline();
        state.append_indent();
        state.line_start = false;
    }

    if(plan.insert_space && !state.line_start)
    {
        state.out.push_back(' ');
    }

    append_rendered_token(state.out, tok);

    for(const auto& comment: tok.trailing_comments)
    {
        if(!state.line_start)
        {
            state.out.push_back(' ');
        }

        state.out += comment.text;

        if(!comment.is_block)
        {
            state.append_newline();
        }
    }
}

/** Apply post-token state transitions and return whether a top-level declaration ended. */
bool apply_post_token_state(
  format_state& state,
  const token& tok,
  const token* next_tok,
  const wrap_plan& plan,
  bool closing_multiline_array)
{
    bool ended_top_level_decl = false;

    if(tok.s == "{")
    {
        ++state.indent_level;
        state.append_newline();
    }
    else if(tok.s == "(")
    {
        ++state.paren_depth;
    }
    else if(tok.s == "[")
    {
        if(plan.expand_array_multiline
           && next_tok != nullptr
           && next_tok->s != "]")
        {
            state.bracket_stack.emplace_back(bracket_context::array_multiline);
            ++state.extra_indent_level;
            state.append_newline();
        }
        else
        {
            state.bracket_stack.emplace_back(bracket_context::other);
        }
    }
    else if(tok.s == ";")
    {
        state.append_newline();
        if(state.indent_level == 0 && !state.in_directive)
        {
            ended_top_level_decl = true;
        }
    }
    else if(state.in_directive && tok.s == "]")
    {
        state.append_newline();
        state.in_directive = false;
    }
    else if(tok.s == "}")
    {
        if(next_tok != nullptr && next_tok->s != ";" && next_tok->s != "else")
        {
            state.append_newline();
        }

        if(state.indent_level == 0 && !state.in_directive && (next_tok == nullptr || next_tok->s != ";"))
        {
            ended_top_level_decl = true;
        }
    }
    else if(tok.s == ")")
    {
        if(state.paren_depth > 0)
        {
            --state.paren_depth;
        }
    }
    else if(tok.s == "]")
    {
        if(!closing_multiline_array && !state.bracket_stack.empty())
        {
            state.bracket_stack.pop_back();
        }
    }
    else if(plan.break_after_token)
    {
        state.append_newline();
    }
    else if(tok.s == ","
            && !state.brace_stack.empty()
            && state.brace_stack.back() == brace_context::struct_members)
    {
        if(state.bracket_stack.empty() && state.paren_depth == 0)
        {
            state.append_newline();
        }
    }
    else if(tok.s == ","
            && !state.bracket_stack.empty()
            && state.bracket_stack.back() == bracket_context::array_multiline)
    {
        state.append_newline();
    }

    return ended_top_level_decl;
}

/** Process a single token and update formatter state. */
void process_token_in_stream(
  format_state& state,
  const std::vector<token>& tokens,
  std::size_t i)
{
    const auto& tok = tokens[i];
    const token* next_tok = (i + 1 < tokens.size()) ? &tokens[i + 1] : nullptr;
    const bool closing_multiline_array =
      tok.s == "]"
      && !state.bracket_stack.empty()
      && state.bracket_stack.back() == bracket_context::array_multiline;

    emit_leading_for_token(state, tok, next_tok);

    if(tok.s == "}")
    {
        if(state.indent_level > 0)
        {
            --state.indent_level;
        }

        if(!state.brace_stack.empty())
        {
            state.brace_stack.pop_back();
        }

        if(!state.line_start)
        {
            state.append_newline();
        }
    }
    else if(closing_multiline_array)
    {
        if(state.extra_indent_level > 0)
        {
            --state.extra_indent_level;
        }

        if(!state.bracket_stack.empty())
        {
            state.bracket_stack.pop_back();
        }

        if(!state.line_start)
        {
            state.append_newline();
        }
    }

    const bool token_starts_line = state.line_start;
    if(state.line_start)
    {
        state.append_indent();
        state.line_start = false;
    }

    const wrap_plan plan = compute_wrap_plan(
      state,
      tokens,
      i,
      tok,
      next_tok,
      token_starts_line);

    emit_token_core(state, tok, plan);

    if(tok.s == "{")
    {
        state.brace_stack.emplace_back(
          is_struct_member_block_open(state.prev_prev, state.prev)
            ? brace_context::struct_members
            : brace_context::other);
    }

    const bool ended_top_level_decl = apply_post_token_state(
      state,
      tok,
      next_tok,
      plan,
      closing_multiline_array);

    if(ended_top_level_decl && next_tok != nullptr)
    {
        state.append_blank_line();
    }

    state.prev_prev = state.prev;
    state.prev = tok;
}

}    // namespace

namespace slang::formatter
{

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
    validate_syntax_if_enabled(opts, source);

    const std::vector<token> tokens = lex_tokens(source);
    format_state state{opts};
    state.out.reserve(source.size());

    for(std::size_t i = 0; i < tokens.size(); ++i)
    {
        process_token_in_stream(state, tokens, i);
    }

    // Trim trailing spaces and normalize final newline policy.
    while(!state.out.empty() && (state.out.back() == ' ' || state.out.back() == '\t' || state.out.back() == '\n' || state.out.back() == '\r'))
    {
        state.out.pop_back();
    }

    if(opts.ensure_trailing_newline)
    {
        state.out.push_back('\n');
    }

    return std::make_pair(
      state.out,
      state.out != source);
}

std::pair<std::string, bool>
  source_formatter::format_file(
    const std::filesystem::path& file) const
{
    return format_text(read_file(file_mgr, file));
}

bool source_formatter::format_file_in_place(
  const std::filesystem::path& file) const
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
