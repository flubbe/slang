/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <format>
#include <print>

#include "archives/memory.h"
#include "shared/module.h"
#include "shared/opcodes.h"
#include "interpreter.h"
#include "package.h"
#include "utils.h"
#include "vector.h"

#ifdef INTERPRETER_DEBUG
#    define DEBUG_LOG(...) std::print("INT: {}\n", std::format(__VA_ARGS__))
#else
#    define DEBUG_LOG(...)
#endif

namespace slang::interpreter
{

/*
 * Verify size assumptions for arrays.
 */

static_assert(sizeof(fixed_vector<std::int32_t>) == sizeof(void*));
static_assert(sizeof(fixed_vector<float>) == sizeof(void*));
static_assert(sizeof(fixed_vector<std::string*>) == sizeof(void*));
static_assert(sizeof(fixed_vector<void*>) == sizeof(void*));

/**
 * Generate the return opcode from the signature's return type for native functions.
 *
 * @note Only used for preparing handover of return values to native code.
 * @param return_type The return type, given as `(name, is_array)`.
 * @returns A return opcode.
 * @throws Throws an `interpreter_error` if the `return_type` is invalid.
 */
static opcode get_return_opcode(const module_::variable_type& return_type)
{
    if(return_type.is_array())
    {
        return opcode::aret;
    }

    if(return_type.base_type() == "void")
    {
        return opcode::ret;
    }

    if(return_type.base_type() == "i32")
    {
        return opcode::iret;
    }

    if(return_type.base_type() == "f32")
    {
        return opcode::fret;
    }

    if(return_type.base_type() == "str")
    {
        return opcode::sret;
    }

    if(return_type.base_type() == "@addr" || return_type.base_type() == "@array")
    {
        return opcode::aret;
    }

    // FIXME Assume all other types are references.
    return opcode::aret;
}

/*
 * function implementation.
 */

function::function(
  context& ctx,
  module_loader& loader,
  module_::function_signature signature,
  std::size_t entry_point,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t size,
  std::vector<module_::variable_descriptor> locals,
  std::size_t locals_size,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t stack_size)
: ctx{ctx}
, loader{loader}
, signature{std::move(signature)}
, native{false}
, entry_point_or_function{entry_point}
, size{size}
, ret_opcode{::slang::interpreter::get_return_opcode(this->signature.return_type)}
, locals{std::move(locals)}
, locals_size{locals_size}
, stack_size{stack_size}
{
}

function::function(
  context& ctx,
  module_loader& loader,
  module_::function_signature signature,
  std::function<void(operand_stack&)> func)
: ctx{ctx}
, loader{loader}
, signature{std::move(signature)}
, native{true}
, entry_point_or_function{std::move(func)}
, size{0}
, ret_opcode{::slang::interpreter::get_return_opcode(this->signature.return_type)}
{
}

value function::invoke(const std::vector<value>& args)
{
    return ctx.invoke(
      loader,
      *this,
      args);
}

/*
 * context implementation.
 */

std::function<void(operand_stack&)> context::resolve_native_function(
  const std::string& name,
  const std::string& library_name) const
{
    auto mod_it = native_function_map.find(library_name);
    if(mod_it == native_function_map.end())
    {
        throw interpreter_error(std::format("Cannot resolve native function '{}' in '{}' (library not found).", name, library_name));
    }

    auto func_it = mod_it->second.find(name);
    if(func_it == mod_it->second.end())
    {
        throw interpreter_error(std::format("Cannot resolve native function '{}' in '{}' (function not found).", name, library_name));
    }

    DEBUG_LOG("resolved imported native function '{}.{}'.", library_name, name);

    return func_it->second;
}

/** Handle local variable construction and destruction. */
class locals_scope
{
    /** The associated interpreter context. */
    context& ctx;

    /** Locals. */
    const std::vector<module_::variable_descriptor>& locals;

    /** The function's stack frame. */
    stack_frame& frame;

    /** Whether to destruct the locals. */
    bool needs_destruct{true};

public:
    /** Deleted constructors. */
    locals_scope() = delete;
    locals_scope(const locals_scope&) = delete;
    locals_scope(locals_scope&&) = delete;

    /**
     * Create a scope for creating/destroying variables. If needed, adds the locals as
     * roots to the garbage collector.
     *
     * @param ctx The interpreter context.
     * @param locals The locals.
     * @param frame The stack frame.
     */
    locals_scope(context& ctx, const std::vector<module_::variable_descriptor>& locals, stack_frame& frame)
    : ctx{ctx}
    , locals{locals}
    , frame{frame}
    {
        for(const auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(static_cast<void*>(&addr), &frame.locals[local.offset], sizeof(void*));
                if(addr != nullptr)
                {
                    ctx.get_gc().add_root(addr);
                    // FIXME We (likely) want to remove the temporaries from the GC here,
                    //       instead of at the call site (see comment there).
                }
            }
        }
    }

    /** Destructor. Calls `destruct`, if needed. */
    ~locals_scope()
    {
        if(needs_destruct)
        {
            try
            {
                destruct();
            }
            catch(const interpreter_error& e)
            {
                // FIXME This is an error, but destructors cannot throw.

                DEBUG_LOG("Error during locals destruction: {}", e.what());
            }
        }
    }

    /** Deleted assignment operators. */
    locals_scope& operator=(const locals_scope&) = delete;
    locals_scope& operator=(locals_scope&&) = delete;

    /**
     * Remove locals from the garbage collector, if needed.
     *
     * @throws Throws `interpreter_error` if the caller tried to destruct the locals more than once.
     */
    void destruct()
    {
        if(!needs_destruct)
        {
            throw interpreter_error("Local destructors called multiple times.");
        }
        needs_destruct = false;

        for(const auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(static_cast<void*>(&addr), &frame.locals[local.offset], sizeof(void*));
                if(addr != nullptr)
                {
                    ctx.get_gc().remove_root(addr);
                }
            }
        }
    }
};

/**
 * Read a value from the binary without bounds check.
 *
 * @param binary The binary data.
 * @param offset The offset into the binary to read from. Holds the new offset after the read.
 * @returns Returns the read value.
 */
template<typename T>
static T read_unchecked(const std::vector<std::byte>& binary, std::size_t& offset)
{
    T v;
    std::memcpy(&v, &binary[offset], sizeof(T));
    offset += sizeof(T);
    return v;
}

opcode context::exec(
  const module_loader& loader,
  std::size_t entry_point,    // NOLINT(bugprone-easily-swappable-parameters)
  std::size_t size,
  const std::vector<module_::variable_descriptor>& locals,
  stack_frame& frame)
{
    std::size_t offset = entry_point;

    try
    {
        ++call_stack_level;
        if(call_stack_level > max_call_stack_depth)
        {
            throw interpreter_error(
              std::format(
                "Function call exceeded maximum call stack depth ({}).",
                max_call_stack_depth));
        }

        const std::vector<std::byte>& binary = loader.get_module().get_binary();
        if(offset >= binary.size())
        {
            throw interpreter_error(
              std::format(
                "Entry point is outside the loaded code segment ({} >= {}).",
                offset,
                binary.size()));
        }

        locals_scope ls{*this, locals, frame};

        const std::size_t function_end = offset + size;
        while(offset < function_end)
        {
            auto instr = binary[offset];
            ++offset;

            // validate opcode.
            if(instr >= static_cast<std::byte>(opcode::opcode_count))
            {
                throw interpreter_error(
                  std::format(
                    "'{:02x}' is not an opcode.",
                    std::to_integer<std::uint8_t>(instr)));
            }
            auto instr_opcode = static_cast<opcode>(instr);

            // return.
            if(instr >= static_cast<std::byte>(opcode::ret) && instr <= static_cast<std::byte>(opcode::aret))
            {
                // NOTE The stack size is validated by the caller.

                // destruct the locals here for the GC to clean them up.
                ls.destruct();

                // run garbage collector.
                gc.run();

                --call_stack_level;
                return instr_opcode;
            }

            if(offset == function_end)
            {
                throw interpreter_error(std::format("Execution reached function boundary."));
            }

            switch(instr_opcode)
            {
            case opcode::idup: [[fallthrough]];
            case opcode::fdup:
            {
                frame.stack.dup_i32();
                break;
            } /* opcode::idup, opcode::fdup */
            case opcode::adup:
            {
                frame.stack.dup_addr();
                void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(static_cast<void*>(&addr), frame.stack.end(sizeof(void*)), sizeof(void*));
                gc.add_temporary(addr);
                break;
            } /* opcode::adup */
            case opcode::dup_x1:
            {
                auto size1 = read_unchecked<std::size_t>(binary, offset);
                auto size2 = read_unchecked<std::size_t>(binary, offset);
                auto needs_gc = read_unchecked<std::uint8_t>(binary, offset);
                frame.stack.dup_x1(size1, size2);

                if(needs_gc != 0)
                {
                    void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                    std::memcpy(static_cast<void*>(&addr), frame.stack.end((2 * size1) + size2), size1);
                    gc.add_temporary(addr);
                }
                break;
            } /* opcode::dup_x1 */
            case opcode::dup_x2:
            {
                auto size1 = read_unchecked<std::size_t>(binary, offset);
                auto size2 = read_unchecked<std::size_t>(binary, offset);
                auto size3 = read_unchecked<std::size_t>(binary, offset);
                auto needs_gc = read_unchecked<std::uint8_t>(binary, offset);
                frame.stack.dup_x2(size1, size2, size3);

                if(needs_gc != 0)
                {
                    void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                    std::memcpy(static_cast<void*>(&addr), frame.stack.end((2 * size1) + size2 + size3), size1);
                    gc.add_temporary(addr);
                }
                break;
            } /* opcode::dup_x2 */
            case opcode::pop:
            {
                frame.stack.pop_i32();
                break;
            } /* opcode::pop */
            case opcode::apop:
            {
                gc.remove_temporary(frame.stack.pop_addr<void>());
                break;
            } /* opcode::apop */
            case opcode::iadd:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i + v; });
                break;
            } /* opcode::iadd */
            case opcode::isub:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i - v; });
                break;
            } /* opcode::isub */
            case opcode::imul:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i * v; });
                break;
            } /* opcode::imul */
            case opcode::idiv:
            {
                std::int32_t divisor = frame.stack.pop_i32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [divisor](std::int32_t dividend) -> std::int32_t
                  { return dividend / divisor; });
                break;
            } /* opcode::idiv */
            case opcode::imod:
            {
                std::int32_t divisor = frame.stack.pop_i32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [divisor](std::int32_t dividend) -> std::int32_t
                  { return dividend % divisor; });
                break;
            } /* opcode::imod */
            case opcode::fadd:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>(
                  [v](float i) -> float
                  { return i + v; });
                break;
            } /* opcode::fadd */
            case opcode::fsub:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>(
                  [v](float i) -> float
                  { return i - v; });
                break;
            } /* opcode::fsub */
            case opcode::fmul:
            {
                float v = frame.stack.pop_f32();
                frame.stack.modify_top<float, float>(
                  [v](float i) -> float
                  { return i * v; });
                break;
            } /* opcode::fmul */
            case opcode::fdiv:
            {
                float divisor = frame.stack.pop_f32();
                if(divisor == 0)
                {
                    throw interpreter_error("Division by zero.");
                }
                frame.stack.modify_top<float, float>(
                  [divisor](float dividend) -> float
                  { return dividend / divisor; });
                break;
            } /* opcode::fdiv */
            case opcode::i2f:
            {
                frame.stack.modify_top<std::int32_t, float>(
                  [](const std::int32_t i) -> float
                  { return static_cast<float>(i); });
                break;
            } /* opcode::i2f */
            case opcode::f2i:
            {
                frame.stack.modify_top<float, std::int32_t>(
                  [](const float f) -> std::int32_t
                  { return static_cast<std::int32_t>(f); });
                break;
            } /* opcode::f2i */
            case opcode::aconst_null:
            {
                frame.stack.push_addr<void>(nullptr);
                break;
            } /* opcode::aconst_null */
            case opcode::iconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int32_t>(binary, offset);
                frame.stack.push_i32(i);
                break;
            } /* opcode::iconst */
            case opcode::fconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto f = read_unchecked<float>(binary, offset);
                frame.stack.push_f32(f);
                break;
            } /* opcode::fconst */
            case opcode::sconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);
                if(i < 0 || static_cast<std::size_t>(i) >= frame.constants.size())
                {
                    throw interpreter_error(std::format("Invalid index '{}' into constant table.", i));
                }

                auto* str = gc.gc_new<std::string>(gc::gc_object::of_temporary);
                if(frame.constants[i].type != module_::constant_type::str)
                {
                    throw interpreter_error(std::format("Entry {} of constant table is not a string.", i));
                }

                *str = std::get<std::string>(frame.constants[i].data);

                frame.stack.push_addr(str);
                break;
            } /* opcode::sconst */
            case opcode::iaload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during iaload.");
                }

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                frame.stack.push_i32((*arr)[array_index]);
                break;
            } /* opcode::iaload */
            case opcode::faload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<float>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during faload.");
                }

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                frame.stack.push_f32((*arr)[array_index]);
                break;
            } /* opcode::faload */
            case opcode::aaload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<void*>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during aaload.");
                }

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                void* obj = (*arr)[array_index];
                gc.add_temporary(obj);

                frame.stack.push_addr(obj);
                break;
            } /* opcode::aaload */
            case opcode::iastore:
            {
                std::int32_t v = frame.stack.pop_i32();
                std::int32_t index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during iastore.");
                }

                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = v;
                break;
            } /* opcode::iastore */
            case opcode::fastore:
            {
                float v = frame.stack.pop_f32();
                std::int32_t index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<float>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during fastore.");
                }

                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = v;
                break;
            } /* opcode::fastore */
            case opcode::aastore:
            {
                void* obj = frame.stack.pop_addr<void>();
                std::int32_t index = frame.stack.pop_i32();
                auto* arr = frame.stack.pop_addr<fixed_vector<void*>>();

                if(arr == nullptr)
                {
                    throw interpreter_error("Null pointer access during aastore.");
                }

                gc.remove_temporary(obj);
                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = obj;

                break;
            } /* opcode::aastore */
            case opcode::iload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(std::int32_t) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                std::int32_t v;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(&v, &frame.locals[i], sizeof(v));
                frame.stack.push_i32(v);
                break;
            } /* opcode::iload */
            case opcode::fload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(float) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                float v;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(&v, &frame.locals[i], sizeof(v));
                frame.stack.push_f32(v);
                break;
            } /* opcode::fload */
            case opcode::aload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(void*) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(static_cast<void*>(&addr), &frame.locals[i], sizeof(addr));
                gc.add_temporary(addr);
                frame.stack.push_addr(addr);
                break;
            } /* opcode::aload */
            case opcode::istore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(std::int32_t) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                std::int32_t v = frame.stack.pop_i32();
                std::memcpy(&frame.locals[i], &v, sizeof(v));
                break;
            } /* opcode::istore */
            case opcode::fstore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(float) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                float v = frame.stack.pop_f32();
                std::memcpy(&frame.locals[i], &v, sizeof(v));
                break;
            } /* opcode::fstore */
            case opcode::astore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(
                      std::format(
                        "'{}': Invalid offset '{}' for local.",
                        to_string(instr_opcode),
                        i));
                }

                if(i + sizeof(std::string*) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                auto* obj = frame.stack.pop_addr<void>();
                gc.remove_temporary(obj);

                void* prev;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(static_cast<void*>(&prev), &frame.locals[i], sizeof(prev));
                if(obj != prev)
                {
                    if(prev != nullptr)
                    {
                        gc.remove_root(prev);
                    }
                    if(obj != nullptr)
                    {
                        gc.add_root(obj);
                    }
                }

                std::memcpy(&frame.locals[i], static_cast<const void*>(&obj), sizeof(obj));
                break;
            } /* opcode::astore */
            case opcode::invoke:
            {
                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_loader const* callee_loader;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(
                  static_cast<void*>(&callee_loader),
                  &binary[offset],
                  sizeof(callee_loader));           // NOLINT(bugprone-sizeof-expression)
                offset += sizeof(callee_loader);    // NOLINT(bugprone-sizeof-expression)

                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_::function_descriptor const* desc;    // NOLINT(cppcoreguidelines-init-variables)
                std::memcpy(
                  static_cast<void*>(&desc),
                  &binary[offset],
                  sizeof(desc));           // NOLINT(bugprone-sizeof-expression)
                offset += sizeof(desc);    // NOLINT(bugprone-sizeof-expression)

                if(desc->native)
                {
                    const auto& details = std::get<module_::native_function_details>(desc->details);
                    if(!details.func)
                    {
                        throw interpreter_error("Tried to invoke unresolved native function.");
                    }

                    details.func(frame.stack);
                }
                else
                {
                    const auto& details = std::get<module_::function_details>(desc->details);

                    // prepare stack frame
                    stack_frame callee_frame{
                      callee_loader->get_module().header.constants,
                      details.locals_size,
                      details.stack_size};

                    auto* args_start = reinterpret_cast<std::byte*>(frame.stack.end(details.args_size));    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    std::copy(args_start, args_start + details.args_size, callee_frame.locals.data());      // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    frame.stack.discard(details.args_size);

                    // clean up arguments in GC
                    // FIXME This (likely) should be done by the callee, after making them roots.
                    for(std::size_t i = 0; i < desc->signature.arg_types.size(); ++i)
                    {
                        const auto& arg = details.locals[i];

                        if(arg.type.is_array() || arg.reference)
                        {
                            void* addr;    // NOLINT(cppcoreguidelines-init-variables)
                            std::memcpy(static_cast<void*>(&addr), &callee_frame.locals[arg.offset], sizeof(addr));
                            gc.remove_temporary(addr);
                        }
                    }

                    // invoke function
                    exec(*callee_loader, details.offset, details.size, details.locals, callee_frame);

                    // store return value
                    if(callee_frame.stack.size() != details.return_size)
                    {
                        throw interpreter_error(std::format("Expected {} bytes to be returned from function call, got {}.", details.return_size, callee_frame.stack.size()));
                    }
                    frame.stack.push_stack(callee_frame.stack);
                }
                break;
            } /* opcode::invoke */
            case opcode::new_:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto size = read_unchecked<std::size_t>(binary, offset);
                auto alignment = read_unchecked<std::size_t>(binary, offset);
                auto layout_id = read_unchecked<std::size_t>(binary, offset);

                frame.stack.push_addr(gc.gc_new(layout_id, size, alignment, gc::gc_object::of_temporary));

                break;
            } /* opcode::new_ */
            case opcode::newarray:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto type = read_unchecked<std::uint8_t>(binary, offset);

                std::int32_t size = frame.stack.pop_i32();
                if(size < 0)
                {
                    throw interpreter_error(std::format("Invalid array size '{}'.", size));
                }

                if(type == static_cast<std::uint8_t>(module_::array_type::i32))
                {
                    frame.stack.push_addr(gc.gc_new_array<std::int32_t>(size, gc::gc_object::of_temporary));
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::f32))
                {
                    frame.stack.push_addr(gc.gc_new_array<float>(size, gc::gc_object::of_temporary));
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::str))
                {
                    auto* array = gc.gc_new_array<std::string*>(size, gc::gc_object::of_temporary);
                    for(std::string*& s: *array)
                    {
                        s = gc.gc_new<std::string>(gc::gc_object::of_none, false);
                    }
                    frame.stack.push_addr(array);
                }
                else if(type == static_cast<std::uint8_t>(module_::array_type::ref))
                {
                    frame.stack.push_addr(gc.gc_new_array<void*>(size, gc::gc_object::of_temporary));
                }
                else
                {
                    throw interpreter_error(std::format("Unkown array type '{}' for newarray.", type));
                }

                break;
            } /* opcode::newarray */
            case opcode::anewarray:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto layout_id = read_unchecked<std::size_t>(binary, offset);

                std::int32_t length = frame.stack.pop_i32();
                if(length < 0)
                {
                    throw interpreter_error(std::format("Invalid array length '{}'.", length));
                }

                frame.stack.push_addr(gc.gc_new_array(layout_id, length, gc::gc_object::of_temporary));

                break;
            } /* opcode::anewarray */
            case opcode::arraylength:
            {
                // convert to any fixed_vector type.
                auto* v = frame.stack.pop_addr<fixed_vector<void*>>();
                if(v == nullptr)
                {
                    throw interpreter_error("Null pointer access during arraylength.");
                }
                gc.remove_temporary(v);
                frame.stack.push_i32(utils::numeric_cast<std::int32_t>(v->size()));
                break;
            } /* opcode::arraylength */
            case opcode::setfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto field_size = read_unchecked<std::size_t>(binary, offset);
                auto field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                if(field_size == sizeof(void*))
                {
                    // this block also gets executed if `sizeof(void*)==sizeof(std::int32_t)`.

                    void* v = frame.stack.pop_addr<void>();
                    void* type_ref = frame.stack.pop_addr<void>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    if(field_needs_gc)
                    {
                        gc.remove_temporary(v);
                    }

                    std::memcpy(
                      reinterpret_cast<std::byte*>(type_ref) + field_offset,    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                      static_cast<void*>(&v),
                      sizeof(v));
                }
                else if(field_size == sizeof(std::int32_t))
                {
                    std::int32_t v = frame.stack.pop_i32();
                    void* type_ref = frame.stack.pop_addr<void>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    std::memcpy(
                      reinterpret_cast<std::byte*>(type_ref) + field_offset,    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                      static_cast<void*>(&v),
                      sizeof(v));
                }
                else
                {
                    throw interpreter_error(std::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::setfield */
            case opcode::getfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto field_size = read_unchecked<std::size_t>(binary, offset);
                auto field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                void* type_ref = frame.stack.pop_addr<void>();
                if(type_ref == nullptr)
                {
                    throw interpreter_error("Null pointer access during getfield.");
                }
                gc.remove_temporary(type_ref);

                if(field_size == sizeof(void*))
                {
                    // this block also gets executed if `sizeof(void*)==sizeof(std::int32_t)`.

                    void* v;    // NOLINT(cppcoreguidelines-init-variables)
                    std::memcpy(
                      static_cast<void*>(&v),
                      reinterpret_cast<std::byte*>(type_ref) + field_offset,    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                      sizeof(v));
                    frame.stack.push_addr(v);

                    if(field_needs_gc)
                    {
                        gc.add_temporary(v);
                    }
                }
                else if(field_size == sizeof(std::int32_t))
                {
                    std::int32_t v;    // NOLINT(cppcoreguidelines-init-variables)
                    std::memcpy(
                      static_cast<void*>(&v),
                      reinterpret_cast<std::byte*>(type_ref) + field_offset,    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
                      sizeof(v));
                    frame.stack.push_i32(v);
                }
                else
                {
                    throw interpreter_error(std::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::getfield */
            case opcode::checkcast:
            {
                auto target_layout_id = read_unchecked<std::size_t>(binary, offset);
                auto flags = read_unchecked<std::size_t>(binary, offset);

                if((flags & static_cast<std::uint8_t>(module_::struct_flags::allow_cast)) == 0)
                {
                    void* obj = frame.stack.pop_addr<void>();
                    if(obj == nullptr)
                    {
                        throw interpreter_error("Null pointer access during checkcast.");
                    }

                    std::size_t source_layout_id = gc.get_type_layout_id(obj);

                    if(target_layout_id != source_layout_id)
                    {
                        throw interpreter_error(
                          std::format("Type cast from '{}' to '{}' failed.",
                                      gc.layout_to_string(source_layout_id), gc.layout_to_string(target_layout_id)));
                    }
                    frame.stack.push_addr(obj);
                }

                break;
            } /* opcode::checkcast */
            case opcode::iand:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i & v; });
                break;
            } /* opcode::iand */
            case opcode::land:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return (i != 0) && (v != 0); });
                break;
            } /* opcode::land */
            case opcode::ior:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i | v; });
                break;
            } /* opcode::ior */
            case opcode::lor:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return (i != 0) || (v != 0); });
                break;
            } /* opcode::lor */
            case opcode::ixor:
            {
                std::int32_t v = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [v](std::int32_t i) -> std::int32_t
                  { return i ^ v; });
                break;
            } /* opcode::ixor */
            case opcode::ishl:
            {
                std::int32_t a = frame.stack.pop_i32();
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a_u32](std::int32_t s) -> std::int32_t
                  { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    return static_cast<std::int32_t>(s_u32 << a_u32); });
                break;
            } /* opcode::ishl */
            case opcode::ishr:
            {
                std::int32_t a = frame.stack.pop_i32();
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,readability-magic-numbers)
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a_u32](std::int32_t s) -> std::int32_t
                  { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);    // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                    return static_cast<std::int32_t>(s_u32 >> a_u32); });
                break;
            } /* opcode::ishr */
            case opcode::icmpl:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b < a; });
                break;
            } /* opcode::icmpl */
            case opcode::fcmpl:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b < a; });
                break;
            } /* opcode::fcmpl */
            case opcode::icmple:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b <= a; });
                break;
            } /* opcode::icmple */
            case opcode::fcmple:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b <= a; });
                break;
            } /* opcode::fcmple */
            case opcode::icmpg:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b > a; });
                break;
            } /* opcode::icmpg */
            case opcode::fcmpg:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b > a; });
                break;
            } /* opcode::fcmpg */
            case opcode::icmpge:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b >= a; });
                break;
            } /* opcode::icmpge */
            case opcode::fcmpge:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b >= a; });
                break;
            } /* opcode::fcmpge */
            case opcode::icmpeq:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b == a; });
                break;
            } /* opcode::icmpeq */
            case opcode::fcmpeq:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b == a; });
                break;
            } /* opcode::fcmpeq */
            case opcode::icmpne:
            {
                std::int32_t a = frame.stack.pop_i32();
                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a](std::int32_t b) -> std::int32_t
                  { return b != a; });
                break;
            } /* opcode::icmpne */
            case opcode::fcmpne:
            {
                float a = frame.stack.pop_f32();
                frame.stack.modify_top<float, std::int32_t>(
                  [a](float b) -> std::int32_t
                  { return b != a; });
                break;
            } /* opcode::fcmpne */
            case opcode::acmpeq:
            {
                void* a = frame.stack.pop_addr<void>();
                void* b = frame.stack.pop_addr<void>();
                gc.remove_temporary(a);
                gc.remove_temporary(b);
                frame.stack.push_i32((b == a) ? 1 : 0);
                break;
            } /* opcode::acmpeq */
            case opcode::acmpne:
            {
                void* a = frame.stack.pop_addr<void>();
                void* b = frame.stack.pop_addr<void>();
                gc.remove_temporary(a);
                gc.remove_temporary(b);
                frame.stack.push_i32((b != a) ? 1 : 0);
                break;
            } /* opcode::acmpne */
            case opcode::jnz:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                auto then_offset = read_unchecked<std::size_t>(binary, offset);
                auto else_offset = read_unchecked<std::size_t>(binary, offset);

                std::int32_t cond = frame.stack.pop_i32();
                if(cond != 0)
                {
                    offset = then_offset;
                }
                else
                {
                    offset = else_offset;
                }
                break;
            } /* opcode::jnz */
            case opcode::jmp:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                offset = read_unchecked<std::int64_t>(binary, offset);
                break;
            } /* opcode::jmp */
            default:
                throw interpreter_error(
                  std::format(
                    "Opcode '{}' ({}) not implemented.",
                    to_string(instr_opcode),
                    static_cast<std::uint8_t>(instr)));
            }
        }

        throw interpreter_error("Out of bounds code read.");
    }
    catch(interpreter_error& e)
    {
        stack_trace_handler(e, loader, entry_point, offset);
        throw e;
    }
    catch(gc::gc_error& e)
    {
        interpreter_error e_int{e.what()};
        stack_trace_handler(e_int, loader, entry_point, offset);
        throw e_int;
    }
}

void context::stack_trace_handler(
  interpreter_error& err,
  const module_loader& loader,
  std::size_t entry_point,
  std::size_t offset)
{
    // find the module name.
    for(auto& [mod_name, mod_loader]: loaders)
    {
        if(mod_loader.get() == &loader)
        {
            err.add_stack_trace_entry(mod_name, entry_point, offset);
            return;
        }
    }

    err.add_stack_trace_entry("<unknown>", entry_point, offset);
}

std::string context::stack_trace_to_string(const std::vector<stack_trace_entry>& stack_trace)
{
    std::string buf;
    for(const auto& entry: stack_trace)
    {
        // resolve the offset to a function name.
        std::string func_name;

        auto loader_it = loaders.find(entry.mod_name);
        if(loader_it != loaders.end())
        {
            func_name = loader_it->second->resolve_entry_point(entry.entry_point)
                          .value_or(std::format("<unknown at {}>", entry.entry_point));
        }

        buf += std::format("  in {}.{}\n", entry.mod_name, func_name);
    }
    return buf;
}

/** Function argument writing and destruction. */
class arguments_scope
{
    /** Interpreter context. */
    context& ctx;

    /** The arguments to manage. */
    const std::vector<value>& args;

    /** The locals. */
    std::vector<std::byte>& locals;

public:
    /** Deleted constructors. */
    arguments_scope() = delete;
    arguments_scope(const arguments_scope&) = delete;
    arguments_scope(arguments_scope&&) = delete;

    /**
     * Construct an argument scope. Validates the argument types and creates
     * them in `locals`.
     *
     * @param ctx The associated interpreter context.
     * @param args The arguments to verify and write.
     * @param arg_types The argument types to validate against.
     * @param locals The locals storage to write into.
     */
    arguments_scope(
      context& ctx,
      const std::vector<value>& args,
      const std::vector<module_::variable_type>& arg_types,
      std::vector<std::byte>& locals)
    : ctx{ctx}
    , args{args}
    , locals{locals}
    {
        if(arg_types.size() != args.size())
        {
            throw interpreter_error(
              std::format(
                "Argument count does not match: Expected {}, got {}.",
                arg_types.size(), args.size()));
        }

        std::size_t offset = 0;
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            module_::variable_type arg_type = args[i].get_type();
            std::optional<std::size_t> layout_id = args[i].get_layout_id();

            if(layout_id.has_value())
            {
                if(arg_types[i].layout_id != layout_id)
                {
                    if(arg_types[i].layout_id.has_value())
                    {
                        throw interpreter_error(
                          std::format(
                            "Argument {} has wrong base type (expected type '{}' with id '{}', got id '{}').",
                            i,
                            arg_types[i].base_type(),
                            arg_types[i].layout_id.value(),    // NOLINT(bugprone-unchecked-optional-access)
                            layout_id.value()));
                    }

                    throw interpreter_error(
                      std::format(
                        "Argument {} has wrong base type (expected type '{}', got id '{}').",
                        i,
                        arg_types[i].base_type(),
                        layout_id.value()));
                }
            }
            else if(arg_types[i].base_type() != arg_type.base_type())
            {
                throw interpreter_error(
                  std::format(
                    "Argument {} has wrong base type (expected '{}', got '{}').",
                    i,
                    arg_types[i].base_type(),
                    arg_type.base_type()));
            }

            if(arg_types[i].is_array() != arg_type.is_array())
            {
                throw interpreter_error(
                  std::format(
                    "Argument {} has wrong array property (expected '{}', got '{}').",
                    i,
                    arg_types[i].is_array(),
                    arg_type.is_array()));
            }

            if(offset + args[i].get_size() > locals.size())
            {
                throw interpreter_error(std::format(
                  "Stack overflow during argument allocation while processing argument {}.", i));
            }

            if(layout_id.has_value())
            {
                ctx.get_gc().add_persistent(&locals[offset], layout_id.value());
            }

            if(is_garbage_collected(arg_type))
            {
                ctx.get_gc().add_temporary(&locals[offset]);
            }

            offset += args[i].create(&locals[offset]);
        }
    }

    /** Destructor. Destroys the created arguments. */
    ~arguments_scope()
    {
        std::size_t offset = 0;
        for(const auto& arg: args)
        {
            if(offset + arg.get_size() > locals.size())
            {
                DEBUG_LOG("Stack overflow during argument destruction while processing argument.");

                // FIXME This is an error, but destructors cannot throw.
                return;
            }

            if(arg.get_layout_id().has_value())
            {
                try
                {
                    ctx.get_gc().remove_persistent(&locals[offset]);
                }
                catch(gc::gc_error& e)
                {
                    // FIXME This is an error, but destructors cannot throw.

                    DEBUG_LOG("GC error during argument destruction while processing argument: {}", e.what());
                }
            }

            if(is_garbage_collected(arg.get_type()))
            {
                try
                {
                    ctx.get_gc().remove_temporary(&locals[offset]);
                }
                catch(gc::gc_error& e)
                {
                    // FIXME This is an error, but destructors cannot throw.

                    DEBUG_LOG("GC error during argument destruction while processing argument: {}", e.what());
                }
            }

            try
            {
                offset += arg.destroy(&locals[offset]);
            }
            catch(std::exception& e)
            {
                DEBUG_LOG("GC error during argument destruction while processing argument: {}", e.what());
            }
        }
    }

    /** Deleted assignment operators. */
    arguments_scope& operator=(const arguments_scope&) = delete;
    arguments_scope& operator=(arguments_scope&&) = delete;
};

value context::exec(
  const module_loader& loader,
  const function& f,
  const std::vector<value>& args)
{
    /*
     * allocate locals and decode arguments.
     */
    stack_frame frame{
      loader.get_module().get_header().constants,
      f.get_locals_size(),
      f.get_stack_size()};

    const auto& arg_types = f.get_signature().arg_types;
    arguments_scope arg_scope{*this, args, arg_types, frame.locals};

    /*
     * Execute the function.
     */

    auto invoke_native = [&f, &frame]() -> opcode
    {
        const auto& func = f.get_function();
        func(frame.stack);
        return f.get_return_opcode();
    };

    auto invoke = [this, &loader, &f, &frame]() -> opcode
    {
        return exec(loader, f.get_entry_point(), f.get_size(), f.get_locals(), frame);
    };

    opcode ret_opcode = f.is_native()
                          ? invoke_native()
                          : invoke();

    /*
     * Decode return value.
     */
    value ret;
    if(ret_opcode == opcode::ret)
    {
        /* return void */
    }
    else if(ret_opcode == opcode::iret)
    {
        ret = value{frame.stack.pop_i32()};
    }
    else if(ret_opcode == opcode::fret)
    {
        ret = value{frame.stack.pop_f32()};
    }
    else if(ret_opcode == opcode::sret)
    {
        auto* s = frame.stack.pop_addr<std::string>();
        if(s != nullptr)
        {
            ret = value{*s};    // copy string
            gc.remove_temporary(s);
        }
    }
    else if(ret_opcode == opcode::aret)
    {
        const auto& sig = f.get_signature();

        void* addr = frame.stack.pop_addr<void>();
        ret = value{sig.return_type, addr};
        // NOTE The caller is responsible for calling `gc.remove_temporary(addr)`.
    }
    else
    {
        throw interpreter_error(std::format(
          "Invalid return opcode '{}' ({}).",
          to_string(ret_opcode), static_cast<int>(ret_opcode)));
    }

    // invoke the garbage collector to clean up before returning.
    gc.run();

    // verify that the stack is empty.
    if(!frame.stack.empty())
    {
        throw interpreter_error("Non-empty stack on function exit.");
    }

    return ret;
}

void context::register_native_function(
  const std::string& mod_name,
  std::string fn_name,
  std::function<void(operand_stack&)> func)
{
    if(!func)
    {
        throw interpreter_error(std::format("Cannot register null native function '{}.{}'.", mod_name, fn_name));
    }

    auto loader_it = loaders.find(mod_name);
    if(loader_it != loaders.end())
    {
        if(loader_it->second->has_function(fn_name))
        {
            throw interpreter_error(
              std::format(
                "Cannot register native function: '{}' is already definded for module '{}'.",
                fn_name, mod_name));
        }
    }

    auto native_mod_it = native_function_map.find(mod_name);
    if(native_mod_it == native_function_map.end())
    {
        native_function_map.insert({mod_name, {{std::move(fn_name), std::move(func)}}});
    }
    else
    {
        if(native_mod_it->second.contains(fn_name))
        {
            throw interpreter_error(
              std::format(
                "Cannot register native function: '{}' is already definded for module '{}'.",
                fn_name,
                mod_name));
        }

        native_mod_it->second.insert({std::move(fn_name), std::move(func)});
    }
}

module_loader& context::resolve_module(const std::string& import_name, std::shared_ptr<instruction_recorder> recorder)
{
    auto it = loaders.find(import_name);
    if(it != loaders.end())
    {
        return *it->second;
    }

    std::string import_path = import_name;
    slang::utils::replace_all(import_path, package::delimiter, "/");

    fs::path fs_path = fs::path{import_path};
    if(!fs_path.has_extension())
    {
        fs_path = fs_path.replace_extension(package::module_ext);
    }
    fs::path resolved_path = file_mgr.resolve(fs_path);

    loaders.insert({import_name, std::make_unique<module_loader>(*this, import_name, resolved_path, recorder)});
    return *loaders[import_name].get();
}

std::string context::get_import_name(const module_loader& loader) const
{
    auto it = std::ranges::find_if(
      std::as_const(loaders),
      [&loader](const std::pair<const std::string, std::unique_ptr<module_loader>>& p) -> bool
      {
          return p.second.get() == &loader;
      });
    if(it == loaders.cend())
    {
        throw interpreter_error("Unable to find name for loader.");
    }

    return it->first;
}

value context::invoke(const std::string& module_name, const std::string& function_name, const std::vector<value>& args)
{
    try
    {
        DEBUG_LOG("invoke: {}.{}", module_name, function_name);

        module_loader& loader = resolve_module(module_name);
        function& fn = loader.get_function(function_name);

        return exec(loader, fn, args);
    }
    catch(interpreter_error& e)
    {
        // Update the error message with the stack trace.
        std::string buf = e.what();

        const auto& stack_trace = e.get_stack_trace();
        if(!stack_trace.empty())
        {
            buf += std::format("\n{}", stack_trace_to_string(stack_trace));
        }

        if(call_stack_level == 0)
        {
            // we never entered context::exec, so add the function explicitly.
            buf += std::format("  in {}.{}\n", module_name, function_name);
        }

        reset();

        throw interpreter_error(buf, stack_trace);
    }
}

value context::invoke(const module_loader& loader, const function& fn, const std::vector<value>& args)
{
    try
    {
        return exec(loader, fn, args);
    }
    catch(interpreter_error& e)
    {
        // Update the error message with the stack trace.
        std::string buf = e.what();

        const auto& stack_trace = e.get_stack_trace();
        if(!stack_trace.empty())
        {
            buf += std::format("\n{}", stack_trace_to_string(stack_trace));
        }

        if(call_stack_level == 0)
        {
            // we never entered context::exec, so add the function explicitly.
            std::string module_name = get_import_name(loader);
            std::string function_name = loader.resolve_entry_point(fn.get_entry_point())
                                          .value_or(std::format("<unknown at {}>", fn.get_entry_point()));

            buf += std::format("  in {}.{}\n", module_name, function_name);
        }

        reset();

        throw interpreter_error(buf, stack_trace);
    }
}

}    // namespace slang::interpreter
