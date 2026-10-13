import std;

fn main(args: str[]) -> i32 {
    let i: i32 = 0;
    let r: i32 = 1;
    while(i < 32) {
        std::assert(1 << i == r, "");

        i++;
        r *= 2;
    }

    return 0;
}
