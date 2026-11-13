use slang_bindings::{Context, Error, NativeCall, NativeType};
use std::ffi::c_void;
use std::path::PathBuf;

fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn repo_examples_dir() -> PathBuf {
    repo_root().join("examples")
}

fn repo_lang_dir() -> PathBuf {
    repo_root().join("lang")
}

fn context_with_test_paths() -> Context {
    let mut context = Context::new().unwrap();

    context.add_search_path(repo_lang_dir()).unwrap();
    context.add_search_path(repo_examples_dir()).unwrap();

    context
}

#[repr(C)]
struct NativeS {
    a: i8,
    b: i8,
    c: i16,
    s: *mut c_void,
    i: i32,
}

unsafe impl NativeType for NativeS {
    const NAME: &'static str = "S";
    const MODULE: &'static str = "native_integration";

    fn pointer_offsets() -> &'static [usize] {
        &[std::mem::offset_of!(Self, s)]
    }
}

fn native_print(call: &mut NativeCall<'_>) -> Result<(), Error> {
    let text = call.pop_string()?; // get the argument from the stack
    print!("{text}");
    Ok(())
}

#[test]
fn register_print() {
    let mut context = Context::new().unwrap();
    context
        .register_native("native_integration", "print", native_print)
        .unwrap();
}

#[test]
fn native_integration() {
    let mut context = context_with_test_paths();

    context.register_type::<NativeS>().unwrap();
    context
        .register_native("slang", "print", native_print)
        .unwrap();
    context.resolve_module("native_integration").unwrap();
}
