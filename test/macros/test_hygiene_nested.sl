import std;

macro inner! {
    () => {
        nested_capture_target;
    };
}

macro outer! {
    () => {
        inner!();
    };
}

fn main(args: str[]) -> i32 {
    let nested_capture_target: i32 = 7;

    // Hygienic: neither outer! nor inner! sees this binding.
    let r: i32 = outer!();
    std::println(std::format!("{}", r));

    return 0;
}
