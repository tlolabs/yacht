# Distribution

Y.A.C.H.T. uses GitHub Actions to build platform-specific release artifacts.

## Build Targets

| GitHub runner | Artifact label | Output |
| --- | --- | --- |
| `macos-15-intel` | `macos-intel` | `.dmg` containing `Y.A.C.H.T..app` |
| `macos-14` | `macos-arm64` | `.dmg` containing `Y.A.C.H.T..app` |
| `windows-latest` | `windows` | `.zip` containing `Y.A.C.H.T.exe` |
| `ubuntu-latest` | `linux` | `.tar.gz` containing `Y.A.C.H.T` |

Note: `macos-13` was previously the standard Intel macOS runner, but GitHub has retired that image. The workflow uses `macos-15-intel` for Intel builds while preserving the requested `macos-intel` artifact name.

## Manual Build

Run the `Build Distributions` workflow from the GitHub Actions tab. Manual runs upload build artifacts but do not create a GitHub release unless a release tag is supplied.

## Release Build

1. Update the version in `setup.py`.
2. Commit and push the version change.
3. Create and push a version tag:

```bash
git tag v1.1.0
git push origin v1.1.0
```

The workflow builds every platform artifact and publishes them to a GitHub release for the tag.

## Local Packaging Notes

macOS builds use `py2app` so the app keeps native `.app` drag-and-drop behavior. Windows and Linux builds use PyInstaller because `py2app` is macOS-only.
