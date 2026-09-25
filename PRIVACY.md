# Privacy

YACHT converts files on the user's computer. The application does not collect, sell, share, or send CSV/TSV contents, generated HTML, preferences, analytics, or telemetry to TLO Labs. Conversion and preview work offline. No account is required.

The three previews use a content security policy that denies remote resources and scripts. macOS uses a nonpersistent WebKit data store; Linux uses an ephemeral WebKit session; Windows blocks WebView2 resource requests. The application itself does not fetch remote fonts, assets, updates, or runtime dependencies. On Windows, WebView2 and Windows system components may have their own Microsoft update and diagnostic behavior outside YACHT's control; review their separate terms. Installing missing system dependencies may use the operating system's package manager and network.

The **User Guide** action opens the GitHub README in the default browser at the user's request. Updates are manual downloads from [GitHub Releases](https://github.com/tlolabs/yacht/releases). YACHT currently performs no automatic update check, download, or installation. A future optional check would use GitHub and be documented here before release.

YACHT stores presets, recent file paths, and UI settings locally. On macOS, presets are in `~/Library/Application Support/Y.A.C.H.T./presets.json` and preferences are in the app's UserDefaults domain. On Windows, `ui.json` and `presets.json` are under `%LOCALAPPDATA%\YACHT`. On Linux, settings are in `$XDG_CONFIG_HOME/yacht/ui.ini` (or the GLib default) and presets in `$XDG_DATA_HOME/yacht/presets.json`. Recent paths may reveal filenames to another person with access to the same user account; the app provides **Clear Recent Files**. Files remain until the user clears or removes them. Existing `~/.yacht_presets.json` data may be copied once into the native preset store; the original is left intact.

macOS records local diagnostic events through the unified logging system, including file-open event counts but not file contents or full paths in the current logger calls. Retention is controlled by macOS. Windows and Linux currently show errors in the application and do not create a dedicated YACHT log file. Operating systems and their web rendering components may keep their own local diagnostics. YACHT does not upload local logs.

Public privacy contact: **TBD**. Until it exists, use [GitHub Issues](https://github.com/tlolabs/yacht/issues) for non-sensitive questions; report sensitive security issues through GitHub private vulnerability reporting as described in [SECURITY.md](SECURITY.md).
