import test_transitive_import1;

fn main(args: [str]) -> i32 {
    test_transitive_import1::test_fn();
    test_transitive_import1::test_macro!();

    return 0;
}