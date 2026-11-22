// CakesCats Drives: a window over sedutil-cli -j -S (docs/json.md).
// The page asks for a command, this side runs sedutil-cli with the passwords
// on stdin and hands back its JSON result. Progress lines of --loadPBAimage
// go to the page as "sedutil-progress" events.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::io::{BufRead, BufReader, Read, Write};
use std::path::PathBuf;
use std::process::{Command, Stdio};

use serde_json::{json, Value};
use tauri::{AppHandle, Emitter};

/// The commands the page may run, with the number of arguments after the
/// command. Nothing that erases data is here yet.
const ALLOWED: &[(&str, usize)] = &[
    ("--scan", 0),
    ("--query", 1),                // device
    ("--listLockingRanges", 2),    // password device
    ("--setLockingRange", 4),      // range RW|RO|LK password device
    ("--setLockingRangeUser", 4),  // range RW|RO|LK rangepassword device
    ("--setMBRDone", 3),           // on|off password device
    ("--setAdmin1Pwd", 3),         // old new device
    ("--loadPBAimage", 3),         // password file device
];

fn cli_path() -> Result<PathBuf, String> {
    // for development on a machine without a drive: point at another build
    if let Ok(p) = std::env::var("SEDUTIL_CLI") {
        return Ok(PathBuf::from(p));
    }
    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let dir = exe.parent().ok_or("the program has no folder")?;
    // the installer puts the bundled sedutil-cli next to this program
    let name = if cfg!(windows) { "sedutil-cli.exe" } else { "sedutil-cli" };
    Ok(dir.join(name))
}

fn check(args: &[String], secrets: &[String], hash: &str) -> Result<(), String> {
    let action = args.first().map(String::as_str).unwrap_or("");
    let (_, count) = ALLOWED
        .iter()
        .find(|(a, _)| *a == action)
        .ok_or_else(|| format!("command {action} is not allowed"))?;
    if args.len() != count + 1 {
        return Err(format!("{action} takes {count} arguments"));
    }
    let dashes = args.iter().filter(|a| a.as_str() == "-").count();
    if dashes != secrets.len() {
        return Err("every password must be passed as -".into());
    }
    if secrets.iter().any(|s| s.contains('\n') || s.contains('\r')) {
        return Err("a password cannot contain a line break".into());
    }
    if !matches!(hash, "" | "sha1" | "sha512") {
        return Err(format!("unknown password hash {hash}"));
    }
    Ok(())
}

fn run(app: AppHandle, args: Vec<String>, secrets: Vec<String>, hash: String) -> Result<Value, String> {
    check(&args, &secrets, &hash)?;
    let path = cli_path()?;
    let mut cmd = Command::new(&path);
    cmd.arg("-j");
    if !secrets.is_empty() {
        cmd.arg("-S");
    }
    if !hash.is_empty() {
        cmd.arg("-H").arg(&hash);
    }
    cmd.args(&args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const CREATE_NO_WINDOW: u32 = 0x0800_0000;
        cmd.creation_flags(CREATE_NO_WINDOW);
    }
    let mut child = cmd
        .spawn()
        .map_err(|e| format!("cannot start {}: {e}", path.display()))?;

    // the passwords, one per line; dropping stdin closes the pipe
    {
        let mut stdin = child.stdin.take().ok_or("no stdin")?;
        let mut input = String::new();
        for s in &secrets {
            input.push_str(s);
            input.push('\n');
        }
        let written = stdin.write_all(input.as_bytes());
        // overwrite our copy, the page's copy is gone with the call
        unsafe {
            for b in input.as_bytes_mut() {
                std::ptr::write_volatile(b, 0);
            }
        }
        written.map_err(|e| e.to_string())?;
    }

    let stderr = child.stderr.take().ok_or("no stderr")?;
    let events = app.clone();
    let reader = std::thread::spawn(move || {
        let mut log = Vec::new();
        for line in BufReader::new(stderr).lines().map_while(Result::ok) {
            if line.starts_with("{\"progress\"") {
                if let Ok(v) = serde_json::from_str::<Value>(&line) {
                    let _ = events.emit("sedutil-progress", v["progress"].clone());
                    continue;
                }
            }
            log.push(line);
        }
        log
    });

    let mut out = String::new();
    child
        .stdout
        .take()
        .ok_or("no stdout")?
        .read_to_string(&mut out)
        .map_err(|e| e.to_string())?;
    let status = child.wait().map_err(|e| e.to_string())?;
    let log = reader.join().unwrap_or_default();

    let mut result: Value = serde_json::from_str(out.trim()).map_err(|_| {
        format!(
            "sedutil-cli ended ({status}) without a result. Its output:\n{}",
            log.join("\n")
        )
    })?;
    if let Some(obj) = result.as_object_mut() {
        obj.insert("log".into(), json!(log));
    }
    Ok(result)
}

/// Run one sedutil-cli command. `args` starts with the command; each
/// password is "-" in `args` and its value is in `secrets`, in order.
#[tauri::command]
async fn sedutil(app: AppHandle, args: Vec<String>, secrets: Vec<String>, hash: String) -> Result<Value, String> {
    tauri::async_runtime::spawn_blocking(move || run(app, args, secrets, hash))
        .await
        .map_err(|e| e.to_string())?
}

/// Choose a PBA image file
#[tauri::command]
async fn pick_image() -> Option<String> {
    tauri::async_runtime::spawn_blocking(|| {
        rfd::FileDialog::new()
            .add_filter("PBA image", &["img"])
            .add_filter("All files", &["*"])
            .pick_file()
            .map(|p| p.display().to_string())
    })
    .await
    .ok()
    .flatten()
}

/// Where sedutil-cli is expected and whether it is there
#[tauri::command]
fn cli_info() -> Value {
    match cli_path() {
        Ok(p) => json!({ "path": p.display().to_string(), "found": p.is_file() }),
        Err(e) => json!({ "path": "", "found": false, "error": e }),
    }
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![sedutil, pick_image, cli_info])
        .run(tauri::generate_context!())
        .expect("error while running the application");
}
