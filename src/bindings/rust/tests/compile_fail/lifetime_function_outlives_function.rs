use slang_bindings::{Context, Function};
use std::path::PathBuf;

fn main() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..");

    let mut context = Context::new().unwrap();
    context.add_search_path(root.join("lang")).unwrap();
    context.add_search_path(root.join("test")).unwrap();

    let function: Function<'static, '_>;

    {
        let module = context.resolve_module("example_package::hello").unwrap();

        function = module.get_function("return_hello").unwrap();
    }

    drop(function);
}
