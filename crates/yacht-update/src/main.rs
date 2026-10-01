use std::{
    fs,
    path::PathBuf,
    time::{SystemTime, UNIX_EPOCH},
};
use tlo_updater::{CheckState, Client, Policy, Result};
fn run() -> Result<serde_json::Value> {
    let trust: serde_json::Value =
        serde_json::from_str(include_str!("../../../config/update-trust.json"))?;
    let command = std::env::args().nth(1).unwrap_or_else(|| "check".into());
    let base = if cfg!(windows) {
        std::env::var_os("LOCALAPPDATA")
    } else {
        std::env::var_os("XDG_STATE_HOME").or_else(|| {
            std::env::var_os("HOME").map(|p| PathBuf::from(p).join(".local/state").into_os_string())
        })
    }
    .ok_or("No private state directory")?;
    let directory = PathBuf::from(base).join("yacht/updater");
    fs::create_dir_all(&directory)?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        fs::set_permissions(&directory, fs::Permissions::from_mode(0o700))?;
    }
    let lock = fs::OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(false)
        .open(directory.join("lock"))?;
    lock.try_lock()
        .map_err(|_| "Another update check is running")?;
    let state_path = directory.join("state.json");
    let mut state: CheckState = match fs::read(&state_path) {
        Ok(b) => serde_json::from_slice(&b)?,
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => CheckState::default(),
        Err(e) => return Err(e.into()),
    };
    if command == "enable" || command == "disable" {
        state.disabled = command == "disable";
        state.save(&state_path)?;
        return Ok(serde_json::json!({"disabled":state.disabled}));
    }
    if !["check", "check-auto", "download"].contains(&command.as_str()) {
        return Err("Unknown command".into());
    }
    let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
    if command == "check-auto" && !state.due(now) {
        return Ok(serde_json::json!({"status":"deferred"}));
    }
    let keys = serde_json::from_value(trust["keys"].clone())?;
    let (platform, format, identity) = if cfg!(windows) {
        ("windows", "inno-setup", "windows_publisher")
    } else if cfg!(target_os = "linux") {
        ("linux", "appimage", "linux_gpg_fingerprint")
    } else {
        return Err("macOS uses Sparkle".into());
    };
    let arch = match std::env::consts::ARCH {
        "x86_64" => "x64",
        "aarch64" => "arm64",
        _ => return Err("Unsupported architecture".into()),
    };
    let policy = Policy {
        application_id: trust["application_id"].as_str().ok_or("Missing ID")?.into(),
        repository: trust["repository"]
            .as_str()
            .ok_or("Missing repository")?
            .into(),
        installed_version: env!("CARGO_PKG_VERSION").into(),
        platform: platform.into(),
        arch: arch.into(),
        os_version: os_version()?,
        format: format.into(),
        native_identity: trust[identity].as_str().unwrap_or("").into(),
        keys,
    };
    if policy.keys.is_empty() || policy.native_identity.is_empty() {
        return Err("Automatic updates are not configured in this build. Use the official GitHub Releases page.".into());
    }
    state.last_attempt = now;
    state.save(&state_path)?;
    let client = Client::new()?;
    let update = client.check(&policy, state.highest_seen.as_deref(), now)?;
    state.last_success = now;
    if let Some(ref u) = update {
        state.highest_seen = Some(u.manifest().version.clone());
    }
    state.save(&state_path)?;
    let Some(update) = update else {
        return Ok(serde_json::json!({"status":"current"}));
    };
    let mut result = serde_json::json!({"status":"available", "version":update.manifest().version, "notes_url":update.manifest().notes_url, "migration":update.manifest().migration, "sha256":update.asset().sha256, "publisher":update.asset().native_identity});
    if command == "download" {
        // No artifact is launched by this helper. Native adapters verify platform
        // identity again before handing control to a mature installer.
        let version = std::env::args()
            .nth(2)
            .ok_or("Download requires the offered version")?;
        let digest = std::env::args()
            .nth(3)
            .ok_or("Download requires the offered digest")?;
        update.require_offer(&version, &digest)?;
        cleanup_downloads(&directory, now)?;
        let destination = directory.join(format!("download-{}-{}", now, std::process::id()));
        match client.download(&update, &destination) {
            Ok(path) => result["path"] = path.to_string_lossy().to_string().into(),
            Err(error) => {
                let _ = fs::remove_dir(&destination);
                return Err(error);
            }
        }
    }
    Ok(result)
}
fn main() {
    match run() {
        Ok(value) => println!("{value}"),
        Err(e) => {
            eprintln!("Update unavailable: {e}");
            std::process::exit(1);
        }
    }
}

#[cfg(windows)]
fn os_version() -> Result<String> {
    #[repr(C)]
    struct VersionInfo {
        size: u32,
        major: u32,
        minor: u32,
        build: u32,
        platform: u32,
        service_pack: [u16; 128],
    }
    #[link(name = "ntdll")]
    extern "system" {
        fn RtlGetVersion(version: *mut VersionInfo) -> i32;
    }
    let mut version = VersionInfo {
        size: std::mem::size_of::<VersionInfo>() as u32,
        major: 0,
        minor: 0,
        build: 0,
        platform: 0,
        service_pack: [0; 128],
    };
    // RtlGetVersion avoids compatibility-manifest version virtualization.
    if unsafe { RtlGetVersion(&mut version) } != 0 {
        return Err("Cannot determine Windows version".into());
    }
    Ok(format!(
        "{}.{}.{}",
        version.major, version.minor, version.build
    ))
}
#[cfg(all(target_os = "linux", target_env = "gnu"))]
fn os_version() -> Result<String> {
    extern "C" {
        fn gnu_get_libc_version() -> *const std::ffi::c_char;
    }
    let version = unsafe { std::ffi::CStr::from_ptr(gnu_get_libc_version()) }.to_str()?;
    // Linux minimum OS is the glibc ABI baseline, not a distribution marketing version.
    Ok(if version.split('.').count() == 2 {
        format!("{version}.0")
    } else {
        version.into()
    })
}
#[cfg(not(any(windows, all(target_os = "linux", target_env = "gnu"))))]
fn os_version() -> Result<String> {
    Err("Unsupported updater operating system".into())
}

// Retain completed installers for seven days so Explorer/manual installation can
// finish. Only remove task-owned directories, never symlinks or unrelated state.
fn cleanup_downloads(directory: &std::path::Path, now: u64) -> Result<()> {
    for entry in fs::read_dir(directory)? {
        let entry = entry?;
        if !entry.file_type()?.is_dir() {
            continue;
        }
        let name = entry.file_name();
        let Some(name) = name.to_str() else { continue };
        let parts: Vec<_> = name.split('-').collect();
        if parts.len() != 3 || parts[0] != "download" || parts[2].parse::<u32>().is_err() {
            continue;
        }
        let Ok(created) = parts[1].parse::<u64>() else {
            continue;
        };
        if now.saturating_sub(created) >= 7 * 86400 {
            fs::remove_dir_all(entry.path())?;
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn cleanup_retains_recent_downloads_and_unrelated_state() {
        let root = std::env::temp_dir().join(format!("yacht-cleanup-test-{}", std::process::id()));
        fs::create_dir_all(root.join("download-1-42")).unwrap();
        fs::create_dir_all(root.join("download-999999-42")).unwrap();
        fs::create_dir_all(root.join("settings")).unwrap();
        cleanup_downloads(&root, 1000000).unwrap();
        assert!(!root.join("download-1-42").exists());
        assert!(root.join("download-999999-42").exists());
        assert!(root.join("settings").exists());
        fs::remove_dir_all(root).unwrap();
    }
}
