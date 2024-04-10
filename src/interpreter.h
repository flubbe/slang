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

/*
 * Forward declarations.
 */
namespace slang
{
class language_module;
}    // namespace slang

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

/** Result type. */
enum class result_type
{
    rt_void,
    rt_int,
    rt_float,
    rt_string,
    rt_aggregate
};

/** Result from calling a function. */
struct call_result
{
    /** The result. */
    std::variant<int, float, std::string> result;

    /** The result type. */
    result_type type{result_type::rt_void};

    /** Default constructors. */
    call_result() = default;
    call_result(const call_result&) = default;
    call_result(call_result&&) = default;

    /** Default assignments. */
    call_result& operator=(const call_result&) = default;
    call_result& operator=(call_result&&) = default;

    /**
     * Construct a `call_result` from an integer.
     *
     * @param i The integer.
     */
    call_result(int i)
    : result{i}
    , type{result_type::rt_int}
    {
    }

    /**
     * Construct a `call_result` from a float.
     *
     * @param f The float.
     */
    call_result(float f)
    : result{f}
    , type{result_type::rt_float}
    {
    }

    /**
     * Construct a `call_result` from a string.
     *
     * @param s The string.
     */
    call_result(std::string s)
    : result{s}
    , type{result_type::rt_string}
    {
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
    call_result pop_result()
    {
        static_assert(
          !std::is_same<T, int>::value
            && !std::is_same<T, float>::value
            && !std::is_same<T, std::string>::value,
          "exec_stack::pop_result is only available for int, float and std::string.");
    }

    /** Pop the call result from the stack. */
    template<>
    call_result pop_result<int>()
    {
        return {pop_i32()};
    }

    /** Pop the call result from the stack. */
    template<>
    call_result pop_result<float>()
    {
        return {pop_f32()};
    }

    /** Pop the call result from the stack. */
    template<>
    call_result pop_result<std::string>()
    {
        throw std::runtime_error("exec_stack::pop_result<std::string> not implemented.");
    }
};

/** Interpreter context. */
class context
{
    /** Loaded modules. */
    std::unordered_map<std::string, language_module> module_map;

protected:
    /**
     * Execute a function.
     */
    call_result exec(const language_module& mod, std::size_t offset);

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
     * TODO arguments and return value.
     *
     * @param module_name Name of the module of the function.
     * @param name The function's name.
     */
    call_result invoke(const std::string& module_name, const std::string& function_name);
};

}    // namespace slang::interpreter