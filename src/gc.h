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
    enum gc_flags : std::uint32_t
    {
        of_none = 0,          /** No flags. */
        of_visited = 1,       /** Whether this object was visited, e.g. during collection or reference counting. */
        of_reachable = 2,     /** Reachable from root set. */
        of_never_collect = 4, /** Never collect this object, even when resetting (e.g. for externally managed objects). */
        of_temporary = 8,     /** A temporary object (i.e., not stored in a variable). */
    };

    /** Object type. */
    gc_object_type type;

    /** Flags. */
    std::uint32_t flags{of_none};

    /** Object address. */
    void* addr{nullptr};

    /** Create an object from a type. */
    template<typename T>
    static gc_object from(T* obj, std::uint32_t flags = of_none)
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
  std::string* obj, std::uint32_t flags)
{
    return {gc_object_type::str, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<std::int32_t>>(
  std::vector<std::int32_t>* obj, std::uint32_t flags)
{
    return {gc_object_type::array_i32, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<float>>(
  std::vector<float>* obj, std::uint32_t flags)
{
    return {gc_object_type::array_f32, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<std::string*>>(
  std::vector<std::string*>* obj, std::uint32_t flags)
{
    return {gc_object_type::array_str, flags, obj};
}

template<>
inline gc_object gc_object::from<std::vector<void*>>(
  std::vector<void*>* obj, std::uint32_t flags)
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

    /**
     * Add array to garbage collected set.
     *
     * @param array The array.
     * @param array_type The array type.
     * @param flags Flags.
     */
    void add_array_root(void* array, array_type type, std::uint32_t flags = gc_object::of_none);

    /**
     * Set GC object flags.
     *
     * @param obj The object to set the flags for.
     * @param flags The new flags to set.
     * @param propagate Whether to propagate flags to referenced objects (e.g. entries in an array).
     */
    void set_flags(void* obj, std::uint32_t flags, bool propagate = false);

    /**
     * Clear GC object flags.
     *
     * @param obj The object.
     * @param flags The flags to clear.
     * @param propagate Whether to propagate flags to references objects (e.g. entries in an array).
     */
    void clear_flags(void* obj, std::uint32_t flags, bool propagate = false);

    /** Run garbage collector. */
    void run();

    /** Reset the garbage collector. */
    void reset();

    /**
     * Add an object to the root set.
     *
     * @param s The object.
     * @param flags Flags.
     * @returns Returns the input object.
     */
    void* add_root(void* s, std::uint32_t flags = gc_object::of_none);

    /**
     * Add a string to the root set.
     *
     * @param s The string.
     * @param flags Flags.
     * @returns Returns the input string.
     */
    std::string* add_root(std::string* s, std::uint32_t flags = gc_object::of_none);

    /**
     * Add array to root set.
     *
     * @param array The array.
     * @param flags Flags.
     * @returns Returns the input array.
     */
    template<typename T>
    std::vector<T>* add_root(std::vector<T>* array, std::uint32_t flags = gc_object::of_none)
    {
        static_assert(!std::is_same_v<T, std::int32_t>
                        && !std::is_same_v<T, float>
                        && !std::is_same_v<T, std::string*>
                        && !std::is_same_v<T, void*>,
                      "No GC implementation for array type.");
    }

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
                return static_cast<T*>(add_temporary(static_cast<void*>(obj)));
            }
            return add_root(obj, flags);
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
        return add_root(array, flags);
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
     * @note This needs to be called on string and array types that are
     * passed to native functions.
     *
     * @param obj The object.
     * @throws Throws a `gc_error` if the object is not found in the object list.
     */
    void remove_temporary(void* obj);

    /**
     * Explicitly delete an object allocated with the garbage collector.
     * The object must me marked as `gc_object::of_never_collect`.
     *
     * @throws Throws a `gc_error` if the object is not part of the root set
     *         or not marked as `gc_object::of_never_collect`.
     *
     * @param obj The object to delete.
     * @param recursive Whether the function should recursively be called for array entries.
     */
    void explicit_delete(void* obj, bool recursive = true);

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

/*
 * Specializations for garbage collector.
 */

template<>
inline std::vector<std::int32_t>* garbage_collector::add_root<std::int32_t>(
  std::vector<std::int32_t>* array, std::uint32_t flags)
{
    add_array_root(reinterpret_cast<void*>(array), array_type::i32, flags);
    return array;
}

template<>
inline std::vector<float>* garbage_collector::add_root<float>(
  std::vector<float>* array, std::uint32_t flags)
{
    add_array_root(reinterpret_cast<void*>(array), array_type::f32, flags);
    return array;
}

template<>
inline std::vector<std::string*>* garbage_collector::add_root<std::string*>(
  std::vector<std::string*>* array, std::uint32_t flags)
{
    add_array_root(reinterpret_cast<void*>(array), array_type::str, flags);
    return array;
}

template<>
inline std::vector<void*>* garbage_collector::add_root<void*>(
  std::vector<void*>* array, std::uint32_t flags)
{
    add_array_root(reinterpret_cast<void*>(array), array_type::ref, flags);
    return array;
}

}    // namespace slang::gc
