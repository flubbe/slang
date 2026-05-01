/**
 * slang - a simple scripting language.
 *
 * Formatter tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2026
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "formatter/formatter.h"
#include "filemanager.h"

namespace
{

using slang::file_manager;

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream in{path, std::ios::in | std::ios::binary};
    EXPECT_TRUE(in.good()) << "Failed to open: " << path;

    std::string content;
    in.seekg(0, std::ios::end);
    content.resize(static_cast<std::size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(content.data(), static_cast<std::streamsize>(content.size()));
    return content;
}

TEST(formatter, format_text_canonical_layout)
{
    const std::string input =
      "fn main()->void{let x:i32=1+2;let y:i32=x*3;}";

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1 + 2;\n"
      "    let y: i32 = x * 3;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_is_idempotent)
{
    const std::string input =
      "fn main() -> void {\n"
      "    let x: i32 = 1 + 2;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    const auto [once, once_changed] = formatter.format_text(input);
    const auto [twice, twice_changed] = formatter.format_text(once);

    EXPECT_FALSE(once_changed);
    EXPECT_FALSE(twice_changed);
    EXPECT_EQ(once, twice);
}

TEST(formatter, format_text_invalid_syntax_throws)
{
    const std::string invalid_input =
      "fn main( -> void { let x: i32 = 1; }";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_THROW(
      static_cast<void>(formatter.format_text(invalid_input)),
      std::runtime_error);
}

TEST(formatter, format_file_returns_formatted_text)
{
    const auto temp_file = std::filesystem::temp_directory_path() / "slang_test_formatter_format_file.sl";

    {
        std::ofstream out{temp_file, std::ios::out | std::ios::trunc};
        ASSERT_TRUE(out.good());
        out << "fn main()->void{let x:i32=1+2;}";
    }

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1 + 2;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_file(temp_file).first, expected);

    std::filesystem::remove(temp_file);
}

TEST(formatter, format_file_in_place_reports_changes)
{
    const auto temp_file = std::filesystem::temp_directory_path() / "slang_test_formatter_in_place.sl";

    {
        std::ofstream out{temp_file, std::ios::out | std::ios::trunc};
        ASSERT_TRUE(out.good());
        out << "fn main()->void{let x:i32=1+2;}";
    }

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_TRUE(formatter.format_file_in_place(temp_file));
    EXPECT_FALSE(formatter.format_file_in_place(temp_file));

    std::filesystem::remove(temp_file);
}

TEST(formatter, format_text_formats_nested_blocks)
{
    const std::string input =
      "fn main()->void{if(1<2){while(3>2){let x:i32=1;}}}";

    const std::string expected =
      "fn main() -> void {\n"
      "    if(1 < 2) {\n"
      "        while(3 > 2) {\n"
      "            let x: i32 = 1;\n"
      "        }\n"
      "    }\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_formats_commas_and_calls)
{
    const std::string input =
      "fn main()->void{foo(1,2,3);bar(a,b,c);}";

    const std::string expected =
      "fn main() -> void {\n"
      "    foo(1, 2, 3);\n"
      "    bar(a, b, c);\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_formats_directives)
{
    const std::string input =
      "#[no_eval]fn main()->void{#[allow_cast]let x:i32=1;}";

    const std::string expected =
      "#[no_eval]\n"
      "fn main() -> void {\n"
      "    #[allow_cast]\n"
      "    let x: i32 = 1;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_wraps_long_lines_when_configured)
{
    const std::string input =
      "fn main()->void{let value:i32=very_long_identifier_name+another_long_identifier_name+third_long_identifier_name;}";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{
      file_mgr,
      slang::formatter::options{
        .indent_size = 4,
        .max_line_length = 60,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    const std::string expected =
      "fn main() -> void {\n"
      "    let value: i32 = very_long_identifier_name + another_long_identifier_name +\n"
      "    third_long_identifier_name;\n"
      "}\n";

    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_does_not_wrap_when_line_length_disabled)
{
    const std::string input =
      "fn main()->void{let value:i32=very_long_identifier_name+another_long_identifier_name+third_long_identifier_name;}";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{
      file_mgr,
      slang::formatter::options{
        .indent_size = 4,
        .max_line_length = 0,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    const std::string expected =
      "fn main() -> void {\n"
      "    let value: i32 = very_long_identifier_name + another_long_identifier_name + third_long_identifier_name;\n"
      "}\n";

    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_wraps_more_aggressively_with_tight_limit)
{
    const std::string input =
      "fn main()->void{let x:i32=abcde+fghij+klmno+pqrst+uvwxy;}";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{
      file_mgr,
      slang::formatter::options{
        .indent_size = 4,
        .max_line_length = 30,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = abcde + fghij +\n"
      "    klmno + pqrst + uvwxy;\n"
      "}\n";

    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_comment_variations)
{
    const std::string input =
      "/* file block */\n"
      "// file line\n"
      "fn main()->void{\n"
      "/* stmt lead block */let a:i32=1; // stmt trail line\n"
      "// stmt lead line\n"
      "let b:i32=2; /* stmt trail block */\n"
      "let c:i32=3; // c1 /* c2 */\n"
      "}\n";

    const std::string expected =
      "/* file block */\n"
      "// file line\n"
      "fn main() -> void {\n"
      "    /* stmt lead block */\n"
      "    let a: i32 = 1; // stmt trail line\n"
      "    // stmt lead line\n"
      "    let b: i32 = 2; /* stmt trail block */\n"
      "    let c: i32 = 3; // c1 /* c2 */\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_single_blank_line_between_leading_block_comments)
{
    const std::string input =
      "/* first */\n"
      "\n"
      "/* second */\n"
      "fn main()->void{}";

    const std::string expected =
      "/* first */\n"
      "\n"
      "/* second */\n"
      "fn main() -> void {\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_comments_before_closing_brace)
{
    const std::string input =
      "fn main()->void{let x:i32=1; // after stmt\n"
      "// before closing brace\n"
      "}";

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1; // after stmt\n"
      "    // before closing brace\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_single_blank_line_between_statements)
{
    const std::string input =
      "fn main()->void{\n"
      "let a:i32=1;\n"
      "\n"
      "\n"
      "let b:i32=2;\n"
      "}";

    const std::string expected =
      "fn main() -> void {\n"
      "    let a: i32 = 1;\n"
      "\n"
      "    let b: i32 = 2;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_wraps_at_operator_boundaries)
{
    const std::string input =
      "fn main()->void{let sum:i32=aaa+bbb+ccc+ddd+eee+fff;}";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{
      file_mgr,
      slang::formatter::options{
        .indent_size = 4,
        .max_line_length = 34,
        .validate_syntax = true,
        .ensure_trailing_newline = true}};

    const std::string expected =
      "fn main() -> void {\n"
      "    let sum: i32 = aaa + bbb + ccc +\n"
      "    ddd + eee + fff;\n"
      "}\n";

    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_trailing_line_comment_at_eof)
{
    const std::string input =
      "fn main()->void{let x:i32=1;} // eof trailing line comment";

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1;\n"
      "} // eof trailing line comment\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_trailing_block_comment_at_eof)
{
    const std::string input =
      "fn main()->void{let x:i32=1;} /* eof trailing block comment */";

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1;\n"
      "} /* eof trailing block comment */\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_ensures_trailing_newline_by_default)
{
    const std::string input = "fn main()->void{let x:i32=1;}";
    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1;\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_can_disable_trailing_newline)
{
    const std::string input = "fn main()->void{let x:i32=1;}";
    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = 1;\n"
      "}";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{
      file_mgr,
      slang::formatter::options{
        .indent_size = 4,
        .max_line_length = 0,
        .validate_syntax = true,
        .ensure_trailing_newline = false}};

    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_formats_macro_definition_layout)
{
    const std::string input =
      "macro sum!{($a:expr,$b:expr...)=>{$a+sum!($b);};}";

    const std::string expected =
      "macro sum! {\n"
      "    ($a: expr, $b: expr...) => {\n"
      "        $a + sum!($b);\n"
      "    };\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_formats_macro_invocation_spacing)
{
    const std::string input =
      "fn main()->void{let x:i32=sum!(1,2);}";

    const std::string expected =
      "fn main() -> void {\n"
      "    let x: i32 = sum!(1, 2);\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_formats_macro_pattern_branch_tokens)
{
    const std::string input =
      "macro copy!{($a:expr,$b:expr...)=>{$a+copy!($b);};}";

    const std::string expected =
      "macro copy! {\n"
      "    ($a: expr, $b: expr...) => {\n"
      "        $a + copy!($b);\n"
      "    };\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, format_text_preserves_escaped_string_literals)
{
    const std::string input =
      "fn main()->void{print(\"line1\\nline2\\t\\\\\\\"\");}";

    const std::string expected =
      "fn main() -> void {\n"
      "    print(\"line1\\nline2\\t\\\\\\\"\");\n"
      "}\n";

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};
    EXPECT_EQ(formatter.format_text(input).first, expected);
}

TEST(formatter, golden_examples_exact_output_and_idempotence)
{
    struct golden_case
    {
        std::filesystem::path path;
        std::string expected;
    };

    const std::vector<golden_case> cases = {
      {"examples/hello_world.sl",
       "import std;\n"
       "\n"
       "fn main(args: str[]) -> i32 {\n"
       "    std::println(\"Hello, World!\");\n"
       "    return 0;\n"
       "}\n"},
      {"examples/array_loop.sl",
       "import std;\n"
       "\n"
       "fn main(args: str[]) -> i32 {\n"
       "    let strs: str[] = [\"This\", \"is\", \"a\", \"loop!\"];\n"
       "\n"
       "    let i: i32 = 0;\n"
       "    while(i < strs.length) {\n"
       "        std::println(strs[i]);\n"
       "        i++;\n"
       "    }\n"
       "\n"
       "    return 0;\n"
       "}\n"},
      {"examples/structs.sl",
       "struct S {\n"
       "    i: i32, j: f32\n"
       "};\n"
       "\n"
       "fn init(i: i32, j: f32) -> S {\n"
       "    return S {\n"
       "        i: i, j: j\n"
       "    };\n"
       "}\n"
       "\n"
       "fn main(args: str[]) -> i32 {\n"
       "    let s: S = init(2, 3.141 as f32);\n"
       "    return s.i + (s.j as i32);\n"
       "}\n"},
    };

    file_manager file_mgr;
    slang::formatter::source_formatter formatter{file_mgr};

    for(const auto& test_case: cases)
    {
        SCOPED_TRACE(test_case.path.string());
        const std::string source = read_text_file(test_case.path);
        const auto [formatted, _] = formatter.format_text(source);
        EXPECT_EQ(formatted, test_case.expected);
        EXPECT_EQ(formatter.format_text(formatted).first, formatted);
    }
}

}    // namespace
