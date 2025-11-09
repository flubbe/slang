/**
 * slang - a simple scripting language.
 *
 * Simple control flow graph analysis.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "compiler/codegen/codegen.h"
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
      std::string,           /* origin block label */
      std::set<std::string>> /* transfer targets. */
      transfer_map;

    auto insert_unique_cfg_transfer =
      [&transfer_map](const std::string& origin, const std::string& target) -> void
    {
        auto [it, success] = transfer_map.try_emplace(origin, std::set{target});
        if(!success)
        {
            it->second.insert(target);
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

    // trace control flow and mark reachable blocks.
    std::set<std::string> active = {"entry"};
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

        auto it = transfer_map.find(label);
        if(it != transfer_map.end())
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

    // remove unreachable blocks.
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
