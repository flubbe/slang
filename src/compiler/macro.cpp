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

#include "compiler/macro.h"
#include "compiler/parser.h"
#include "sema.h"
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

/** Result of selecting a macro branch. */
struct branch_selection
{
    const slang::module_::macro_branch_descriptor* branch{nullptr};
    bool ambiguous{false};
};

/**
 * Return whether a descriptor has a complete token-expansion body.
 *
 * Capability is deliberately checked together with the body metadata. This
 * keeps the token-expansion entry point from treating a partially populated
 * migration descriptor as a successful empty expansion.
 */
bool has_token_expansion_body(
  const slang::module_::macro_branch_descriptor& branch)
{
    return branch.capability
             == slang::module_::macro_expansion_capability::token_stream
           && branch.body.has_value()
           && branch.body_is_expression;
}

/**
 * Select the best matching branch for a macro invocation.
 *
 * Exact non-variadic branches win over exact variadic branches, which win
 * over variadic branches with additional arguments.
 *
 * TODO(macro-migration): Replace this compatibility matcher with complete
 * token-pattern matching once all branch patterns are represented.
 */
branch_selection select_branch(
  const std::vector<slang::module_::macro_branch_descriptor>& branches,
  const std::vector<token_stream>& args)
{
    const slang::module_::macro_branch_descriptor* branch{nullptr};
    std::size_t best_score{0};
    bool ambiguous{false};

    for(const auto& candidate: branches)
    {
        std::optional<std::size_t> score;
        if(args.size() == candidate.params.size())
        {
            score = candidate.args_end_with_list ? 2 : 3;
        }
        else if(candidate.args_end_with_list
                && args.size() > candidate.params.size())
        {
            score = 1;
        }

        if(!score.has_value())
        {
            continue;
        }

        if(branch == nullptr || score.value() > best_score)
        {
            branch = &candidate;
            best_score = score.value();
            ambiguous = false;
        }
        else if(score.value() == best_score)
        {
            ambiguous = true;
        }
    }

    return branch_selection{branch, ambiguous};
}

/**
 * Pass-through for format! macro arguments.
 *
 * The actual format string evaluation is deferred to
 * expand_late_macros, where type information is available.
 *
 * This simply concatenates the arguments as a comma-separated
 * expression list. The format_macro_expression::expand_late_macros
 * method will detect the format string and validate/evaluate it.
 */
token_stream build_format_pass_through(
  const std::vector<token_stream>& args,
  [[maybe_unused]] slang::source_location call_loc,
  [[maybe_unused]] slang::syntax_context_table& contexts,
  [[maybe_unused]] const slang::typing::context* type_ctx)
{
    // Validate format string argument exists.
    if(args.empty() || args[0].data().empty())
    {
        throw slang::macro::macro_error{
          "Macro 'format!': No format string provided."};
    }

    // Build a comma-separated expression list of all arguments.
    std::vector<slang::token> body;
    body.reserve(256);

    auto add = [&](const slang::token_stream& ts)
    {
        for(const auto& t: ts.data())
        {
            body.push_back(t);
        }
    };

    // Format string (first argument).
    add(args[0]);

    // Remaining arguments.
    for(std::size_t i = 1; i < args.size(); ++i)
    {
        body.emplace_back(slang::token{",", call_loc, slang::token_type::delimiter});
        add(args[i]);
    }

    return slang::token_stream{std::move(body)};
}

}    // namespace

namespace slang::macro
{

bool macro::supports_token_expansion(
  const std::vector<token_stream>& args) const
{
    // A macro-local placeholder can only be resolved by the legacy AST
    // expander at this stage. Keep this as an explicit capability gate rather
    // than allowing a partial token expansion to reach the parser.
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

    const auto selection = select_branch(desc.branches, args);

    return selection.branch != nullptr
           && !selection.ambiguous
           && has_token_expansion_body(*selection.branch);
}

token_stream macro::expand_macro(
  const std::vector<token_stream>& args,
  syntax_context_id call_site,
  syntax_context_table& contexts,
  source_location call_loc,
  const typing::context* type_ctx) const
{
    // Don't expand empty macros.
    if(desc.branches.empty())
    {
        return token_stream{};
    }

    // Nested macro placeholders still require the legacy AST substitution
    // path. Reject them here as well as in supports_token_expansion() so that
    // direct callers cannot accidentally consume a partial token expansion.
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

    const auto selection = select_branch(desc.branches, args);
    const auto* branch = selection.branch;

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

    if(!branch->body.has_value())
    {
        throw macro_error{
          std::format(
            "Macro '{}' branch has no token-expansion body.",
            name)};
    }

    if(!has_token_expansion_body(*branch))
    {
        throw macro_error{
          std::format(
            "Macro '{}' branch is not supported by token expansion.",
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

    // Special case: format! builtin. Detect the placeholder body and
    // produce a std::format_runtime() call instead of walking the body.
    if(branch->body.has_value()
       && !branch->body.value().data().empty()
       && branch->body.value().data()[0].s == "__format_builtin__")
    {
        return build_format_pass_through(args, call_loc, contexts, type_ctx);
    }

    /*
     * Fresh expansion context.
     *
     * The expansion happens at the call site, but identifiers originating
     * in the macro body retain the macro's definition-site ancestry.
     */
    syntax_context_id expansion_ctx = contexts.allocate(definition_context);

    // Process each token.
    for(const auto& tok: branch->body.value().data())
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
                        throw macro_error{
                          std::format(
                            "Macro '{}' is missing argument for '{}'.",
                            name,
                            param.name)};
                    }
                    result = result.concat(
                      retag(args[i], call_site));    // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
                    replaced = true;
                    break;
                }
            }
        }

        if(tok.type == token_type::macro_identifier && !replaced)
        {
            throw macro_error{
              std::format(
                "Macro '{}' references unknown parameter '{}'.",
                name,
                tok.s)};
        }

        if(!replaced)
        {
            // Keep definition-originating tokens in this expansion's context.
            token copy = tok;
            copy.context = expansion_ctx;
            result = result.append(copy);
        }
    }

    if(branch->body_is_expression
       && !result.empty()
       && result.back().s == ";")
    {
        std::vector<token> expression_tokens;
        expression_tokens.reserve(result.size() - 1);
        for(std::size_t i = 0; i + 1 < result.size(); ++i)
        {
            expression_tokens.emplace_back(result[i]);
        }
        return token_stream{std::move(expression_tokens)};
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

    // Idempotent: replace existing macro rather than throwing.
    // This supports format! migration where collect_macro() may
    // produce an empty descriptor that gets replaced by the
    // token-stream descriptor in the detection block.
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
