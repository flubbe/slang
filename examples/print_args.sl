import std;

fn main(args: str[]) -> i32
{
    std::println("Arguments:");
    
    let i: i32 = 0;
    while(i < args.length)
    {
        std::println(args[i]);
        i += 1;
    }

    return 0;
}
