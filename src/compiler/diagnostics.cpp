/**
 * slang - a simple scripting language.
 *
 * Compiler diagnostics subsystem.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <format>
#include <print>

#include "compiler/diagnostics.h"

namespace slang::compiler::diagnostics
{

namespace
{

/**
 * Returns the line to display for a diagnostic span.
 * If the line at `line_no` is empty and the column is 1,
 * fall back to the previous non‑empty line (if any) and shift the column.
 */
std::optional<
  std::pair<
    std::string_view,
    source_location>>
  get_display_line(
    const context& ctx,
    const std::string& filename,
    source_location loc)
{
    // Try the requested line first.
    auto line_opt = ctx.get_line(filename, loc.line);
    if(line_opt
       && !(line_opt->empty()))
    {
        return std::make_pair(*line_opt, loc);
    }

    // If empty and we are at column 1, look at the previous line.
    if(loc.col == 1
       && loc.line > 1)
    {
        auto prev_opt = ctx.get_line(filename, loc.line - 1);
        if(prev_opt)
        {
            // New column = length of previous line + 1 (point after last char)
            source_location new_loc = loc;
            new_loc.line -= 1;
            new_loc.col = prev_opt->size() + 1;
            return std::make_pair(*prev_opt, new_loc);
        }
    }

    // Fallback – return whatever we have (may be empty).
    if(line_opt)
    {
        return std::make_pair(*line_opt, loc);
    }

    return std::nullopt;
}

}    // namespace

std::string_view to_string(severity s)
{
    switch(s)
    {
    case severity::error:
        return "error";
    case severity::warning:
        return "warning";
    case severity::note:
        return "note";
    case severity::help:
        return "help";
    }
    return "unknown";
}

std::size_t levenshtein_distance(std::string_view a, std::string_view b)
{
    const std::size_t m = a.size();
    const std::size_t n = b.size();
    if(m == 0)
    {
        return n;
    }
    if(n == 0)
    {
        return m;
    }

    std::vector<std::size_t> prev(n + 1);
    std::vector<std::size_t> curr(n + 1);

    for(std::size_t j = 0; j <= n; ++j)
    {
        prev[j] = j;
    }

    for(std::size_t i = 1; i <= m; ++i)
    {
        curr[0] = i;
        for(std::size_t j = 1; j <= n; ++j)
        {
            if(a[i - 1] == b[j - 1])
            {
                curr[j] = prev[j - 1];
            }
            else
            {
                curr[j] = 1 + std::min({prev[j], curr[j - 1], prev[j - 1]});
            }
        }
        prev = curr;
    }

    return prev[n];
}

std::optional<std::string> find_closest_match(
  std::string_view target,
  const std::vector<std::string>& candidates,
  std::optional<std::size_t> max_distance)
{
    if(candidates.empty() || target.empty())
    {
        return std::nullopt;
    }

    const std::size_t threshold =
      max_distance.value_or(std::max(target.size(), std::size_t{3}) / 3);

    std::optional<std::string> best_match;
    std::size_t best_dist = threshold + 1;

    for(const auto& cand: candidates)
    {
        if(cand == target)
        {
            continue;
        }

        const std::size_t dist = levenshtein_distance(target, cand);
        if(dist <= threshold && dist < best_dist)
        {
            best_dist = dist;
            best_match = cand;
        }
    }

    return best_match;
}

void context::register_file(std::string filename, std::string content)
{
    source_files[std::move(filename)] = std::move(content);
}

bool context::has_file(const std::string& filename) const
{
    return source_files.contains(filename);
}

std::optional<std::string_view> context::get_line(
  const std::string& filename,
  std::size_t line) const
{
    auto it = source_files.find(filename);
    if(it == source_files.end() || line == 0)
    {
        return std::nullopt;
    }

    std::string_view sv = it->second;
    std::size_t cur_line = 1;
    std::size_t start = 0;

    for(std::size_t i = 0; i < sv.size(); ++i)
    {
        if(sv[i] == '\n')
        {
            if(cur_line == line)
            {
                std::size_t end = i;
                if(end > start && sv[end - 1] == '\r')
                {
                    --end;
                }
                return sv.substr(start, end - start);
            }
            ++cur_line;
            start = i + 1;
        }
    }

    if(cur_line == line && start <= sv.size())
    {
        std::size_t end = sv.size();
        if(end > start && sv[end - 1] == '\r')
        {
            --end;
        }
        return sv.substr(start, end - start);
    }

    return std::nullopt;
}

diagnostic& context::report(diagnostic diag)
{
    if(diag.sev == severity::error)
    {
        ++error_count;
    }
    else if(diag.sev == severity::warning)
    {
        ++warning_count;
    }

    diagnostics.push_back(std::move(diag));
    return diagnostics.back();
}

diagnostic& context::report_error(
  std::optional<std::string> filename,
  std::optional<source_span> span,
  std::string message,
  std::optional<std::string> code)
{
    diagnostic d{
      severity::error,
      std::move(message),
      std::move(filename),
      span};
    if(code.has_value())
    {
        d.with_code(std::move(code.value()));
    }
    return report(std::move(d));
}

diagnostic& context::report_warning(
  std::optional<std::string> filename,
  std::optional<source_span> span,
  std::string message,
  std::optional<std::string> code)
{
    diagnostic d{
      severity::warning,
      std::move(message),
      std::move(filename),
      span};
    if(code.has_value())
    {
        d.with_code(std::move(code.value()));
    }
    return report(std::move(d));
}

void context::clear()
{
    diagnostics.clear();
    error_count = 0;
    warning_count = 0;
}

static void render_snippet(
  std::string& out,
  const context& ctx,
  const std::string& filename,
  const source_span& span,
  const std::optional<std::string>& label)
{
    auto src_line = get_display_line(ctx, filename, span.location);
    if(!src_line.has_value())
    {
        return;
    }

    const auto& [source_line, location] = *src_line;

    const std::size_t length =
      span.length == 0
        ? 1
        : span.length;

    const std::string line_str = std::to_string(location.line);
    const std::size_t width = std::max<std::size_t>(line_str.length(), 2);

    out += std::format(
      "{:>{}} |\n",
      "",
      width);
    out += std::format(
      "{:>{}} | {}\n",
      line_str,
      width,
      source_line);
    out += std::format(
      "{:>{}} | ",
      "",
      width);

    // Calculate horizontal indentation matching tabs/spaces.
    const std::size_t indent_cols =
      location.col > 0
        ? location.col - 1
        : 0;

    for(std::size_t i = 0; i < indent_cols; ++i)
    {
        if(i < source_line.size()
           && source_line[i] == '\t')
        {
            out += '\t';
        }
        else
        {
            out += ' ';
        }
    }

    out.append(length, '^');
    if(label.has_value()
       && !label->empty())
    {
        out += " ";
        out += *label;
    }
    out += "\n";
    out += std::format(
      "{:>{}} |\n",
      "",
      width);
}

std::string context::format(
  const diagnostic& diag) const
{
    std::string out;

    // Header.
    if(diag.code.has_value())
    {
        out += std::format(
          "{}[{}]: {}\n",
          to_string(diag.sev),
          diag.code.value(),
          diag.message);
    }
    else
    {
        out += std::format(
          "{}: {}\n",
          to_string(diag.sev),
          diag.message);
    }

    // Location & snippet.
    std::size_t width = 2;
    if(diag.span.has_value())
    {
        std::string line_str = std::to_string(diag.span->location.line);
        width = std::max<std::size_t>(line_str.length(), 2);
    }

    if(diag.filename.has_value())
    {
        if(diag.span.has_value())
        {
            out += std::format(
              "{:>{}}--> {}:{}:{}\n",
              "",
              width,
              diag.filename.value(),
              diag.span.value().location.line,
              diag.span.value().location.col);
            render_snippet(
              out,
              *this,
              diag.filename.value(),
              *diag.span,
              diag.label);
        }
        else
        {
            out += std::format(
              "{:>{}}--> {}\n",
              "",
              width,
              diag.filename.value());
        }
    }

    // Sub-diagnostics.
    for(const auto& child: diag.children)
    {
        out += std::format(
          "{}: {}\n",
          to_string(child.sev),
          child.message);
        if(child.span.has_value()
           && diag.filename.has_value())
        {
            render_snippet(
              out,
              *this,
              diag.filename.value(),
              *child.span,
              child.label);
        }
    }

    return out;
}

std::string context::format_all() const
{
    std::string out;
    bool first = true;
    for(const auto& diag: diagnostics)
    {
        // Separate consecutive diagnostics with a blank line.
        if(!first)
        {
            out += "\n";
        }
        out += format(diag);
        first = false;
    }
    return out;
}

std::string context::format_summary() const
{
    if(error_count > 0)
    {
        return std::format(
          "error: {} error{} generated\n",
          error_count,
          error_count == 1 ? "" : "s");
    }
    if(warning_count > 0)
    {
        return std::format(
          "warning: {} warning{} generated\n",
          warning_count,
          warning_count == 1 ? "" : "s");
    }
    return {};
}

void context::emit_all(
  std::FILE* stream,
  bool with_summary) const
{
    std::string out = format_all();
    if(with_summary)
    {
        std::string summary = format_summary();
        if(!summary.empty())
        {
            // Separate the summary from the diagnostics with a blank line.
            if(!out.empty())
            {
                out += "\n";
            }
            out += summary;
        }
    }
    std::print(stream, "{}", out);
}

}    // namespace slang::compiler::diagnostics
