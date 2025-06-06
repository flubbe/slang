/**
 * slang - a simple scripting language.
 *
 * type system context.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "token.h"
#include "type.h"

/*
 * Forward declartions.
 */
namespace slang::ast
{
class expression;
}    // namespace slang::ast

namespace slang::typing
{

/** Type errors. */
class type_error : public std::runtime_error
{
public:
    /**
     * Construct a type_error.
     *
     * @note Use the other constructor if you want to include location information in the error message.
     *
     * @param message The error message.
     */
    explicit type_error(const std::string& message)
    : std::runtime_error{message}
    {
    }

    /**
     * Construct a type_error.
     *
     * @param loc The error location in the source.
     * @param message The error message.
     */
    type_error(const slang::token_location& loc, const std::string& message);
};

/** A variable type. */
struct variable_type
{
    /** The variable's name. */
    token name;

    /** The variable's type. */
    type_info var_type;

    /** No default constructor. */
    variable_type() = delete;

    /** Default copy and move constructors. */
    variable_type(const variable_type&) = default;
    variable_type(variable_type&&) = default;

    /** Default assignments. */
    variable_type& operator=(const variable_type&) = default;
    variable_type& operator=(variable_type&&) = default;

    /**
     * Construct a new variable type.
     *
     * @param name The variable's name.
     * @param type The variable's type.
     */
    variable_type(token name, type_info var_type)
    : name{std::move(name)}
    , var_type{std::move(var_type)}
    {
    }
};

/** A function signature. */
struct function_signature
{
    /** Name of the function. */
    token name;

    /** Argument types. */
    std::vector<type_info> arg_types;

    /** Return type. */
    type_info ret_type;

    /** The function type (e.g. a combination of `ret_type`, `name` and `arg_types`, together with a type id). */
    type_info func_type;

    /** No default constructor. */
    function_signature() = delete;

    /** Default copy and move constructors. */
    function_signature(const function_signature&) = default;
    function_signature(function_signature&&) = default;

    /** Default assignments. */
    function_signature& operator=(const function_signature&) = default;
    function_signature& operator=(function_signature&&) = default;

    /**
     * Construct a new function_signature.
     *
     * @param name The function's name.
     * @param arg_types The function's argument types.
     * @param ret_type The function's return type.
     * @param func_type The function's type. E.g. a combination of return type, names and arguments, together with a type id.
     */
    function_signature(
      token name,
      std::vector<type_info> arg_types,
      type_info ret_type,
      type_info func_type)
    : name{std::move(name)}
    , arg_types{std::move(arg_types)}
    , ret_type{std::move(ret_type)}
    , func_type{std::move(func_type)}
    {
    }

    /** Return a string representation of the signature. */
    std::string to_string() const;
};

/** A struct. */
struct struct_definition
{
    /** The struct's name. */
    token name;

    /** The struct's members as `(name, type)` pairs. */
    std::vector<std::pair<token, type_info>> members;

    /** An optional import path. */
    std::optional<std::string> import_path;

    /** Default constructors. */
    struct_definition() = default;
    struct_definition(const struct_definition&) = default;
    struct_definition(struct_definition&&) = default;

    /** Default assignments. */
    struct_definition& operator=(const struct_definition&) = default;
    struct_definition& operator=(struct_definition&&) = default;

    /**
     * Construct a struct definition.
     *
     * @param name The struct's name.
     * @param members The struct's members.
     * @param import_path An optional import path.
     */
    struct_definition(
      token name,
      std::vector<std::pair<token, type_info>> members,
      std::optional<std::string> import_path = std::nullopt)
    : name{std::move(name)}
    , members{std::move(members)}
    , import_path{std::move(import_path)}
    {
    }
};

/** A scope. */
struct scope
{
    /** The scope's name. */
    token name;

    /** Parent scope (if any). */
    scope* parent = nullptr;

    /** Child scopes. */
    std::list<scope> children;

    /** Variables. */
    std::unordered_map<std::string, variable_type> variables;

    /** Functions. */
    std::unordered_map<std::string, function_signature> functions;

    /** Structs. */
    std::unordered_map<std::string, struct_definition> structs;

    /** Default constructors. */
    scope() = default;
    scope(const scope&) = default;
    scope(scope&&) = default;

    /** Default assignment. */
    scope& operator=(const scope&) = default;
    scope& operator=(scope&&) = default;

    /**
     * Construct a scope from location and name.
     *
     * @param loc The scope location.
     * @param name The scope name.
     */
    scope(token_location loc, std::string scope_name)
    {
        name.s = std::move(scope_name);
        name.location = std::move(loc);
    }

    /**
     * Construct a scope from name and parent.
     *
     * @param name The scope's name.
     * @param parent The parent scope, if any.
     */
    scope(token name, scope* parent)
    : name{std::move(name)}
    , parent{parent}
    {
    }

    /**
     * Check whether this scope contains a name or a type.
     *
     * @param name The name to check for.
     * @returns True if the name exists in this scope or false if not.
     */
    bool contains(const std::string& name) const
    {
        return variables.find(name) != variables.end()
               || functions.find(name) != functions.end()
               || structs.find(name) != structs.end();
    }

    /**
     * Find the token of a name in this scope.
     *
     * @param name The name to find.
     * @returns If the name is found, returns its token, and std::nullopt otherwise.
     */
    std::optional<token> find(const std::string& name) const
    {
        auto var_it = variables.find(name);
        if(var_it != variables.end())
        {
            return {var_it->second.name};
        }

        auto func_it = functions.find(name);
        if(func_it != functions.end())
        {
            return {func_it->second.name};
        }

        auto struct_it = structs.find(name);
        if(struct_it != structs.end())
        {
            return {struct_it->second.name};
        }

        return std::nullopt;
    }

    /**
     * Get the type of a name in this scope.
     *
     * @param name The name to find.
     * @returns If the name is found, returns its type and `std::nullopt` otherwise.
     */
    std::optional<type_info> get_type(const std::string& name) const
    {
        auto var_it = variables.find(name);
        if(var_it != variables.end())
        {
            return var_it->second.var_type;
        }

        auto func_it = functions.find(name);
        if(func_it != functions.end())
        {
            return func_it->second.func_type;
        }

        return std::nullopt;
    }

    /** Get the qualified scope name. */
    std::string get_qualified_name() const;

    /** Get a string representation of the scope. */
    std::string to_string() const;
};

/** An imported module. */
struct imported_module
{
    /** Module path. */
    std::string path;

    /** Whether this is a transitive import. */
    bool transitive;
};

/** Type system context. */
class context
{
    /** The global scope. */
    scope global_scope = {{1, 1}, "<global>"};

    /** The current scope. */
    scope* current_scope = &global_scope;

    /** The current named scope. */
    std::optional<token> named_scope;

    /** The current anonymous scope id. */
    std::size_t anonymous_scope_id = 0;

    /** Imported modules. */
    std::vector<imported_module> imported_modules;

    /** Imported functions, indexed by `(import_path, function_name)`. */
    std::unordered_map<
      std::string,
      std::unordered_map<
        std::string, function_signature>>
      imported_functions;

    /** Imported constants, indexed by `(import_path, constant_desc)`. */
    std::unordered_map<
      std::string,
      std::vector<variable_type>>
      imported_constants;

    /** Imported macros, indexed by `(import_path, macro_name)`. */
    std::unordered_map<
      std::string,
      std::vector<std::string>>
      imported_macros;

    /** Local macros. */
    std::vector<std::string> macros;

    /** Struct/type stack, for member/type lookups. */
    std::vector<const struct_definition*> struct_stack;

    /** Unresolved types. */
    std::vector<type_info> unresolved_types;

    /** Map of types to type ids. */
    std::vector<std::pair<type_info, std::uint64_t>> type_map;

    /** Base types, stored as `(name, is_reference_type)`. */
    std::vector<std::pair<std::string, bool>> base_types;

    /** The next type id to use. */
    std::uint64_t next_type_id = 0;

    /** Directive stack with entries `(name, restore_function)`. */
    std::vector<std::pair<token, std::function<void(void)>>> directive_stack;

    /** Expression type map. */
    std::unordered_map<const ast::expression*, type_info> expression_types;

    /**
     * Generate a unique type id.
     *
     * @returns A unique type id.
     */
    std::uint64_t generate_type_id()
    {
        return next_type_id++;
    }

    /**
     * Add a base type.
     *
     * @param name The type's name.
     * @param is_reference_type Whether the given type is a reference type.
     * @throws Throws a `type_error` if the type already exists.
     */
    void add_base_type(std::string name, bool is_reference_type);

public:
    /** Default constructor. */
    context()
    {
        // Add null type.
        add_base_type("@null", true);

        //  Initialize the default types `void`, `i32`, `f32`, `str`.
        add_base_type("void", false);
        add_base_type("i32", false);
        add_base_type("f32", false);
        add_base_type("str", true);

        // Add array type.
        add_struct(token{"@array", {0, 0}}, {std::make_pair(token{"length", {0, 0}}, get_type("i32", false))});
    }
    context(const context&) = default;
    context(context&&) = default;

    /** Default assignments. */
    context& operator=(const context&) = default;
    context& operator=(context&&) = default;

    /**
     * Add an import to the context.
     *
     * @throws A `type_error` if the import is not in the global scope.
     * @throws A `type_error` if the import is already added.
     *
     * @param path The import path.
     * @param transitive Whether this is a transitive import.
     */
    void add_import(std::vector<token> path, bool transitive);

    /**
     * Add an import to the context. The function is idempotent, i.e. it
     * can be called multiple times.
     *
     * @throws A `std::runtime_error` if an already-imported non-transitive package
     *         is being re-imported as transitive.
     *
     * @param path The import path.
     * @param transitive Whether this is a transitive import.
     */
    void add_import(std::string path, bool transitive);

    /**
     * Return whether an import is transitive.
     *
     * @throws A `type_error` if the import does not exist.
     *
     * @param namespace_path The namespace path of the module.
     * @returns Return `true` if the import is transitive, `false` otherwise.
     */
    bool is_transitive_import(const std::string& namespace_path) const;

    /**
     * Check if the import exists in the context.
     *
     * @param path The import path.
     * @returns Return `true` if the import exists, `false` otherwise.
     */
    bool has_import(const std::string& path);

    /**
     * Add a variable to the context.
     *
     * FIXME This also handles constants, which mandates having the `import_path` argument.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if the given type is unknown.
     *
     * @param name The variable's name.
     * @param var_type The variable's type.
     * @param import_path An optional import path.
     */
    void add_variable(
      token name,
      type_info var_type,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a function to the context.
     *
     * @throws A type_error if the name already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The function's name.
     * @param arg_types The functions's argument types.
     * @param ret_type The function's return type.
     * @param import_path The import path for the function.
     */
    void add_function(
      token name,
      std::vector<type_info> arg_types,
      type_info ret_type,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Add a struct to the context in the global scope.
     *
     * @throws A type_error if the type already exists in the scope.
     * @throws A type_error if any of the supplied types are unknown.
     *
     * @param name The name of the type.
     * @param members The members, given as pairs `(name, type)`.
     * @param import_path The import path for the struct.
     */
    void add_struct(
      token name,
      std::vector<std::pair<token, type_info>> members,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Check if the context contains a specific type, given by a string.
     *
     * @param name The type's name.
     * @param import_path Optional import path.
     * @returns True if the type exists within the current scope.
     */
    bool has_type(const std::string& name, const std::optional<std::string>& import_path = std::nullopt) const;

    /**
     * Add a macro name.
     *
     * @throws A type_error if the macro already exists.
     *
     * @param name The macro's name.
     * @param import_path Optional import path.
     */
    void add_macro(std::string name, std::optional<std::string> import_path = std::nullopt);

    /**
     * Check if the context contains a macro.
     *
     * @param name The macro's name.
     * @param import_path Optional import path.
     * @returns True if the macro exists.
     */
    bool has_macro(const std::string& name, const std::optional<std::string>& import_path = std::nullopt) const;

    /**
     * Check if the context contains a specific type.
     *
     * @param t The type.
     * @returns True if the type exists within the current scope.
     */
    bool has_type(const type_info& t) const;

    /**
     * Check if given string is a reference type within the context.
     *
     * @note Returns `false` if the type is unknown.
     *
     * @param name The type name.
     * @param import_path Optional import path.
     * @returns Whether the type is a reference type.
     */
    bool is_reference_type(
      const std::string& name,
      const std::optional<std::string>& import_path = std::nullopt) const;

    /**
     * Check if given string is a reference type within the context.
     *
     * @note Returns `false` if the type is unknown.
     *
     * @param t The type.
     * @returns Whether the type is a reference type.
     */
    bool is_reference_type(const type_info& t) const;

    /**
     * Get the type for an identifier.
     *
     * @param identifier The identifier to resolve the type for.
     * @param namespace_path Optional namespace path of the identifier.
     * @returns The resolved type.
     * @throws Throws a `type_error` if the identifier's type could not be resolved.
     */
    type_info get_identifier_type(
      const token& identifier,
      const std::optional<std::string>& namespace_path = std::nullopt) const;

    /**
     * Get the type for a name and an optional array length.
     *
     * Adds the array type to the type map if the base type exists.
     *
     * @param name The type name.
     * @param array Whether the type is an array type.
     * @param import_path Optional import path.
     * @returns An existing type.
     * @throws A `type_error` if the type cnould not be resolved.
     */
    type_info get_type(
      const token& name,
      bool array,
      const std::optional<std::string>& import_path = std::nullopt)
    {
        return get_type(name.s, array, import_path);
    }

    /**
     * Get the type for a name.
     *
     * Adds the array type to the type map if the base type exists.
     *
     * @param name The type name.
     * @param array Whether the type is an array.
     * @param import_path Optional import path. Ignored for built-in types.
     * @returns An existing type.
     */
    type_info get_type(
      const std::string& name,
      bool array,
      const std::optional<std::string>& import_path = std::nullopt);

    /**
     * Get a type object for an unresolved type and add mark the type for resolution.
     *
     * @param name The type name.
     * @param cls The type class.
     * @param import_path Optional import path. Ignored for built-in types.
     * @returns An unresolved type.
     */
    type_info get_unresolved_type(
      token name,
      type_class cls,
      std::optional<std::string> import_path = std::nullopt);

    /**
     * Return whether a type is convertible into another type.
     *
     * @param loc A reference location for error reporting.
     * @param from The type to convert from.
     * @param to The type to convert to.
     * @returns Whether the type `from` is convertible to the type `to`.
     */
    bool is_convertible(token_location loc, const type_info& from, const type_info& to) const;

    /**
     * Resolve a type and set its type id.
     *
     * @param t The type to resolve.
     * @throws Throws a `type_error` if type resolution failed.
     */
    void resolve(type_info& t);

    /**
     * Resolve all unresolved types.
     *
     * @throws Throws a `type_error` if type resolution failed.
     */
    void resolve_types();

    /**
     * The the type of a function.
     *
     * @param name The function's name.
     * @param arg_types The argument types.
     * @param ret_type The return type.
     * @returns The function type.
     */
    type_info get_function_type(const token& name, const std::vector<type_info>& arg_types, const type_info& ret_type);

    /**
     * Get the signature of a function.
     *
     * @throws A type_error if the function is not known.
     *
     * @param name The name of the function.
     * @param import_path Optional import path of the function.
     * @returns A reference to the function signature.
     */
    const function_signature& get_function_signature(
      const token& name,
      const std::optional<std::string>& import_path = std::nullopt) const;

    /**
     * Enter a function's scope.
     *
     * @param name The function's name.
     */
    void enter_function_scope(token name);

    /** Get the current function scope. */
    std::optional<function_signature> get_current_function() const;

    /**
     * Enter a struct's scope.
     *
     * @param name The struct's name.
     */
    void enter_struct_scope(token name);

    /**
     * Exit a named scope.
     *
     * @param name The scope's name. Used for validation.
     */
    void exit_named_scope(const token& name);

    /** Enter an anonymous scope. */
    void enter_anonymous_scope(token_location loc);

    /** Exit an anonymous scope. */
    void exit_anonymous_scope();

    /** Get the current scope's name. */
    const token& get_scope_name() const;

    /**
     * Get the definition of a struct.
     *
     * @throws A type_error if the struct does not exist.
     *
     * @param loc A reference location for error reporting.
     * @param name The struct's name.
     * @param import_path Optional import path.
     * @returns A reference to the struct's definition.
     */
    const struct_definition* get_struct_definition(
      token_location loc,
      const std::string& name,
      const std::optional<std::string>& import_path = std::nullopt) const;

    /** Push a struct lookup. */
    void push_struct_definition(const struct_definition* s);

    /** Pop a struct definition. */
    void pop_struct_definition();

    /**
     * Get the type of an expression.
     *
     * @param expr The expression.
     * @returns The expression's type.
     * @throws A type_error if the expression's type could not be determined.
     */
    type_info get_expression_type(const ast::expression& expr) const;

    /**
     * Set the type of an expression.
     *
     * @param expr The expression.
     * @param t The expression's type.
     */
    void set_expression_type(const ast::expression* expr, type_info t);

    /**
     * Check whether an expression has a type registered.
     *
     * @param expr The expression.
     * @returns True if the expression has a type.
     */
    bool has_expression_type(const ast::expression& expr) const;

    /** Get the imported modules. */
    const std::vector<imported_module>& get_imported_modules() const
    {
        return imported_modules;
    }

    /** Get a string representation of the context. */
    std::string to_string() const;
};

}    // namespace slang::typing
