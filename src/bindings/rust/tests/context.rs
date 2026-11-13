use slang_bindings::Context;
use slang_bindings::Error;
use std::path::PathBuf;

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn repo_tests_dir() -> PathBuf {
    repo_root().join("test")
}

fn repo_lang_dir() -> PathBuf {
    repo_root().join("lang")
}

fn context_with_test_paths() -> Context {
    let mut context = Context::new().unwrap();

    context.add_search_path(repo_lang_dir()).unwrap();
    context.add_search_path(repo_tests_dir()).unwrap();

    context
}

#[test]
fn create_and_drop_context() {
    let context = Context::new().unwrap();
    drop(context);
}

#[test]
fn add_search_path() {
    let mut context = Context::new().unwrap();
    let path = repo_tests_dir();

    assert!(
        path.is_dir(),
        "Test directory does not exist: {}",
        path.display()
    );

    context.add_search_path(path).unwrap();
}

#[test]
fn add_nonexistent_search_path() {
    let mut context = Context::new().unwrap();
    let path = repo_tests_dir().join("this-directory-should-not-exist");

    match context.add_search_path(path) {
        Err(slang_bindings::Error::Filesystem(message)) => {
            assert!(!message.is_empty());
        }
        result => panic!("Expected filesystem error, got {result:?}"),
    }
}

#[test]
fn resolve_nonexistent_module() {
    let context = context_with_test_paths();
    let result = context.resolve_module("this_module_should_not_exist");

    assert!(matches!(result, Err(Error::Filesystem(_))));
}

#[test]
fn resolve_module() {
    let context = context_with_test_paths();
    context.resolve_module("example_package::hello").unwrap();
    drop(context);
}

#[test]
fn resolve_module_twice() {
    let context = context_with_test_paths();

    {
        let _module1 = context.resolve_module("example_package::hello").unwrap();
        let _module2 = context.resolve_module("example_package::hello").unwrap();
    }
}

#[test]
fn resolve_module_with_invalid_name() {
    let context = context_with_test_paths();

    assert!(matches!(
        context.resolve_module("example\0package"),
        Err(Error::InvalidModuleName)
    ));
}

#[test]
fn get_function() {
    let context = context_with_test_paths();
    let module = context.resolve_module("example_package::hello").unwrap();

    module.get_function("return_hello").unwrap();
}
