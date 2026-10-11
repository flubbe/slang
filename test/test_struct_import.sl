import std;

struct Holder {
    item: std::i32s
};

fn test_imported_struct_field() -> void {
    let holder: Holder = Holder {
        item: std::i32s {
            value: 42
        }
    };

    std::assert(holder.item.value == 42, "imported struct field");
}

fn main(args: str[]) -> i32 {
    test_imported_struct_field();
    return 0;
}
