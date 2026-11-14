use slang_bindings::{Context, Error};
use std::path::PathBuf;

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn repo_test_dir() -> PathBuf {
    repo_root().join("test")
}

fn repo_lang_dir() -> PathBuf {
    repo_root().join("lang")
}

fn context_with_test_paths() -> Context {
    let mut context = Context::new().unwrap();

    context.add_search_path(repo_lang_dir()).unwrap();
    context.add_search_path(repo_test_dir()).unwrap();

    context
}

#[test]
fn has_function() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();

    assert!(module.has_function("return_hello").unwrap());
}

#[test]
fn has_nonexistent_function() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();

    assert!(!module.has_function("this_function_does_not_exist").unwrap());
}

#[test]
fn get_nonexistent_function() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();

    assert!(matches!(
        module.get_function("this_function_does_not_exist"),
        Err(Error::Generic(_)) // FIXME Should report FunctionNotFound or similar.
    ));
}

#[test]
fn get_function_with_invalid_name() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();

    assert!(matches!(
        module.get_function("invalid\0name"),
        Err(Error::InvalidFunctionName)
    ));
}
