/**
 * slang - a simple scripting language.
 *
 * AST serialization tests.
 *
 * \author Felix Lubbe
 * \copyright Copyright (c) 2025
 * \license Distributed under the MIT software license (see accompanying LICENSE.txt).
 */

#include <gtest/gtest.h>

#include "compiler/ast/node_registry.h"
#include "compiler/codegen.h"
#include "compiler/parser.h"
#include "compiler/typing.h"
#include "archives/file.h"
#include "loader.h"

namespace ast = slang::ast;
namespace cg = slang::codegen;
namespace co = slang::collect;
namespace ld = slang::loader;
namespace sema = slang::sema;
namespace ty = slang::typing;

namespace
{

/** Helper to test AST serialization by compiling the test input. */
void run_test(
  std::size_t test_id,
  const std::string& test_input)
{
    const std::string filename = std::format("ast_serialization_{}.bin", test_id);

    slang::lexer lexer;
    slang::parser parser;

    lexer.set_input(test_input);
    ASSERT_NO_THROW(parser.parse(lexer));

    slang::file_manager mgr;
    mgr.add_search_path(".");
    mgr.add_search_path("lang");

    ld::context loader_ctx{mgr};
    sema::env env;
    co::context co_ctx{env};
    ty::context type_ctx;
    cg::context codegen_ctx;

    std::shared_ptr<ast::expression> ast = parser.get_ast();

    ASSERT_NO_THROW(ast->collect_names(co_ctx));
    ASSERT_NO_THROW(loader_ctx.resolve_imports(codegen_ctx, type_ctx));
    ASSERT_NO_THROW(ast->type_check(type_ctx, env));

    {
        slang::file_write_archive write_ar{filename};
        auto ast = parser.get_ast();
        ASSERT_NO_THROW(write_ar & ast::expression_serializer{ast});
    }

    slang::file_read_archive read_ar{filename};
    std::unique_ptr<ast::expression> root;
    ASSERT_NO_THROW(read_ar & ast::expression_serializer{root});
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(parser.get_ast()->to_string(), root->to_string());
}

/** Helper to run the tests. */
#define AST_SERIALIZATION_TEST(number, input) \
    TEST(ast_serialization, number)           \
    {                                         \
        run_test(number, input);              \
    }

AST_SERIALIZATION_TEST(
  0,
  "/**\n"
  " * Print a string to stdout.\n"
  " *\n"
  " * @param s The string to print.\n"
  " */\n"
  "#[native(lib=slang)]\n"
  "fn print(s: str) -> void;\n"
  "\n"
  "/**\n"
  " * Print a string to stdout and append a new-line character.\n"
  " *\n"
  " * @param s The string to print.\n"
  " */\n"
  "#[native(lib=slang)]\n"
  "fn println(s: str) -> void;\n")

AST_SERIALIZATION_TEST(
  1,
  /* i32 */
  "fn itest() -> i32 {\n"
  " return 1;\n"
  "}\n"
  "fn iadd() -> i32 {\n"
  " return 1+2;\n"
  "}\n"
  "fn isub() -> i32 {\n"
  " return 3-2;\n"
  "}\n"
  "fn imul() -> i32 {\n"
  " return 2*3;\n"
  "}\n"
  "fn idiv() -> i32 {\n"
  " return 6 / 2;\n"
  "}\n"
  /* f32 */
  "fn ftest() -> f32 {\n"
  " return 1.1;\n"
  "}\n"
  "fn fadd() -> f32 {\n"
  " return 1.1+2.1;\n"
  "}\n"
  "fn fsub() -> f32 {\n"
  " return 3.1-2.1;\n"
  "}\n"
  "fn fmul() -> f32 {\n"
  " return 2.1*3.1;\n"
  "}\n"
  "fn fdiv() -> f32 {\n"
  " return 6.4 / 2.0;\n"
  "}\n"
  /* str */
  "fn stest() -> str {\n"
  " return \"Test\";\n"
  "}\n"
  /* arguments */
  "fn arg(a: i32) -> i32 {\n"
  " return 1 + a;\n"
  "}\n"
  "fn arg2(a: f32) -> f32 {\n"
  " return 2.0*a+1.0;\n"
  "}\n"
  "fn sid(a: str) -> str {\n"
  " return a;\n"
  "}\n"
  "fn arg3(a: f32, s: str) -> f32 {\n"
  " s = \"Test\";\n"
  " return 2.0 + a;\n"
  "}\n"
  /* function calls */
  "fn call(a: i32) -> i32 {\n"
  " return arg(a) - 1;\n"
  "}\n"
  /* locals. */
  "fn local(a: i32) -> i32 {\n"
  " let b: i32 = -1;\n"
  " return a+b;\n"
  "}\n"
  "fn local2(a: i32) -> i32 {\n"
  " let b: i32 = -1;\n"
  " return a-b;\n"
  "}\n"
  "fn local3() -> str {\n"
  " let s: str = \"Test\";\n"
  " return s;\n"
  "}\n"
  /* Type casts. */
  "fn cast_i2f(a: i32) -> f32 {\n"
  " return a as f32;\n"
  "}\n"
  "fn cast_f2i(a: f32) -> i32 {\n"
  " return a as i32;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  2,
  "import std;\n"
  "\n"
  "fn main(args: [str]) -> i32\n"
  "{\n"
  "\tstd::println(\"Hello, World!\");\n"
  "\treturn 0;\n"
  "}")

AST_SERIALIZATION_TEST(
  3,
  "fn main() -> i32\n"
  "{\n"
  "\tlet a: i32 = 1 + 2;\n"
  "\ta = 1 - 2;\n"
  "\ta = 1 * 2;\n"
  "\ta = 1 / 2;\n"
  "\ta += -1;\n"
  "\ta -= -2;\n"
  "\ta *= -3;\n"
  "\ta /= -2;\n"
  "\ta %= 1;\n"
  "\tlet b: f32 = 1.0 + 2.0;\n"
  "\tb = 1.0 - 2.0;\n"
  "\tb = 1.0 * 2.0;\n"
  "\tb = 1.0 / 2.0;\n"
  "\tlet c: i32 = 1 & 2;\n"
  "\tc = 1 | 2;\n"
  "\tc = 1 ^ 2;\n"
  "\tc = 1 << 2;\n"
  "\tc = 1 >> 2;\n"
  "\treturn 0;\n"
  "}\n"
  "fn and(a: i32, b: i32) -> i32 { return a & b; }\n"
  "fn land(a: i32, b: i32) -> i32 { return a && b; }\n"
  "fn or(a: i32, b: i32) -> i32 { return a | b; }\n"
  "fn lor(a: i32, b: i32) -> i32 { return a || b; }\n"
  "fn xor(a: i32, b: i32) -> i32 { return a ^ b; }\n"
  "fn shl(a: i32, b: i32) -> i32 { return a << b; }\n"
  "fn shr(a: i32, b: i32) -> i32 { return a >> b; }\n"
  "fn mod(a: i32, b: i32) -> i32 { return a % b; }\n")

AST_SERIALIZATION_TEST(
  4,
  "#[native(lib=\"slang\")]\n"
  "fn string_equals(s1: str, s2: str) -> i32;\n"
  "#[native(lib=\"slang\")]\n"
  "fn string_concat(s1: str, s2: str) -> str;\n"
  "fn main() -> i32\n"
  "{\n"
  "\tlet s: str = string_concat(\"a\", \"b\")\n"
  "\tif(string_equals(s, \"ab\"))\n"
  "\t{\n"
  "\t\treturn 10;\n"
  "\t}\n"
  "\treturn 0;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  5,
  "fn prefix_add_i32(i: i32) -> i32\n"
  "{\n"
  "\treturn ++i;\n"
  "}\n"
  "fn prefix_sub_i32(i: i32) -> i32\n"
  "{\n"
  "\treturn --i;\n"
  "}\n"
  "fn postfix_add_i32(i: i32) -> i32\n"
  "{\n"
  "\treturn i++;\n"
  "}\n"
  "fn postfix_sub_i32(i: i32) -> i32\n"
  "{\n"
  "\treturn i--;\n"
  "}\n"
  "fn prefix_add_f32(i: f32) -> f32\n"
  "{\n"
  "\treturn ++i;\n"
  "}\n"
  "fn prefix_sub_f32(i: f32) -> f32\n"
  "{\n"
  "\treturn --i;\n"
  "}\n"
  "fn postfix_add_f32(i: f32) -> f32\n"
  "{\n"
  "\treturn i++;\n"
  "}\n"
  "fn postfix_sub_f32(i: f32) -> f32\n"
  "{\n"
  "\treturn i--;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  6,
  "import std;\n"
  "fn test_if_else(a: i32) -> i32\n"
  "{\n"
  " if(a > 0)\n"
  " {\n"
  "  return 1;\n"
  " }\n"
  " else\n"
  " {\n"
  "  return 0;\n"
  " }\n"
  "}\n"
  "fn conditional_hello_world(a: f32) -> void\n"
  "{\n"
  " if(a > 2.5)\n"
  " {\n"
  "  std::println(\"Hello, World!\");\n"
  " }\n"
  " else\n"
  " {\n"
  "  std::println(\"World, hello!\");\n"
  " }\n"
  "}\n"
  "fn no_else(a: i32) -> void\n"
  "{\n"
  " if(a > 0)\n"
  " {\n"
  "  std::println(\"a>0\");\n"
  " }\n"
  " std::println(\"Test\");\n"
  "}\n")

AST_SERIALIZATION_TEST(
  7,
  "import std;\n"
  "\n"
  "fn main() -> void\n"
  "{\n"
  " let i: i32 = 0;\n"
  " while(i < 10)\n"
  " {\n"
  "  std::println(\"Hello, World!\");\n"
  "  i += 1;\n"
  " }\n"
  "}")

AST_SERIALIZATION_TEST(
  8,
  "import std;\n"
  "\n"
  "fn main_b() -> void\n"
  "{\n"
  " let i: i32 = 0;\n"
  " while(i < 10)\n"
  " {\n"
  "  std::println(\"Hello, World!\");\n"
  "  i += 1;\n"
  "  break;\n"
  " }\n"
  "}\n"
  "fn main_c() -> void\n"
  "{\n"
  " let i: i32 = 0;\n"
  " while(i < 10)\n"
  " {\n"
  "  std::println(\"Hello, World!\");\n"
  "  i = 10;\n"
  "  continue;\n"
  "  i = 1;\n"
  " }\n"
  "}")

AST_SERIALIZATION_TEST(
  9,
  "fn inf() -> void\n"
  "{\n"
  " inf();\n"
  "}")

AST_SERIALIZATION_TEST(
  10,
  "fn f() -> i32\n"
  "{\n"
  " let b: [i32] = [1, 2];\n"
  " return b[1];\n"
  "}\n"
  "fn g() -> i32\n"
  "{\n"
  " let b: [i32] = [-1, 0, f()];\n"
  " b[1] = 3;\n"
  " return b[1];\n"
  "}")

AST_SERIALIZATION_TEST(
  11,
  "fn return_array() -> [i32]\n"
  "{\n"
  " let b: [i32] = [1, 2];\n"
  " return b;\n"
  "}\n"
  "fn pass_array() -> i32\n"
  "{\n"
  " let b: [i32] = [2, 3];\n"
  " return f(b);\n"
  "}\n"
  "fn f(a: [i32]) -> i32\n"
  "{\n"
  " return a[1];\n"
  "}\n"
  "fn invalid_index() -> i32\n"
  "{\n"
  " let b: [i32] = [0, 1];\n"
  " return b[3];\n"
  "}\n"
  "fn str_array() -> [str]\n"
  "{\n"
  " let s: [str] = [\"a\", \"test\", \"123\"];\n"
  " return s;\n"
  "}\n"
  "fn ret_str() -> str\n"
  "{\n"
  " let s: [str] = [\"a\", \"test\", \"123\"];\n"
  " return s[2];\n"
  "}\n"
  "fn call_return() -> i32\n"
  "{\n"
  " return return_array()[0];\n"
  "}\n"
  "fn new_array() -> void\n"
  "{\n"
  " let b: [i32] = new i32[2];\n"
  " b[0] = 1;\n"
  " b[1] = 10;\n"
  "}\n"
  "fn new_array_invalid_size() -> void\n"
  "{\n"
  " let b: [i32] = new i32[-1];\n"
  "}\n")

AST_SERIALIZATION_TEST(
  12,
  "fn len() -> i32\n"
  "{\n"
  " let b: [i32] = [2, 3];\n"
  " return b.length;\n"
  "}\n"
  "fn len2() -> i32\n"
  "{\n"
  " let b: [i32];\n"
  " return b.length;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  13,
  "#[allow_cast]\n"
  "struct type {};\n"
  "#[native(lib=\"slang\")]\n"
  "fn array_copy(from: type, to: type) -> void;\n"
  "#[native(lib=\"slang\")]\n"
  "fn string_equals(s1: str, s2: str) -> i32;\n"
  "fn test_copy() -> i32\n"
  "{\n"
  " let a: [i32] = [2, 3];\n"
  " let b: [i32] = new i32[2];\n"
  " array_copy(a, b);\n"
  " return a.length == b.length && a[0] == b[0] && a[1] == b[1];\n"
  "}\n"
  "fn test_copy_str() -> i32\n"
  "{\n"
  " let a: [str] = [\"a\", \"123\"];\n"
  " let b: [str] = new str[2];\n"
  " array_copy(a, b);\n"
  " return a.length == b.length && string_equals(a[0], b[0]) && string_equals(a[1], b[1]);\n"
  "}\n"
  "fn test_copy_fail_none() -> void\n"
  "{\n"
  " let a: [i32] = [2, 3];\n"
  " let b: [i32];\n"    // no target array
  " array_copy(a, b);\n"
  "}\n"
  "fn test_copy_fail_type() -> void\n"
  "{\n"
  " let a: [i32] = [2, 3];\n"
  " let b: [f32] = new f32[2];\n"    // wrong type
  " array_copy(a, b);\n"
  "}\n")

AST_SERIALIZATION_TEST(
  14,
  "fn f() -> void\n"
  "{\n"
  " g();\n"
  "}\n"
  "fn g() -> i32\n"
  "{\n"
  " return 123;\n"
  "}")

AST_SERIALIZATION_TEST(
  15,
  "fn f() -> void\n"
  "{\n"
  " g();\n"
  "}\n"
  "fn g() -> [i32]\n"
  "{\n"
  " let r: [i32] = [1, 2];\n"
  " return r;\n"
  "}")

AST_SERIALIZATION_TEST(
  16,
  "fn f() -> void\n"
  "{\n"
  " g();\n"
  "}\n"
  "fn g() -> [str]\n"
  "{\n"
  " let r: [str] = [\"a\", \"test\"];\n"
  " return r;\n"
  "}")

AST_SERIALIZATION_TEST(
  17,
  "struct S {\n"
  " i: i32,\n"
  " j: f32\n"
  "};\n"
  "struct T{\n"
  " s: S,\n"
  " t: str\n"
  "};")

AST_SERIALIZATION_TEST(
  18,
  "struct S {\n"
  " i: i32,\n"
  " j: f32\n"
  "};\n"
  "fn test() -> i32\n"
  "{\n"
  " let s: S = S{ i: 2, j: 3 as f32 };\n"
  " s.i = 1;\n"
  " return s.i + s.j as i32;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  19,
  "struct S {\n"
  " i: i32,\n"
  " j: i32\n"
  "};\n"
  "fn test() -> i32\n"
  "{\n"
  " let s: S = S{ i: 2, j: 3 };\n"
  " s.i = s.j = 1;\n"
  " return s.i + s.j;\n"
  "}\n"
  "fn test_local() -> i32\n"
  "{\n"
  " let s: S = S{ i: 2, j: 3 };\n"
  " let i: i32 = s.j = 1;\n"
  " return i + s.i + s.j;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  20,
  "struct S {\n"
  " s: S\n"
  "};\n"
  "fn test() -> void\n"
  "{\n"
  " let s: S = S{s: null};\n"
  "}\n")

AST_SERIALIZATION_TEST(
  21,
  "struct S {\n"
  " i: i32\n"
  "};\n"
  "fn test() -> void\n"
  "{\n"
  " let s: S = null;\n"
  " s.i = 10;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  22,
  "struct S {\n"
  " i: i32,\n"
  " j: f32,\n"
  " s: str\n"
  "};\n"
  "fn return_struct() -> S\n"
  "{\n"
  " return S{i:1, j:2.3, s: \"test\"};"
  "}\n")

AST_SERIALIZATION_TEST(
  23,
  "struct S {\n"
  " i: i32,\n"
  " j: f32,\n"
  " s: str\n"
  "};\n"
  "fn struct_arg(s: S) -> void\n"
  "{\n"
  " s.i = 1;\n"
  " s.j = 2.3;\n"
  " s.s = \"test\";\n"
  "}\n")

AST_SERIALIZATION_TEST(
  24,
  "struct Link {\n"
  " next: Link\n"
  "};\n"
  "fn test() -> void\n"
  "{\n"
  " let root: Link = Link{next: Link{next: null}};\n"
  " root.next.next = root;\n"
  " root.next.next = null;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  25,
  "struct Data {\n"
  " i: i32,\n"
  " f: f32,\n"
  " s: str,\n"
  " next: Data"
  "};"
  "struct Container {\n"
  " data: Data,\n"
  " flags: i32\n"
  "};\n"
  "fn test() -> i32\n"
  "{\n"
  " let c: Container = Container{\n"
  "  data: Data{i: -1, f: 3.14, s: \"Test\", next: null},\n"
  "  flags: 4096\n"
  " };\n"
  " return c.data.i + (c.data.f as i32);\n"
  "}\n")

AST_SERIALIZATION_TEST(
  26,
  "import nested_structs2;\n"
  "fn test() -> i32\n"
  "{\n"
  " let c: nested_structs2::Container = nested_structs2::Container{\n"
  "  data: nested_structs2::Data{i: -1, f: 3.14, s: \"Test\", next: null},\n"
  "  flags: 4096\n"
  " };\n"
  " return c.data.i + (c.data.f as i32);\n"
  "}\n")

AST_SERIALIZATION_TEST(
  27,
  "fn main() -> i32\n"
  "{\n"
  " let i: [f32] = new f32[10];\n"
  " let s: [i32] = null;\n"
  " s[0] = 1;\n"
  " return 0;\n"
  "}\n")

AST_SERIALIZATION_TEST(
  28,
  "macro sum! {\n"
  "    () => {\n"
  "        return 0;\n"
  "    };\n"
  "    ($a: expr) => {\n"
  "       return $a;\n"
  "    };\n"
  "    ($a: expr, $b: expr...) => {\n"
  "        return $a + sum!($b);\n"
  "    };\n"
  "}")

}    // namespace
