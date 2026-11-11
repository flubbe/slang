use crate::sys;

pub const VALUE_VOID: sys::slang_value_kind = sys::slang_value_kind_SLANG_VALUE_VOID;
pub const VALUE_I32: sys::slang_value_kind = sys::slang_value_kind_SLANG_VALUE_I32;
pub const VALUE_F32: sys::slang_value_kind = sys::slang_value_kind_SLANG_VALUE_F32;
pub const VALUE_STR: sys::slang_value_kind = sys::slang_value_kind_SLANG_VALUE_STR;
pub const VALUE_TYPE: sys::slang_value_kind = sys::slang_value_kind_SLANG_VALUE_TYPE;

pub fn value_from_i32(value: i32) -> sys::slang_value {
    sys::slang_value {
        kind: VALUE_I32,
        __bindgen_anon_1: sys::slang_value__bindgen_ty_1 { i32_: value },
    }
}
pub fn value_from_f32(value: f32) -> sys::slang_value {
    sys::slang_value {
        kind: VALUE_F32,
        __bindgen_anon_1: sys::slang_value__bindgen_ty_1 { f32_: value },
    }
}

pub fn value_from_str(value: &[u8]) -> sys::slang_value {
    sys::slang_value {
        kind: VALUE_STR,
        __bindgen_anon_1: sys::slang_value__bindgen_ty_1 {
            str_: sys::slang_value__bindgen_ty_1__bindgen_ty_1 {
                length: value.len(),
                data: value.as_ptr().cast(),
            },
        },
    }
}

pub fn value_from_cstr(value: &std::ffi::CStr) -> sys::slang_value {
    value_from_str(value.to_bytes())
}

pub fn value_from_type(layout_id: usize, data: *const std::ffi::c_void) -> sys::slang_value {
    value_from_type_inner(layout_id, data)
}

fn value_from_type_inner(layout_id: usize, data: *const std::ffi::c_void) -> sys::slang_value {
    sys::slang_value {
        kind: VALUE_TYPE,
        __bindgen_anon_1: sys::slang_value__bindgen_ty_1 {
            type_: sys::slang_value__bindgen_ty_1__bindgen_ty_2 { layout_id, data },
        },
    }
}
