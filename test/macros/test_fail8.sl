macro missing_import! {
    () => {
        std::print("test"); // std is not imported.
    };
}

fn main(args: str[]) -> i32 {
    return 0;
}
