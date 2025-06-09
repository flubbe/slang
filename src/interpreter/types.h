/**
 * slang - a simple scripting language.
 *
 * interpreter type definitions.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <any>
#include <cstring>
#include <exception>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "shared/module.h"
#include "shared/opcodes.h"
#include "value.h"
#include "vector.h"

namespace slang::interpreter
{

/** Entries of a stack trace. */
struct stack_trace_entry
{
    /** Name of the module containing the function. */
    std::string mod_name;

    /** Function entry point. */
    std::size_t entry_point;

    /** Offset. */
    std::size_t offset;

    /** Defaulted constructors. */
    stack_trace_entry() = default;
    stack_trace_entry(const stack_trace_entry&) = default;
    stack_trace_entry(stack_trace_entry&&) = default;

    /**
     * Initializing constructor.
     *
     * @param mod_name The module name.
     * @param entry_point Entry point of the function for which the stack is unwound.
     * @param offset Offset in the function for which the stack is unwound.
     */
    stack_trace_entry(std::string mod_name, std::size_t entry_point, std::size_t offset)
    : mod_name{std::move(mod_name)}
    , entry_point{entry_point}
    , offset{offset}
    {
    }

    /** Defaulted assignments. */
    stack_trace_entry& operator=(const stack_trace_entry&) = default;
    stack_trace_entry& operator=(stack_trace_entry&&) = default;
};

/** Interpreter error. */
class interpreter_error : public std::runtime_error
{
    /** Call stack. */
    std::vector<stack_trace_entry> stack_trace;

public:
    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     */
    explicit interpreter_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     * @param stack_trace A stack trace to be passed along.
     */
    interpreter_error(const std::string& message, const std::vector<stack_trace_entry>& stack_trace)
    : std::runtime_error{message}
    , stack_trace{stack_trace}
    {
    }

    /**
     * Add a stack trace entry.
     *
     * @param mod_name The module name.
     * @param entry_point Entry point of the function for which the stack is unwound.
     * @param offset Offset in the function for which the stack is unwound.
     */
    void add_stack_trace_entry(std::string module_name, std::size_t entry_point, std::size_t offset)
    {
        stack_trace.emplace_back(module_name, entry_point, offset);
    }

    /** Get the stack trace. */
    const std::vector<stack_trace_entry>& get_stack_trace() const
    {
        return stack_trace;
    }
};

/** Operand stack. */
class operand_stack
{
protected:
    /** The stack. */
    std::vector<std::uint8_t> stack;

    /** Maximal size. */
    std::size_t max_size;

public:
    /** Defaulted and deleted constructors. */
    operand_stack() = delete;
    operand_stack(const operand_stack&) = default;
    operand_stack(operand_stack&&) = default;

    /** Default assignments. */
    operand_stack& operator=(const operand_stack&) = default;
    operand_stack& operator=(operand_stack&&) = default;

    /**
     * Construct a stack with a maximal size.
     *
     * @param max_size The maximal stack size.
     */
    explicit operand_stack(std::size_t max_size)
    : max_size{max_size}
    {
        stack.reserve(max_size);
    }

    /** Check if the stack is empty. */
    bool empty() const noexcept
    {
        return stack.empty();
    }

    /** Get the current stack size. */
    std::size_t size() const noexcept
    {
        return stack.size();
    }

    /** Get the maximal stack size. */
    std::size_t get_max_size() const noexcept
    {
        return max_size;
    }

    /** Duplicate the top i32 on the stack. */
    void dup_i32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(
          stack.end(),
          stack.end() - 4,
          stack.end());
    }

    /** Duplicate the top f32 on the stack. */
    void dup_f32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(
          stack.end(),
          stack.end() - 4,
          stack.end());
    }

    /** Duplicate the top address on the stack. */
    void dup_addr()
    {
        if(stack.size() < sizeof(void*))
        {
            throw interpreter_error("Stack underflow.");
        }

        if(stack.size() + sizeof(void*) > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }

        stack.insert(
          stack.end(),
          stack.end() - sizeof(void*),
          stack.end());
    }

    /** Duplicate a memory block of size `size1` and insert it at `size1 + size2` from the top. */
    void dup_x1(std::size_t size1, std::size_t size2)
    {
        if(stack.size() < size1 + size2)
        {
            throw interpreter_error("Invalid stack access.");
        }

        std::vector<std::uint8_t> copy = {stack.end() - size1, stack.end()};
        stack.insert(
          stack.end() - size1 - size2,
          copy.begin(),
          copy.end());
    }

    /** Duplicate a memory block of size `size1` and insert it at `size1 + size2 + size3` from the top. */
    void dup_x2(std::size_t size1, std::size_t size2, std::size_t size3)
    {
        if(stack.size() < size1 + size2 + size3)
        {
            throw interpreter_error("Invalid stack access.");
        }

        std::vector<std::uint8_t> copy = {stack.end() - size1, stack.end()};
        stack.insert(
          stack.end() - size1 - size2 - size3,
          copy.begin(),
          copy.end());
    }

    /**
     * Push an i32 onto the stack.
     *
     * @param i The integer.
     */
    void push_i32(std::int32_t i)
    {
        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(
          stack.end(),
          reinterpret_cast<std::uint8_t*>(&i),
          reinterpret_cast<std::uint8_t*>(&i) + 4);
    }

    /**
     * Push an f32 onto the stack.
     *
     * @param f The floating-point value.
     */
    void push_f32(float f)
    {
        if(stack.size() + 4 > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(
          stack.end(),
          reinterpret_cast<std::uint8_t*>(&f),
          reinterpret_cast<std::uint8_t*>(&f) + 4);
    }

    /**
     * Push an address onto the stack.
     *
     * @param addr The address.
     */
    template<typename T>
    void push_addr(const T* addr)
    {
        if(stack.size() + sizeof(const T*) > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(
          stack.end(),
          reinterpret_cast<std::uint8_t*>(&addr),
          reinterpret_cast<std::uint8_t*>(&addr) + sizeof(const T*));
    }

    /**
     * Push another stack onto this stack.
     *
     * @param other The other stack.
     */
    void push_stack(const operand_stack& other)
    {
        if(stack.size() + other.stack.size() > max_size)
        {
            throw interpreter_error("Stack overflow.");
        }
        stack.insert(
          stack.end(),
          other.stack.begin(),
          other.stack.end());
    }

    /** Pop an i32 from the stack. */
    std::int32_t pop_i32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        std::int32_t i;
        std::memcpy(&i, &stack[stack.size() - 4], 4);
        stack.resize(stack.size() - 4);
        return i;
    }

    /** Pop an f32 from the stack. */
    float pop_f32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        float f;
        std::memcpy(&f, &stack[stack.size() - 4], 4);
        stack.resize(stack.size() - 4);
        return f;
    }

    /** Modify the top value on the stack in-place. */
    template<typename T, typename U>
        requires(sizeof(T) == sizeof(U)
                 && std::is_scalar_v<T> && std::is_scalar_v<U>)
    void modify_top(std::function<U(T)> func)
    {
        if(stack.size() < sizeof(T))
        {
            throw interpreter_error("Stack underflow");
        }

        T v;
        std::memcpy(&v, &stack[stack.size() - sizeof(T)], sizeof(T));
        U u = func(v);
        std::memcpy(&stack[stack.size() - sizeof(U)], &u, sizeof(U));
    }

    /** Pop an address from the stack. */
    template<typename T>
    T* pop_addr()
    {
        if(stack.size() < sizeof(T*))
        {
            throw interpreter_error("Stack underflow.");
        }

        T* addr;
        std::memcpy(&addr, &stack[stack.size() - sizeof(T*)], sizeof(T*));
        stack.resize(stack.size() - sizeof(T*));
        return addr;
    }

    /**
     * Get a pointer to the end minus an offset.
     *
     * @param offset Offset to subtract from the end.
     * @throws Throws an `interpreter_error` if a stack underflow occurs.
     */
    std::uint8_t* end(std::size_t offset = 0)
    {
        if(offset > stack.size())
        {
            throw interpreter_error("Stack underflow");
        }
        return &stack[stack.size() - offset];
    }

    /**
     * Discard bytes.
     *
     * @param byte_count The count of bytes to discard.
     * @throws Throws an `interpreter_error` if a stack underflow occurs.
     */
    void discard(std::size_t byte_count)
    {
        if(byte_count > stack.size())
        {
            throw interpreter_error("Stack underflow");
        }
        stack.resize(stack.size() - byte_count);
    }
};

/** A function. */
class function
{
    /** The interpreter context. */
    context& ctx;

    /** Module loader for this function. */
    module_loader& loader;

    /** Function signature. */
    module_::function_signature signature;

    /** Whether this is a native function. */
    bool native;

    /** Entry point (offset into binary) or function pointer for native functions. */
    std::variant<std::size_t, std::function<void(operand_stack&)>> entry_point_or_function;

    /** Bytecode size for interpreted functions. */
    std::size_t size;

    /** Return opcode. */
    opcode ret_opcode;

    /** Locals. Not serialized. */
    std::vector<module_::variable_descriptor> locals;

    /** Argument and locals size. Not serialized. */
    std::size_t locals_size = 0;

    /** Stack size. Not serialized. */
    std::size_t stack_size = 0;

public:
    /** Defaulted and deleted constructors. */
    function() = delete;
    function(const function&) = delete;
    function(function&&) = default;

    /** Default and deleted assignments. */
    function& operator=(const function&) = delete;
    function& operator=(function&&) = delete;

    /**
     * Construct a function.
     *
     * @param ctx Interpreter context.
     * @param loader The module loader for this function.
     * @param signature The function's signature.
     * @param entry_point The function's entry point, given as an offset into a module binary.
     * @param size The bytecode size.
     * @param locals Local variables.
     * @param locals_size The arguments and locals size.
     * @param stack_size The operand stack size.
     */
    function(
      context& ctx,
      module_loader& loader,
      module_::function_signature signature,
      std::size_t entry_point,
      std::size_t size,
      std::vector<module_::variable_descriptor> locals,
      std::size_t locals_size,
      std::size_t stack_size);

    /**
     * Construct a native function.
     *
     * @param ctx Interpreter context.
     * @param loader The module loader for this function.
     * @param signature The function's signature.
     * @param func An std::function.
     */
    function(
      context& ctx,
      module_loader& loader,
      module_::function_signature signature,
      std::function<void(operand_stack&)> func);

    /**
     * Invoke the function.
     *
     * @param args Function arguments.
     * @return Result of the invocation.
     */
    value invoke(const std::vector<value>& args);

    /**
     * Invoke the function.
     *
     * @param args Function arguments.
     * @return Result of the invocation.
     */
    value operator()(const std::vector<value>& args)
    {
        return invoke(args);
    }

    /**
     * Invoke the function.
     *
     * @param args Function arguments.
     * @return Result of the invocation.
     */
    template<typename... Args>
    value operator()(Args&&... args)
    {
        return invoke(move_into_value_vector(std::make_tuple(args...)));
    }

    /** Get the function signature. */
    const module_::function_signature& get_signature() const
    {
        return signature;
    }

    /** Return whether this is a native function. */
    bool is_native() const
    {
        return native;
    }

    /** Get the function's entry point. */
    std::size_t get_entry_point() const
    {
        return std::get<std::size_t>(entry_point_or_function);
    }

    /** Get a std::function. */
    const std::function<void(operand_stack&)>& get_function() const
    {
        return std::get<std::function<void(operand_stack&)>>(entry_point_or_function);
    }

    /** Get the bytecode size. */
    std::size_t get_size() const
    {
        return size;
    }

    /** Get the function's locals. */
    const std::vector<module_::variable_descriptor>& get_locals() const
    {
        return locals;
    }

    /** Get the local's size. */
    std::size_t get_locals_size() const
    {
        return locals_size;
    }

    /** Get the operand stack size. */
    std::size_t get_stack_size() const
    {
        return stack_size;
    }

    /** Get the return opcode. */
    opcode get_return_opcode() const
    {
        return ret_opcode;
    }
};

/** A stack frame. */
struct stack_frame
{
    /** Constant table reference. */
    const std::vector<module_::constant_table_entry>& constants;

    /** Locals and arguments. */
    std::vector<std::byte> locals;

    /** The operand stack. */
    operand_stack stack;

    /** Default constructors. */
    stack_frame() = delete;
    stack_frame(const stack_frame&) = delete;
    stack_frame(stack_frame&&) = default;

    /** Default assignments. */
    stack_frame& operator=(const stack_frame&) = delete;
    stack_frame& operator=(stack_frame&&) = delete;

    /**
     * Construct a stack frame.
     *
     * @param constants Reference to the module constant table.
     * @param locals_size Size to allocate for the locals.
     * @param stack_size The operand stack size.
     */
    stack_frame(const std::vector<module_::constant_table_entry>& constants,
                std::size_t locals_size,
                std::size_t stack_size)
    : constants{constants}
    , locals{locals_size}
    , stack{stack_size}
    {
    }
};

/** Type properties. */
struct type_properties
{
    /** Type flags. */
    std::size_t flags{0};

    /** Type size. */
    std::size_t size{0};

    /** Type alignment. */
    std::size_t alignment{0};

    /** Type layout id (always `0` for non-struct types). */
    std::size_t layout_id{0};
};

/** Type properties. */
struct field_properties
{
    /** Field size. */
    std::size_t size{0};

    /** Field offset. */
    std::size_t offset{0};

    /** Whether this is a garbage collected field. */
    bool needs_gc{false};
};

}    // namespace slang::interpreter
