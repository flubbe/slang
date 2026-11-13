// FIXME This shouldn't be necessary

fn main() {
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
        .join("src/bindings/c")
        .canonicalize()
        .expect("Could not find C library directory");

    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", library_dir.display());
}
