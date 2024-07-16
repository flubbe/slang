/**
 * slang - a simple scripting language.
 *
 * garbage collector.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "vector.h"
#include "gc.h"

#ifdef GC_DEBUG
#    define GC_LOG(...) fmt::print("GC: {}\n", fmt::format(__VA_ARGS__))
#else
#    define GC_LOG(...)
#endif

namespace si = slang::interpreter;

namespace slang::gc
{

void garbage_collector::mark_object(void* obj)
{
    auto it = objects.find(obj);
    if(it == objects.end())
    {
        // nothing to mark.
        return;
    }

    gc_object& obj_info = it->second;
    if(obj_info.flags & gc_object::of_reachable)
    {
        return;
    }

    GC_LOG("mark_object {}", obj);
    obj_info.flags |= gc_object::of_reachable;

    if(obj_info.type == gc_object_type::array_str || obj_info.type == gc_object_type::array_aref)
    {
        auto array = static_cast<si::fixed_vector<void*>*>(obj_info.addr);
        for(void*& ref: *array)
        {
            mark_object(ref);
        }
    }
}

/**
 * Delete an object and track the allocated bytes.
 *
 * @tparam T The object type.
 * @param obj The object to delete.
 * @param allocated_bytes The currently allocated bytes to by updated after the deletion.
 * @throws Throws a `gc_error` if the object size is larger than `allocated_bytes`.
 */
template<typename T>
void object_deleter(void* obj, std::size_t& allocated_bytes)
{
    delete static_cast<T*>(obj);

    if(sizeof(T) > allocated_bytes)
    {
        throw gc_error("Inconsistent allocation stats: sizeof(T) > allocated_bytes");
    }
    allocated_bytes -= sizeof(T);
}

void garbage_collector::delete_object(gc_object& obj_info)
{
    GC_LOG("delete_object {}", obj_info.addr);

    if(obj_info.type == gc_object_type::str)
    {
        object_deleter<std::string>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_i32)
    {
        object_deleter<si::fixed_vector<std::int32_t>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_f32)
    {
        object_deleter<si::fixed_vector<float>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_str)
    {
        object_deleter<si::fixed_vector<std::string*>>(obj_info.addr, allocated_bytes);
    }
    else if(obj_info.type == gc_object_type::array_aref)
    {
        object_deleter<si::fixed_vector<void*>>(obj_info.addr, allocated_bytes);
    }
    else
    {
        throw gc_error(fmt::format("Invalid type '{}' for GC array.", static_cast<std::size_t>(obj_info.type)));
    }
}

void* garbage_collector::add_root(void* obj, std::uint32_t flags)
{
    GC_LOG("add root {}, flags {}", obj, flags);

    auto it = root_set.find(obj);
    if(it == root_set.end())
    {
        root_set.insert({obj, 1});
    }
    else
    {
        ++it->second;
    }

    return obj;
}

void garbage_collector::remove_root(void* obj)
{
    GC_LOG("remove_root {}", obj);

    auto it = root_set.find(obj);
    if(it == root_set.end())
    {
        throw gc_error(fmt::format("Cannot remove root for object at {}, since it does not exist in the GC root set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(fmt::format("Negative reference count for GC root {}", obj));
    }
    --it->second;

    GC_LOG("            ref_count {}", it->second);

    if(it->second == 0)
    {
        root_set.erase(it);
    }
}

void garbage_collector::run()
{
    std::size_t object_set_size = objects.size();

    // collect roots.
    std::set<void*> current_root_set;
    for(auto& [obj, ref_count]: root_set)
    {
        current_root_set.insert(obj);
    }
    for(auto& [obj, ref_count]: temporary_objects)
    {
        current_root_set.insert(obj);
    }

    // mark objects.
    for(void* obj: current_root_set)
    {
        mark_object(obj);
    }

    // free unreachable objects.
    for(auto& [obj, obj_info]: objects)
    {
        if(obj_info.flags & gc_object::of_reachable)
        {
            continue;
        }

        GC_LOG("collecting {}", obj);
        delete_object(obj_info);
    }

    for(auto it = objects.begin(); it != objects.end();)
    {
        if(!(it->second.flags & gc_object::of_reachable))
        {
            it = objects.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for(auto& [obj, obj_info]: objects)
    {
        obj_info.flags &= ~gc_object::of_reachable;
    }

    if(object_set_size < objects.size())
    {
        throw gc_error(fmt::format("Object list grew during GC run: {} -> {}", object_set_size, objects.size()));
    }

    GC_LOG("run: {} -> {}, {} bytes allocated", object_set_size, objects.size(), allocated_bytes);
    GC_LOG("----- objects -----");
    for(auto& [obj, obj_info]: objects)
    {
        GC_LOG("     obj {}, type {}, flags {}", obj, to_string(obj_info.type), obj_info.flags);
    }
    GC_LOG("------ roots ------");
    for(auto& [obj, ref_count]: root_set)
    {
        GC_LOG("     obj {}, ref_count {}", obj, ref_count);
    }
    GC_LOG("--- temporaries ---");
    for(auto& [obj, ref_count]: temporary_objects)
    {
        GC_LOG("     obj {}, ref_count {}", obj, ref_count);
    }
    GC_LOG("-------------------");
}

void garbage_collector::reset()
{
    std::size_t object_count = objects.size();

    root_set.clear();
    temporary_objects.clear();

    for(auto& [obj, obj_info]: objects)
    {
        delete_object(obj_info);
    }
    objects.clear();

    GC_LOG("reset {} -> 0", object_count);
}

void* garbage_collector::add_temporary(void* obj)
{
    GC_LOG("add_temporary {}", obj);

    auto it = temporary_objects.find(obj);
    if(it == temporary_objects.end())
    {
        temporary_objects.insert({obj, 1});
    }
    else
    {
        ++it->second;
    }

    return obj;
}

void garbage_collector::remove_temporary(void* obj)
{
    GC_LOG("remove_temporary {}", obj);

    auto it = temporary_objects.find(obj);
    if(it == temporary_objects.end())
    {
        throw gc_error(fmt::format("Reference at {} does not exists in GC temporary object set.", obj));
    }

    if(it->second == 0)
    {
        throw gc_error(fmt::format("Temporary at {} has no references.", obj));
    }

    --it->second;
    if(it->second == 0)
    {
        temporary_objects.erase(it);
    }
}

}    // namespace slang::gc
