import std;

macro capture_probe! {
    () => {
        hygiene_capture_target;
    };
}

fn main(args: str[]) -> i32 {
    let hygiene_capture_target: i32 = 42;

    // Hygienic: capture_probe! does NOT see this binding.
    let r: i32 = capture_probe!();
    std::println(std::format!("{}", r));

    return 0;
}
