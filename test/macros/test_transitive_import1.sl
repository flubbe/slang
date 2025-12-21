import std;
import test_import;

fn test_fn() -> void
{
    test_import::test_format_print!();
}

macro test_macro! {
    () => {
        test_import::test_format_print!();
    };
}

fn main(args: str[]) -> i32 {
    std::assert(test_import::two!() == 2, "test_import::two!() == 2");

    test_import::test!();

    return 0;
}
