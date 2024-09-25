import std;

struct L
{
    data: str,
    next: L
};

fn append(node: L, data: str) -> L
{
    std::print("Appending node with data: ");
    std::println(data);

    while(node.next != null)
    {
        node = node.next;
    }

    node.next = L{data: data, next: null};

    return node.next;
}

fn print(node: L) -> void
{
    while(node != null)
    {
        std::println(node.data);
        node = node.next;
    }
}

fn main(s: str) -> i32
{
    let root: L = L{data: "root", next: null};

    append(root, "1");
    append(root, "2");

    print(root);
    std::println(root.next.data);

    return 0;
}