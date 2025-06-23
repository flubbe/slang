/**
 * slang - a simple scripting language.
 *
 * Simple control flow graph analysis.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <print>

#include "compiler/codegen.h"
#include "cfg.h"

namespace slang::opt::cfg
{

/*
 * context.
 */

void context::run_on_function(cg::function& func)
{
    // collect all control flow transfers.
    std::unordered_map<
      std::string,              /* origin block label */
      std::vector<std::string>> /* transfer targets. */
      transfer_map;

    auto insert_unique_cfg_transfer =
      [&transfer_map](const std::string& origin, const std::string& target) -> void
    {
        if(transfer_map.contains(origin))
        {
            auto& targets_vector = transfer_map[origin];
            if(std::ranges::find(targets_vector, target) == targets_vector.end())
            {
                targets_vector.emplace_back(target);
            }
        }
        else
        {
            transfer_map.insert({origin, {target}});
        }
    };

    for(auto it = func.get_basic_blocks().begin(); it != func.get_basic_blocks().end(); ++it)
    {
        for(auto& instr: (*it)->get_instructions())
        {
            if(instr->get_name() == "jnz")
            {
                const auto& args = instr->get_args();
                insert_unique_cfg_transfer(
                  (*it)->get_label(),
                  static_cast<cg::label_argument*>(args.at(0).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                insert_unique_cfg_transfer(
                  (*it)->get_label(),
                  static_cast<cg::label_argument*>(args.at(1).get())->get_label());    // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
            }
            else if(instr->get_name() == "jmp")
            {
                const auto& args = instr->get_args();
                insert_unique_cfg_transfer(
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

            insert_unique_cfg_transfer(
              (*it)->get_label(),
              (*next_it)->get_label());
        }
    }

    // trace control flow and mark unreachable blocks.
    std::vector<std::string> active = {"entry"};
    std::vector<std::string> marked;

    while(!active.empty())
    {
        auto label = active.back();
        active.pop_back();

        // don't add nodes twice. this ensures loop termination.
        if(std::ranges::find(marked, label) != marked.end())
        {
            continue;
        }

        marked.push_back(label);

        auto it = transfer_map.find(label);
        if(it != transfer_map.end())
        {
            for(const auto& target: it->second)
            {
                // a marked node cannot become active again.
                if(std::ranges::find(marked, target) != marked.end())
                {
                    continue;
                }

                // don't add nodes twice.
                if(std::ranges::find(active, target) == active.end())
                {
                    active.push_back(target);
                }
            }
        }
    }

    // remove unmarked blocks.
    std::vector<std::string> unmarked;

    for(const auto& it: func.get_basic_blocks())
    {
        if(std::ranges::find(marked, it->get_label()) == marked.end())
        {
            unmarked.push_back(it->get_label());
        }
    }

    for(const auto& label: unmarked)
    {
        func.remove_basic_block(label);
    }
}

void context::run()
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

}    // namespace slang::opt::cfg
