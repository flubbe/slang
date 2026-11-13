use crate::context::Context;
use crate::error::Error;
use crate::sys;
use std::ptr::NonNull;

pub struct NativeCall<'a> {
    context: &'a Context,
    stack: NonNull<sys::slang_operand_stack_impl>,
}

impl<'a> NativeCall<'a> {
    /// Construct a new `NativeCall` from a raw stack pointer.
    ///
    /// # Safety
    ///
    /// The caller must guarantee that `stack` is a valid operand‑stack
    /// obtained from a previous native function call and that it lives
    /// as long as this `NativeCall` instance.
    pub unsafe fn from_raw(
        context: &'a Context,
        stack: *mut sys::slang_operand_stack_impl,
    ) -> Result<Self, Error> {
        let stack = NonNull::new(stack).ok_or(Error::NullPointer)?;
        Ok(Self { context, stack })
    }

    /// Return the underlying context.
    #[inline]
    pub fn context(&self) -> &Context {
        self.context
    }

    /// Pop a 32‑bit integer from the stack.
    pub fn pop_i32(&mut self) -> Result<i32, Error> {
        let mut out: i32 = 0;
        unsafe {
            // SAFETY: we have a valid stack pointer and context.
            sys::slang_stack_pop_i32(self.context.as_raw(), self.stack.as_ptr(), &mut out);
        }
        Ok(out)
    }

    /// Pop a 32‑bit float from the stack.
    pub fn pop_f32(&mut self) -> Result<f32, Error> {
        let mut out: f32 = 0.0;
        unsafe {
            sys::slang_stack_pop_f32(self.context.as_raw(), self.stack.as_ptr(), &mut out);
        }
        Ok(out)
    }

    /// Pop a 64‑bit integer from the stack.
    pub fn pop_i64(&mut self) -> Result<i64, Error> {
        let mut out: i64 = 0;
        unsafe {
            // SAFETY: we have a valid stack pointer and context.
            sys::slang_stack_pop_i64(self.context.as_raw(), self.stack.as_ptr(), &mut out);
        }
        Ok(out)
    }

    /// Pop a 64‑bit float from the stack.
    pub fn pop_f64(&mut self) -> Result<f64, Error> {
        let mut out: f64 = 0.0;
        unsafe {
            sys::slang_stack_pop_f64(self.context.as_raw(), self.stack.as_ptr(), &mut out);
        }
        Ok(out)
    }

    /// Pop a string from the stack and return it as a Rust `String`.
    pub fn pop_string(&mut self) -> Result<String, Error> {
        let mut length: usize = 0;
        let mut data: *const std::os::raw::c_char = std::ptr::null();
        unsafe {
            // SAFETY: valid context & stack; the API guarantees that on
            // success `data` points to a string that lives until the next GC run.
            let status = sys::slang_stack_pop_string(
                self.context.as_raw(),
                self.stack.as_ptr(),
                &mut length as *mut usize,
                &mut data as *mut *const std::os::raw::c_char,
            );
            self.context.check_status(status)?;
        }
        if data.is_null() {
            return Ok(String::new());
        }
        // SAFETY: `data` points to `length` bytes of valid UTF‑8 per API contract.
        let slice = unsafe { std::slice::from_raw_parts(data as *const u8, length) };
        String::from_utf8(slice.to_vec()).map_err(|_| Error::InvalidUtf8)
    }

    /// Push a 32‑bit integer onto the stack.
    pub fn push_i32(&mut self, value: i32) -> Result<(), Error> {
        unsafe {
            sys::slang_stack_push_i32(self.context.as_raw(), self.stack.as_ptr(), value);
        }
        Ok(())
    }

    /// Push a 32‑bit float onto the stack.
    pub fn push_f32(&mut self, value: f32) -> Result<(), Error> {
        unsafe {
            sys::slang_stack_push_f32(self.context.as_raw(), self.stack.as_ptr(), value);
        }
        Ok(())
    }

    /// Push a 64‑bit integer onto the stack.
    pub fn push_i64(&mut self, value: i64) -> Result<(), Error> {
        unsafe {
            sys::slang_stack_push_i64(self.context.as_raw(), self.stack.as_ptr(), value);
        }
        Ok(())
    }

    /// Push a 64‑bit float onto the stack.
    pub fn push_f64(&mut self, value: f64) -> Result<(), Error> {
        unsafe {
            sys::slang_stack_push_f64(self.context.as_raw(), self.stack.as_ptr(), value);
        }
        Ok(())
    }

    /// Push a string onto the stack.
    pub fn push_string(&mut self, s: &str) -> Result<(), Error> {
        let c_str = std::ffi::CString::new(s).map_err(|_| Error::InvalidUtf8)?;
        unsafe {
            sys::slang_stack_push_string(
                self.context.as_raw(),
                self.stack.as_ptr(),
                c_str.as_bytes().len(),
                c_str.as_ptr(),
            );
        }
        Ok(())
    }
}
