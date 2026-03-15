/**
 * slang - a simple scripting language.
 *
 * Control flow analysis and simplification.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/codegen/codegen.h"
#include "simplify.h"

namespace slang::cfg
{

/*
 * helpers.
 */

struct cfg_info
{
    /** Predecessor blocks for each block. */
    std::unordered_map<std::string, std::set<std::string>> preds;

    /** Successor blocks for each block. */
    std::unordered_map<std::string, std::set<std::string>> succs;
};

/**
 * Build the control flow info map for a function's basic blocks.
 *
 * @param func The function to build the map for.
 * @returns Returns the control flow info by mapping origin block
 *          labels to transfer target block labels.
 */
static cfg_info build_cfg_info(const cg::function& func)
{
    cfg_info info;

    auto insert_edge =
      [&info](const std::string& origin, const std::string& target) -> void
    {
        {
            auto [entry, success] = info.succs.try_emplace(origin, std::set{target});
            if(!success)
            {
                entry->second.insert(target);
            }
        }

        {
            auto [entry, success] = info.preds.try_emplace(target, std::set{origin});
            if(!success)
            {
                entry->second.insert(origin);
            }
        }
    };

    for(auto it = func.get_basic_blocks().begin(); it != func.get_basic_blocks().end(); ++it)
    {
        for(auto& instr: (*it)->get_instructions())
        {
            if(instr->get_name() == "jnz")
            {
                const auto& args = instr->get_args();
                insert_edge(
                  (*it)->get_label(),
                  static_cast<cg::label_argument*>(args.at(0).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                insert_edge(
                  (*it)->get_label(),
                  static_cast<cg::label_argument*>(args.at(1).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
            else if(instr->get_name() == "jmp")
            {
                const auto& args = instr->get_args();
                insert_edge(
                  (*it)->get_label(),
                  static_cast<cg::label_argument*>(args.at(0).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
        }

        if(!(*it)->is_terminated())
        {
            // fall-through case.
            auto next_it = std::next(it);
            if(next_it == func.get_basic_blocks().end())
            {
                throw cg::codegen_error(
                  std::format(
                    "'{}': Unexpected function end.",
                    func.get_name()));
            }

            insert_edge(
              (*it)->get_label(),
              (*next_it)->get_label());
        }
    }

    return info;
}

/**
 * Compute reachable blocks given a set of roots and a transfer map.
 *
 * @param info CFG info map encoding the reachability information.
 * @param entry Entry label.
 * @returns Returns all blocks/labels reachable from the entry label.
 */
static std::set<std::string>
  compute_reachable(
    const cfg_info& info,
    std::string_view entry)
{
    std::set<std::string> active = {std::string{entry}};
    std::set<std::string> reachable;

    while(!active.empty())
    {
        auto label = std::move(active.extract(active.begin()).value());

        // don't process nodes twice. this ensures loop termination.
        if(reachable.contains(label))
        {
            continue;
        }

        reachable.emplace(label);

        auto it = info.succs.find(label);
        if(it != info.succs.end())
        {
            for(const auto& target: it->second)
            {
                // a reachable node cannot become active again.
                if(reachable.contains(target))
                {
                    continue;
                }

                // don't add nodes twice.
                if(!active.contains(target))
                {
                    active.emplace(target);
                }
            }
        }
    }

    return reachable;
}

/**
 * Removes all unreachable basic blocks.
 *
 * @param func The functions containing the blocks.
 * @param reachable Set of reachable blocks.
 */
static void remove_unreachable_blocks(
  cg::function& func,
  const std::set<std::string>& reachable)
{
    std::set<std::string> unreachable;

    for(const auto& it: func.get_basic_blocks())
    {
        if(!reachable.contains(it->get_label()))
        {
            unreachable.emplace(it->get_label());
        }
    }

    for(const auto& label: unreachable)
    {
        func.remove_basic_block(label);
    }
}

/**
 * Remove empty basic blocks.
 *
 * @param func The functions containing the blocks.
 */
static void remove_empty_blocks(
  cg::function& func)
{
    std::set<std::string> empty;
    for(const auto& it: func.get_basic_blocks())
    {
        if(it->get_instructions().empty())
        {
            empty.emplace(it->get_label());
        }
    }

    for(const auto& label: empty)
    {
        auto info = build_cfg_info(func);

        auto succs = info.succs.find(label);
        if(succs == info.succs.end()
           || succs->second.empty())
        {
            throw cg::codegen_error(
              std::format(
                "'{}': Empty block '{}' has no successor.",
                func.get_name(),
                label));
        }
        if(succs->second.size() > 1)
        {
            throw cg::codegen_error(
              std::format(
                "'{}': Empty block '{}' has multiple successors.",
                func.get_name(),
                label));
        }
        const auto& succ_label = *succs->second.begin();

        // rewire the predecessor blocks.
        auto preds = info.preds.find(label);
        if(preds == info.preds.end())
        {
            // first block has no predecessor.
            if(label != func.get_basic_blocks().front()->get_label())
            {
                throw cg::codegen_error(
                  std::format(
                    "'{}': Block '{}' has no predecessor.",
                    func.get_name(),
                    label));
            }
        }
        else
        {
            for(const auto& pred_block_label: preds->second)
            {
                auto* block = func.get_basic_block(pred_block_label);
                if(!block->is_terminated())
                {
                    // Fall-through block. Does not need to be rewired.
                    continue;
                }

                auto& branch_instr = *block->get_instructions().back();
                auto& args = branch_instr.get_args();

                if(branch_instr.get_name() == "jmp")
                {
                    static_cast<cg::label_argument*>(args.at(0).get())->set_label(succ_label);
                }
                else if(branch_instr.get_name() == "jnz")
                {
                    auto arg0 = static_cast<cg::label_argument*>(args.at(0).get());
                    if(arg0->get_label() == label)
                    {
                        arg0->set_label(succ_label);
                    }

                    auto arg1 = static_cast<cg::label_argument*>(args.at(1).get());
                    if(arg1->get_label() == label)
                    {
                        arg1->set_label(succ_label);
                    }
                }
                else
                {
                    throw cg::codegen_error(
                      std::format(
                        "'{}': Block '{}' has unknown terminating instruction '{}'.",
                        func.get_name(),
                        label,
                        branch_instr.get_name()));
                }
            }
        }

        // remove block.
        func.remove_basic_block(label);
    }
}

/*
 * simplification context.
 */

void simplify::run_on_function(cg::function& func)
{
    // collect all control flow transfers.
    auto info = build_cfg_info(func);

    std::set<std::string> reachable =
      compute_reachable(
        info,
        func.get_basic_blocks().front()->get_label());

    remove_unreachable_blocks(
      func,
      reachable);

    remove_empty_blocks(func);
}

void simplify::run()
{
    for(auto& func: ctx.funcs)
    {
        if(func->is_native())
        {
            continue;
        }

        run_on_function(*func);
    }
}

}    // namespace slang::cfg
