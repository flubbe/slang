import std;

fn main(args: [str]) -> i32
{
    let s: i32 = 1*2 + 3;

    #[disable(const_eval)]
    let t: i32 = 1*2 + 3;

    std::assert(s == 5, "s == 5");
    std::assert(t == 5, "t == 5");

    return 0;
}
