# CakesCats Drives

A window for self-encrypting drives: see their state, unlock and lock them,
open locking ranges, change the Admin1 password and write the PBA image.
It runs the `sedutil-cli` of this repository with `-j -S`
([docs/json.md](../docs/json.md)), so passwords go through a pipe and never
show up in the process list.

Status: first test version for Windows. It asks for administrator rights at
start. Nothing in it erases data yet: setting a drive up (`--initialSetup`),
reverts and PSID revert stay on the command line for now.

## Layout

- `ui/`: the page (plain HTML, CSS and JS, no build step).
- `src-tauri/`: the Tauri side. `src/main.rs` runs `sedutil-cli` and lets
  through only the commands in `ALLOWED`.
- `dev/`: a stand-in backend for trying the page in a browser. Run
  `python3 dev/make-preview.py` and open `dev/preview.html` (Admin1
  password `test`, range 1 password `r1`).
- `icon-source.png`: the logo, scaled up; CI makes the icons from it.

## Build

CI (`windows-gui` in `.github/workflows/build.yml`) builds the MSI. By hand on
Windows, with Rust and Node installed and `sedutil-cli.exe` built:

```
copy path\to\sedutil-cli.exe src-tauri\binaries\sedutil-cli-x86_64-pc-windows-msvc.exe
npx @tauri-apps/cli@2 icon icon-source.png -o src-tauri/icons
npx @tauri-apps/cli@2 build
```

The MSI is in `src-tauri/target/release/bundle/msi/`. `SEDUTIL_CLI` points
the program at another `sedutil-cli` for development.
