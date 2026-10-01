// SPDX-License-Identifier: GPL-3.0-or-later
//! Trust and transport shared across applications. Installation belongs to native adapters.
//! Based on ATIV's Ed25519 envelope/download design; no AVID or application dependency.
use base64::{engine::general_purpose::STANDARD as B64, Engine as _};
use ring::signature::{UnparsedPublicKey, ED25519};
use semver::Version;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    error::Error,
    fs,
    io::{Read, Write},
    path::{Path, PathBuf},
    time::Duration,
};
pub type Result<T> = std::result::Result<T, Box<dyn Error + Send + Sync>>;
pub const MAX_METADATA: u64 = 1024 * 1024;
pub const CHECK_INTERVAL: u64 = 86400;
pub const RETRY_INTERVAL: u64 = 3600;

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Asset {
    pub platform: String,
    pub arch: String,
    pub min_os: String,
    pub format: String,
    pub filename: String,
    pub url: String,
    pub size: u64,
    pub sha256: String,
    /// Native verification is additional to the signed digest, never a replacement.
    pub native_identity: String,
}
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Manifest {
    pub schema: u32,
    pub application_id: String,
    pub repository: String,
    pub channel: String,
    pub version: String,
    pub tag: String,
    pub draft: bool,
    pub prerelease: bool,
    pub published_at: u64,
    pub expires_at: u64,
    pub notes_url: String,
    pub restart_required: bool,
    pub migration: String,
    pub assets: Vec<Asset>,
}
#[derive(Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct Envelope {
    pub key_id: String,
    /// Base64 of exact UTF-8 JSON bytes, avoiding JSON canonicalization ambiguity.
    pub payload: String,
    pub signature: String,
}
#[derive(Clone)]
pub struct Policy {
    pub application_id: String,
    pub repository: String,
    pub installed_version: String,
    pub platform: String,
    pub arch: String,
    pub os_version: String,
    pub format: String,
    pub native_identity: String,
    pub keys: BTreeMap<String, String>,
}
/// Only signature-verified, validated manifests can construct this type.
#[derive(Debug)]
pub struct VerifiedUpdate {
    manifest: Manifest,
    asset: Asset,
}
impl VerifiedUpdate {
    pub fn manifest(&self) -> &Manifest {
        &self.manifest
    }
    pub fn asset(&self) -> &Asset {
        &self.asset
    }
    /// Bind a user-approved offer to immutable signed fields before downloading.
    /// A release replaced between check and download requires a new confirmation.
    pub fn require_offer(&self, version: &str, sha256: &str) -> Result<()> {
        if self.manifest.version != version || self.asset.sha256 != sha256 {
            return Err("The update changed since confirmation; check again".into());
        }
        if self.manifest.migration != "none" {
            return Err("This update requires manual migration".into());
        }
        Ok(())
    }
}
pub fn stable_version(value: &str) -> Result<Version> {
    let v = Version::parse(value)?;
    if !v.pre.is_empty() || !v.build.is_empty() {
        return Err("Expected stable MAJOR.MINOR.PATCH".into());
    }
    Ok(v)
}
fn safe_name(value: &str) -> bool {
    !value.is_empty()
        && value != "."
        && value != ".."
        && value
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b"._-".contains(&b))
}
fn release_root(repository: &str) -> Result<String> {
    let parts: Vec<_> = repository.split('/').collect();
    if parts.len() != 2 || !parts.iter().all(|p| safe_name(p)) {
        return Err("Invalid repository".into());
    }
    Ok(format!("https://github.com/{repository}/releases"))
}
/// Signature verification precedes all use of manifest fields, including URLs and notes.
pub fn verify(
    bytes: &[u8],
    policy: &Policy,
    highest_seen: Option<&str>,
    now: u64,
) -> Result<Option<VerifiedUpdate>> {
    if bytes.len() as u64 > MAX_METADATA {
        return Err("Metadata exceeds limit".into());
    }
    let envelope: Envelope = serde_json::from_slice(bytes)?;
    let public = policy
        .keys
        .get(&envelope.key_id)
        .ok_or("Untrusted signing key")?;
    let payload = B64.decode(envelope.payload)?;
    UnparsedPublicKey::new(&ED25519, B64.decode(public)?)
        .verify(&payload, &B64.decode(envelope.signature)?)
        .map_err(|_| "Invalid update signature")?;
    let m: Manifest = serde_json::from_slice(&payload)?;
    let root = release_root(&policy.repository)?;
    if m.schema != 1
        || m.application_id != policy.application_id
        || m.repository != policy.repository
        || m.channel != "stable"
    {
        return Err("Manifest identity, schema or channel mismatch".into());
    }
    if m.draft || m.prerelease {
        return Ok(None);
    }
    let version = stable_version(&m.version)?;
    if m.tag != format!("v{}", m.version) || m.notes_url != format!("{root}/tag/{}", m.tag) {
        return Err("Release tag or notes URL mismatch".into());
    }
    if m.published_at > now.saturating_add(300)
        || m.expires_at <= now
        || m.expires_at <= m.published_at
        || m.expires_at - m.published_at > 366 * 86400
    {
        return Err("Expired or invalid metadata validity period".into());
    }
    if !["none", "manual"].contains(&m.migration.as_str()) {
        return Err("Unknown migration policy".into());
    }
    let mut targets = std::collections::BTreeSet::new();
    for a in &m.assets {
        if !["macos", "windows", "linux"].contains(&a.platform.as_str())
            || !["x64", "arm64"].contains(&a.arch.as_str())
            || !safe_name(&a.filename)
            || a.url != format!("{root}/download/{}/{}", m.tag, a.filename)
            || a.size == 0
            || a.size > 4 * 1024 * 1024 * 1024
            || a.sha256.len() != 64
            || !a
                .sha256
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
            || a.native_identity.is_empty()
            || !targets.insert((&a.platform, &a.arch, &a.format))
        {
            return Err("Invalid or duplicate artifact metadata".into());
        }
        stable_version(&a.min_os)?;
        let valid_format = matches!(
            (a.platform.as_str(), a.format.as_str()),
            ("macos", "sparkle-zip") | ("windows", "inno-setup") | ("linux", "appimage")
        );
        if !valid_format {
            return Err("Unsupported native package format".into());
        }
    }
    if version <= stable_version(&policy.installed_version)? {
        return Ok(None);
    }
    if let Some(highest) = highest_seen {
        if version < stable_version(highest)? {
            return Err("Metadata rollback detected".into());
        }
    }
    let Some(asset) = m.assets.iter().find(|a| {
        a.platform == policy.platform && a.arch == policy.arch && a.format == policy.format
    }) else {
        return Ok(None);
    };
    if stable_version(&asset.min_os)? > stable_version(&policy.os_version)? {
        return Err("Update requires a newer operating system".into());
    }
    if asset.native_identity != policy.native_identity {
        return Err("Native signing identity mismatch".into());
    }
    Ok(Some(VerifiedUpdate {
        asset: asset.clone(),
        manifest: m,
    }))
}
/// Bounded streaming; callers never expose partial downloads to an installer.
pub fn verify_stream(mut input: impl Read, mut output: impl Write, asset: &Asset) -> Result<()> {
    let mut hash = Sha256::new();
    let mut total = 0u64;
    let mut buffer = [0; 65536];
    loop {
        let n = input.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        total += n as u64;
        if total > asset.size {
            return Err("Download exceeds signed size".into());
        }
        hash.update(&buffer[..n]);
        output.write_all(&buffer[..n])?;
    }
    if total != asset.size || format!("{:x}", hash.finalize()) != asset.sha256 {
        return Err("Artifact size or SHA-256 mismatch".into());
    }
    Ok(())
}
pub struct Client {
    http: reqwest::blocking::Client,
}
impl Client {
    pub fn new() -> Result<Self> {
        Ok(Self {
            http: reqwest::blocking::Client::builder()
                .https_only(true)
                .redirect(reqwest::redirect::Policy::custom(|attempt| {
                    let allowed = matches!(
                        attempt.url().host_str(),
                        Some(
                            "github.com"
                                | "release-assets.githubusercontent.com"
                                | "objects.githubusercontent.com"
                        )
                    );
                    if attempt.previous().len() >= 5
                        || attempt.url().scheme() != "https"
                        || !allowed
                    {
                        attempt.error("Untrusted redirect")
                    } else {
                        attempt.follow()
                    }
                }))
                .connect_timeout(Duration::from_secs(15))
                .timeout(Duration::from_secs(900))
                .user_agent("TLO-Updater/1")
                .build()?,
        })
    }
    pub fn check(
        &self,
        policy: &Policy,
        highest: Option<&str>,
        now: u64,
    ) -> Result<Option<VerifiedUpdate>> {
        let url = format!(
            "{}/latest/download/tlo-update.json",
            release_root(&policy.repository)?
        );
        self.check_url(&url, policy, highest, now)
    }
    fn check_url(
        &self,
        url: &str,
        policy: &Policy,
        highest: Option<&str>,
        now: u64,
    ) -> Result<Option<VerifiedUpdate>> {
        let mut response = self
            .http
            .get(url)
            .timeout(Duration::from_secs(30))
            .send()?
            .error_for_status()?
            .take(MAX_METADATA + 1);
        let mut bytes = Vec::new();
        response.read_to_end(&mut bytes)?;
        verify(&bytes, policy, highest, now)
    }
    pub fn download(&self, update: &VerifiedUpdate, directory: &Path) -> Result<PathBuf> {
        self.download_url(update, directory, &update.asset.url)
    }
    fn download_url(
        &self,
        update: &VerifiedUpdate,
        directory: &Path,
        url: &str,
    ) -> Result<PathBuf> {
        fs::create_dir_all(directory)?;
        let mut file = tempfile::NamedTempFile::new_in(directory)?;
        let response = self.http.get(url).send()?.error_for_status()?;
        verify_stream(response, &mut file, &update.asset)?;
        file.as_file().sync_all()?;
        // The caller owns a private directory; no clobbering of existing artifacts.
        let path = directory.join(&update.asset.filename);
        file.persist_noclobber(&path)?;
        Ok(path)
    }
}
#[derive(Default, Deserialize, Serialize)]
#[serde(default, deny_unknown_fields)]
pub struct CheckState {
    pub last_attempt: u64,
    pub last_success: u64,
    pub highest_seen: Option<String>,
    pub disabled: bool,
}
impl CheckState {
    pub fn due(&self, now: u64) -> bool {
        !self.disabled
            && (self.last_success == 0 || now.saturating_sub(self.last_success) >= CHECK_INTERVAL)
            && (self.last_attempt == 0 || now.saturating_sub(self.last_attempt) >= RETRY_INTERVAL)
    }
    pub fn save(&self, path: &Path) -> Result<()> {
        let mut file =
            tempfile::NamedTempFile::new_in(path.parent().ok_or("Missing state directory")?)?;
        file.write_all(&serde_json::to_vec(self)?)?;
        file.as_file().sync_all()?;
        file.persist(path)?;
        Ok(())
    }
}

#[cfg(test)]
mod transport_tests;
