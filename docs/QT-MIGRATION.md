# Shared Qt presentation migration

## Architecture and scope

The shared cross-platform presentation layer has been migrated from Avalonia/.NET to
Qt 6 Widgets (C++17, CMake) in `platform/qt`. The tracked active source and build
inputs contain no Avalonia or .NET UI implementation.

- **Production macOS**: Continues to use native SwiftUI/AppKit (`platform/macos`).
- **Windows (x64 / ARM64)**: Shares one Qt 6 Widgets presentation implementation (`platform/qt`).
- **Linux (x64 / ARM64)**: Shares the same Qt 6 Widgets presentation implementation (`platform/qt`).
- **Internal macOS ARM64 reference**: Qt 6 Widgets reference application for development, debugging, and parity testing (`dist/internal/YACHT Qt Internal.app`).
- **Authoritative Rust core**: Unchanged Rust core and C ABI (`crates/yacht-core`, `crates/yacht-ffi`).

There is no legacy presentation fallback. Avalonia source code, .NET SDK configurations,
NuGet package definitions, and .sln/.csproj files have been eliminated.

### Architectural separation

The Qt presentation layer is intentionally kept thin:
- **Qt / C++ owns**: Windows, controls, view state, presentation, user interaction, desktop integration (`MainWindow`, `StyleField`, `HtmlPreviewWidget`, `SettingsDialog`, `BatchDialog`, `DesktopServices`), and translation between UI events and engine APIs.
- **Rust core owns**: CSV/TSV parsing, validation, HTML generation, preset models, bounded preview logic, atomic export, batch processing, and cancellation tracking.

Communication uses the existing stable C ABI (`yacht_request` and `yacht_free`) through `YachtCore`, a thin RAII C++ wrapper. Handles (`Table`) are tracked via JSON IDs and released deterministically.

## Dependencies and distribution

The shared host uses:
- **Qt 6 Widgets**: Qt 6.4+ (Core, Gui, Widgets, Network).
- **C++17**: Standard language level configured in `platform/qt/CMakeLists.txt`.
- **CMake**: Cross-platform build system.

Qt 6 is licensed under GNU LGPLv3 and GNU GPLv3; upstream license provenance is recorded in `licenses/qt/SOURCES.md` and `licenses/qt/Qt-LGPL-3.0.txt`. Packaged distributions bundle `ThirdPartyLicenses` collected via `script/collect_qt_notices.py`.

Windows packages bundle `windeployqt`-deployed Qt libraries and the MSVC runtime
DLLs required by Qt plugins; Rust binaries link the compiler runtime statically.
The package does not require a separate VC runtime installation. Linux packages
use standard distribution dependencies (`qt6-base-dev` / `libqt6widgets6t64`).

## Storage, single instance and updates

- **Windows**: Retains `%LOCALAPPDATA%/YACHT/ui.json` and `presets.json`.
- **Linux**: Retains `$XDG_DATA_HOME/yacht/presets.json` and `$XDG_CONFIG_HOME/yacht/ui.json`. Automatically migrates legacy `$XDG_CONFIG_HOME/yacht/ui.ini` on first run while preserving the original INI.
- **Legacy migration**: Migrates `~/.yacht_presets.json` on Windows and Linux via Rust core preset operations.
- **Test isolation**: Enforced via `YACHT_TEST_DATA`. Corrupted preference files are preserved without overwrite.
- **Single-instance activation**: Implemented via `QLocalServer` and `QLocalSocket` with pipe name `yacht-ipc-<user>`. File paths dropped or passed via command line are forwarded to the primary instance with window debouncing.
- **Updater isolation**:
  - Windows checks updates via the authenticated `yacht-update` helper with Authenticode publisher and SHA256 checksum verification before launching installers.
  - Linux guides the user to distribution package updates.
  - The internal macOS Qt reference build explicitly isolates updates: bundle ID `com.tlolabs.yacht.qt.internal`, `YACHTInternalReference=true`, no Sparkle framework, and production update checks are disabled. `script/check_qt_contract.py` enforces this isolation.

## Feature parity status

The shared UI exposes the following workflows. The preview and platform behavior
still require the acceptance work described below.

| Avalonia feature | Qt 6 Widgets implementation | Status |
|---|---|---|
| 16 Style fields | `StyleField.h/.cpp` with text, integer, flag, choice and color dialogs | Complete |
| HTML table preview | `HtmlPreviewWidget.h/.cpp` uses `QTextBrowser` for a static table | Partial: browser CSS interactions such as hover are not reproduced |
| HTML source viewer | `QPlainTextEdit` read-only source tab | Complete |
| Bounded preview | Debounced (180ms) Rust preview call with 200/50/1000 row limits | Complete |
| Presets management | Save, load, select, and delete presets with overwrite confirmation | Complete |
| File open / drag & drop | Native file dialogs + `dragEnterEvent`/`dropEvent` handling | Complete |
| Built-in sample | "Load Sample" toolbar button and menu action | Complete |
| Refresh | Re-reads active file or re-renders sample | Complete |
| Copy complete HTML | Copies full un-truncated HTML document to clipboard | Complete |
| Export HTML | Native save dialog with overwrite protection | Complete |
| Batch conversion | Review dialog showing inputs, overwrite option, and error reporting | Complete |
| Responsive cancel | Core calls run on a worker while the Qt event loop services Escape and Cancel; an in-flight read test verifies cancellation and retry | Automated test passed locally; native Windows/Linux UI acceptance pending |
| Settings dialog | `SettingsDialog.h/.cpp` for remember style, preview rows, appearance, clear recent | Complete |
| Recent files | Up to 10 recent files, stored in `ui.json`, synced to menu and toolbar combo | Complete |
| Appearance themes | System, Light, and Dark via Qt color-scheme hints where available, with a Fusion fallback | Automated palette transitions passed locally; native contrast and display acceptance pending |
| Single instance IPC | `SingleInstance.h/.cpp` via `QLocalServer` / `QLocalSocket` | Complete |
| Update client | `UpdateClient.h/.cpp` with Authenticode validation on Windows | Complete |
| Headless smoke test | `--ui-smoke-test <dir>` verifies startup, conversion and shutdown workflows | Automated scope only |

## Accessibility

The Qt presentation targets WCAG 2.2 AA accessibility:
- **Accessible names and descriptions**: Every control, field, button, and tab sets `setAccessibleName` and `setAccessibleDescription`.
- **Keyboard navigation**: Full Tab / Shift+Tab keyboard order across controls.
- **Keyboard shortcuts**: Standard desktop shortcuts (Ctrl+O, Ctrl+S, Ctrl+Shift+C, Ctrl+Shift+B, Ctrl+R, F5, Ctrl+1, Ctrl+2, Escape).
- **Status announcements**: Accessible status bar label for screen readers.
- **High-DPI / display scaling**: Managed natively by Qt 6 display scaling engine.

## Validation and test suite

- **Unit and view model tests**: `platform/qt/tests/test_integration.cpp` validates the C ABI bindings, data conversion, error recovery, preset lifecycle, batch execution, and view model presentation logic.
- **UI smoke tests**: `script/test_qt_ui.py` runs the real application executable in smoke test mode against isolated test data.
- **Contract enforcement**: `script/check_qt_contract.py` validates repository paths, bundle metadata, and CI release isolation.
