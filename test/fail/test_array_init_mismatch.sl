// Mixed element types in an initializer should yield a "same type" hint.
fn main() -> i32 {
    let a: i32[] = [1, 1.0];
    return 0;
}
