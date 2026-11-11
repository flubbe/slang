//! Example port of `examples/bindings/c/native_integration.c` using the Rust bindings.

use slang_bindings::{sys, value};
use std::ffi::{CStr, c_char, c_void};
use std::io::Write;

#[repr(C)]
struct SNative {
    a: i8,
    b: i8,
    c: i16,
    s: sys::slang_string,
    i: i32,
}

unsafe extern "C" fn slang_print(
    context: *mut sys::slang_context,
    stack: sys::slang_operand_stack,
    _user_data: *mut c_void,
) {
    let mut length = 0;
    let mut data: *const c_char = std::ptr::null();

    let status = unsafe { sys::slang_stack_pop_string(context, stack, &mut length, &mut data) };

    if status != slang_bindings::STATUS_OK || data.is_null() {
        return;
    }

    let bytes = unsafe { std::slice::from_raw_parts(data.cast::<u8>(), length) };

    let _ = std::io::stdout().lock().write_all(bytes);
}

fn main() {
    unsafe {
        // Create a new context.
        let context = sys::slang_context_create();
        if context.is_null() {
            eprintln!("Error: could not create context");
            std::process::exit(1);
        }

        // Add search paths (same as C example).
        if sys::slang_context_add_search_path(context, c".".as_ptr()) != slang_bindings::STATUS_OK {
            eprintln!(
                "Error adding search path: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }
        if sys::slang_context_add_search_path(context, c"examples".as_ptr())
            != slang_bindings::STATUS_OK
        {
            eprintln!(
                "Error adding search path: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        // Register type.

        let s_layout = [std::mem::offset_of!(SNative, s)];

        let status = sys::slang_type_register_layout(
            context,
            c"native_integration.S".as_ptr(),
            s_layout.as_ptr(),
            s_layout.len(),
        );

        if status != slang_bindings::STATUS_OK {
            eprintln!(
                "Error registering type layout: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        let mut layout_id: usize = 0;

        let status = sys::slang_context_get_type_layout_id(
            context,
            c"native_integration.S".as_ptr(),
            &mut layout_id,
        );

        if status != slang_bindings::STATUS_OK {
            let error = std::ffi::CStr::from_ptr(sys::slang_context_error_string(context));

            eprintln!("Error: {}", error.to_string_lossy());
            sys::slang_context_destroy(context);
            return;
        }

        let status = sys::slang_function_register_native(
            context,
            c"slang".as_ptr(),
            c"print".as_ptr(),
            Some(slang_print),
            std::ptr::null_mut(),
        );

        if status != slang_bindings::STATUS_OK {
            eprintln!(
                "Error registering native function: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        // Resolve the module "native_integration".
        let mut module: sys::slang_module = std::ptr::null_mut();
        let status = sys::slang_context_resolve_module(
            context,
            c"native_integration".as_ptr() as *const u8 as *const i8,
            &mut module,
        );
        if status != slang_bindings::STATUS_OK {
            eprintln!(
                "Error resolving module: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        // Get the function "test".
        let mut func: sys::slang_function = std::ptr::null_mut();
        let mut has_test = false;
        let status = sys::slang_module_has_function(
            context,
            module,
            c"test".as_ptr() as *const u8 as *const i8,
            &mut has_test,
        );
        if status != slang_bindings::STATUS_OK || !has_test {
            eprintln!("Cannot find function 'test' in module");
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }
        let status = sys::slang_module_get_function(
            context,
            module,
            c"test".as_ptr() as *const u8 as *const i8,
            &mut func,
        );
        if status != slang_bindings::STATUS_OK {
            eprintln!(
                "Error getting function: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        // Create the input string.
        let str_literal = c"Hello from native code!";
        let str = sys::slang_string_create(str_literal.to_bytes().len(), str_literal.as_ptr());

        if str.is_null() {
            eprintln!("Error creating string");
            sys::slang_context_destroy(context);
            std::process::exit(1);
        }

        // Construct the native struct.
        let mut s = SNative {
            a: 12,
            b: 3,
            c: 99,
            s: str,
            i: 123,
        };

        // Construct the arguments.
        let args = [
            value::value_from_type(layout_id, (&mut s as *mut SNative).cast()),
            #[allow(clippy::approx_constant)]
            value::value_from_f32(3.141),
        ];

        // Invoke the function.
        let mut result: sys::slang_value = std::mem::zeroed();

        let status =
            sys::slang_function_invoke(context, func, args.as_ptr(), args.len(), &mut result);

        if status != slang_bindings::STATUS_OK {
            eprintln!(
                "Error calling function: {}",
                CStr::from_ptr(sys::slang_context_error_string(context)).to_string_lossy()
            );
        }

        // Read the result.
        if result.kind != value::VALUE_TYPE || result.__bindgen_anon_1.type_.data.is_null() {
            eprintln!("Got unexpected return type.");
            sys::slang_context_destroy(context);
            return;
        }

        let ret_s = &*result.__bindgen_anon_1.type_.data.cast::<SNative>();

        // Read back the (script-created) string field.
        if ret_s.s.is_null() {
            eprintln!("Received null instead of string.");
        } else {
            let mut length: usize = 0;
            let mut data: *const std::ffi::c_char = std::ptr::null();

            sys::slang_string_view(ret_s.s, &mut length, &mut data);

            if !data.is_null() {
                let bytes = std::slice::from_raw_parts(data.cast::<u8>(), length);

                println!("String: {}", String::from_utf8_lossy(bytes));
            }
        }

        println!("Value a: {}", ret_s.a);
        println!("Value b: {}", ret_s.b);
        println!("Value c: {}", ret_s.c);
        println!("Value i: {}", ret_s.i);

        // Clean up return values.
        sys::slang_context_gc_remove_temporary(
            context,
            result.__bindgen_anon_1.type_.data.cast_mut(),
        );

        sys::slang_context_gc_run(context);

        // Check that the memory was cleaned up.
        let mut object_count: usize = 0;
        let mut root_set_size: usize = 0;
        let mut byte_size: usize = 0;

        if sys::slang_context_gc_object_count(context, &mut object_count)
            == slang_bindings::STATUS_OK
            && object_count != 0
        {
            println!("GC: There are objects left.");
        }

        if sys::slang_context_gc_root_set_size(context, &mut root_set_size)
            == slang_bindings::STATUS_OK
            && root_set_size != 0
        {
            println!("GC: There are roots left.");
        }

        if sys::slang_context_gc_byte_size(context, &mut byte_size) == slang_bindings::STATUS_OK
            && byte_size != 0
        {
            println!("GC: There is memory left.");
        }

        // Free the input string we allocated.
        sys::slang_string_destroy(str);

        sys::slang_context_destroy(context);
    }
}
