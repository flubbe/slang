/**
 * slang - a simple scripting language.
 *
 * runtime type registration.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2024
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <fmt/core.h>

#include "interpreter/interpreter.h"
#include "runtime/runtime.h"
#include "../utils.h"

namespace slang::runtime
{

void register_builtin_type_layouts(gc::garbage_collector& gc)
{
    /*
     * layout for `i32s`, `f32s`.
     */
    gc.register_type_layout(si::make_type_name("std", "i32s"), {});
    gc.register_type_layout(si::make_type_name("std", "f32s"), {});

    static_assert(std::is_standard_layout_v<i32s>);
    static_assert(std::is_standard_layout_v<f32s>);

    /*
     * layout for `result`.
     */
    std::size_t offset = 0, alignment = 0;
    std::vector<std::size_t> layout;

    offset = sizeof(decltype(result::ok));

    alignment = std::alignment_of_v<decltype(result::value)>;
    offset = utils::align(alignment, offset);
    layout.push_back(offset);

    gc.register_type_layout(si::make_type_name("std", "result"), layout);

    static_assert(std::is_standard_layout_v<result>);
}

}    // namespace slang::runtime
