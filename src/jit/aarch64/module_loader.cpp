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
#include "package.h"
#include "utils.h"

#include <unordered_set>

namespace ty = slang::typing;

namespace slang::jit::aarch64
{

namespace
{

/** The modules currently being loaded. Used for circular import detection. */
std::unordered_set<std::string>& loading_modules()
{
    static std::unordered_set<std::string> modules;
    return modules;
}

/** Module load guard, used for circular import detection. */
class module_load_guard final
{
    /** Module name. */
    std::string module_name;

public:
    module_load_guard(const module_load_guard&) = delete;
    module_load_guard(module_load_guard&&) = delete;

    module_load_guard& operator=(const module_load_guard&) = delete;
    module_load_guard& operator=(module_load_guard&&) = delete;

    /**
     * Construct a module load guard.
     *
     * @param name The module name.
     * @throws Throws a `jit_error` if the module is already in the process
     *     of being loaded.
     */
    explicit module_load_guard(std::string name)
    : module_name{std::move(name)}
    {
        if(!loading_modules().insert(module_name).second)
        {
            throw jit_error{
              std::format(
                "Circular JIT module import involving '{}'.",
                module_name)};
        }
    }

    /** Destructor. */
    ~module_load_guard()
    {
        loading_modules().erase(module_name);
    }
};

}    // namespace

/**
 * Check if a type is garbage collected.
 *
 * FIXME duplicated from the interpreter's module_loader.
 *
 * @param t The type string.
 * @returns Return whether a type is garbage collected.
 */
bool is_garbage_collected(
  const module_::variable_type& t) noexcept
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
 * FIXME duplicated from the interpreter's module_loader.
 *
 * @param info The type info.
 * @returns Return whether a type is garbage collected.
 */
static bool is_garbage_collected(
  const slang::module_::field_descriptor& info) noexcept
{
    return info.base_type.is_array() || is_garbage_collected(info.base_type);
}

/** Byte sizes and alignments for built-in types. */
// FIXME duplicated from the interpreter's module_loader.
static const auto& type_properties_map()
{
    static const std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> map{
      {"void", {0, 0}},
      {"i8", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},     // cat1
      {"i16", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},    // cat1
      {"i32", {sizeof(std::int32_t), std::alignment_of_v<std::int32_t>}},    // cat1
      {"i64", {sizeof(std::int64_t), std::alignment_of_v<std::int64_t>}},    // cat2
      {"f32", {sizeof(float), std::alignment_of_v<float>}},                  // cat1
      {"f64", {sizeof(double), std::alignment_of_v<double>}},                // cat2
      {"str", {sizeof(std::string*), std::alignment_of_v<std::string*>}},    // ref
    };

    return map;
};

/**
 * Get the size of a type.
 *
 * @param type The type.
 * @returns Returns the type's size, in bytes.
 */
static std::size_t get_type_size(
  const module_::variable_type& type)
{
    if(is_garbage_collected(type))
    {
        return sizeof(void*);
    }

    auto built_in_it = type_properties_map().find(type.base_type());
    if(built_in_it != type_properties_map().end())
    {
        return built_in_it->second.first;
    }

    return sizeof(void*);
}

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
                if(auto import_index = member_type.base_type.get_import_index();
                   import_index.has_value())
                {
                    // load the package containing the type definition.

                    std::size_t index = import_index.value();
                    if(index >= header.imports.size())
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve size for type '{}': Invalid import index {}.",
                            member_type.base_type.base_type(),
                            index)};
                    }

                    if(header.imports.at(index).type != module_::symbol_type::type)
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve size for type '{}': Import table entry {} is not a type.",
                            member_type.base_type.base_type(),
                            index)};
                    }

                    const auto package_index = header.imports.at(index).package_index;
                    if(package_index >= header.imports.size()
                       || header.imports.at(package_index).type != module_::symbol_type::package)
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve size for type '{}': Import table entry {} is not a package.",
                            member_type.base_type.base_type(),
                            package_index)};
                    }

                    const auto* imported_module = std::get<const module_loader*>(
                      header.imports.at(package_index).export_reference);
                    if(imported_module == nullptr
                       || !imported_module->struct_map.contains(member_type.base_type.base_type()))
                    {
                        throw jit_error{
                          std::format(
                            "Cannot resolve imported type '{}' from package '{}'.",
                            member_type.base_type.base_type(),
                            header.imports.at(package_index).name)};
                    }
                }

                // Struct values are represented as GC-managed pointers.
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

        if(!layout.empty()
           && runtime_context == nullptr)
        {
            throw jit_error{
              std::format(
                "Cannot register GC layout for type '{}': no JIT runtime context was provided.",
                name)};
        }

        if(runtime_context != nullptr)
        {
            auto& gc = runtime_context->get_gc();
            const auto type_name = interpreter::make_type_name(import_name, name);

            // FIXME add method to check for type layout and avoid try-catch.
            try
            {
                desc.layout_id = gc.get_type_layout_id(type_name);
                gc.check_type_layout(type_name, layout);
            }
            catch(const gc::gc_error&)
            {
                desc.layout_id = gc.register_type_layout(type_name, std::move(layout));
            }
        }
        else
        {
            desc.layout_id = 0;
        }
    }
}

module_loader::module_loader(
  file_manager& file_mgr,
  std::string import_name,
  fs::path path,
  interpreter::context* runtime_context)
: file_mgr{file_mgr}
, runtime_context{runtime_context}
, import_name{std::move(import_name)}
, path{std::move(path)}
{
    module_load_guard load_guard{this->import_name};

    auto read_ar = file_mgr.open(this->path, slang::file_manager::open_mode::read);
    module_::module_header serialized_header;
    (*read_ar) & serialized_header;
    (*read_ar) & binary;

    header.exports = std::move(serialized_header.exports);
    header.constants = std::move(serialized_header.constants);
    header.imports.reserve(serialized_header.imports.size());
    for(auto& import: serialized_header.imports)
    {
        header.imports.push_back({
          .type = import.type,
          .name = std::move(import.name),
          .package_index = import.package_index,
        });
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

    // resolve exports, imports and structs.
    auto call_target_table = create_export_call_target_table();
    resolve_imports();
    decode_structs();

    // compile the module.
    compile_functions(call_target_table);
}

std::vector<jit_call_target*> module_loader::create_export_call_target_table()
{
    call_targets.reserve(header.exports.size());

    std::vector<jit_call_target*> call_target_table;
    call_target_table.reserve(header.exports.size());

    for(auto& symbol: header.exports)
    {
        if(symbol.type != module_::symbol_type::function)
        {
            call_targets.push_back(nullptr);
            call_target_table.push_back(nullptr);
            continue;
        }

        const auto& desc = std::get<module_::function_descriptor>(symbol.desc);

        auto target = std::make_unique<jit_call_target>();
        target->constants = &header.constants;
        target->return_size = get_type_size(desc.signature.return_type);

        for(const auto& argument_type: desc.signature.arg_types)
        {
            target->argument_size += get_type_size(argument_type);
        }

        if(desc.native)
        {
            target->native_library = std::get<module_::native_function_details>(desc.details).library_name;
        }

        if(runtime_context != nullptr)
        {
            target->safepoint = [ctx = runtime_context]()
            {
                auto& gc = ctx->get_gc();
                if(gc.is_run_requested())
                {
                    gc.run();
                }
            };
        }

        call_target_table.push_back(target.get());
        call_targets.push_back(std::move(target));
    }
    return call_target_table;
}

void module_loader::resolve_imports()
{
    import_call_targets.resize(
      header.imports.size(),
      nullptr);

    for(auto& import: header.imports)
    {
        if(import.type != module_::symbol_type::package)
        {
            continue;
        }

        auto& imported_module = resolve_module(import.name);
        import.export_reference = &imported_module;
    }

    for(std::size_t import_index = 0;
        import_index < header.imports.size();
        ++import_index)
    {
        auto& import = header.imports[import_index];

        if(import.type == module_::symbol_type::package)
        {
            continue;
        }

        if(import.package_index >= header.imports.size())
        {
            throw jit_error{
              std::format(
                "Import '{}' has invalid package index {}.",
                import.name,
                import.package_index)};
        }

        const auto& package_import = header.imports.at(import.package_index);
        if(package_import.type != module_::symbol_type::package)
        {
            throw jit_error{
              std::format(
                "Import '{}' refers to a non-package entry.",
                import.name)};
        }

        const auto& imported_module = *std::get<const module_loader*>(package_import.export_reference);
        auto export_it = std::ranges::find_if(
          imported_module.header.exports,
          [&import](const module_::exported_symbol& symbol)
          { return symbol.name == import.name; });
        if(export_it == imported_module.header.exports.end())
        {
            throw jit_error{
              std::format(
                "Module '{}' does not export imported symbol '{}'.",
                package_import.name,
                import.name)};
        }
        if(export_it->type != import.type)
        {
            throw jit_error{
              std::format(
                "Imported symbol '{}' has type '{}', expected '{}'.",
                import.name,
                module_::to_string(export_it->type),
                module_::to_string(import.type))};
        }

        import.export_reference = &*export_it;
        if(import.type == module_::symbol_type::function)
        {
            const auto export_index = static_cast<std::size_t>(
              std::distance(imported_module.header.exports.begin(), export_it));
            import_call_targets.at(import_index) = imported_module.call_targets.at(export_index).get();
        }
    }
}

void module_loader::compile_functions(
  const std::vector<jit_call_target*>& call_target_table)
{
    for(std::size_t symbol_index = 0;
        symbol_index < header.exports.size();
        ++symbol_index)
    {
        auto& symbol = header.exports.at(symbol_index);
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
              std::format(
                "Invalid bytecode range for function '{}'.",
                symbol.name)};
        }

        std::span<const std::byte> bytecode{
          binary.begin() + static_cast<std::ptrdiff_t>(details.offset),
          details.size};

        std::vector<std::size_t> local_offsets;
        local_offsets.reserve(details.locals.size());

        std::size_t locals_size = 0;
        for(const auto& local: details.locals)
        {
            local_offsets.push_back(locals_size);
            if(is_garbage_collected(local.type))
            {
                call_target_table.at(symbol_index)->gc_local_offsets.push_back(locals_size);
            }
            locals_size += get_type_size(local.type);
        }

        auto resolve_type = [this](std::int64_t type_index) -> module_::struct_descriptor
        {
            const module_loader* type_loader = this;
            std::string type_name;

            if(type_index < 0)
            {
                const auto import_index = static_cast<std::size_t>(-(type_index + 1));
                if(import_index >= header.imports.size())
                {
                    throw jit_error{
                      std::format(
                        "Type import index {} is out of range.",
                        import_index)};
                }

                const auto& type_import = header.imports.at(import_index);
                if(type_import.type != module_::symbol_type::type
                   || type_import.package_index >= header.imports.size())
                {
                    throw jit_error{
                      "JIT type index does not refer to an imported type."};
                }

                const auto& package_import = header.imports.at(type_import.package_index);
                if(package_import.type != module_::symbol_type::package)
                {
                    throw jit_error{
                      "JIT type import refers to a non-package entry."};
                }

                type_loader = std::get<const module_loader*>(package_import.export_reference);
                type_name = type_import.name;
            }
            else
            {
                const auto export_index = static_cast<std::size_t>(type_index);
                if(export_index >= header.exports.size()
                   || header.exports.at(export_index).type != module_::symbol_type::type)
                {
                    throw jit_error{
                      std::format(
                        "Type export index {} is invalid.",
                        export_index)};
                }

                type_name = header.exports.at(export_index).name;
            }

            const auto type_it = type_loader->struct_map.find(type_name);
            if(type_it == type_loader->struct_map.end())
            {
                throw jit_error{std::format("JIT type '{}' was not decoded.", type_name)};
            }

            return type_it->second;
        };

        jit_function compiled = [&]
        {
            try
            {
                return jit_compiler::compile(
                  bytecode,
                  local_offsets,
                  locals_size,
                  call_target_table,
                  import_call_targets,
                  resolve_type);
            }
            catch(const jit_error& e)
            {
                throw jit_error{
                  std::format(
                    "Failed to compile '{}.{}': {}",
                    import_name,
                    symbol.name,
                    e.what())};
            }
        }();

        auto* target = call_targets.at(symbol_index).get();
        target->function = compiled.get();
        target->locals_size = compiled.get_locals_size();
        target->stack_size = compiled.get_stack_size();

        function_map.emplace(symbol.name, std::move(compiled));
    }
}

module_loader::module_loader(
  module_loader&& other) noexcept
: file_mgr{other.file_mgr}
, runtime_context{other.runtime_context}
, import_name{std::move(other.import_name)}
, path{std::move(other.path)}
, header{std::move(other.header)}
, binary{std::move(other.binary)}
, struct_map{std::move(other.struct_map)}
, function_map{std::move(other.function_map)}
, call_targets{std::move(other.call_targets)}
, imported_modules{std::move(other.imported_modules)}
, import_call_targets{std::move(other.import_call_targets)}
{
    for(auto& target: call_targets)
    {
        if(target != nullptr)
        {
            target->constants = &header.constants;
        }
    }
}

module_loader& module_loader::resolve_module(
  const std::string& module_name)
{
    if(module_name == import_name)
    {
        return *this;
    }

    auto existing = imported_modules.find(module_name);
    if(existing != imported_modules.end())
    {
        return *existing->second;
    }

    std::string relative_path = module_name;
    utils::replace_all(relative_path, package::delimiter, "/");
    fs::path module_path{relative_path};
    if(!module_path.has_extension())
    {
        module_path.replace_extension(package::module_ext);
    }

    auto imported = std::make_unique<module_loader>(
      file_mgr,
      module_name,
      file_mgr.resolve(module_path),
      runtime_context);
    auto* imported_ptr = imported.get();

    imported_modules.emplace(module_name, std::move(imported));
    return *imported_ptr;
}

jit_function& module_loader::get_function(const std::string& name)
{
    auto it = function_map.find(name);
    if(it == function_map.end())
    {
        throw jit_error{
          std::format(
            "Function '{}' not found in module '{}'.",
            name,
            import_name)};
    }

    return it->second;
}

void module_loader::register_native_function(
  const std::string& library_name,
  const std::string& function_name,
  const std::function<void(interpreter::operand_stack&)>& function)
{
    if(!function)
    {
        throw jit_error{
          "Cannot register an empty native function."};
    }

    const auto registrations = register_native_function_recursive(
      library_name,
      function_name,
      function);
    if(registrations == 0)
    {
        throw jit_error{
          std::format(
            "Native function '{}' not found in module tree.",
            function_name)};
    }
}

void module_loader::register_native_functions(
  const std::function<native_function_type(const std::string&, const std::string&)>& resolver)
{
    if(!resolver)
    {
        throw jit_error{
          "Cannot register native functions without a resolver."};
    }

    for(std::size_t i = 0; i < header.exports.size(); ++i)
    {
        const auto& symbol = header.exports.at(i);
        if(symbol.type != module_::symbol_type::function)
        {
            continue;
        }

        const auto& desc = std::get<module_::function_descriptor>(symbol.desc);
        if(!desc.native)
        {
            continue;
        }

        auto& target = *call_targets.at(i);
        if(std::holds_alternative<native_function_type>(target.function))
        {
            continue;
        }

        if(!target.native_library.has_value())
        {
            throw jit_error{
              std::format(
                "Unable to resolve '{}': No library name available.",
                symbol.name)};
        }

        target.function = resolver(symbol.name, *target.native_library);
    }

    for(auto& [name, imported_module]: imported_modules)
    {
        imported_module->register_native_functions(resolver);
    }
}

std::size_t module_loader::register_native_function_recursive(
  const std::string& library_name,
  const std::string& function_name,
  const std::function<void(interpreter::operand_stack&)>& function)
{
    std::size_t registrations = 0;
    for(std::size_t i = 0; i < header.exports.size(); ++i)
    {
        const auto& symbol = header.exports.at(i);
        if(symbol.type != module_::symbol_type::function
           || symbol.name != function_name)
        {
            continue;
        }

        const auto& desc = std::get<module_::function_descriptor>(symbol.desc);
        if(!desc.native)
        {
            continue;
        }

        auto& target = *call_targets.at(i);
        if(target.native_library != library_name)
        {
            continue;
        }

        if(std::holds_alternative<native_function_type>(target.function))
        {
            throw jit_error{
              std::format(
                "Native function '{}' is already registered.",
                function_name)};
        }

        target.function = function;
        ++registrations;
    }

    for(auto& [name, imported_module]: imported_modules)
    {
        registrations += imported_module->register_native_function_recursive(
          library_name,
          function_name,
          function);
    }

    return registrations;
}

}    // namespace slang::jit::aarch64
