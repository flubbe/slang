import std;

fn first() -> void {
    second();
}

fn third() -> void {
    std::assert(0);
}

fn second() -> void {
    third();
}

fn main(args: [str]) -> i32 {
    first();
    return 0;
}