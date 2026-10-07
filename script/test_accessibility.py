#!/usr/bin/env python3
"""Automated Accessibility and Platform Support Audit Verification.

Verifies:
1. macOS SwiftUI accessibility labels, values, hints, tooltips, and dynamic theme switching.
2. Qt 6 accessible names, accessible descriptions, tooltips, buddy relationships, and dark theme contrast.
3. Linux desktop entry metadata and accessibility infrastructure readiness.
4. Windows high-DPI scaling manifest configuration.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def check(condition: bool, message: str) -> None:
    if not condition:
        print(f"FAIL: {message}", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"PASS: {message}")


def audit_macos_swiftui():
    print("\n--- Auditing macOS SwiftUI Accessibility & HIG Integration ---")
    sources = [
        ROOT / "platform/macos/Sources/YachtApp/Views/StyleInspector.swift",
        ROOT / "platform/macos/Sources/YachtApp/Views/ContentView.swift",
        ROOT / "platform/macos/Sources/YachtApp/Views/ColorSetting.swift",
        ROOT / "platform/macos/Sources/YachtApp/Views/SettingsView.swift",
        ROOT / "platform/macos/Sources/YachtApp/Views/BatchView.swift",
    ]
    for src in sources:
        check(src.is_file(), f"Swift view file exists: {src.name}")
        content = src.read_text(encoding="utf-8")
        check(
            ".accessibilityLabel(" in content,
            f"{src.name} provides accessibility labels for VoiceOver",
        )
        check(
            ".help(" in content,
            f"{src.name} provides tooltips/help text for user discoverability",
        )

    # Check preferences dynamic appearance support
    prefs_swift = ROOT / "platform/macos/Sources/YachtApp/Stores/Preferences.swift"
    prefs_text = prefs_swift.read_text(encoding="utf-8")
    check(
        "NSApp" in prefs_text and ".darkAqua" in prefs_text,
        "Preferences.swift dynamically configures NSApp appearance (.darkAqua / .aqua)",
    )

    # Check StyleInspector accessibility values
    inspector_text = (
        ROOT / "platform/macos/Sources/YachtApp/Views/StyleInspector.swift"
    ).read_text(encoding="utf-8")
    check(
        ".accessibilityValue(" in inspector_text,
        "StyleInspector provides dynamic accessibility values for numeric inputs",
    )


def audit_qt():
    print("\n--- Auditing Qt Accessibility & Platform Integration ---")
    qt_sources = [
        ROOT / "platform/qt/src/MainWindow.cpp",
        ROOT / "platform/qt/src/StyleField.cpp",
        ROOT / "platform/qt/src/SettingsDialog.cpp",
        ROOT / "platform/qt/src/BatchDialog.cpp",
    ]
    for src in qt_sources:
        check(src.is_file(), f"Qt source exists: {src.name}")
        content = src.read_text(encoding="utf-8")
        check(
            "setAccessibleName(" in content,
            f"{src.name} exposes accessible names for screen readers",
        )
        check(
            "setToolTip(" in content,
            f"{src.name} provides descriptive tooltips for discoverability",
        )

    # Check buddy label associations in forms
    for src in [
        ROOT / "platform/qt/src/MainWindow.cpp",
        ROOT / "platform/qt/src/StyleField.cpp",
        ROOT / "platform/qt/src/SettingsDialog.cpp",
    ]:
        content = src.read_text(encoding="utf-8")
        check(
            "setBuddy(" in content,
            f"{src.name} wires buddy labels for accessible relationships",
        )

    # Check dark theme tooltip contrast bug is absent
    desktop_services = (
        ROOT / "platform/qt/src/DesktopServices.cpp"
    ).read_text(encoding="utf-8")
    main_window = (ROOT / "platform/qt/src/MainWindow.cpp").read_text(
        encoding="utf-8"
    )
    check(
        "m_desktop->applyAppearance(appearance)" in main_window,
        "MainWindow delegates appearance changes to the shared desktop service",
    )
    check(
        "setColorScheme(" in desktop_services,
        "Qt 6.8+ follows the OS color-scheme API",
    )
    check(
        "ToolTipBase, Qt::white" not in desktop_services
        and "ToolTipText, Qt::white" not in desktop_services,
        "Dark fallback avoids white-on-white tooltips",
    )


def audit_linux_desktop():
    print("\n--- Auditing Linux Desktop Integration ---")
    desktop_file = (
        ROOT
        / "platform/linux/data/com.local.yacht.csvhtmltranslator.desktop"
    )
    check(desktop_file.is_file(), "Linux desktop file exists")
    content = desktop_file.read_text(encoding="utf-8")
    check("Name=YACHT" in content, "Desktop entry has Name")
    check("Comment=" in content, "Desktop entry has Comment description")
    check("Categories=" in content, "Desktop entry has Categories")
    check("Exec=" in content, "Desktop entry has Exec line")
    check("Icon=" in content, "Desktop entry has Icon reference")


def audit_windows_hidpi():
    print("\n--- Auditing Windows HiDPI & Accessibility Manifest ---")
    manifest_file = ROOT / "platform/windows/app.manifest"
    check(manifest_file.is_file(), "Windows app.manifest exists")
    content = manifest_file.read_text(encoding="utf-8")
    check(
        "dpiAware" in content or "dpiAwareness" in content,
        "Windows manifest specifies HiDPI awareness for crisp display scaling",
    )


def main():
    print("Running Automated Accessibility & Platform Support Verification...")
    audit_macos_swiftui()
    audit_qt()
    audit_linux_desktop()
    audit_windows_hidpi()
    print("\nStructural accessibility checks passed. Manual assistive-technology testing remains separate.")


if __name__ == "__main__":
    main()
