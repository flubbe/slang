import std;

// A float passed to the integer 'd' specifier should yield a "valid integer types" hint.
fn fmt_bad_d() -> void {
    std::format!("{d}", 1.0);
}
