use slang_bindings::{Context, OwnedValue};
use std::path::PathBuf;

fn main() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..");

    let value: OwnedValue<'static>;

    {
        let mut context = Context::new().unwrap();
        context.add_search_path(root.join("lang")).unwrap();
        context.add_search_path(root.join("test")).unwrap();

        let module = context.resolve_module("example_package::hello").unwrap();

        let function = module.get_function("return_hello").unwrap();

        value = function.invoke(()).unwrap();
    }

    drop(value);
}
