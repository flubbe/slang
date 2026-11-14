#[test]
fn lifetime_tests() {
    let tests = trybuild::TestCases::new();

    tests.compile_fail("tests/compile_fail/lifetime_*.rs");
}
