use crate::error::Error;
use crate::module::Module;
use crate::native::NativeCall;
use crate::sys;
use crate::types::{NativeType, TypeLayout};
use std::ffi::{CStr, CString};
use std::marker::PhantomData;
use std::ptr::NonNull;

pub struct Context {
    raw: NonNull<sys::slang_context>,
    _marker: PhantomData<std::rc::Rc<()>>,
}

impl std::fmt::Debug for Context {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{:?}", self.raw)
    }
}

fn qualified_type_name<T: NativeType>() -> Result<String, Error> {
    let type_name = T::NAME;
    let module_name = T::MODULE;

    if type_name.is_empty() || type_name.contains('.') {
        return Err(Error::InvalidTypeName);
    }

    if module_name.is_empty() {
        return Err(Error::InvalidModuleName);
    }

    Ok(format!("{module_name}.{type_name}").to_string())
}

impl Context {
    fn last_error(&self) -> String {
        let ptr = unsafe { sys::slang_context_error_string(self.raw.as_ptr()) };

        if ptr.is_null() {
            return String::new();
        }

        unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
    }

    fn from_raw(ctx: *mut sys::slang_context) -> Result<Self, Error> {
        let raw = NonNull::new(ctx).ok_or(Error::NullPointer)?;

        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    pub fn new() -> Result<Self, Error> {
        let raw = unsafe { sys::slang_context_create() };
        let raw = NonNull::new(raw).ok_or(Error::NullPointer)?;

        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    pub fn add_search_path(&mut self, path: impl AsRef<std::path::Path>) -> Result<(), Error> {
        let path = path.as_ref().to_str().ok_or(Error::InvalidPath)?;
        let path = CString::new(path).map_err(|_| Error::InvalidPath)?;

        let status =
            unsafe { sys::slang_context_add_search_path(self.raw.as_ptr(), path.as_ptr()) };

        self.check_status(status)
    }

    pub fn resolve_module(&self, name: &str) -> Result<Module<'_>, Error> {
        let name = CString::new(name).map_err(|_| Error::InvalidModuleName)?;
        let mut raw_module = std::ptr::null_mut();

        let status = unsafe {
            sys::slang_context_resolve_module(self.raw.as_ptr(), name.as_ptr(), &mut raw_module)
        };
        self.check_status(status)?;

        let raw_module = NonNull::new(raw_module).ok_or(Error::NullPointer)?;
        Ok(Module::from_raw(self, raw_module))
    }

    /// Register a native Rust function so it can be called from Slang.
    ///
    /// The function can be a normal Rust `fn(&mut NativeCall) -> Result<(), Error>`
    /// (or a closure that satisfies the same signature).
    pub fn register_native<F>(
        &mut self,
        module_name: &str,
        function_name: &str,
        func: F,
    ) -> Result<(), Error>
    where
        F: FnMut(&mut NativeCall) -> Result<(), Error> + 'static,
    {
        // Store both the closure and a raw pointer to the Context.
        // The Context lives for the entire lifetime of the registration, so
        // keeping a raw pointer is safe.
        struct CallbackData<F> {
            func: F,
            error: Option<Error>,
        }

        // Box the struct to obtain a stable heap pointer for the C API.
        let boxed = Box::new(CallbackData { func, error: None });
        let user_data = Box::into_raw(boxed) as *mut std::ffi::c_void;

        // Trampoline that converts the C call into a Rust closure call.
        extern "C" fn shim<F>(
            ctx: *mut sys::slang_context,
            stack: *mut sys::slang_operand_stack_impl,
            user: *mut std::ffi::c_void,
        ) where
            F: FnMut(&mut NativeCall) -> Result<(), Error>,
        {
            if ctx.is_null() || stack.is_null() || user.is_null() {
                return;
            }

            let cb = unsafe { &mut *(user as *mut CallbackData<F>) };

            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                let ctx = std::mem::ManuallyDrop::new(
                    Context::from_raw(ctx).expect("invalid context pointer"),
                );

                let mut call = unsafe { NativeCall::from_raw(&ctx, stack)? };

                (cb.func)(&mut call)
            }));

            match result {
                Ok(Ok(())) => {}
                Ok(Err(error)) => {
                    cb.error = Some(error);
                }
                Err(_) => {
                    cb.error = Some(Error::Generic("Rust native callback panicked".into()));
                }
            }

            // TODO: Propagate callback errors and panics to the runtime.
        }

        let c_module = std::ffi::CString::new(module_name).map_err(|_| Error::InvalidArgument)?;
        let c_func = std::ffi::CString::new(function_name).map_err(|_| Error::InvalidArgument)?;

        let status = unsafe {
            sys::slang_function_register_native(
                self.as_raw(),
                c_module.as_ptr(),
                c_func.as_ptr(),
                Some(shim::<F> as extern "C" fn(_, _, _)),
                user_data,
            )
        };

        let result = self.check_status(status);

        if result.is_err() {
            unsafe {
                drop(Box::from_raw(user_data as *mut CallbackData<F>));
            }
        }

        result
    }

    pub fn register_type<T: NativeType>(&self) -> Result<TypeLayout<'_, T>, Error> {
        let name = qualified_type_name::<T>().map_err(|_| Error::InvalidTypeName)?;
        let c_name = CString::new(name.as_str()).map_err(|_| Error::InvalidTypeName)?;

        let offsets = T::pointer_offsets();

        let status = unsafe {
            sys::slang_type_register_layout(
                self.raw.as_ptr(),
                c_name.as_ptr(),
                offsets.as_ptr(),
                offsets.len(),
            )
        };

        self.check_status(status)?;
        let layout = self.get_type_layout::<T>()?;

        Ok(layout)
    }

    pub fn get_type_layout<T: NativeType>(&self) -> Result<TypeLayout<'_, T>, Error> {
        let name = qualified_type_name::<T>()?;
        let c_name = CString::new(name).map_err(|_| Error::InvalidTypeName)?;
        let mut layout_id: usize = 0;

        let status = unsafe {
            sys::slang_context_get_type_layout_id(self.as_raw(), c_name.as_ptr(), &mut layout_id)
        };

        self.check_status(status)?;

        Ok(TypeLayout::new(self, layout_id))
    }

    /// Remove a temporary GC root.
    ///
    /// # Safety
    ///
    /// `object` must be a valid GC-managed object belonging to this context.
    pub unsafe fn gc_remove_temporary(
        &mut self,
        object: *mut std::ffi::c_void,
    ) -> Result<(), Error> {
        // SAFETY: The caller guarantees that `object` is a valid GC-managed
        // object belonging to this context.
        let status = unsafe { sys::slang_context_gc_remove_temporary(self.as_raw(), object) };

        self.check_status(status)
    }

    pub fn gc_run(&mut self) -> Result<(), Error> {
        let status = unsafe { sys::slang_context_gc_run(self.as_raw()) };
        self.check_status(status)
    }

    pub fn gc_object_count(&self) -> Result<usize, Error> {
        let mut count: usize = 0;
        let status =
            unsafe { sys::slang_context_gc_object_count(self.as_raw(), &mut count as *mut usize) };
        self.check_status(status)?;
        Ok(count)
    }

    pub fn gc_root_set_size(&self) -> Result<usize, Error> {
        let mut size: usize = 0;
        let status =
            unsafe { sys::slang_context_gc_root_set_size(self.as_raw(), &mut size as *mut usize) };
        self.check_status(status)?;
        Ok(size)
    }

    pub fn gc_byte_size(&self) -> Result<usize, Error> {
        let mut size: usize = 0;
        let status =
            unsafe { sys::slang_context_gc_byte_size(self.as_raw(), &mut size as *mut usize) };
        self.check_status(status)?;
        Ok(size)
    }

    pub fn gc_stats(&self) -> Result<(usize, usize, usize), Error> {
        Ok((
            self.gc_object_count()?,
            self.gc_root_set_size()?,
            self.gc_byte_size()?,
        ))
    }

    pub(crate) fn as_raw(&self) -> *mut sys::slang_context {
        self.raw.as_ptr()
    }

    pub(crate) fn check_status(&self, status: sys::slang_status) -> Result<(), Error> {
        match status {
            sys::slang_status_SLANG_STATUS_OK => Ok(()),
            sys::slang_status_SLANG_STATUS_INVALID_ARGUMENT => Err(Error::InvalidArgument),
            sys::slang_status_SLANG_STATUS_FILESYSTEM_ERROR => {
                Err(Error::Filesystem(self.last_error()))
            }
            sys::slang_status_SLANG_STATUS_GC_ERROR => {
                Err(Error::GarbageCollector(self.last_error()))
            }
            sys::slang_status_SLANG_STATUS_ERROR => Err(Error::Generic(self.last_error())),
            _ => Err(Error::UnknownStatus(status, self.last_error())),
        }
    }
}

impl Drop for Context {
    fn drop(&mut self) {
        unsafe {
            sys::slang_context_destroy(self.raw.as_ptr());
        }
    }
}
