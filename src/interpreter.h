/**
 * slang - a simple scripting language.
 *
 * interpreter.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

#include "module.h"

namespace slang::interpreter
{

/** Interpreter error. */
class interpreter_error : public std::runtime_error
{
public:
    /**
     * Construct a `interpreter_error`.
     *
     * @param message The error message.
     */
    interpreter_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/** Result and argument type. */
using value = std::variant<int, float, std::string>;

/** A function. */
class function
{
    /** Function signature. */
    function_signature signature;

    /** Entry point (offset into binary). */
    std::size_t entry_point;

public:
    /** Default constructors. */
    function() = default;
    function(const function&) = default;
    function(function&&) = default;

    /** Default assignments. */
    function& operator=(const function&) = default;
    function& operator=(function&&) = default;

    /**
     * Construct a function.
     *
     * @param signature The function's signature.
     * @param entry_point The function's entry point, given as an offset into a module binary.
     */
    function(function_signature signature, std::size_t entry_point)
    : signature{std::move(signature)}
    , entry_point{entry_point}
    {
    }

    /** Get the function signature. */
    const function_signature& get_signature() const
    {
        return signature;
    }

    /** Get the function's entry point. */
    const std::size_t get_entry_point() const
    {
        return entry_point;
    }
};

/** Execution stack. */
class exec_stack
{
protected:
    /** The stack. */
    std::vector<std::uint8_t> stack;

public:
    /** Default constructors. */
    exec_stack() = default;
    exec_stack(const exec_stack&) = default;
    exec_stack(exec_stack&&) = default;

    /** Default assignments. */
    exec_stack& operator=(const exec_stack&) = default;
    exec_stack& operator=(exec_stack&&) = default;

    /** Check if the stack is empty. */
    inline bool empty() const noexcept
    {
        return stack.empty();
    }

    /**
     * Push an i32 onto the stack.
     *
     * @param i The integer.
     */
    void push_i32(std::int32_t i)
    {
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&i), reinterpret_cast<std::uint8_t*>(&i) + 4);
    }

    /**
     * Push an f32 onto the stack.
     *
     * @param F The floating-point value.
     */
    void push_i32(float f)
    {
        stack.insert(stack.end(), reinterpret_cast<std::uint8_t*>(&f), reinterpret_cast<std::uint8_t*>(&f) + 4);
    }

    /** Pop an i32 from the stack. */
    std::int32_t pop_i32()
    {
        if(stack.size() < 4)
        {
            throw interpreter_error("Stack underflow.");
        }

        std::int32_t i = *reinterpret_cast<std::int32_t*>(&stack[stack.size() - 4]);
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

        float f = *reinterpret_cast<float*>(&stack[stack.size() - 4]);
        stack.resize(stack.size() - 4);
        return f;
    }

    /** Disallow popping from stack for non-(int, float, std::string) types. */
    template<typename T>
    value pop_result()
    {
        static_assert(
          !std::is_same<T, int>::value
            && !std::is_same<T, float>::value
            && !std::is_same<T, std::string>::value,
          "exec_stack::pop_result is only available for int, float and std::string.");
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<int>()
    {
        return {pop_i32()};
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<float>()
    {
        return {pop_f32()};
    }

    /** Pop the call result from the stack. */
    template<>
    value pop_result<std::string>()
    {
        throw std::runtime_error("exec_stack::pop_result<std::string> not implemented.");
    }
};

/** Interpreter context. */
class context
{
    /** Loaded modules. */
    std::unordered_map<std::string, language_module> module_map;

    /** Functions, ordered by module and name. */
    std::unordered_map<std::string, std::unordered_map<std::string, function>> function_map;

protected:
    /**
     * Execute a function.
     *
     * @param mod The function's module.
     * @param f The function to execute.
     * @param args The function's arguments.
     * @return The function's return value.
     */
    value exec(const language_module& mod, const function& f, const std::vector<value>& args);

public:
    /** Default constructors. */
    context() = default;
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = default;
    context& operator=(context&&) = default;

    /**
     * Load a module.
     *
     * @param name The module name.
     * @param mod The module.
     */
    void load_module(const std::string& name, const language_module& mod);

    /**
     * Invoke a function from a module.
     *
     * @param module_name Name of the module of the function.
     * @param function_name The function's name.
     * @param args The function's arguments.
     * @returns The function's return value.
     */
    value invoke(const std::string& module_name, const std::string& function_name, const std::vector<value>& args);
};

}    // namespace slang::interpreter