import std;

macro pi_2! {
    () => {
        std::PI / 2.0;
    };
}

fn main(args: [str]) -> i32
{
    std::println(std::format!("pi_2: {f}", pi_2!()));
    return 0;
}
