// A duplicate top-level declaration should yield an actionable "rename/remove" hint.
fn main() -> i32 {
    let x: i32 = 1;
    let x: i32 = 2;
    return 0;
}
