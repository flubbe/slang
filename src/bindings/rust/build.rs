fn main() {
    // Instruct cargo to rerun if the header changes
    println!("cargo:rerun-if-changed=../c/include/slang.h");

    let bindings = bindgen::Builder::default()
        .header("../../bindings/c/include/slang.h")
        .clang_arg("-std=c23")
        .allowlist_function("^slang_.*")
        .allowlist_type("^slang_.*")
        .allowlist_var("^SLANG_.*")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    // link to C library

    let profile = std::env::var("PROFILE").unwrap();

    let cmake_config = match profile.as_str() {
        "debug" => "Debug",
        "release" => "Release",
        other => panic!("Unsupported Cargo profile: {other}"),
    };

    let manifest_dir = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());

    let repo_root = manifest_dir.join("../../../");
    let library_dir = repo_root
        .join("build")
        .join(cmake_config)
        .join("src/bindings/c");

    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=dylib=slang_c");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", library_dir.display());
}
