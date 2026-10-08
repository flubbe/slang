/**
 * slang - a simple scripting language.
 *
 * Module loader using the Just In Time compiler for AArch64.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include "jit/aarch64.h"
#include "shared/type_utils.h"

namespace ty = slang::typing;

namespace slang::jit::aarch64
{

bool is_garbage_collected(const module_::variable_type& t) noexcept
{
    if(t.base_type() == "void")
    {
        return false;
    }

    // check for built-in non-gc types.
    return t.is_array() || ty::is_reference_type(t.base_type());
}

/**
 * Check if a field is garbage collected.
 *
 * @param info The type info.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(const slang::module_::field_descriptor& info) noexcept
{
    return info.base_type.is_array() || is_garbage_collected(info.base_type);
}

/** Byte sizes and alignments for built-in types. */
static const auto& type_properties_map()
{
    static const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> map{
      {"void", {0, 0}},
      {"i8", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},      // cat1
      {"i16", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},     // cat1
      {"i32", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},     // cat1
      {"i64", {sizeof(std::int64_t), std::alignment_of_v<std::int64_t>}},     // cat2
      {"f32", {sizeof(float), std::alignment_of_v<float>}},                   // cat1
      {"f64", {sizeof(double), std::alignment_of_v<double>}},                 // cat2
      {"str", {sizeof(std::string*), std::alignment_of_v<std::string*>}}};    // ref

    return map;
};

void module_loader::decode_structs()
{
    for(auto& [name, desc]: struct_map)
    {
        std::size_t size = 0;
        std::size_t alignment = 0;
        std::vector<std::size_t> layout;
        int offset = 0;

        for(auto& [member_name, member_type]: desc.member_types)
        {
            bool add_to_layout = false;

            // check that the type exists and get its properties.
            auto built_in_it = type_properties_map().find(member_type.base_type.base_type());
            if(built_in_it != type_properties_map().end())
            {
                if(is_garbage_collected(member_type))
                {
                    member_type.size = sizeof(void*);
                    member_type.alignment = std::alignment_of_v<void*>;

                    add_to_layout = true;
                }
                else
                {
                    member_type.size = built_in_it->second.first;
                    member_type.alignment = built_in_it->second.second;
                }
            }
            else
            {
                if(auto import_index = member_type.base_type.get_import_index(); import_index.has_value())
                {
                    std::size_t index = import_index.value();
                    if(index >= header.imports.size())
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve size for type '{}': Invalid import index {}.",
                            member_type.base_type.base_type(),
                            index)};
                    }

                    if(header.imports[index].type != module_::symbol_type::type)
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve size for type '{}': Import table entry {} is not a type.",
                            member_type.base_type.base_type(),
                            index)};
                    }

                    throw jit_error{
                      std::format(
                        "Cannot resolve imported type '{}': JIT module resolution is not implemented.",
                        member_type.base_type.base_type())};
                }

                // size and alignment are the same for both array and non-array types.
                member_type.size = sizeof(void*);
                member_type.alignment = std::alignment_of_v<void*>;

                add_to_layout = true;
            }

            // store offset.
            member_type.offset = utils::align(member_type.alignment, offset);

            // calculate member offset as `size_after - size_before`.
            offset -= utils::numeric_cast<int>(size);

            // update struct size and alignment.
            size += member_type.size;
            size = utils::align(member_type.alignment, size);

            alignment = std::max(alignment, member_type.alignment);

            // calculate member offset as `size_after - size_before`.
            offset += utils::numeric_cast<int>(size);

            // update type layout.
            if(add_to_layout)
            {
                layout.push_back(member_type.offset);
            }
        }

        // trailing padding.
        size = utils::align(alignment, size);

        // store type size and alignment.
        desc.size = size;
        desc.alignment = alignment;

        if(!layout.empty())
        {
            throw jit_error{
              std::format("Cannot register GC layout for type '{}': JIT GC integration is not implemented.", name)};
        }
        desc.layout_id = 0;
    }
}

module_loader::module_loader(
  file_manager& file_mgr,
  std::string import_name,
  fs::path path)
: import_name{std::move(import_name)}
, path{std::move(path)}
{
    auto read_ar = file_mgr.open(this->path, slang::file_manager::open_mode::read);
    module_::module_header serialized_header;
    (*read_ar) & serialized_header;
    (*read_ar) & binary;

    header.exports = std::move(serialized_header.exports);
    header.constants = std::move(serialized_header.constants);
    header.imports.reserve(serialized_header.imports.size());
    for(auto& import: serialized_header.imports)
    {
        header.imports.push_back({.type = import.type,
                                  .name = std::move(import.name),
                                  .package_index = import.package_index});
    }

    // populate type map before compiling the module.
    for(auto& it: header.exports)
    {
        if(it.type != module_::symbol_type::type)
        {
            continue;
        }

        struct_map.insert({it.name, std::get<module_::struct_descriptor>(it.desc)});
    }
    decode_structs();

    for(auto& symbol: header.exports)
    {
        if(symbol.type != module_::symbol_type::function)
        {
            continue;
        }

        auto& desc = std::get<module_::function_descriptor>(symbol.desc);
        if(desc.native)
        {
            continue;
        }

        auto& details = std::get<module_::function_details>(desc.details);
        if(details.offset > binary.size() || details.size > binary.size() - details.offset)
        {
            throw jit_error{
              std::format("Invalid bytecode range for function '{}'.", symbol.name)};
        }

        std::vector<std::byte> bytecode{
          binary.begin() + static_cast<std::ptrdiff_t>(details.offset),
          binary.begin() + static_cast<std::ptrdiff_t>(details.offset + details.size)};
        function_map.emplace(symbol.name, jit_compiler::compile(bytecode));
    }
}

jit_function& module_loader::get_function(const std::string& name)
{
    auto it = function_map.find(name);
    if(it == function_map.end())
    {
        throw jit_error{std::format("Function '{}' not found in module '{}'.", name, import_name)};
    }

    return it->second;
}

}    // namespace slang::jit::aarch64
