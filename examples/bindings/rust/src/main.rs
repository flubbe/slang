use slang_bindings::{
    Context, Error, NativeCall, NativeType, OwnedString, OwnedValue, StringRef, Value,
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
    let mut ctx = Context::new()?;

    ctx.add_search_path(".")?;
    ctx.add_search_path("examples")?;

    ctx.register_type::<NativeS>("native_integration")?;
    ctx.register_native("slang", "print", native_print)?;

    Ok(ctx)
}

fn main() -> Result<(), Error> {
    let mut ctx = init_context()?;

    {
        let module = ctx.resolve_module("native_integration")?;
        let func = module.get_function("test")?;

        let native_string = OwnedString::new("Hello from native code!")?;
        let native_instance = NativeS {
            a: 12,
            b: 3,
            c: 99,
            s: native_string.as_ptr(),
            i: 123,
        };

        let struct_layout = ctx.get_type_layout::<NativeS>("native_integration")?;
        let struct_val = Value::from_type(&native_instance, &struct_layout);
        let float_val = Value::from_f32(1.234_f32);

        let args = &[struct_val, float_val];
        let result = func.invoke(args)?;

        let native_ret = match result {
            OwnedValue::Native(native) => native,
            other => {
                eprintln!("Unexpected return type: {:?}", other);
                return Ok(());
            }
        };

        let ret = native_ret.as_ref::<NativeS>(&struct_layout)?;

        println!("String from script: {}", ret.string()?);
        println!("Value a: {}", ret.a);
        println!("Value b: {}", ret.b);
        println!("Value c: {}", ret.c);
        println!("Value i: {}", ret.i);
    }

    ctx.gc_run()?;
    let (objects, roots, bytes) = ctx.gc_stats()?;
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
