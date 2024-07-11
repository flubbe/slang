/**
 * slang - a simple scripting language.
 *
 * garbage collector.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <vector>
#include <set>
#include <unordered_map>

#include "module.h"

namespace slang::gc
{

/** Garbage collection error. */
class gc_error : public std::runtime_error
{
public:
    /**
     * Construct a `gc_error`.
     *
     * @param message The error message.
     */
    gc_error(const std::string& message)
    : std::runtime_error{message}
    {
    }
};

/**
 * Garbage collector object type.
 */
enum class gc_object_type : std::uint8_t
{
    str,
    array_i32,
    array_f32,
    array_str,
    array_aref,
};

/** Convert GC object type to string. */
inline std::string to_string(gc_object_type type)
{
    switch(type)
    {
    case gc_object_type::str: return "str";
    case gc_object_type::array_i32: return "array_i32";
    case gc_object_type::array_f32: return "array_f32";
    case gc_object_type::array_str: return "array_str";
    case gc_object_type::array_aref: return "array_aref";
    }

    return "unknown";
}

/** Garbage collector object. */
struct gc_object
{
    /** Flags. */
    enum gc_flags : std::uint8_t
    {
        of_none = 0,      /** No flags. */
        of_reachable = 1, /** Reachable from root set. */
        of_temporary = 2, /** A temporary object (i.e., not stored in a variable). */
    };

    /** Object type. */
    gc_object_type type;

    /** Flags. */
    std::uint8_t flags{of_none};

    /** Object address. */
    void* addr{nullptr};

    /** Create an object from a type. */
    template<typename T>
    static gc_object from(T* obj, std::uint8_t flags = of_none)
    {
        static_assert(
          !std::is_same_v<T, std::string*>
            && !std::is_same_v<T, std::vector<std::int32_t>*>
            && !std::is_same_v<T, std::vector<float>*>
            && !std::is_same_v<T, std::vector<std::string*>*>
            && !std::is_same_v<T, std::vector<void*>*>,
          "Cannot create GC object from type.");
    }
};

template<>
inline gc_object gc_object::from<std::string>(
  std::string* obj, std::uint8_t flags)
{
    return {gc_object_type::str, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<std::int32_t>>(
  std::vector<std::int32_t>* obj, std::uint8_t flags)
{
    return {gc_object_type::array_i32, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<float>>(
  std::vector<float>* obj, std::uint8_t flags)
{
    return {gc_object_type::array_f32, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<std::string*>>(
  std::vector<std::string*>* obj, std::uint8_t flags)
{
    return {gc_object_type::array_str, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<void*>>(
  std::vector<void*>* obj, std::uint8_t flags)
{
    return {gc_object_type::array_aref, flags, obj};
}

/** Garbage collector. */
class garbage_collector
{
    /** All allocated objects. */
    std::unordered_map<void*, gc_object> objects;

    /** Roots. */
    std::unordered_map<void*, std::size_t> root_set;

    /** Reference-counted temporary objects. */
    std::unordered_map<void*, std::size_t> temporary_objects;

    /** Allocated bytes. */
    std::size_t allocated_bytes{0};

    /**
     * Mark object as reachable.
     *
     * @note No-op if `obj` is not in the object list.
     *
     * @param obj The object to mark.
     */
    void mark_object(void* obj);

    /**
     * Delete an object.
     *
     * @param obj_info Info for fhe object to delete.
     */
    void delete_object(gc_object& obj_info);

public:
    /** Defaulted and deleted constructors. */
    garbage_collector() = default;
    garbage_collector(const garbage_collector&) = delete;
    garbage_collector(garbage_collector&&) = default;

    /** Defaulted and deleted assignment operators. */
    garbage_collector& operator=(const garbage_collector&) = delete;
    garbage_collector& operator=(garbage_collector&&) = default;

    /** Destructor. */
    virtual ~garbage_collector()
    {
        reset();
    }

    /** Run garbage collector. */
    void run();

    /** Reset the garbage collector and free all allocated memory. */
    void reset();

    /**
     * Add an object to the root set.
     *
     * @param obj The object.
     * @param flags Flags.
     * @returns Returns the input object.
     */
    void* add_root(void* obj, std::uint32_t flags = gc_object::of_none);

    /**
     * Remove an object from the root set.
     *
     * @param obj The object.
     * @throws Throws a `gc_error` if the object is not a root.
     */
    void remove_root(void* obj);

    /**
     * Allocate a new garbage collected variable and optionally add it to the root set or temporary set.
     *
     * @param flags Flags.
     * @param add Whether to add the object to the root set or temporary set (depending on the flags).
     * @returns Returns a (pointer to a) garbage collected variable.
     */
    template<typename T>
    T* gc_new(std::uint32_t flags = gc_object::of_none, bool add = true)
    {
        allocated_bytes += sizeof(T);

        T* obj = new T;
        if(objects.find(obj) != objects.end())
        {
            throw gc_error("Allocated object already exists.");
        }
        objects.insert({obj, gc_object::from(obj, flags)});

        if(add)
        {
            if(flags & gc_object::of_temporary)
            {
                return static_cast<T*>(add_temporary(obj));
            }
            return static_cast<T*>(add_root(obj, flags));
        }

        return obj;
    }

    /**
     * Allocate a new garbage collected array and add it to the root set.
     *
     * @param size The array size.
     * @param flags Flags.
     * @returns Returns a (pointer to a) garbage collected array.
     */
    template<typename T>
    std::vector<T>* gc_new_array(std::size_t size, std::uint32_t flags = gc_object::of_none)
    {
        auto array = new std::vector<T>();

        if(objects.find(array) != objects.end())
        {
            throw gc_error("Allocated object already exists.");
        }
        objects.insert({array, gc_object::from(array, flags)});

        allocated_bytes += sizeof(std::vector<T>);
        array->resize(size);

        if(flags & gc_object::of_temporary)
        {
            return static_cast<std::vector<T>*>(add_temporary(array));
        }
        return static_cast<std::vector<T>*>(add_root(array, flags));
    }

    /**
     * Add a temporary object. If the object already exists in the
     * temporary objects set, its reference count is increased.
     *
     * @param obj The object.
     * @returns The object.
     * @throws Throws a `gc_error` if the object is not found in the object list.
     */
    void* add_temporary(void* obj);

    /**
     * Remove a temporary object (or decrease it's reference count).
     *
     * @note This needs to be called on `string` and `array` types, that are
     * 1. passed to native functions, or
     * 2. returned from `invoke` to native code.
     *
     * @param obj The object.
     * @throws Throws a `gc_error` if the object is not found in the object list.
     */
    void remove_temporary(void* obj);

    /**
     * Check if an object is in the root set.
     *
     * @param obj The object.
     * @returns True if the object is found in the root set and false otherwise.
     */
    bool is_root(void* obj) const
    {
        return root_set.find(obj) != root_set.end();
    }

    /**
     * Check if an object is in the temporary set.
     *
     * @param obj The object.
     * @returns True if the object is found in the temporary set and false otherwise.
     */
    bool is_temporary(void* obj) const
    {
        return temporary_objects.find(obj) != temporary_objects.end();
    }

    /** Get allocated object count. */
    std::size_t object_count() const
    {
        return objects.size();
    }

    /** Get the root set size. */
    std::size_t root_set_size() const
    {
        return root_set.size();
    }

    /** Get allocated bytes. */
    std::size_t byte_size() const
    {
        return allocated_bytes;
    }
};

}    // namespace slang::gc
