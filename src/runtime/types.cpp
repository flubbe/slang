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

#include "interpreter.h"
#include "runtime/runtime.h"

namespace slang::runtime
{

void register_builtin_type_layouts(gc::garbage_collector& gc)
{
    /*
     * layout for `i32s`, `f32s`.
     */
    std::size_t offset = 0, alignment = 0;
    std::vector<std::size_t> layout;

    alignment = std::alignment_of_v<decltype(i32s::value)>;
    offset = (static_cast<std::size_t>(offset) + (alignment - 1)) & ~(alignment - 1);
    layout.push_back(offset);

    gc.register_type_layout("i32s", layout);
    gc.register_type_layout("f32s", layout);

    static_assert(std::is_standard_layout_v<i32s>);
    static_assert(std::is_standard_layout_v<f32s>);

    /*
     * layout for `result`.
     */
    layout.clear();

    alignment = std::alignment_of_v<decltype(result::ok)>;
    offset = (static_cast<std::size_t>(offset) + (alignment - 1)) & ~(alignment - 1);
    layout.push_back(offset);
    offset += sizeof(decltype(result::ok));

    alignment = std::alignment_of_v<decltype(result::value)>;
    offset = (static_cast<std::size_t>(offset) + (alignment - 1)) & ~(alignment - 1);
    layout.push_back(offset);

    gc.register_type_layout("result", layout);

    static_assert(std::is_standard_layout_v<result>);
}

}    // namespace slang::runtime
