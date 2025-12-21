import std;

macro missing_import! {
    () => {
        std::no_print!("test"); // no_print! is not part of std.
    };
}

fn main(args: str[]) -> i32 {
    return 0;
}
