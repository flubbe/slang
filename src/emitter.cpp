/**
 * slang - a simple scripting language.
 *
 * instruction emitter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <set>
#include <stdexcept>
#include <unordered_map>

#include "codegen.h"
#include "emitter.h"
#include "module.h"
#include "opcodes.h"

namespace cg = slang::codegen;

namespace slang
{

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

std::set<std::string> instruction_emitter::collect_jump_targets() const
{
    std::set<std::string> targets;

    for(auto& f: ctx.funcs)
    {
        for(auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "jnz")
                {
                    auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());
                    targets.emplace(static_cast<cg::label_argument*>(args.at(1).get())->get_label());
                }
                else if(instr->get_name() == "jmp")
                {
                    auto& args = instr->get_args();
                    targets.emplace(static_cast<cg::label_argument*>(args.at(0).get())->get_label());
                }
            }
        }
    }

    return targets;
}

void instruction_emitter::collect_imports()
{
    for(auto& f: ctx.funcs)
    {
        for(auto& it: f->get_basic_blocks())
        {
            for(auto& instr: it->get_instructions())
            {
                if(instr->get_name() == "invoke")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(fmt::format("Expected 1 argument for 'invoke', got {}.", instr->get_args().size()));
                    }

                    cg::function_argument* arg = static_cast<cg::function_argument*>(instr->get_args()[0].get());
                    auto& import_path = arg->get_import_path();
                    if(import_path.has_value())
                    {
                        auto import_it = std::find_if(ctx.prototypes.begin(), ctx.prototypes.end(),
                                                      [arg, import_path](const std::unique_ptr<cg::prototype>& p) -> bool
                                                      {
                                                          return p->is_import() && *p->get_import_path() == import_path
                                                                 && p->get_name() == *arg->get_value()->get_name();
                                                      });
                        if(import_it == ctx.prototypes.end())
                        {
                            throw emitter_error(fmt::format("Could not resolve imported function '{}'.", *arg->get_value()->get_name()));
                        }

                        ctx.add_import(symbol_type::function, *(*import_it)->get_import_path(), (*import_it)->get_name());
                    }
                }
                else if(instr->get_name() == "new")
                {
                    if(instr->get_args().size() != 1)
                    {
                        throw emitter_error(fmt::format("Expected 1 argument for 'new', got {}.", instr->get_args().size()));
                    }

                    cg::type_argument* arg = static_cast<cg::type_argument*>(instr->get_args()[0].get());
                    auto& import_path = arg->get_import_path();
                    if(import_path.has_value())
                    {
                        auto import_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                                      [arg, import_path](const std::unique_ptr<cg::type>& p) -> bool
                                                      {
                                                          return p->is_import() && *p->get_import_path() == import_path
                                                                 && p->get_name() == *arg->get_value()->get_name();
                                                      });
                        if(import_it == ctx.types.end())
                        {
                            throw emitter_error(fmt::format("Could not resolve imported type '{}'.", *arg->get_value()->get_name()));
                        }

                        ctx.add_import(symbol_type::type, *(*import_it)->get_import_path(), (*import_it)->get_name());
                    }
                }
            }
        }
    }
}

void instruction_emitter::emit_instruction(const std::unique_ptr<cg::function>& func, const std::unique_ptr<cg::instruction>& instr)
{
    auto& name = instr->get_name();
    auto& args = instr->get_args();

    auto expect_arg_size = [&name, &args](std::size_t s1, std::optional<std::size_t> s2 = std::nullopt)
    {
        if(args.size() != s1)
        {
            if(!s2.has_value())
            {
                throw emitter_error(fmt::format("Expected {} argument(s) for '{}', got {}.", s1, name, args.size()));
            }

            if(args.size() != *s2)
            {
                throw emitter_error(fmt::format("Expected {} or {} argument(s) for '{}', got {}.", s1, *s2, name, args.size()));
            }
        }
    };

    auto emit_typed = [this, &name, &args, expect_arg_size](
                        opcode i32_opcode,
                        std::optional<opcode> f32_opcode = std::nullopt,
                        std::optional<opcode> str_opcode = std::nullopt,
                        std::optional<opcode> array_opcode = std::nullopt,
                        std::optional<opcode> ref_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::const_argument* arg = static_cast<cg::const_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        if(array_opcode.has_value() && v->is_array())
        {
            emit(instruction_buffer, *array_opcode);
            return;
        }

        std::string type = v->get_resolved_type();
        if(type == "i32")
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(type == "f32")
        {
            if(!f32_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *f32_opcode);
        }
        else if(type == "str")
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *str_opcode);
        }
        else if(type == "addr")
        {
            if(!ref_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
        }
    };

    auto emit_typed_one_arg = [this, &name, &args, expect_arg_size](opcode i32_opcode, opcode f32_opcode, std::optional<opcode> str_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::const_argument* arg = static_cast<cg::const_argument*>(args[0].get());
        const cg::value* v = arg->get_value();
        std::string type = v->get_resolved_type();

        if(type == "i32")
        {
            emit(instruction_buffer, i32_opcode, static_cast<std::int32_t>(static_cast<const cg::constant_int*>(v)->get_int()));
        }
        else if(type == "f32")
        {
            emit(instruction_buffer, f32_opcode, static_cast<const cg::constant_float*>(v)->get_float());
        }
        else if(type == "str")
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *str_opcode, vle_int{static_cast<const cg::constant_str*>(v)->get_constant_index()});
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
        }
    };

    auto emit_typed_one_var_arg =
      [this, &name, &args, &func, expect_arg_size](
        opcode i32_opcode,
        opcode f32_opcode,
        std::optional<opcode> str_opcode = std::nullopt,
        std::optional<opcode> array_opcode = std::nullopt,
        std::optional<opcode> ref_opcode = std::nullopt)
    {
        expect_arg_size(1);

        cg::variable_argument* arg = static_cast<cg::variable_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        if(!v->has_name())
        {
            throw std::runtime_error(fmt::format("Cannot emit load instruction: Value has no name."));
        }
        vle_int index = func->get_scope()->get_index(*v->get_name());

        std::string type = v->get_resolved_type();
        if(v->is_array())
        {
            if(!array_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *array_opcode);
        }
        else if(type == "i32")
        {
            emit(instruction_buffer, i32_opcode);
        }
        else if(type == "f32")
        {
            emit(instruction_buffer, f32_opcode);
        }
        else if(type == "str")
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *str_opcode);
        }
        else if(type == "addr")
        {
            if(!str_opcode.has_value())
            {
                throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
            }

            emit(instruction_buffer, *ref_opcode);
        }
        else
        {
            throw std::runtime_error(fmt::format("Invalid type '{}' for instruction '{}'.", type, name));
        }

        instruction_buffer & index;
    };

    if(name == "add")
    {
        emit_typed(opcode::iadd, opcode::fadd);
    }
    else if(name == "sub")
    {
        emit_typed(opcode::isub, opcode::fsub);
    }
    else if(name == "mul")
    {
        emit_typed(opcode::imul, opcode::fmul);
    }
    else if(name == "div")
    {
        emit_typed(opcode::idiv, opcode::fdiv);
    }
    else if(name == "mod")
    {
        emit_typed(opcode::imod);
    }
    else if(name == "const")
    {
        emit_typed_one_arg(opcode::iconst, opcode::fconst, opcode::sconst);
    }
    else if(name == "load")
    {
        emit_typed_one_var_arg(opcode::iload, opcode::fload, opcode::sload, opcode::aload);
    }
    else if(name == "store")
    {
        emit_typed_one_var_arg(opcode::istore, opcode::fstore, opcode::sstore, opcode::astore, opcode::astore);
    }
    else if(name == "load_element")
    {
        emit_typed(opcode::iaload, opcode::faload, opcode::saload);
    }
    else if(name == "store_element")
    {
        emit_typed(opcode::iastore, opcode::fastore, opcode::sastore);
    }
    else if(name == "dup")
    {
        emit_typed(opcode::idup, opcode::fdup, std::nullopt, opcode::adup, opcode::adup);
    }
    else if(name == "pop")
    {
        emit_typed(opcode::pop, opcode::pop, opcode::spop, opcode::apop);    // same instruction for i32 and f32.
    }
    else if(name == "cast")
    {
        expect_arg_size(1);
        cg::cast_argument* arg = static_cast<cg::cast_argument*>(args[0].get());
        if(arg->get_cast() == "i32_to_f32")
        {
            emit(instruction_buffer, opcode::i2f);
        }
        else if(arg->get_cast() == "f32_to_i32")
        {
            emit(instruction_buffer, opcode::f2i);
        }
        else
        {
            throw emitter_error(fmt::format("Invalid cast type '{}'.", arg->get_cast()));
        }
    }
    else if(name == "invoke")
    {
        expect_arg_size(1);
        cg::function_argument* arg = static_cast<cg::function_argument*>(args[0].get());
        const cg::value* v = arg->get_value();

        auto& import_path = arg->get_import_path();
        if(!import_path.has_value())
        {
            // resolve module-local functions.
            auto it = std::find_if(ctx.funcs.begin(), ctx.funcs.end(),
                                   [&v](const std::unique_ptr<cg::function>& f) -> bool
                                   {
                                       return f->get_name() == *v->get_name();
                                   });
            if(it == ctx.funcs.end())
            {
                throw emitter_error(fmt::format("Could not resolve module-local function '{}'.", *v->get_name()));
            }

            vle_int index = it - ctx.funcs.begin();
            emit(instruction_buffer, opcode::invoke);
            instruction_buffer & index;
            return;
        }

        // resolve imports.
        auto import_it = std::find_if(ctx.prototypes.begin(), ctx.prototypes.end(),
                                      [arg, import_path](const std::unique_ptr<cg::prototype>& p) -> bool
                                      {
                                          return p->is_import() && *p->get_import_path() == import_path
                                                 && p->get_name() == *arg->get_value()->get_name();
                                      });
        if(import_it == ctx.prototypes.end())
        {
            throw emitter_error(fmt::format("Could not resolve imported function '{}'.", *arg->get_value()->get_name()));
        }

        vle_int index = -ctx.get_import_index(symbol_type::function, *(*import_it)->get_import_path(), (*import_it)->get_name()) - 1;
        emit(instruction_buffer, opcode::invoke);
        instruction_buffer & index;
    }
    else if(name == "ret")
    {
        expect_arg_size(1);

        if(args[0]->get_value()->get_resolved_type() == "void")
        {
            // return void from a function.
            emit(instruction_buffer, opcode::ret);
        }
        else
        {
            emit_typed(opcode::iret, opcode::fret, opcode::sret, opcode::aret);
        }
    }
    else if(name == "set_field")
    {
        expect_arg_size(1);

        cg::field_access_argument* arg = static_cast<cg::field_access_argument*>(args[0].get());

        // resolve references to struct and field name.
        vle_int struct_index = 0;
        vle_int field_index = 0;

        auto struct_it = std::find_if(ctx.types.begin(), ctx.types.end(),
                                      [arg](const std::unique_ptr<cg::type>& t) -> bool
                                      {
                                          return t->get_name() == arg->get_struct_name();
                                      });
        if(struct_it != ctx.types.end())
        {
            struct_index = std::distance(ctx.types.begin(), struct_it);
        }
        else
        {
            // TODO search imported symbols. The type should have `import_path` set in this case.
            throw emitter_error(fmt::format("Type not found / type import resolution not implemented (type name: '{}').", arg->get_struct_name()));
        }

        auto& members = struct_it->get()->get_members();
        auto field_it = std::find_if(members.begin(), members.end(),
                                     [arg](const std::pair<std::string, cg::value>& m) -> bool
                                     {
                                         return m.first == *arg->get_member().get_name();
                                     });
        if(field_it == members.end())
        {
            throw emitter_error(fmt::format("Could not resolve field '{}' in struct '{}'.", *arg->get_member().get_name(), arg->get_struct_name()));
        }

        emit(instruction_buffer, opcode::setfield);
        instruction_buffer & struct_index;
        instruction_buffer & field_index;
    }
    else if(name == "and")
    {
        emit_typed(opcode::iand);
    }
    else if(name == "land")
    {
        emit_typed(opcode::land);
    }
    else if(name == "or")
    {
        emit_typed(opcode::ior);
    }
    else if(name == "lor")
    {
        emit_typed(opcode::lor);
    }
    else if(name == "xor")
    {
        emit_typed(opcode::ixor);
    }
    else if(name == "shl")
    {
        emit_typed(opcode::ishl);
    }
    else if(name == "shr")
    {
        emit_typed(opcode::ishr);
    }
    else if(name == "cmpl")
    {
        emit_typed(opcode::icmpl, opcode::fcmpl);
    }
    else if(name == "cmple")
    {
        emit_typed(opcode::icmple, opcode::fcmple);
    }
    else if(name == "cmpg")
    {
        emit_typed(opcode::icmpg, opcode::fcmpg);
    }
    else if(name == "cmpge")
    {
        emit_typed(opcode::icmpge, opcode::fcmpge);
    }
    else if(name == "cmpeq")
    {
        emit_typed(opcode::icmpeq, opcode::fcmpeq);
    }
    else if(name == "cmpne")
    {
        emit_typed(opcode::icmpne, opcode::fcmpne);
    }
    else if(name == "jnz")
    {
        expect_arg_size(2);

        auto& args = instr->get_args();

        auto then_label = static_cast<cg::label_argument*>(args[0].get())->get_label();
        auto else_label = static_cast<cg::label_argument*>(args[1].get())->get_label();

        auto then_it = jump_targets.find(then_label);
        if(then_it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", then_label));
        }

        auto else_it = jump_targets.find(else_label);
        if(else_it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", else_label));
        }

        vle_int then_index = std::distance(jump_targets.begin(), then_it);
        vle_int else_index = std::distance(jump_targets.begin(), else_it);

        emit(instruction_buffer, opcode::jnz);
        instruction_buffer & then_index;
        instruction_buffer & else_index;
    }
    else if(name == "jmp")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto label = static_cast<cg::label_argument*>(args[0].get())->get_label();

        auto it = jump_targets.find(label);
        if(it == jump_targets.end())
        {
            throw emitter_error(fmt::format("Cannot find label '{}'.", label));
        }

        vle_int index = std::distance(jump_targets.begin(), it);
        emit(instruction_buffer, opcode::jmp);
        instruction_buffer & index;
    }
    else if(name == "new")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto type_str = static_cast<cg::type_argument*>(args[0].get())->get_value()->get_resolved_type();

        // resolve type to index. first check local types, then imported types.
        vle_int index = 0;

        auto it = std::find_if(ctx.types.begin(), ctx.types.end(),
                               [&type_str](const std::unique_ptr<cg::type>& t) -> bool
                               {
                                   return t->get_name() == type_str;
                               });
        if(it != ctx.types.end())
        {
            index = std::distance(ctx.types.begin(), it);
        }
        else
        {
            // TODO search imported symbols. The type should have `import_path` set in this case.
            throw emitter_error(fmt::format("Type not found / type import resolution not implemented (type name: '{}').", type_str));
        }

        emit(instruction_buffer, opcode::new_);
        instruction_buffer & index;
    }
    else if(name == "newarray")
    {
        expect_arg_size(1);

        auto& args = instr->get_args();
        auto type_str = static_cast<cg::type_argument*>(args[0].get())->get_value()->get_type();

        array_type type;
        if(type_str == "i32")
        {
            type = array_type::i32;
        }
        else if(type_str == "f32")
        {
            type = array_type::f32;
        }
        else if(type_str == "str")
        {
            type = array_type::str;
        }
        else if(type_str == "aggregate")
        {
            type = array_type::ref;
        }
        else
        {
            throw std::runtime_error(fmt::format("Unknown array type '{}' for newarray.", type_str));
        }

        emit(instruction_buffer, opcode::newarray);
        instruction_buffer & type;
    }
    else if(name == "arraylength")
    {
        emit(instruction_buffer, opcode::arraylength);
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

    if(func_details.size() != 0)
    {
        throw emitter_error("Entry points not empty.");
    }

    if(jump_targets.size() != 0)
    {
        throw emitter_error("Jump targets not empty.");
    }

    // collect jump targets.
    jump_targets = collect_jump_targets();

    // collect imports.
    collect_imports();

    // the import count is not allowed to change, so store it here and check later.
    std::size_t import_count = ctx.imports.size();

    /*
     * generate bytecode.
     */
    for(auto& f: ctx.funcs)
    {
        // verify that no entry point for the function exists.
        if(func_details.find(f->get_name()) != func_details.end())
        {
            throw emitter_error(fmt::format("Function '{}' already has an entry point.", f->get_name()));
        }

        // skip native functions.
        if(f->is_native())
        {
            continue;
        }

        /*
         * allocate and map locals.
         */
        auto* s = f->get_scope();
        auto& func_args = s->get_args();
        auto& func_locals = f->get_scope()->get_locals();
        const std::size_t local_count = func_args.size() + func_locals.size();

        std::vector<variable> locals{local_count};

        std::set<std::size_t> unset_indices;
        for(std::size_t i = 0; i < local_count; ++i)
        {
            unset_indices.insert(unset_indices.end(), i);
        }

        for(auto& it: func_args)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(fmt::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(*it->get_name());
            if(unset_indices.find(index) == unset_indices.end())
            {
                throw emitter_error(fmt::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            locals[index] = {it->get_type(), it->is_array()};
        }

        for(auto& it: func_locals)
        {
            auto name = it->get_name();
            if(!name.has_value())
            {
                throw emitter_error(fmt::format("Unnamed variable in function '{}'.", f->get_name()));
            }

            std::size_t index = s->get_index(*it->get_name());
            if(unset_indices.find(index) == unset_indices.end())
            {
                throw emitter_error(fmt::format("Tried to map local '{}' with index '{}' multiple times.", *name, index));
            }
            unset_indices.erase(index);

            locals[index] = {it->get_type(), it->is_array()};
        }

        if(unset_indices.size() != 0)
        {
            throw emitter_error(fmt::format("Inconsistent local count for function '{}'.", f->get_name()));
        }

        /*
         * instruction generation.
         */
        std::size_t entry_point = instruction_buffer.tell();

        for(auto& it: f->get_basic_blocks())
        {
            auto jump_it = jump_targets.find(it->get_label());
            if(jump_it != jump_targets.end())
            {
                vle_int index = std::distance(jump_targets.begin(), jump_it);
                emit(instruction_buffer, opcode::label);
                instruction_buffer & index;
            }

            for(auto& instr: it->get_instructions())
            {
                emit_instruction(f, instr);
            }
        }

        /*
         * store function details
         */
        std::size_t size = instruction_buffer.tell() - entry_point;
        func_details.insert({f->get_name(), {size, entry_point, locals}});
    }

    // check that the import count did not change
    if(import_count != ctx.imports.size())
    {
        throw emitter_error(fmt::format("Import count changed during instruction emission ({} -> {}).", import_count, ctx.imports.size()));
    }
}

language_module instruction_emitter::to_module() const
{
    language_module mod;

    // imports.
    std::vector<std::string> packages;
    for(const auto& it: ctx.imports)
    {
        if(it.type == symbol_type::package)
        {
            auto p_it = std::find(packages.begin(), packages.end(), it.name);
            if(p_it != packages.end())
            {
                packages.erase(p_it);
            }
        }
        else
        {
            // ensure we import the symbol's package.
            auto p_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                     [&it](auto& s) -> bool
                                     {
                                         return s.type == symbol_type::package && s.name == it.import_path;
                                     });
            if(p_it == ctx.imports.end())
            {
                auto p_it = std::find(packages.begin(), packages.end(), it.name);
                if(p_it == packages.end())
                {
                    packages.push_back(it.import_path);
                }
            }
        }
    }

    for(const auto& it: ctx.imports)
    {
        std::uint32_t package_index = 0;

        auto import_it = std::find_if(ctx.imports.begin(), ctx.imports.end(),
                                      [&it](auto& s) -> bool
                                      {
                                          return s.type == symbol_type::package && s.name == it.import_path;
                                      });
        if(import_it != ctx.imports.end())
        {
            package_index = std::distance(ctx.imports.begin(), import_it);
        }
        else
        {
            auto package_it = std::find(packages.begin(), packages.end(), it.import_path);
            if(package_it == packages.end())
            {
                throw std::runtime_error(fmt::format("Package '{}' not found in package table.", it.import_path));
            }
            package_index = ctx.imports.size() + std::distance(packages.begin(), package_it);
        }

        mod.add_import(it.type, it.name, package_index);
    }
    for(const auto& it: packages)
    {
        mod.add_import(symbol_type::package, it);
    }

    // strings.
    mod.set_string_table(ctx.strings);

    // types.
    for(auto& it: ctx.types)
    {
        auto members = it->get_members();
        std::vector<std::pair<std::string, type_info>> transformed_members;

        std::transform(members.cbegin(), members.cend(), std::back_inserter(transformed_members),
                       [](const std::pair<std::string, cg::value>& m) -> std::pair<std::string, type_info>
                       {
                           const auto& t = std::get<1>(m);
                           return std::make_pair(std::get<0>(m), type_info{t.get_resolved_type(), t.is_array()});
                       });

        mod.add_type(it->get_name(), std::move(transformed_members));
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
            auto details = func_details.find(it->get_name());
            if(details == func_details.end())
            {
                throw emitter_error(fmt::format("Unable to find entry point for function '{}'.", it->get_name()));
            }

            mod.add_function(it->get_name(), std::move(return_type), std::move(arg_types), details->second.size, details->second.offset, details->second.locals);
        }
    }

    mod.set_binary(instruction_buffer.get_buffer());

    return mod;
}

}    // namespace slang