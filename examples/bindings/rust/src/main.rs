use slang_bindings::{
    Context, Error, NativeCall, NativeType, OwnedString, OwnedValue, StringRef, call,
};
use std::ffi::c_void;

#[repr(C)]
struct NativeS {
    a: i8,
    b: i8,
    c: i16,
    s: *mut c_void,
    i: i32,
}

impl NativeS {
    fn string(&self) -> Result<String, Error> {
        unsafe { StringRef::from_raw(self.s)?.to_string() }
    }
}

unsafe impl NativeType for NativeS {
    const NAME: &'static str = "S";
    const MODULE: &'static str = "native_integration";

    fn pointer_offsets() -> &'static [usize] {
        // Only the `s` field is a pointer.
        &[std::mem::offset_of!(Self, s)]
    }
}

fn native_print(call: &mut NativeCall<'_>) -> Result<(), Error> {
    let txt = call.pop_string()?;
    print!("{txt}");
    Ok(())
}

fn init_context() -> Result<Context, Error> {
    let mut context = Context::new()?;

    context.add_search_path(".")?;
    context.add_search_path("examples")?;

    context.register_type::<NativeS>()?;
    context.register_native("slang", "print", native_print)?;

    Ok(context)
}

fn main() -> Result<(), Error> {
    let mut context = init_context()?;

    {
        let module = context.resolve_module("native_integration")?;
        let func = module.get_function("test")?;

        let native_string = OwnedString::new("Hello from native code!")?;
        let native_instance = NativeS {
            a: 12,
            b: 3,
            c: 99,
            s: native_string.as_ptr(),
            i: 123,
        };

        // Alternatively: let result = func.invoke((&native_instance, 1.234_f32))?;
        let result = call!(func, &native_instance, 1.234_f32)?;

        let native_ret = match result {
            OwnedValue::Native(native) => native,
            other => {
                eprintln!("Unexpected return type: {:?}", other);
                return Ok(());
            }
        };

        let ret = native_ret.as_ref::<NativeS>()?;
        match ret {
            Some(ret) => {
                println!("String from script: {}", ret.string()?);
                println!("Value a: {}", ret.a);
                println!("Value b: {}", ret.b);
                println!("Value c: {}", ret.c);
                println!("Value i: {}", ret.i);
            }
            None => {
                eprintln!("Script returned None");
            }
        };
    }

    context.gc_run()?;
    let (objects, roots, bytes) = context.gc_stats()?;
    if objects != 0 {
        println!("GC: {} objects still alive", objects);
    }
    if roots != 0 {
        println!("GC: {} root set entries still alive", roots);
    }
    if bytes != 0 {
        println!("GC: {} bytes still allocated", bytes);
    }

    Ok(())
}
