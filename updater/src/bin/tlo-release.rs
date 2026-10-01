//! Offline signing/verification and read-only GitHub qualification; never publishes.
use base64::{engine::general_purpose::STANDARD as B64, Engine as _};
use ring::signature::{Ed25519KeyPair, KeyPair};
use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs,
    io::{Read, Write},
    path::Path,
    time::{SystemTime, UNIX_EPOCH},
};
use tlo_updater::*;
#[derive(Deserialize)]
struct Trust {
    application_id: String,
    repository: String,
    keys: BTreeMap<String, String>,
    windows_publisher: String,
    macos_team_id: String,
    linux_gpg_fingerprint: String,
}
fn policy(t: &Trust, platform: &str, arch: &str, installed: &str) -> Policy {
    let (format, identity) = match platform {
        "macos" => ("sparkle-zip", &t.macos_team_id),
        "windows" => ("inno-setup", &t.windows_publisher),
        _ => ("appimage", &t.linux_gpg_fingerprint),
    };
    Policy {
        application_id: t.application_id.clone(),
        repository: t.repository.clone(),
        installed_version: installed.into(),
        platform: platform.into(),
        arch: arch.into(),
        os_version: "9999.0.0".into(),
        format: format.into(),
        native_identity: identity.clone(),
        keys: t.keys.clone(),
    }
}
fn digest(path: &Path) -> Result<(u64, String)> {
    let mut file = fs::File::open(path)?;
    let mut h = Sha256::new();
    let mut b = [0; 65536];
    let mut size = 0;
    loop {
        let n = file.read(&mut b)?;
        if n == 0 {
            break;
        }
        size += n as u64;
        h.update(&b[..n]);
    }
    Ok((size, format!("{:x}", h.finalize())))
}
fn run() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 6 {
        return Err("Usage: tlo-release <sign|verify|qualify> <manifest-or-tag> <assets-directory> <trust.json> <version>".into());
    }
    let command = &args[1];
    let assets = Path::new(&args[3]);
    let trust: Trust = serde_json::from_slice(&fs::read(&args[4])?)?;
    stable_version(&args[5])?;
    let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
    if command == "qualify" {
        // Discover exactly as an older installed client would. Does not qualify installation.
        let client = Client::new()?;
        fs::create_dir_all(assets)?;
        for platform in ["macos", "windows", "linux"] {
            for arch in ["x64", "arm64"] {
                let p = policy(&trust, platform, arch, &args[5]);
                let u = client
                    .check(&p, None, now)?
                    .ok_or("No newer compatible published release")?;
                if u.manifest().tag != args[2] {
                    return Err("Latest release differs from expected tag".into());
                }
                let folder = assets.join(format!("{platform}-{arch}"));
                let downloaded = client.download(&u, &folder)?;
                println!("Authenticated published artifact: {}", downloaded.display());
            }
        }
        println!("Discovery and authentication passed; native installation is NOT qualified.");
        return Ok(());
    }
    let bytes = if command == "sign" {
        let mut m: Manifest = serde_json::from_slice(&fs::read(&args[2])?)?;
        if m.version != args[5] {
            return Err("Manifest/application version disagreement".into());
        }
        for a in &mut m.assets {
            // Reject path traversal before opening any candidate.
            if a.filename.is_empty()
                || !a
                    .filename
                    .bytes()
                    .all(|b| b.is_ascii_alphanumeric() || b"._-".contains(&b))
                || a.filename == "."
                || a.filename == ".."
            {
                return Err("Invalid artifact filename".into());
            }
            (a.size, a.sha256) = digest(&assets.join(&a.filename))?;
        }
        let key_id = std::env::var("TLO_UPDATE_KEY_ID")?;
        let seed = B64.decode(std::env::var("TLO_UPDATE_PRIVATE_KEY")?)?;
        let key = Ed25519KeyPair::from_seed_unchecked(&seed).map_err(|_| "Invalid Ed25519 seed")?;
        if trust.keys.get(&key_id) != Some(&B64.encode(key.public_key().as_ref())) {
            return Err("Signing key does not match committed public key".into());
        }
        let payload = serde_json::to_vec(&m)?;
        serde_json::to_vec(&Envelope {
            key_id,
            payload: B64.encode(&payload),
            signature: B64.encode(key.sign(&payload)),
        })?
    } else if command == "verify" {
        fs::read(&args[2])?
    } else {
        return Err("Unknown release command".into());
    };
    let mut sums = String::new();
    for platform in ["macos", "windows", "linux"] {
        for arch in ["x64", "arm64"] {
            let p = policy(&trust, platform, arch, "0.0.0");
            if p.native_identity.is_empty() {
                return Err("Missing pinned native signing identity".into());
            }
            let u = verify(&bytes, &p, None, now)?
                .ok_or("Release is missing a required production target")?;
            if u.manifest().version != args[5] {
                return Err("Manifest/application version mismatch".into());
            }
            verify_stream(
                fs::File::open(assets.join(&u.asset().filename))?,
                std::io::sink(),
                u.asset(),
            )?;
            sums += &format!("{}  {}\n", u.asset().sha256, u.asset().filename);
        }
    }
    if command == "sign" {
        let path = assets.join("tlo-update.json");
        let mut f = fs::OpenOptions::new()
            .create_new(true)
            .write(true)
            .open(path)?;
        f.write_all(&bytes)?;
        f.sync_all()?;
        fs::write(assets.join("SHA256SUMS"), sums)?;
    }
    println!("Signed manifest and all six artifact digests verified. Native signing and installation require separate platform gates.");
    Ok(())
}
fn main() {
    if let Err(e) = run() {
        eprintln!("Release gate failed: {e}");
        std::process::exit(1)
    }
}
