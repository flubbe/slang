use crate::error::Error;
use crate::sys;
use std::ffi::c_void;
use std::marker::PhantomData;
use std::ptr::NonNull;

/// Create a Slang string owned by the interpreter.
/// The returned handle must later be destroyed with `slang_string_destroy`.
pub fn create_string(s: &str) -> Result<sys::slang_string, Error> {
    // SAFETY: the C API expects a length and a pointer to UTF‑8 data that
    // remains valid for the call. `s.as_ptr()` is valid for the lifetime of
    // this call, and we immediately hand ownership to the interpreter.
    let len = s.len();
    let ptr = s.as_ptr() as *const std::os::raw::c_char;
    let raw = unsafe { sys::slang_string_create(len, ptr) };
    if raw.is_null() {
        Err(Error::NullPointer)
    } else {
        Ok(raw)
    }
}

/// Destroy a Slang string previously created with `create_string`.
///
/// # Safety
///
/// `s` must be a valid Slang string that has not already been destroyed.
pub unsafe fn destroy_string(s: sys::slang_string) {
    // SAFETY: The caller guarantees that `s` is a valid, live Slang string.
    unsafe { sys::slang_string_destroy(s) };
}

pub struct OwnedString {
    raw: NonNull<c_void>,
}

impl OwnedString {
    pub fn new(value: &str) -> Result<Self, Error> {
        let raw = create_string(value)?;
        let raw = NonNull::new(raw.cast()).expect("create_string returned a null pointer");

        Ok(Self { raw })
    }

    pub fn as_ptr(&self) -> *mut c_void {
        self.raw.as_ptr()
    }
}

impl Drop for OwnedString {
    fn drop(&mut self) {
        unsafe {
            destroy_string(self.raw.as_ptr().cast());
        }
    }
}

pub struct StringRef<'a> {
    raw: NonNull<c_void>,
    _marker: PhantomData<&'a c_void>,
}

impl<'a> StringRef<'a> {
    /// # Safety
    /// `raw` must point to a valid Slang string that remains alive
    /// for the lifetime `'a`.
    pub unsafe fn from_raw(raw: *mut c_void) -> Result<Self, Error> {
        let raw = NonNull::new(raw).ok_or(Error::NullPointer)?;

        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    pub fn to_string(&self) -> Result<String, Error> {
        let mut len: usize = 0;
        let mut data: *const i8 = std::ptr::null();

        // SAFETY: The caller guarantees that `s` is valid and alive.
        // The C API provides a pointer and length for the string contents.
        unsafe {
            sys::slang_string_view(
                self.raw.as_ptr().cast(),
                &mut len as *mut usize,
                &mut data as *mut *const i8,
            );
        }

        if data.is_null() {
            return Err(Error::NullPointer);
        }

        // SAFETY: The C API guarantees that `data` points to `len` valid UTF-8
        // bytes that remain valid until the string is destroyed. We copy them
        // immediately, before returning.
        let slice = unsafe { std::slice::from_raw_parts(data as *const u8, len) };

        Ok(String::from_utf8_lossy(slice).into_owned())
    }
}
