use base64::{engine::general_purpose::STANDARD as B64, Engine as _};
use ring::signature::{Ed25519KeyPair, KeyPair};
use std::{
    fs,
    process::Command,
    time::{SystemTime, UNIX_EPOCH},
};
use tlo_updater::{Asset, Manifest};
#[test]
fn release_tool_signs_roundtrips_and_rejects_artifact_and_version_mismatch() {
    let dir = tempfile::tempdir().unwrap();
    let root = dir.path();
    let key = Ed25519KeyPair::from_seed_unchecked(&[42; 32]).unwrap();
    let trust = serde_json::json!({"application_id":"org.tlo.test","repository":"tlolabs/test","keys":{"test":B64.encode(key.public_key().as_ref())},"windows_publisher":"test-publisher","macos_team_id":"test-team","linux_gpg_fingerprint":"test-gpg"});
    let config = root.join("trust.json");
    fs::write(&config, serde_json::to_vec(&trust).unwrap()).unwrap();
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let mut m = Manifest {
        schema: 1,
        application_id: "org.tlo.test".into(),
        repository: "tlolabs/test".into(),
        channel: "stable".into(),
        version: "2.0.0".into(),
        tag: "v2.0.0".into(),
        draft: false,
        prerelease: false,
        published_at: now,
        expires_at: now + 86400,
        notes_url: "https://github.com/tlolabs/test/releases/tag/v2.0.0".into(),
        restart_required: true,
        migration: "none".into(),
        assets: vec![],
    };
    for (platform, format, identity, suffix) in [
        ("macos", "sparkle-zip", "test-team", "zip"),
        ("windows", "inno-setup", "test-publisher", "exe"),
        ("linux", "appimage", "test-gpg", "AppImage"),
    ] {
        for arch in ["x64", "arm64"] {
            let filename = format!("Test-{platform}-{arch}.{suffix}");
            fs::write(root.join(&filename), b"fixture bytes, not executable").unwrap();
            m.assets.push(Asset {
                platform: platform.into(),
                arch: arch.into(),
                min_os: "1.0.0".into(),
                format: format.into(),
                url: format!("https://github.com/tlolabs/test/releases/download/v2.0.0/{filename}"),
                filename,
                size: 0,
                sha256: String::new(),
                native_identity: identity.into(),
            });
        }
    }
    let spec = root.join("manifest.json");
    fs::write(&spec, serde_json::to_vec(&m).unwrap()).unwrap();
    let tool = env!("CARGO_BIN_EXE_tlo-release");
    let invoke = |command: &str, path: &std::path::Path, version: &str| {
        Command::new(tool)
            .arg(command)
            .arg(path)
            .arg(root)
            .arg(&config)
            .arg(version)
            .env("TLO_UPDATE_KEY_ID", "test")
            .env("TLO_UPDATE_PRIVATE_KEY", B64.encode([42; 32]))
            .output()
            .unwrap()
    };
    assert!(!invoke("sign", &spec, "3.0.0").status.success());
    let result = invoke("sign", &spec, "2.0.0");
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    let envelope = root.join("tlo-update.json");
    assert!(invoke("verify", &envelope, "2.0.0").status.success());
    fs::write(root.join(&m.assets[0].filename), b"substituted").unwrap();
    assert!(!invoke("verify", &envelope, "2.0.0").status.success());
}
