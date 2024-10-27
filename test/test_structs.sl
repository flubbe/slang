import std;

/*
 * struct creation and member access.
 */

struct S 
{
    i: i32,
    j: f32
};

fn init_struct(i: i32, j: f32) -> S 
{
    return S{i: i, j: j};
}

fn test_struct() -> void
{
    let s: S = init_struct(2, 3.141);
    std::assert(s.i + (s.j as i32) == 5, "s.i + (s.j as i32) == 5");
}

/*
 * linked lists.
 */

struct L
{
    data: str,
    next: L
};

fn create_node(data: str) -> L
{
    return L{data: data, next: null};
}

fn append(node: L, data: str) -> L
{
    while(node.next != null)
    {
        node = node.next;
    }

    node.next = create_node(data);

    return node.next;
}

fn test_linked_list() -> void
{
    let root: L = create_node("root");

    std::assert(root.next == null, "root.next == null");

    append(root, "1");
    append(root, "2");

    std::assert(root.next != null, "root.next != null");
    std::assert(root.next.next != null, "root.next.next != null");

    let l1: L = root.next;
    let l2: L = root.next.next;

    std::assert(std::string_equals(l1.data, "1"), "l1.data == \"1\"");
    std::assert(std::string_equals(l2.data, "2"), "l2.data == \"2\"");

    std::assert(l2.next == null, "l2.next == null");
}

/*
 * test main.
 */

fn main(args: [str]) -> i32
{
    test_struct();
    test_linked_list();

    return 0;
}
