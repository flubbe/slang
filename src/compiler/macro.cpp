/**
 * slang - a simple scripting language.
 *
 * macro collection / expansion environment.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <algorithm>
#include <utility>

#include "compiler/macro.h"
#include "compiler/parser.h"
#include "syntax_context.h"

namespace
{

using slang::syntax_context_id;
using slang::token;
using slang::token_stream;

/**
 * Helper to retag tokens with a given context id.
 *
 * @param ts The input token stream.
 * @param ctx Current syntax context id to use.
 * @returns Returns the token stream tagged with the syntax context id.
 */
token_stream retag(
  const token_stream& ts,
  syntax_context_id ctx)
{
    std::vector<token> new_tokens;
    new_tokens.reserve(ts.size());
    for(const auto& t: ts.data())
    {
        token copy = t;
        copy.context = ctx;
        new_tokens.push_back(std::move(copy));
    }
    return token_stream{std::move(new_tokens)};
}

/** Result of selecting a macro rule. */
struct rule_selection
{
    const slang::module_::macro_rule_descriptor* rule{nullptr};
    bool ambiguous{false};
};

/**
 * Check whether a branch's captured pattern metadata is internally consistent
 * and matches the invocation's argument count and separators.
 */
bool matches_branch_pattern(
  const slang::module_::macro_rule_descriptor& branch,
  const std::vector<token_stream>& args,
  const std::vector<token>& separators)
{
    if(branch.args_end_with_list && branch.params.empty())
    {
        return false;
    }

    if(branch.params.empty())
    {
        if(!branch.separators.empty())
        {
            return false;
        }
    }
    else if(branch.separators.size() != branch.params.size() - 1)
    {
        return false;
    }

    const std::size_t fixed_param_count =
      branch.params.size() - static_cast<std::size_t>(branch.args_end_with_list);
    if(separators.size() != (args.empty() ? 0 : args.size() - 1))
    {
        return false;
    }

    if(!branch.args_end_with_list && args.size() != branch.params.size())
    {
        return false;
    }
    if(branch.args_end_with_list && args.size() < fixed_param_count)
    {
        return false;
    }

    for(std::size_t i = 0; i < separators.size(); ++i)
    {
        if(branch.args_end_with_list && fixed_param_count == 0)
        {
            if(separators[i].s != ",")
            {
                return false;
            }
        }
        else
        {
            const std::size_t pattern_index =
              std::min(i, branch.separators.size() - 1);
            const auto& expected = branch.separators[pattern_index];
            // Token kinds can differ between parsed macro patterns and calls;
            // the separator spelling is the syntax that determines a match.
            if(separators[i].s != expected.s)
            {
                return false;
            }
        }
    }

    return true;
}

/**
 * Select the most specific matching branch. Fixed-arity branches take
 * precedence over variadic branches; among variadic branches, the one with
 * the longest fixed prefix takes precedence.
 */
rule_selection select_branch(
  const std::vector<slang::module_::macro_rule_descriptor>& branches,
  const std::vector<token_stream>& args,
  const std::vector<token>& separators)
{
    const slang::module_::macro_rule_descriptor* branch{nullptr};
    std::pair<bool, std::size_t> best_score{false, 0};
    bool ambiguous{false};

    for(const auto& candidate: branches)
    {
        if(!matches_branch_pattern(candidate, args, separators))
        {
            continue;
        }

        const std::size_t fixed_param_count =
          candidate.params.size()
          - static_cast<std::size_t>(candidate.args_end_with_list);
        const auto score = std::pair{
          !candidate.args_end_with_list,
          fixed_param_count};

        if(branch == nullptr || score > best_score)
        {
            branch = &candidate;
            best_score = score;
            ambiguous = false;
        }
        else if(score == best_score)
        {
            ambiguous = true;
        }
    }

    return rule_selection{branch, ambiguous};
}

}    // namespace

namespace slang::macro
{

bool macro::supports_token_expansion(
  const std::vector<token_stream>& args,
  const std::vector<token>& separators) const
{
    // Nested macro invocations must be expanded before their argument tokens
    // can be substituted into this macro.
    for(const auto& arg: args)
    {
        if(std::ranges::any_of(
             arg,
             [](const token& tok)
             {
                 return tok.type == token_type::macro_identifier;
             }))
        {
            return false;
        }
    }

    const auto selection = select_branch(desc.branches, args, separators);

    return selection.rule != nullptr && !selection.ambiguous;
}

token_stream macro::expand_macro(
  const std::vector<token_stream>& args,
  syntax_context_id call_site,
  syntax_context_table& contexts,
  source_location call_loc,
  const std::vector<token>& separators) const
{
    // Don't expand empty macros.
    if(desc.branches.empty())
    {
        return token_stream{};
    }

    // Reject unresolved nested macro invocations here as well as in
    // supports_token_expansion() so direct callers cannot consume a partial
    // expansion.
    for(const auto& arg: args)
    {
        if(std::ranges::any_of(
             arg,
             [](const token& tok)
             {
                 return tok.type == token_type::macro_identifier;
             }))
        {
            throw macro_error{
              std::format(
                "Macro '{}' argument contains an unresolved macro placeholder.",
                name)};
        }
    }

    const auto selection = select_branch(desc.branches, args, separators);
    const auto* branch = selection.rule;

    if(branch == nullptr)
    {
        throw macro_error{
          std::format(
            "Could not match a branch of macro '{}'.",
            name)};
    }
    if(selection.ambiguous)
    {
        throw macro_error{
          std::format(
            "Macro '{}' has ambiguous branches.",
            name)};
    }

    /*
     * Expand a macro hygienically:
     *
     * 1. Create a fresh syntax context for this expansion, parented at the
     *    macro's definition context. This preserves the definition-site
     *    ancestry of identifiers originating in the macro body.
     * 2. Walk the macro body token stream.
     * 3. If an identifier matches a macro parameter, substitute the
     *    corresponding argument token stream, retagging its tokens with
     *    the call-site context. Preserving nested capture contexts is deferred
     *    to the hygiene completion step.
     * 4. Otherwise, keep the token but retag it with the fresh expansion
     *    context, so that identifiers originating in the macro body remain
     *    distinct from identifiers originating at the call site.
     * 5. Concatenate the resulting streams and return the expanded token stream.
     */

    token_stream result;

    /*
     * Fresh expansion context.
     *
     * The expansion happens at the call site, but identifiers originating
     * in the macro body retain the macro's definition-site ancestry.
     */
    syntax_context_id expansion_ctx = contexts.allocate(definition_context);

    // Process each token.
    for(const auto& tok: branch->body.tokens.data())
    {
        // Check if token is an identifier that matches a macro parameter.
        bool replaced = false;
        if(tok.type == token_type::macro_identifier)
        {
            // Search parameters by name.
            for(std::size_t i = 0; i < branch->params.size(); ++i)
            {
                const auto& param = branch->params[i];    // bounds are check in loop condition. // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                if(tok.s == param.name)
                {
                    // Parameter match -> splice argument tokens, retagged.
                    if(i >= args.size())
                    {
                        // Variadic parameter with no remaining arguments.
                        if(branch->args_end_with_list)
                        {
                            // Empty variadic capture is valid.
                        }
                        else
                        {
                            throw macro_error{
                              std::format(
                                "Macro '{}' is missing argument for '{}'.",
                                name,
                                param.name)};
                        }
                    }
                    else if(branch->args_end_with_list && i == branch->params.size() - 1)
                    {
                        // Variadic parameter: splice all remaining args.
                        std::vector<token> variadic_tokens;
                        for(std::size_t j = i; j < args.size(); ++j)
                        {
                            if(j > i)
                            {
                                variadic_tokens.emplace_back(
                                  ",", call_loc, token_type::delimiter);
                            }
                            for(const auto& t: args[j].data())
                            {
                                token copy = t;
                                copy.context = call_site;
                                variadic_tokens.push_back(std::move(copy));
                            }
                        }
                        result = result.concat(
                          token_stream{std::move(variadic_tokens)});
                    }
                    else
                    {
                        result = result.concat(retag(args[i], call_site));
                    }
                    replaced = true;
                    break;
                }
            }
        }

        if(tok.type == token_type::macro_identifier && !replaced)
        {
            // Allow self-recursion: don't throw for the macro's own name.
            if(tok.s != name)
            {
                throw macro_error{
                  std::format(
                    "Macro '{}' references unknown parameter '{}'.",
                    name,
                    tok.s)};
            }
        }

        if(!replaced)
        {
            // Keep definition-originating tokens in this expansion's context.
            token copy = tok;
            copy.context = expansion_ctx;
            result = result.append(copy);
        }
    }

    if(branch->body.kind == slang::module_::macro_body_kind::expression
       && !result.empty()
       && result.back().s == ";")
    {
        // Single-expression body: drop the trailing ';' so the parse-back
        // sees a bare expression.
        std::vector<token> expression_tokens;
        expression_tokens.reserve(result.size() - 1);
        for(std::size_t i = 0; i + 1 < result.size(); ++i)
        {
            expression_tokens.emplace_back(result[i]);
        }
        return token_stream{std::move(expression_tokens)};
    }

    // Block body: wrap the retagged statement list in synthetic braces so the
    // parse-back (parse_expression) can rebuild it as a block whose value is
    // its tail expression. The braces are syntactic and carry no context. This
    // is produced only on the token path, so block-as-value stays a
    // macro-expansion construct rather than a general language expression.
    if(branch->body.kind == slang::module_::macro_body_kind::statement_list)
    {
        std::vector<token> wrapped;
        wrapped.reserve(result.size() + 2);
        wrapped.emplace_back("{", call_loc, token_type::delimiter);
        for(std::size_t i = 0; i < result.size(); ++i)
        {
            wrapped.emplace_back(result[i]);
        }
        wrapped.emplace_back("}", call_loc, token_type::delimiter);
        return token_stream{std::move(wrapped)};
    }

    return result;
}

void env::add_macro(
  std::string name,
  module_::macro_descriptor desc,
  std::optional<std::string> import_path,
  syntax_context_id definition_context)
{
    auto it = std::ranges::find_if(
      macros,
      [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
      {
          return m->get_name() == name
                 && m->get_import_path() == import_path;
      });

    // Repeated import-resolution passes may register the same macro again.
    // Replace its descriptor rather than adding duplicate entries.
    if(it != macros.end())
    {
        (*it) = std::make_unique<macro>(
          std::move(name),
          std::move(desc),
          std::move(import_path),
          definition_context);
        return;
    }

    macros.emplace_back(
      std::make_unique<macro>(
        std::move(name),
        std::move(desc),
        std::move(import_path),
        definition_context));
}

macro* env::get_macro(
  const std::string& name,
  std::optional<std::string> import_path)
{
    auto it = std::ranges::find_if(
      macros,
      [&name, &import_path](const std::unique_ptr<macro>& m) -> bool
      {
          return m->get_name() == name
                 && m->get_import_path() == import_path;
      });

    if(it != macros.end())
    {
        return it->get();
    }

    // macro was not found.
    if(import_path.has_value())
    {
        throw macro_error(
          std::format(
            "Macro '{}::{}' not found.",
            import_path.value(),
            name));
    }
    throw macro_error(
      std::format(
        "Macro '{}' not found.",
        name));
}

}    // namespace slang::macro
