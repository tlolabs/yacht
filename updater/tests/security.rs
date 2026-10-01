use base64::{engine::general_purpose::STANDARD as B64, Engine as _};
use ring::signature::{Ed25519KeyPair, KeyPair};
use sha2::{Digest, Sha256};
use std::io::{self, Read};
use tlo_updater::*;
fn fixture() -> (Policy, Manifest) {
    let key = Ed25519KeyPair::from_seed_unchecked(&[7; 32]).unwrap();
    let policy = Policy {
        application_id: "org.tlo.fixture".into(),
        repository: "tlolabs/fixture".into(),
        installed_version: "1.9.0".into(),
        platform: "linux".into(),
        arch: "arm64".into(),
        os_version: "24.4.0".into(),
        format: "appimage".into(),
        native_identity: "fixture-gpg".into(),
        keys: [("test".into(), B64.encode(key.public_key().as_ref()))].into(),
    };
    let artifact = b"fixture artifact";
    let m = Manifest {
        schema: 1,
        application_id: policy.application_id.clone(),
        repository: policy.repository.clone(),
        channel: "stable".into(),
        version: "1.10.0".into(),
        tag: "v1.10.0".into(),
        draft: false,
        prerelease: false,
        published_at: 100,
        expires_at: 10000,
        notes_url: "https://github.com/tlolabs/fixture/releases/tag/v1.10.0".into(),
        restart_required: true,
        migration: "none".into(),
        assets: vec![Asset {
            platform: "linux".into(),
            arch: "arm64".into(),
            min_os: "24.4.0".into(),
            format: "appimage".into(),
            filename: "Fixture.AppImage".into(),
            url: "https://github.com/tlolabs/fixture/releases/download/v1.10.0/Fixture.AppImage"
                .into(),
            size: artifact.len() as u64,
            sha256: format!("{:x}", Sha256::digest(artifact)),
            native_identity: "fixture-gpg".into(),
        }],
    };
    (policy, m)
}
fn sign(m: &Manifest) -> Vec<u8> {
    let key = Ed25519KeyPair::from_seed_unchecked(&[7; 32]).unwrap();
    let payload = serde_json::to_vec(m).unwrap();
    serde_json::to_vec(&Envelope {
        key_id: "test".into(),
        payload: B64.encode(&payload),
        signature: B64.encode(key.sign(&payload)),
    })
    .unwrap()
}
#[test]
fn valid_newer_semver_and_authenticated_download() {
    let (p, m) = fixture();
    let u = verify(&sign(&m), &p, None, 1000).unwrap().unwrap();
    let mut out = Vec::new();
    verify_stream(&b"fixture artifact"[..], &mut out, u.asset()).unwrap();
    assert_eq!(out, b"fixture artifact");
}
#[test]
fn same_and_older_are_not_updates() {
    let (mut p, m) = fixture();
    for v in ["1.10.0", "2.0.0"] {
        p.installed_version = v.into();
        assert!(verify(&sign(&m), &p, None, 1000).unwrap().is_none());
    }
}
#[test]
fn malformed_and_prerelease_versions_are_rejected() {
    let (p, mut m) = fixture();
    for v in ["wat", "1.0", "01.2.3", "2.0.0-rc.1", "2.0.0+build"] {
        m.version = v.into();
        assert!(verify(&sign(&m), &p, None, 1000).is_err());
    }
}
#[test]
fn drafts_and_prereleases_are_ignored() {
    let (p, mut m) = fixture();
    m.draft = true;
    assert!(verify(&sign(&m), &p, None, 1000).unwrap().is_none());
    m.draft = false;
    m.prerelease = true;
    assert!(verify(&sign(&m), &p, None, 1000).unwrap().is_none());
}
#[test]
fn wrong_platform_architecture_format_and_os() {
    let (mut p, m) = fixture();
    p.arch = "x64".into();
    assert!(verify(&sign(&m), &p, None, 1000).unwrap().is_none());
    p.arch = "arm64".into();
    p.platform = "windows".into();
    assert!(verify(&sign(&m), &p, None, 1000).unwrap().is_none());
    p.platform = "linux".into();
    p.os_version = "22.4.0".into();
    assert!(verify(&sign(&m), &p, None, 1000).is_err());
}
#[test]
fn wrong_application_repository_and_publisher() {
    let (p, m) = fixture();
    for field in 0..3 {
        let mut m = m.clone();
        match field {
            0 => m.application_id = "other".into(),
            1 => m.repository = "tlolabs/other".into(),
            _ => m.assets[0].native_identity = "other".into(),
        };
        assert!(verify(&sign(&m), &p, None, 1000).is_err());
    }
}
#[test]
fn invalid_signature_unknown_key_and_tampering() {
    let (mut p, m) = fixture();
    let bytes = sign(&m);
    let mut e: Envelope = serde_json::from_slice(&bytes).unwrap();
    e.signature = B64.encode([0; 64]);
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &p, None, 1000).is_err());
    e = serde_json::from_slice(&bytes).unwrap();
    e.payload = B64.encode(b"{}");
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &p, None, 1000).is_err());
    p.keys.clear();
    assert!(verify(&bytes, &p, None, 1000).is_err());
}
#[test]
fn expiry_future_dates_and_rollback() {
    let (p, m) = fixture();
    assert!(verify(&sign(&m), &p, None, 10001).is_err());
    assert!(verify(&sign(&m), &p, Some("1.11.0"), 1000).is_err());
    let mut m = m;
    m.published_at = 5000;
    assert!(verify(&sign(&m), &p, None, 1000).is_err());
}
#[test]
fn artifact_manifest_mismatch_and_duplicate_targets() {
    let (p, m) = fixture();
    for field in 0..5 {
        let mut m = m.clone();
        match field {
            0 => m.assets[0].url = m.assets[0].url.replace("v1.10.0", "v9.0.0"),
            1 => m.assets[0].filename = "../evil".into(),
            2 => m.assets[0].sha256 = "BAD".into(),
            3 => m.assets.push(m.assets[0].clone()),
            _ => m.tag = "v2.0.0".into(),
        };
        assert!(verify(&sign(&m), &p, None, 1000).is_err());
    }
}
#[test]
fn corruption_truncation_overflow_and_bad_digest() {
    let (_, m) = fixture();
    let a = &m.assets[0];
    for bytes in [
        &b"fixture artifacX"[..],
        &b"fixture"[..],
        &b"fixture artifact overflow"[..],
    ] {
        assert!(verify_stream(bytes, Vec::new(), a).is_err());
    }
    let mut a = a.clone();
    a.sha256 = "0".repeat(64);
    assert!(verify_stream(&b"fixture artifact"[..], Vec::new(), &a).is_err());
}
struct Interrupted;
impl Read for Interrupted {
    fn read(&mut self, _: &mut [u8]) -> io::Result<usize> {
        Err(io::Error::new(
            io::ErrorKind::ConnectionReset,
            "interrupted",
        ))
    }
}
#[test]
fn interrupted_download_never_succeeds() {
    let (_, m) = fixture();
    assert!(verify_stream(Interrupted, Vec::new(), &m.assets[0]).is_err());
}
#[test]
fn policy_caches_success_and_outage_and_respects_opt_out() {
    let mut s = CheckState::default();
    assert!(s.due(1000));
    s.last_attempt = 1000;
    assert!(!s.due(1100));
    assert!(s.due(5000));
    s.last_success = 5000;
    assert!(!s.due(6000));
    assert!(s.due(100000));
    s.disabled = true;
    assert!(!s.due(200000));
    assert!(!s.due(1));
}
#[test]
fn atomic_state_roundtrip() {
    let d = tempfile::tempdir().unwrap();
    let path = d.path().join("state.json");
    let s = CheckState {
        last_success: 100,
        highest_seen: Some("1.10.0".into()),
        ..Default::default()
    };
    s.save(&path).unwrap();
    let restored: CheckState = serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
    assert_eq!(restored.highest_seen, s.highest_seen);
}
#[test]
fn production_transport_refuses_http_even_on_loopback() {
    assert!(Client::new().is_ok());
    let (mut p, _) = fixture();
    p.repository = "http://localhost".into();
    assert!(Client::new().unwrap().check(&p, None, 1000).is_err());
}

#[test]
fn changed_offer_or_manual_migration_requires_new_confirmation() {
    let (p, mut m) = fixture();
    let u = verify(&sign(&m), &p, None, 1000).unwrap().unwrap();
    assert!(u.require_offer(&m.version, &m.assets[0].sha256).is_ok());
    assert!(u.require_offer("1.9.9", &m.assets[0].sha256).is_err());
    assert!(u.require_offer(&m.version, &"0".repeat(64)).is_err());
    m.migration = "manual".into();
    let u = verify(&sign(&m), &p, None, 1000).unwrap().unwrap();
    assert!(u.require_offer(&m.version, &m.assets[0].sha256).is_err());
}
#[test]
fn malformed_manifest_and_envelope_are_rejected() {
    let (p, m) = fixture();
    for b in [b"{".as_slice(), b"null", b"{}", b"[]"] {
        assert!(verify(b, &p, None, 1000).is_err());
    }
    let key = Ed25519KeyPair::from_seed_unchecked(&[7; 32]).unwrap();
    for payload in [b"{".as_slice(), b"{}", b"null"] {
        let e = Envelope {
            key_id: "test".into(),
            payload: B64.encode(payload),
            signature: B64.encode(key.sign(payload)),
        };
        assert!(verify(&serde_json::to_vec(&e).unwrap(), &p, None, 1000).is_err());
    }
    let mut value = serde_json::to_value(m).unwrap();
    value["unexpected"] = true.into();
    let payload = serde_json::to_vec(&value).unwrap();
    let e = Envelope {
        key_id: "test".into(),
        payload: B64.encode(&payload),
        signature: B64.encode(key.sign(&payload)),
    };
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &p, None, 1000).is_err());
}
