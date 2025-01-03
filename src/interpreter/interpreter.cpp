/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "archives/memory.h"
#include "interpreter/interpreter.h"
#include "module.h"
#include "opcodes.h"
#include "package.h"
#include "utils.h"
#include "vector.h"

#ifdef INTERPRETER_DEBUG
#    define DEBUG_LOG(...) fmt::print("INT: {}\n", fmt::format(__VA_ARGS__))
#else
#    define DEBUG_LOG(...)
#endif

namespace slang::interpreter
{

namespace ty = slang::typing;

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
static opcode get_return_opcode(const std::pair<module_::variable_type, bool>& return_type)
{
    auto& name = std::get<0>(return_type);

    if(std::get<1>(return_type))
    {
        return opcode::aret;
    }

    if(name.base_type() == "void")
    {
        return opcode::ret;
    }
    else if(name.base_type() == "i32")
    {
        return opcode::iret;
    }
    else if(name.base_type() == "f32")
    {
        return opcode::fret;
    }
    else if(name.base_type() == "str")
    {
        return opcode::sret;
    }
    else if(name.base_type() == "@addr" || name.base_type() == "@array")
    {
        return opcode::aret;
    }

    // FIXME Assume all other types are references.
    return opcode::aret;
}

/**
 * Check if a type is garbage collected.
 *
 * @param t The type string.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const module_::variable_type& t)
{
    if(t.base_type() == "void")
    {
        throw interpreter_error("Found void type in type info for garbage collector.");
    }

    // check for built-in non-gc types.
    return t.is_array() || ty::is_reference_type(t.base_type());
}

/**
 * Check if a type is garbage collected.
 *
 * @param info The type info, given as a pair `(base_type, is_array)`.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const std::pair<std::string, bool>& info)
{
    return std::get<1>(info) || is_garbage_collected(std::get<0>(info));
}

/*
 * function implementation.
 */

function::function(module_::function_signature signature,
                   std::size_t entry_point,
                   std::size_t size,
                   std::vector<module_::variable_descriptor> locals,
                   std::size_t locals_size,
                   std::size_t stack_size)
: signature{std::move(signature)}
, native{false}
, entry_point_or_function{entry_point}
, size{size}
, locals{std::move(locals)}
, locals_size{locals_size}
, stack_size{stack_size}
{
    ret_opcode = ::slang::interpreter::get_return_opcode(this->signature.return_type);
}

function::function(module_::function_signature signature, std::function<void(operand_stack&)> func)
: signature{std::move(signature)}
, native{true}
, entry_point_or_function{std::move(func)}
{
    ret_opcode = ::slang::interpreter::get_return_opcode(this->signature.return_type);
}

/*
 * context implementation.
 */

std::function<void(operand_stack&)> context::resolve_native_function(const std::string& name, const std::string& library_name) const
{
    auto mod_it = native_function_map.find(library_name);
    if(mod_it == native_function_map.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (library not found).", name, library_name));
    }

    auto func_it = mod_it->second.find(name);
    if(func_it == mod_it->second.end())
    {
        throw interpreter_error(fmt::format("Cannot resolve native function '{}' in '{}' (function not found).", name, library_name));
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
        for(auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;
                std::memcpy(&addr, &frame.locals[local.offset], sizeof(void*));
                if(addr != nullptr)
                {
                    ctx.get_gc().add_root(addr);
                    // FIXME We (likely) want to remove the temporaries from the GC here,
                    //       instead of at the caller (see comment there).
                }
            }
        }
    }

    /** Destructor. Calls `destruct`, if needed. */
    ~locals_scope()
    {
        if(needs_destruct)
        {
            destruct();
        }
    }

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

        for(auto& local: locals)
        {
            if(local.type.is_array() || local.reference)
            {
                void* addr;
                std::memcpy(&addr, &frame.locals[local.offset], sizeof(void*));
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
  std::size_t entry_point,
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
            throw interpreter_error(fmt::format("Function call exceeded maximum call stack depth ({}).", max_call_stack_depth));
        }

        const std::vector<std::byte>& binary = loader.get_module().get_binary();
        if(offset >= binary.size())
        {
            throw interpreter_error(fmt::format("Entry point is outside the loaded code segment ({} >= {}).", offset, binary.size()));
        }

        locals_scope ls{*this, locals, frame};

        const std::size_t function_end = offset + size;
        while(offset < function_end)
        {
            auto instr = binary[offset];
            ++offset;

            // return.
            if(instr >= static_cast<std::byte>(opcode::ret) && instr <= static_cast<std::byte>(opcode::aret))
            {
                // NOTE The stack size is validated by the caller.

                // destruct the locals here for the GC to clean them up.
                ls.destruct();

                // run garbage collector.
                gc.run();

                --call_stack_level;
                return static_cast<opcode>(instr);
            }

            if(offset == function_end)
            {
                throw interpreter_error(fmt::format("Execution reached function boundary."));
            }

            switch(static_cast<opcode>(instr))
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
                void* addr;
                std::memcpy(&addr, frame.stack.end(sizeof(void*)), sizeof(void*));
                gc.add_temporary(addr);
                break;
            } /* opcode::adup */
            case opcode::dup_x1:
            {
                std::size_t size1 = read_unchecked<std::size_t>(binary, offset);
                std::size_t size2 = read_unchecked<std::size_t>(binary, offset);
                std::uint8_t needs_gc = read_unchecked<std::uint8_t>(binary, offset);
                frame.stack.dup_x1(size1, size2);

                if(needs_gc)
                {
                    void* addr;
                    std::memcpy(&addr, frame.stack.end(2 * size1 + size2), size1);
                    gc.add_temporary(addr);
                }
                break;
            } /* opcode::dup_x1 */
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
            case opcode::iconst: [[fallthrough]];
            case opcode::fconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::uint32_t i_u32 = read_unchecked<std::uint32_t>(binary, offset);
                frame.stack.push_i32(i_u32);
                break;
            } /* opcode::iconst, opcode::fconst */
            case opcode::sconst:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);
                if(i < 0 || static_cast<std::size_t>(i) >= frame.constants.size())
                {
                    throw interpreter_error(fmt::format("Invalid index '{}' into constant table.", i));
                }

                auto str = gc.gc_new<std::string>(gc::gc_object::of_temporary);
                if(frame.constants[i].type != module_::constant_type::str)
                {
                    throw interpreter_error(fmt::format("Entry {} of constant table is not a string.", i));
                }

                *str = std::get<std::string>(frame.constants[i].data);

                frame.stack.push_addr(str);
                break;
            } /* opcode::sconst */
            case opcode::iaload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                fixed_vector<std::int32_t>* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

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
                fixed_vector<float>* arr = frame.stack.pop_addr<fixed_vector<float>>();

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                frame.stack.push_f32((*arr)[array_index]);
                break;
            } /* opcode::faload */
            case opcode::saload:
            {
                std::int32_t array_index = frame.stack.pop_i32();
                fixed_vector<std::string*>* arr = frame.stack.pop_addr<fixed_vector<std::string*>>();

                gc.remove_temporary(arr);

                if(array_index < 0 || static_cast<std::size_t>(array_index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                void* obj = (*arr)[array_index];
                gc.add_temporary(obj);

                frame.stack.push_addr(obj);
                break;
            } /* opcode::saload */
            case opcode::iastore:
            {
                std::int32_t v = frame.stack.pop_i32();
                std::int32_t index = frame.stack.pop_i32();
                fixed_vector<std::int32_t>* arr = frame.stack.pop_addr<fixed_vector<std::int32_t>>();

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
                fixed_vector<float>* arr = frame.stack.pop_addr<fixed_vector<float>>();

                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = v;
                break;
            } /* opcode::fastore */
            case opcode::sastore:
            {
                std::string* s = frame.stack.pop_addr<std::string>();
                std::int32_t index = frame.stack.pop_i32();
                fixed_vector<std::string*>* arr = frame.stack.pop_addr<fixed_vector<std::string*>>();

                gc.remove_temporary(s);
                gc.remove_temporary(arr);

                if(index < 0 || static_cast<std::size_t>(index) >= arr->size())
                {
                    throw interpreter_error("Out of bounds array access.");
                }

                (*arr)[index] = s;

                break;
            } /* opcode::sastore */
            case opcode::iload: [[fallthrough]];
            case opcode::fload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::uint32_t) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                std::uint32_t v;
                std::memcpy(&v, &frame.locals[i], sizeof(v));
                frame.stack.push_i32(v);
                break;
            } /* opcode::iload, opcode::fload */
            case opcode::aload:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(void*) > frame.locals.size())
                {
                    throw interpreter_error("Invalid memory access.");
                }

                void* addr;
                std::memcpy(&addr, &frame.locals[i], sizeof(addr));
                gc.add_temporary(addr);
                frame.stack.push_addr(addr);
                break;
            } /* opcode::aload */
            case opcode::istore: [[fallthrough]];
            case opcode::fstore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::uint32_t) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                std::int32_t v = frame.stack.pop_i32();
                std::memcpy(&frame.locals[i], &v, sizeof(v));
                break;
            } /* opcode::istore, opcode::fstore */
            case opcode::astore:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::int64_t i = read_unchecked<std::int64_t>(binary, offset);

                if(i < 0)
                {
                    throw interpreter_error(fmt::format("'{}': Invalid offset '{}' for local.", to_string(static_cast<opcode>(instr)), i));
                }

                if(i + sizeof(std::string*) > frame.locals.size())
                {
                    throw interpreter_error("Stack overflow.");
                }

                auto obj = frame.stack.pop_addr<void>();
                gc.remove_temporary(obj);

                void* prev;
                std::memcpy(&prev, &frame.locals[i], sizeof(prev));
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

                std::memcpy(&frame.locals[i], &obj, sizeof(obj));
                break;
            } /* opcode::astore */
            case opcode::invoke:
            {
                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_loader const* callee_loader;
                std::memcpy(&callee_loader, &binary[offset], sizeof(callee_loader));
                offset += sizeof(callee_loader);

                /* no out-of-bounds read possible, since this is checked during decode.
                 * NOTE this does not use `read_unchecked`, since we do not de-reference the result. */
                module_::function_descriptor const* desc;
                std::memcpy(&desc, &binary[offset], sizeof(desc));
                offset += sizeof(desc);

                if(desc->native)
                {
                    auto& details = std::get<module_::native_function_details>(desc->details);
                    if(!details.func)
                    {
                        throw interpreter_error("Tried to invoke unresolved native function.");
                    }

                    details.func(frame.stack);
                }
                else
                {
                    auto& details = std::get<module_::function_details>(desc->details);

                    // prepare stack frame
                    stack_frame callee_frame{frame.constants, details.locals_size, details.stack_size};

                    auto* args_start = reinterpret_cast<std::byte*>(frame.stack.end(details.args_size));
                    std::copy(args_start, args_start + details.args_size, callee_frame.locals.data());
                    frame.stack.discard(details.args_size);

                    // clean up arguments in GC
                    // FIXME This (likely) should be done by the callee, after making them roots.
                    for(std::size_t i = 0; i < desc->signature.arg_types.size(); ++i)
                    {
                        auto& arg = details.locals[i];

                        if(arg.type.is_array() || arg.reference)
                        {
                            void* addr;
                            std::memcpy(&addr, &callee_frame.locals[arg.offset], sizeof(addr));
                            gc.remove_temporary(addr);
                        }
                    }

                    // invoke function
                    exec(*callee_loader, details.offset, details.size, details.locals, callee_frame);

                    // store return value
                    if(callee_frame.stack.size() != details.return_size)
                    {
                        throw interpreter_error(fmt::format("Expected {} bytes to be returned from function call, got {}.", details.return_size, callee_frame.stack.size()));
                    }
                    frame.stack.push_stack(callee_frame.stack);
                }
                break;
            } /* opcode::invoke */
            case opcode::new_:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t size = read_unchecked<std::size_t>(binary, offset);
                std::size_t alignment = read_unchecked<std::size_t>(binary, offset);
                std::size_t layout_id = read_unchecked<std::size_t>(binary, offset);

                frame.stack.push_addr(gc.gc_new(layout_id, size, alignment, gc::gc_object::of_temporary));

                break;
            } /* opcode::new_ */
            case opcode::newarray:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::uint8_t type = read_unchecked<std::uint8_t>(binary, offset);

                std::int32_t size = frame.stack.pop_i32();
                if(size < 0)
                {
                    throw interpreter_error(fmt::format("Invalid array size '{}'.", size));
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
                    auto array = gc.gc_new_array<std::string*>(size, gc::gc_object::of_temporary);
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
                    throw interpreter_error(fmt::format("Unkown array type '{}' for newarray.", type));
                }

                break;
            } /* opcode::newarray */
            case opcode::arraylength:
            {
                // convert to any fixed_vector type.
                auto v = frame.stack.pop_addr<fixed_vector<void*>>();
                if(v == nullptr)
                {
                    throw interpreter_error("Null pointer access during arraylength.");
                }
                gc.remove_temporary(v);
                frame.stack.push_i32(v->size());
                break;
            } /* opcode::arraylength */
            case opcode::setfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t field_size = read_unchecked<std::size_t>(binary, offset);
                std::size_t field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                if(field_size == sizeof(void*))
                {
                    // this block also gets executed if `sizeof(void*)==sizeof(std::int32_t)`.

                    void* v = frame.stack.pop_addr<void>();
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    if(field_needs_gc)
                    {
                        gc.remove_temporary(v);
                    }

                    std::memcpy(reinterpret_cast<std::byte*>(type_ref) + field_offset, &v, sizeof(v));
                }
                else if(field_size == sizeof(std::int32_t))
                {
                    std::int32_t v = frame.stack.pop_i32();
                    void* type_ref = frame.stack.pop_addr<void*>();
                    if(type_ref == nullptr)
                    {
                        throw interpreter_error("Null pointer access during setfield.");
                    }
                    gc.remove_temporary(type_ref);

                    std::memcpy(reinterpret_cast<std::byte*>(type_ref) + field_offset, &v, sizeof(v));
                }

                else
                {
                    throw interpreter_error(fmt::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::setfield */
            case opcode::getfield:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t field_size = read_unchecked<std::size_t>(binary, offset);
                std::size_t field_offset = read_unchecked<std::size_t>(binary, offset);
                bool field_needs_gc = read_unchecked<bool>(binary, offset);

                void* type_ref = frame.stack.pop_addr<void*>();
                if(type_ref == nullptr)
                {
                    throw interpreter_error("Null pointer access during getfield.");
                }
                gc.remove_temporary(type_ref);

                if(field_size == sizeof(void*))
                {
                    // this block also gets executed if `sizeof(void*)==sizeof(std::int32_t)`.

                    void* v;
                    std::memcpy(&v, reinterpret_cast<std::byte*>(type_ref) + field_offset, sizeof(v));
                    frame.stack.push_addr(v);

                    if(field_needs_gc)
                    {
                        gc.add_temporary(v);
                    }
                }
                else if(field_size == sizeof(std::int32_t))
                {
                    std::int32_t v;
                    std::memcpy(&v, reinterpret_cast<std::byte*>(type_ref) + field_offset, sizeof(v));
                    frame.stack.push_i32(v);
                }
                else
                {
                    throw interpreter_error(fmt::format("Invalid field size {} encountered in setfield.", size));
                }

                break;
            } /* opcode::getfield */
            case opcode::checkcast:
            {
                std::size_t target_layout_id = read_unchecked<std::size_t>(binary, offset);
                std::size_t flags = read_unchecked<std::size_t>(binary, offset);

                if((flags & static_cast<std::uint8_t>(module_::struct_flags::allow_cast)) == 0)
                {
                    void* obj = frame.stack.pop_addr<void>();
                    std::size_t source_layout_id = gc.get_type_layout_id(obj);

                    if(target_layout_id != source_layout_id)
                    {
                        throw interpreter_error(
                          fmt::format("Type cast from '{}' to '{}' failed.",
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
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a_u32](std::int32_t s) -> std::int32_t
                  { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s); 
                    return s_u32 << a_u32; });
                break;
            } /* opcode::ishl */
            case opcode::ishr:
            {
                std::int32_t a = frame.stack.pop_i32();
                std::uint32_t a_u32 = *reinterpret_cast<std::uint32_t*>(&a) & 0x1f;    // mask because of 32-bit int.

                frame.stack.modify_top<std::int32_t, std::int32_t>(
                  [a_u32](std::int32_t s) -> std::int32_t
                  { std::uint32_t s_u32 = *reinterpret_cast<std::uint32_t*>(&s);
                    return s_u32 >> a_u32; });
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
                frame.stack.push_i32(b == a);
                break;
            } /* opcode::acmpeq */
            case opcode::acmpne:
            {
                void* a = frame.stack.pop_addr<void>();
                void* b = frame.stack.pop_addr<void>();
                gc.remove_temporary(a);
                gc.remove_temporary(b);
                frame.stack.push_i32(b != a);
                break;
            } /* opcode::acmpne */
            case opcode::jnz:
            {
                /* no out-of-bounds read possible, since this is checked during decode. */
                std::size_t then_offset = read_unchecked<std::size_t>(binary, offset);
                std::size_t else_offset = read_unchecked<std::size_t>(binary, offset);

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
                throw interpreter_error(fmt::format("Opcode '{}' ({}) not implemented.", to_string(static_cast<opcode>(instr)), instr));
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

void context::stack_trace_handler(interpreter_error& err,
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
    for(auto& entry: stack_trace)
    {
        // resolve the offset to a function name.
        std::string func_name;

        auto loader_it = loaders.find(entry.mod_name);
        if(loader_it != loaders.end())
        {
            func_name = loader_it->second->resolve_entry_point(entry.entry_point)
                          .value_or(fmt::format("<unknown at {}>", entry.entry_point));
        }

        buf += fmt::format("  in {}.{}\n", entry.mod_name, func_name);
    }
    return buf;
}

/** Function argument writing and destruction. */
class arguments_scope
{
    /** The arguments to manage. */
    const std::vector<value>& args;

    /** The locals. */
    std::vector<std::byte>& locals;

public:
    /**
     * Construct an argument scope. Validates the argument types and creates
     * them in `locals`.
     *
     * @param ctx The associated interpreter context.
     * @param args The arguments to verify and write.
     * @param arg_types The argument types to validate against.
     * @param locals The locals storage to write into.
     */
    arguments_scope(context& ctx,
                    const std::vector<value>& args,
                    const std::vector<std::pair<module_::variable_type, bool>>& arg_types,
                    std::vector<std::byte>& locals)
    : args{args}
    , locals{locals}
    {
        if(arg_types.size() != args.size())
        {
            throw interpreter_error(
              fmt::format("Argument count does not match: Expected {}, got {}.",
                          arg_types.size(), args.size()));
        }

        std::size_t offset = 0;
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            if(std::get<0>(arg_types[i]) != std::get<0>(args[i].get_type()))
            {
                throw interpreter_error(
                  fmt::format("Argument {} has wrong base type (expected '{}', got '{}').",
                              i,
                              std::get<0>(arg_types[i]).base_type(),
                              std::get<0>(args[i].get_type())));
            }

            if(std::get<1>(arg_types[i]) != std::get<1>(args[i].get_type()))
            {
                throw interpreter_error(
                  fmt::format("Argument {} has wrong array property (expected '{}', got '{}').",
                              i,
                              std::get<1>(arg_types[i]), std::get<1>(args[i].get_type())));
            }

            if(offset + args[i].get_size() > locals.size())
            {
                throw interpreter_error(fmt::format(
                  "Stack overflow during argument allocation while processing argument {}.", i));
            }

            if(is_garbage_collected(args[i].get_type()))
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
        for(std::size_t i = 0; i < args.size(); ++i)
        {
            if(offset + args[i].get_size() > locals.size())
            {
                DEBUG_LOG("Stack overflow during argument destruction while processing argument {}.", i);

                // FIXME This is an error, but destructors cannot throw.
                return;
            }

            offset += args[i].destroy(&locals[offset]);
        }
    }
};

value context::exec(
  const module_loader& loader,
  const function& f,
  std::vector<value> args)
{
    /*
     * allocate locals and decode arguments.
     */
    stack_frame frame{
      loader.get_module().get_header().constants,
      f.get_locals_size(),
      f.get_stack_size()};

    auto& arg_types = f.get_signature().arg_types;
    arguments_scope arg_scope{*this, args, arg_types, frame.locals};

    /*
     * Execute the function.
     */
    opcode ret_opcode;
    if(f.is_native())
    {
        auto& func = f.get_function();
        func(frame.stack);
        ret_opcode = f.get_return_opcode();
    }
    else
    {
        ret_opcode = exec(loader, f.get_entry_point(), f.get_size(), f.get_locals(), frame);
    }

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
        std::string* s = frame.stack.pop_addr<std::string>();
        ret = value{*s};    // copy string
        gc.remove_temporary(s);
    }
    else if(ret_opcode == opcode::aret)
    {
        void* addr = frame.stack.pop_addr<void>();
        ret = value{addr};
        // FIXME The caller is responsible for calling `gc.remove_temporary(addr)`.
    }
    else
    {
        throw interpreter_error(fmt::format(
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

void context::register_native_function(const std::string& mod_name, std::string fn_name, std::function<void(operand_stack&)> func)
{
    if(!func)
    {
        throw interpreter_error(fmt::format("Cannot register null native function '{}.{}'.", mod_name, fn_name));
    }

    auto loader_it = loaders.find(mod_name);
    if(loader_it != loaders.end())
    {
        if(loader_it->second->has_function(fn_name))
        {
            throw interpreter_error(
              fmt::format(
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
        if(native_mod_it->second.find(fn_name) != native_mod_it->second.end())
        {
            throw interpreter_error(fmt::format("Cannot register native function: '{}' is already definded for module '{}'.", fn_name, mod_name));
        }

        native_mod_it->second.insert({std::move(fn_name), std::move(func)});
    }
}

module_loader* context::resolve_module(const std::string& import_name, std::shared_ptr<instruction_recorder> recorder)
{
    auto it = loaders.find(import_name);
    if(it != loaders.end())
    {
        return it->second.get();
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
    return loaders[import_name].get();
}

std::string context::get_import_name(const module_loader& loader) const
{
    auto it = std::find_if(loaders.cbegin(), loaders.cend(),
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

value context::invoke(const std::string& module_name, const std::string& function_name, std::vector<value> args)
{
    try
    {
        DEBUG_LOG("invoke: {}.{}", module_name, function_name);

        module_loader* loader = resolve_module(module_name);
        function& fn = loader->get_function(function_name);

        return exec(*loader, fn, std::move(args));
    }
    catch(interpreter_error& e)
    {
        // Update the error message with the stack trace.
        std::string buf = e.what();

        auto stack_trace = e.get_stack_trace();
        if(stack_trace.size() > 0)
        {
            buf += fmt::format("\n{}", stack_trace_to_string(stack_trace));
        }

        if(call_stack_level == 0)
        {
            // we never entered context::exec, so add the function explicitly.
            buf += fmt::format("  in {}.{}\n", module_name, function_name);
        }

        throw interpreter_error(buf, stack_trace);
    }
}

value context::invoke(const module_loader& loader, const function& fn, std::vector<value> args)
{
    try
    {
        return exec(loader, fn, std::move(args));
    }
    catch(interpreter_error& e)
    {
        // Update the error message with the stack trace.
        std::string buf = e.what();

        auto stack_trace = e.get_stack_trace();
        if(stack_trace.size() > 0)
        {
            buf += fmt::format("\n{}", stack_trace_to_string(stack_trace));
        }

        if(call_stack_level == 0)
        {
            // we never entered context::exec, so add the function explicitly.
            std::string module_name = get_import_name(loader);
            std::string function_name = loader.resolve_entry_point(fn.get_entry_point())
                                          .value_or(fmt::format("<unknown at {}>", fn.get_entry_point()));

            buf += fmt::format("  in {}.{}\n", module_name, function_name);
        }

        throw interpreter_error(buf, stack_trace);
    }
}

}    // namespace slang::interpreter