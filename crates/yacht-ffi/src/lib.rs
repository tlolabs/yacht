//! See bindings/c/yacht.h for ownership and threading rules.
use std::{
    ffi::{c_char, c_void, CStr, CString},
    sync::LazyLock,
};
static ENGINE: LazyLock<yacht_core::api::Engine> = LazyLock::new(Default::default);
pub type CancelFn = Option<unsafe extern "C" fn(*mut c_void) -> bool>;
/// # Safety
/// request must be a valid NUL-terminated UTF-8 string for the duration of the
/// call. Callback/context must remain valid, be callable on this thread, and
/// must not unwind. The returned allocation must be freed exactly once.
#[no_mangle]
pub unsafe extern "C" fn yacht_request(
    request: *const c_char,
    cancelled: CancelFn,
    context: *mut c_void,
) -> *mut c_char {
    let response = std::panic::catch_unwind(|| {
        if request.is_null() { return br#"{"error":{"code":"invalid_input","message":"Null request."}}"#.to_vec(); }
        let bytes = unsafe { CStr::from_ptr(request) }.to_bytes();
        ENGINE.dispatch(bytes, &|| cancelled.is_some_and(|f| unsafe { f(context) }))
    }).unwrap_or_else(|_| br#"{"error":{"code":"internal_error","message":"The Rust core encountered an internal error."}}"#.to_vec());
    CString::new(response).expect("JSON escapes NUL").into_raw()
}
/// # Safety
/// value must be null or an allocation returned by yacht_request, not yet freed.
#[no_mangle]
pub unsafe extern "C" fn yacht_free(value: *mut c_char) {
    if !value.is_null() {
        drop(unsafe { CString::from_raw(value) });
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn boundary_ownership_errors_and_unicode() {
        unsafe {
            for request in [
                r#"{"version":1,"op":"escape","text":"🛥<&"}"#,
                "invalid",
                r#"{"version":99,"op":"sample"}"#,
            ] {
                let input = CString::new(request).unwrap();
                let result = yacht_request(input.as_ptr(), None, std::ptr::null_mut());
                let v: serde_json::Value =
                    serde_json::from_slice(CStr::from_ptr(result).to_bytes()).unwrap();
                assert!(v.get("ok").is_some() || v.get("error").is_some());
                yacht_free(result);
            }
            yacht_free(std::ptr::null_mut());
        }
    }
}
