import std;
import test_macro_import;

fn main(args: [str]) -> i32 {
    std::assert(test_macro_import::two!() == 2, "test_macro_import::two!() == 2");

    test_macros::test_format(); // should fail (implicit transitive import).

    return 0;
}
