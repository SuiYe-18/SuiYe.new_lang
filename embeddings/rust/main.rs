#[link(name = "SuiYeEmbed")]
extern "C" {
    fn suiye_embed_new() -> *mut std::ffi::c_void;
    fn suiye_embed_run_source(vm: *mut std::ffi::c_void, name: *const i8, source: *const i8) -> i32;
    fn suiye_embed_free(vm: *mut std::ffi::c_void);
}

fn main() {
    unsafe {
        let vm = suiye_embed_new();
        let name = std::ffi::CString::new("rust-host").unwrap();
        let src = std::ffi::CString::new("println \"hello from Rust\"\n").unwrap();
        suiye_embed_run_source(vm, name.as_ptr(), src.as_ptr());
        suiye_embed_free(vm);
    }
}
