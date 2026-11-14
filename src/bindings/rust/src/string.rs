use crate::error::Error;
use crate::sys;
use std::ffi::c_void;
use std::marker::PhantomData;
use std::ptr::NonNull;

/// An owned string allocated by the interpreter.
///
/// The string is destroyed when the `OwnedString` is dropped.
pub struct OwnedString {
    raw: NonNull<c_void>,
}

impl OwnedString {
    /// Creates a new interpreter-managed string from a UTF-8 string.
    ///
    /// # Errors
    ///
    /// Returns [`Error::NullPointer`] if the string could not be allocated.    
    pub fn new(value: &str) -> Result<Self, Error> {
        // SAFETY: The C API copies the UTF-8 data into an interpreter-owned string.
        // `value.as_ptr()` is valid for the duration of the call.

        let len = value.len();
        let ptr = value.as_ptr() as *const std::os::raw::c_char;
        let raw = unsafe { sys::slang_string_create(len, ptr) };
        let raw = NonNull::new(raw.cast()).ok_or(Error::NullPointer)?;

        Ok(Self { raw })
    }

    /// Returns a pointer to the underlying interpreter-managed string.
    ///
    /// The pointer remains valid as long as this `OwnedString` is alive.
    pub fn as_ptr(&self) -> *mut c_void {
        self.raw.as_ptr()
    }
}

impl Drop for OwnedString {
    fn drop(&mut self) {
        // SAFETY: `self.raw` is a valid interpreter-owned string created by
        // `slang_string_create` and has not previously been destroyed.
        unsafe { sys::slang_string_destroy(self.raw.as_ptr().cast()) };
    }
}

/// A borrowed reference to an interpreter-managed string.
///
/// The referenced string must remain alive for the lifetime `'a`.
pub struct StringRef<'a> {
    raw: NonNull<c_void>,
    _marker: PhantomData<&'a c_void>,
}

impl<'a> StringRef<'a> {
    /// Creates a borrowed string reference from a raw pointer.
    ///
    /// # Safety
    ///
    /// `raw` must point to a valid interpreter-managed string that remains alive
    /// for the lifetime `'a`. Ownership is not transferred.
    ///
    /// # Errors
    ///
    /// Returns [`Error::NullPointer`] if `raw` is null.
    pub unsafe fn from_raw(raw: *mut c_void) -> Result<Self, Error> {
        let raw = NonNull::new(raw).ok_or(Error::NullPointer)?;

        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    /// Copies the string contents into an owned Rust `String`.
    ///
    /// # Errors
    ///
    /// Returns [`Error::NullPointer`] if the string data pointer is null.
    pub fn to_string(&self) -> Result<String, Error> {
        let mut len: usize = 0;
        let mut data: *const i8 = std::ptr::null();

        // SAFETY: `self.raw` points to a valid interpreter-managed string that
        // remains alive for the lifetime of `self`.
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

        // SAFETY: The C API guarantees that `data` points to `len` bytes of valid
        // UTF-8 data that remain valid until the string is destroyed. We copy the
        // data immediately, before returning.
        let slice = unsafe { std::slice::from_raw_parts(data as *const u8, len) };

        String::from_utf8(slice.to_vec()).map_err(|_| Error::InvalidUtf8)
    }
}
