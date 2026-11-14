mod context;
mod error;
mod function;
mod module;
mod native;
mod string;
mod sys;
mod types;
mod value;

pub use context::Context;
pub use error::Error;
pub use function::Function;
pub use module::Module;
pub use native::NativeCall;
pub use string::{OwnedString, StringRef};
pub use types::NativeType;
pub use value::{NativeValue, OwnedValue, Value};

/// Invokes a Slang function with the given arguments.
///
/// This macro forwards the arguments as a tuple to [`Function::invoke`].
///
/// # Examples
///
/// ```ignore
/// let result = call!(function, 42, "hello")?;
/// ```
#[macro_export]
macro_rules! call {
    ($func:expr $(, $arg:expr)* $(,)?) => {
        $func.invoke(($($arg,)*))
    };
}
