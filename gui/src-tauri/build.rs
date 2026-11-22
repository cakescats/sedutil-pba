fn main() {
    // Talking to a drive needs administrator rights on Windows, so the
    // manifest asks for them at start (one UAC prompt). The manifest keeps
    // the Common Controls v6 dependency that Tauri's default one has.
    let windows = tauri_build::WindowsAttributes::new()
        .app_manifest(include_str!("windows-app-manifest.xml"));
    tauri_build::try_build(tauri_build::Attributes::new().windows_attributes(windows))
        .expect("failed to run the tauri build script");
}
