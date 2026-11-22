import std;

fn main() -> void {
    let x: std::results = 1; // A mistyped imported type should yield a "did you mean" suggestion.
}
