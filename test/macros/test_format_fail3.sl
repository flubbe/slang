import std;

fn not_a_string() -> void
{
    std::format!("{s}", 123);
}
