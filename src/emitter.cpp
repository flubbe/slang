/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <exception>
#include <unordered_map>

#include "codegen.h"
#include "emitter.h"
#include "module.h"
#include "opcodes.h"

namespace cg = slang::codegen;

namespace slang
{

/** Info about a variable mapping. */
struct variable_map_info
{
    /** The offset of the variable. */
    std::size_t offset;

    /** The size of the variable. */
    std::size_t size;
};

/** Helper function for opcode emission. */
inline void emit(archive& ar, opcode op)
{
    ar & op;
}

/** Helper function for opcode emission. */
template<typename T>
void emit(archive& ar, opcode op, T arg)
{
    ar & op;
    ar & arg;
}

void instruction_emitter::emit_instruction(const std::unique_ptr<slang::codegen::instruction>& instr)
{
    auto& name = instr->get_name();
    auto& args = instr->get_args();

    auto expect_arg_size = [&name, &args](std::size_t s1, std::optional<std::size_t> s2 = std::nullopt)
    {
        if(args.size() != s1)
        {
            if(!s2.has_value())
            {
                throw emitter_error(fmt::format("Expected {} arguments for '{}', got {}.", s1, name, args.size()));
            }

            if(args.size() != *s2)
            {
                throw emitter_error(fmt::format("Expected {} or {} arguments for '{}', got {}.", s1, *s2, name, args.size()));
            }
        }
    };

    if(name == "const")
    {
        expect_arg_size(1);

        slang::codegen::const_argument* arg = static_cast<slang::codegen::const_argument*>(args[0].get());
        const cg::value* v = arg->get_value();
        std::string type = v->get_resolved_type();

        if(type == "i32")
        {
            emit(instruction_buffer, opcode::iload, static_cast<const cg::constant_int*>(v)->get_int());
        }
        else if(type == "f32")
        {
            // TODO
            throw std::runtime_error(fmt::format("instruction_emitter::emit_instruction: instruction generation for '{}'/f32 not implemented.", name));
        }
        else if(type == "str")
        {
            // TODO
            throw std::runtime_error(fmt::format("instruction_emitter::emit_instruction: instruction generation for '{}'/str not implemented.", name));
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
        }
    }
    else if(name == "ret")
    {
        // the arguments are relevant for type checking, but can be ignored here.
        expect_arg_size(0, 1);
        emit(instruction_buffer, opcode::ret);
    }
    else
    {
        // TODO
        throw std::runtime_error(fmt::format("instruction_emitter::emit_instruction: instruction generation for '{}' not implemented.", name));
    }
}

void instruction_emitter::run()
{
    if(instruction_buffer.size() != 0)
    {
        throw emitter_error("Instruction buffer not empty.");
    }

    if(entry_points.size() != 0)
    {
        throw emitter_error("Entry points not empty.");
    }

    for(auto& f: ctx.funcs)
    {
        if(f->is_native())
        {
            // verify that no entry point for a native function exists.
            if(entry_points.find(f->get_name()) != entry_points.end())
            {
                throw emitter_error(fmt::format("Function '{}' already has an entry point.", f->get_name()));
            }

            continue;
        }

        // verify that an entry point for the function exists.
        if(entry_points.find(f->get_name()) != entry_points.end())
        {
            throw emitter_error(fmt::format("Function '{}' already has an entry point.", f->get_name()));
        }
        entry_points[f->get_name()] = instruction_buffer.tell();

        // allocate and map locals.
        auto& locals = f->get_scope()->get_locals();
        for(auto& it: locals)
        {
            // TODO
            throw std::runtime_error("instruction_emitter::run: variable mapping not implemented.");
        }

        // generate instructions.
        for(auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it.get_instructions())
            {
                emit_instruction(instr);
            }

            // TODO Jump offset resolution
        }
    }
}

language_module instruction_emitter::to_module() const
{
    language_module mod;

    for(auto& it: ctx.strings)
    {
        // TODO write strings.
        throw std::runtime_error("instruction_emitter::to_module: not implemented for strings.");
    }

    for(auto& it: ctx.types)
    {
        // TODO write type definitions.
        throw std::runtime_error("instruction_emitter::to_module: not implemented for type definitions.");
    }

    for(auto& it: ctx.funcs)
    {
        auto [return_type, arg_types] = it->get_signature();

        if(it->is_native())
        {
            mod.add_native_function(it->get_name(), std::move(return_type), std::move(arg_types), it->get_import_library());
        }
        else
        {
            auto ep = entry_points.find(it->get_name());
            if(ep == entry_points.end())
            {
                throw emitter_error(fmt::format("Unable to find entry point for function '{}'.", it->get_name()));
            }

            mod.add_function(it->get_name(), std::move(return_type), std::move(arg_types), ep->second);
        }
    }

    mod.set_binary(instruction_buffer.get_buffer());

    return mod;
}

}    // namespace slang