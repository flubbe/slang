use slang_bindings::{Context, Module};
use std::path::PathBuf;

fn context_with_test_paths() -> Context {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..");

    let mut context = Context::new().unwrap();
    context.add_search_path(root.join("lang")).unwrap();
    context.add_search_path(root.join("test")).unwrap();

    context
}

fn main() {
    let module: Module<'static>;

    {
        let context = context_with_test_paths();
        module = context.resolve_module("example_package::hello").unwrap();
    }

    drop(module);
}
