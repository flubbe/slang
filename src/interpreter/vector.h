/**
 * slang - a simple scripting language.
 *
 * a vector class with fixed-size heap allocated element count and memory.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <type_traits>

namespace slang::interpreter
{

/** A vector class. Not resizable. */
template<typename T>
class fixed_vector
{
public:
    using value_type = T;
    using reference = value_type&;
    using const_reference = const value_type&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    using iterator = pointer;
    using const_iterator = const_pointer;

private:
    /** Vector data stored on the heap. */
    struct vector_data
    {
        /** Element count. */
        size_type size;

        /** Beginning of allocated data. */
        value_type data[1];
    };

    /** Custom deleter for the vector. */
    struct vector_deleter
    {
        void operator()(vector_data* v) const
        {
            // operator should be no-op if applied to nullptr.
            if(v != nullptr)
            {
                std::destroy(v->data, v->data + v->size);
                std::free(v);
            }
        }
    };

    using data_pointer = std::unique_ptr<vector_data, vector_deleter>;
    data_pointer data_ptr;

    /**
     * Allocate a `std::unique_ptr` for the vector.
     *
     * @param n The element count.
     * @param uninitialized Whether to leave the allocated memory uninitialized.
     * @returns Returns a `std::unique_ptr` to the allocated memory.
     * @throws Throws `std::bad_alloc` if allocation fails.
     */
    static data_pointer allocate(size_type n, bool uninitialized = false)
    {
        if(n == 0)
        {
            return {nullptr};
        }

        std::size_t byte_size = sizeof(data_pointer) + static_cast<std::size_t>(n * sizeof(value_type));
        auto ptr = std::malloc(byte_size);
        if(ptr == nullptr)
        {
            throw std::bad_alloc();
        }

        auto data_ptr = data_pointer{static_cast<vector_data*>(ptr)};
        data_ptr->size = n;

        if(!uninitialized)
        {
            std::uninitialized_default_construct_n(data_ptr->data, data_ptr->size);
        }

        return data_ptr;
    }

public:
    /** Default constructor. Does not allocate. */
    fixed_vector() noexcept = default;

    /** Construct a `fixed_vector` with the contents of `init`. */
    fixed_vector(std::initializer_list<T> init)
    : data_ptr{allocate(init.size(), true)}
    {
        std::uninitialized_move(init.begin(), init.end(), begin());
    }

    /** Copy constructor. */
    fixed_vector(fixed_vector const& other)
    : data_ptr{allocate(other.size(), true)}
    {
        std::uninitialized_copy(other.begin(), other.end(), begin());
    }

    /** Move constructor. */
    fixed_vector(fixed_vector&& other) noexcept
    {
        other.swap(*this);
    }

    /** Default destructor. */
    ~fixed_vector() = default;

    /** Copy assignment. */
    fixed_vector& operator=(const fixed_vector& other)
    {
        if(size() != other.size())
        {
            data_ptr = allocate(other.size(), true);
        }
        std::uninitialized_copy(other.begin(), other.end(), begin());
        return *this;
    }

    /** Move assignment. */
    fixed_vector& operator=(fixed_vector&& other) noexcept
    {
        other.swap(*this);
        return *this;
    }

    /** Construct a vector with `n` uninitialized elements. */
    explicit fixed_vector(size_type n)
    : data_ptr{allocate(n)}
    {
    }

    /** Swaps the contents. */
    void swap(fixed_vector& other) noexcept
    {
        std::swap(data_ptr, other.data_ptr);
    }

    /** Returns an iterator to the first element of the vector. */
    iterator begin() noexcept
    {
        return data_ptr ? data_ptr->data : nullptr;
    }

    /** Returns an iterator to the first element of the vector. */
    const_iterator begin() const noexcept
    {
        return data_ptr ? data_ptr->data : nullptr;
    }

    /** Returns an iterator to the first element of the vector. */
    const_iterator cbegin() const noexcept
    {
        return data_ptr ? data_ptr->data : nullptr;
    }

    /** Returns an iterator to the end. */
    iterator end() noexcept
    {
        return begin() + size();
    }

    /** Returns an iterator to the end. */
    const_iterator end() const noexcept
    {
        return begin() + size();
    }

    /** Returns an iterator to the end. */
    const_iterator cend() const noexcept
    {
        return cbegin() + size();
    }

    /** Checks whether the container is empty. */
    bool empty() const noexcept
    {
        return size() == 0;
    }

    /**
     * Return the number of elements.
     *
     * @note Casting to a different `fixed_vector<T>` does not affect the returned size,
     *       but the resulting vector is otherwise in an invalid state.
     */
    size_type size() const noexcept
    {
        return data_ptr ? data_ptr->size : 0;
    }

    /**
     * Returns the maximum possible number of elements.
     *
     * @note Casting to a different `fixed_vector<T>` does not affect the returned max size,
     *       but the resulting vector is otherwise in an invalid state.
     */
    size_type max_size() const noexcept
    {
        return size();
    }

    /**
     * Returns the number of elements that can be held in currently allocated storage.
     *
     * @note Casting to a different `fixed_vector<T>` does not affect the returned capacity,
     *       but the resulting vector is otherwise in an invalid state.
     */
    size_type capacity() const noexcept
    {
        return size();
    }

    /** Access specified element with bounds checking. */
    reference at(size_type pos)
    {
        if(pos >= size())
        {
            throw std::out_of_range("fixed_vector");
        }
        return data_ptr->data[pos];
    }

    /** Access specified element with bounds checking. */
    const_reference at(size_type pos) const
    {
        if(pos >= size())
        {
            throw std::out_of_range("fixed_vector");
        }
        return data_ptr->data[pos];
    }

    /** Access specified element. */
    reference operator[](size_type n)
    {
        assert(n < size());
        return data_ptr->data[n];
    }

    /** Access specified element. */
    const_reference operator[](size_type n) const
    {
        assert(n < size());
        return data_ptr->data[n];
    }

    /** Returns a reference to the first element in the container. */
    reference front()
    {
        // NOTE Calling front on an empty container causes undefined behavior.
        return data_ptr->data[0];
    }

    /** Returns a reference to the first element in the container. */
    const_reference front() const
    {
        // NOTE Calling front on an empty container causes undefined behavior.
        return data_ptr->data[0];
    }

    /** Returns a reference to the last element in the container. */
    reference back()
    {
        // NOTE Calling back on an empty container causes undefined behavior.
        return data_ptr->data[data_ptr->size - 1];
    }

    /** Returns a reference to the last element in the container. */
    const_reference back() const
    {
        // NOTE Calling back on an empty container causes undefined behavior.
        return data_ptr->data[data_ptr->size - 1];
    }

    /** Returns pointer to the underlying array serving as element storage. */
    pointer data() noexcept
    {
        return data_ptr ? data_ptr->data : nullptr;
    }

    /** Returns pointer to the underlying array serving as element storage. */
    const_pointer data() const noexcept
    {
        return data_ptr ? data_ptr->data : nullptr;
    }
};

/**
 * Estimate the heap byte size of a heap-allocated container.
 *
 * @tparam Container Container type.
 * @param c The container.
 * @returns Estimated heap byte size of the container.
 */
template<typename Container>
    requires requires(const Container& t) {
        typename Container::size_type;
        typename Container::value_type;
        { t.capacity() } noexcept -> std::convertible_to<typename Container::size_type>;
    }
auto estimate_heap_byte_size(const Container& c) -> typename Container::size_type
{
    return static_cast<typename Container::size_type>(
      sizeof(Container) + c.capacity() * sizeof(typename Container::value_type));
}

}    // namespace slang::interpreter
