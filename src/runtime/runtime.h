/**
 * slang - a simple scripting language.
 *
 * runtime.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

/* Forward declarations. */
namespace slang::interpreter
{
class operand_stack;
class context;
}    // namespace slang::interpreter

namespace slang::runtime
{

namespace si = slang::interpreter;

/*
 * Stack / GC helpers.
 */

/** Non-templated helper for getting temporary garbage collected objects from the stack. */
class gc_object_base
{
protected:
    /** Interpreter context reference. */
    si::context& ctx;

    /** The object. */
    void* obj{nullptr};

public:
    /** Deleted constructors. */
    gc_object_base() = delete;
    gc_object_base(const gc_object_base&) = delete;

    /** Move constructor. */
    gc_object_base(gc_object_base&& other) noexcept
    : ctx{other.ctx}
    , obj{other.obj}
    {
        other.obj = nullptr;
    }

    /**
     * Construct a new `gc_object_base` holding a pointer to an object.
     *
     * @param ctx The interpreter context.
     * @param obj The object.
     */
    gc_object_base(si::context& ctx, void* obj)
    : ctx{ctx}
    , obj{obj}
    {
    }

    /** Destructor. */
    ~gc_object_base();

    /** Get the contained object. */
    void* get()
    {
        return obj;
    }

    /** Get the contained object. */
    const void* get() const
    {
        return obj;
    }
};

/** Helper for getting temporary garbage collected objects from the stack. */
template<typename T>
class gc_object : public gc_object_base
{
public:
    /** Deleted constructors. */
    gc_object() = delete;
    gc_object(const gc_object&) = delete;

    /** Move constructor. */
    gc_object(gc_object&& other) noexcept
    : gc_object_base{other}
    {
    }

    /** Move `gc_object_base` into `gc_object`. */
    gc_object(gc_object_base&& other) noexcept
    : gc_object_base{std::move(other)}
    {
    }

    /**
     * Construct a new `gc_object` holding a pointer to an object.
     *
     * @param ctx The interpreter context.
     * @param obj The object.
     */
    gc_object(si::context& ctx, void* obj)
    : gc_object_base{ctx, obj}
    {
    }

    /** Get the contained object. */
    T* get()
    {
        return static_cast<T*>(gc_object_base::get());
    }

    /** Get the contained object. */
    const T* get() const
    {
        return static_cast<const T*>(gc_object_base::get());
    }
};

/**
 * Pop an object from the stack and return it as a `gc_object_base`.
 *
 * @param ctx The interpreter context.
 * @param stack The stack.
 * @return Returns the popped object wrapped in a `gc_object_base`.
 */
gc_object_base gc_pop(si::context& ctx, si::operand_stack& stack);

/*
 * Arrays.
 */

/**
 * Copy an array into another array.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void array_copy(si::context& ctx, si::operand_stack& stack);

/*
 * Strings.
 */

/**
 * Check strings for equality. Pushes `1` onto the stack if the strings are equal, and `0` otherwise.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void string_equals(si::context& ctx, si::operand_stack& stack);

/**
 * Concatenate two strings.
 *
 * @param ctx The interpreter context.
 * @param stack The operand stack.
 */
void string_concat(si::context& ctx, si::operand_stack& stack);

}    // namespace slang::runtime
