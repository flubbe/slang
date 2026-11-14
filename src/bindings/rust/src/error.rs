use std::fmt;

/// Errors returned by the interpreter bindings.
#[derive(Debug)]
pub enum Error {
    /// An error occurred while accessing the filesystem.
    Filesystem(String),

    /// An error occurred during garbage collection.
    GarbageCollector(String),

    /// An unspecified error occurred in the interpreter.
    Generic(String),

    /// An invalid argument was provided.
    InvalidArgument,

    /// The function name contains an invalid value.
    InvalidFunctionName,

    /// The module name contains an invalid value.
    InvalidModuleName,

    /// The path contains an invalid value.
    InvalidPath,

    /// The native type name contains an invalid value.
    InvalidTypeName,

    /// A string returned by the interpreter contains invalid UTF-8.
    InvalidUtf8,

    /// A null pointer was encountered where a valid pointer was expected.
    NullPointer,

    /// The interpreter returned an unrecognized status code.
    UnknownStatus(i32, String),

    /// The interpreter returned a value type that cannot be represented by [`crate::value::OwnedValue`].
    UnsupportedValue,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Filesystem(message) => write!(f, "Filesystem error: {message}"),
            Self::GarbageCollector(message) => write!(f, "Garbage collector error: {message}"),
            Self::Generic(message) => write!(f, "{message}"),
            Self::InvalidArgument => write!(f, "Invalid argument"),
            Self::InvalidFunctionName => write!(f, "Invalid function name"),
            Self::InvalidModuleName => write!(f, "Invalid module name"),
            Self::InvalidPath => write!(f, "Invalid path"),
            Self::InvalidTypeName => write!(f, "Invalid type name"),
            Self::InvalidUtf8 => write!(f, "Invalid UTF-8"),
            Self::NullPointer => write!(f, "Unexpected null pointer"),
            Self::UnknownStatus(status, message) => {
                write!(f, "Unknown interpreter status {status}: {message}")
            }
            Self::UnsupportedValue => write!(f, "Unsupported value"),
        }
    }
}

impl std::error::Error for Error {}
