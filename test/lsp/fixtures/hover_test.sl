// test/lsp/fixtures/hover_test.sl
//
// A minimal valid .sl source used by LSP hover integration tests.
// Contains a module-level constant and a function-local variable.
//
// Line positions (0-based) for test coordinates:
//   8  `const myConst: i32 = 42;`
//  11  `let myVar: str = "hello";`
const myConst: i32 = 42;

fn myAdd(a: i32, b: i32) -> i32 {
    let myVar: str = "hello";
    return a + b;
}
