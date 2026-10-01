use super::*;
use ring::signature::{Ed25519KeyPair, KeyPair};
use std::net::TcpListener;
fn server(
    body: Vec<u8>,
    status: &str,
    claimed: Option<usize>,
) -> (String, std::thread::JoinHandle<()>) {
    let socket = TcpListener::bind("127.0.0.1:0").unwrap();
    let url = format!("http://{}", socket.local_addr().unwrap());
    let status = status.to_owned();
    let thread = std::thread::spawn(move || {
        let (mut connection, _) = socket.accept().unwrap();
        let mut request = [0; 8192];
        let _ = connection.read(&mut request);
        write!(
            connection,
            "HTTP/1.1 {status}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
            claimed.unwrap_or(body.len())
        )
        .unwrap();
        connection.write_all(&body).unwrap();
    });
    (url, thread)
}
fn fixture() -> (Policy, Vec<u8>) {
    let k = Ed25519KeyPair::from_seed_unchecked(&[9; 32]).unwrap();
    let p = Policy {
        application_id: "test".into(),
        repository: "tlolabs/test".into(),
        installed_version: "1.0.0".into(),
        platform: "linux".into(),
        arch: "x64".into(),
        os_version: "24.4.0".into(),
        format: "appimage".into(),
        native_identity: "test-gpg".into(),
        keys: [("test".into(), B64.encode(k.public_key().as_ref()))].into(),
    };
    let m = Manifest {
        schema: 1,
        application_id: "test".into(),
        repository: p.repository.clone(),
        channel: "stable".into(),
        version: "2.0.0".into(),
        tag: "v2.0.0".into(),
        draft: false,
        prerelease: false,
        published_at: 1,
        expires_at: 10000,
        notes_url: "https://github.com/tlolabs/test/releases/tag/v2.0.0".into(),
        restart_required: true,
        migration: "none".into(),
        assets: vec![Asset {
            platform: "linux".into(),
            arch: "x64".into(),
            min_os: "24.4.0".into(),
            format: "appimage".into(),
            filename: "Test.AppImage".into(),
            url: "https://github.com/tlolabs/test/releases/download/v2.0.0/Test.AppImage".into(),
            size: 8,
            sha256: format!("{:x}", Sha256::digest(b"artifact")),
            native_identity: "test-gpg".into(),
        }],
    };
    let payload = serde_json::to_vec(&m).unwrap();
    let envelope = Envelope {
        key_id: "test".into(),
        payload: B64.encode(&payload),
        signature: B64.encode(k.sign(&payload)),
    };
    (p, serde_json::to_vec(&envelope).unwrap())
}
fn local_client() -> Client {
    Client {
        http: reqwest::blocking::Client::builder()
            .no_proxy()
            .timeout(Duration::from_secs(3))
            .build()
            .unwrap(),
    }
}
#[test]
fn mock_release_discovery_authentication_and_download() {
    let (p, bytes) = fixture();
    let (url, server) = server(bytes, "200 OK", None);
    let c = local_client();
    let u = c.check_url(&url, &p, None, 1000).unwrap().unwrap();
    server.join().unwrap();
    let (url, server) = self::server(b"artifact".to_vec(), "200 OK", None);
    let dir = tempfile::tempdir().unwrap();
    let path = c.download_url(&u, dir.path(), &url).unwrap();
    assert_eq!(fs::read(path).unwrap(), b"artifact");
    server.join().unwrap();
}
#[test]
fn unavailable_service_preserves_application_state() {
    let (p, _) = fixture();
    let (url, t) = server(b"outage".to_vec(), "503 Service Unavailable", None);
    assert!(local_client().check_url(&url, &p, None, 1000).is_err());
    t.join().unwrap();
}
#[test]
fn interrupted_http_download_cleans_partial_file() {
    let (p, b) = fixture();
    let u = verify(&b, &p, None, 1000).unwrap().unwrap();
    let (url, t) = server(b"art".to_vec(), "200 OK", Some(8));
    let dir = tempfile::tempdir().unwrap();
    assert!(local_client().download_url(&u, dir.path(), &url).is_err());
    assert_eq!(fs::read_dir(dir.path()).unwrap().count(), 0);
    t.join().unwrap();
}
#[test]
fn corrupted_http_download_cannot_replace_existing_file() {
    let (p, b) = fixture();
    let u = verify(&b, &p, None, 1000).unwrap().unwrap();
    let (url, t) = server(b"tampered".to_vec(), "200 OK", None);
    let dir = tempfile::tempdir().unwrap();
    let target = dir.path().join("Test.AppImage");
    fs::write(&target, b"previous").unwrap();
    assert!(local_client().download_url(&u, dir.path(), &url).is_err());
    assert_eq!(fs::read(target).unwrap(), b"previous");
    t.join().unwrap();
}

#[test]
fn interrupted_download_recovers_on_retry() {
    let (p, b) = fixture();
    let u = verify(&b, &p, None, 1000).unwrap().unwrap();
    let dir = tempfile::tempdir().unwrap();
    let (url, t) = server(b"art".to_vec(), "200 OK", Some(8));
    assert!(local_client().download_url(&u, dir.path(), &url).is_err());
    t.join().unwrap();
    let (url, t) = server(b"artifact".to_vec(), "200 OK", None);
    let path = local_client().download_url(&u, dir.path(), &url).unwrap();
    t.join().unwrap();
    assert_eq!(fs::read(path).unwrap(), b"artifact");
    assert_eq!(fs::read_dir(dir.path()).unwrap().count(), 1);
}
#[test]
fn unwritable_destination_preserves_existing_installation() {
    let (p, b) = fixture();
    let u = verify(&b, &p, None, 1000).unwrap().unwrap();
    let dir = tempfile::tempdir().unwrap();
    let installed = dir.path().join("Test.AppImage");
    fs::write(&installed, b"installed").unwrap();
    // A file in place of the staging directory fails before any network access.
    assert!(local_client()
        .download_url(&u, &installed, "http://127.0.0.1:1")
        .is_err());
    assert_eq!(fs::read(installed).unwrap(), b"installed");
}
#[test]
fn valid_download_cannot_overwrite_existing_artifact() {
    let (p, b) = fixture();
    let u = verify(&b, &p, None, 1000).unwrap().unwrap();
    let dir = tempfile::tempdir().unwrap();
    let installed = dir.path().join("Test.AppImage");
    fs::write(&installed, b"installed").unwrap();
    let (url, t) = server(b"artifact".to_vec(), "200 OK", None);
    assert!(local_client().download_url(&u, dir.path(), &url).is_err());
    t.join().unwrap();
    assert_eq!(fs::read(installed).unwrap(), b"installed");
    assert_eq!(fs::read_dir(dir.path()).unwrap().count(), 1);
}
#[test]
fn connection_failure_is_recoverable() {
    let (p, b) = fixture();
    let socket = TcpListener::bind("127.0.0.1:0").unwrap();
    let url = format!("http://{}", socket.local_addr().unwrap());
    drop(socket);
    let c = local_client();
    assert!(c.check_url(&url, &p, None, 1000).is_err());
    let (url, t) = server(b, "200 OK", None);
    assert!(c.check_url(&url, &p, None, 1000).unwrap().is_some());
    t.join().unwrap();
}
