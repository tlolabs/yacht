# Developer guide

Start with [architecture](ARCHITECTURE.md), [behavior](BEHAVIOR.md),
[parity](FEATURE-PARITY.md) and [build instructions](BUILDING.md).

## Commit signing and history

Use unsigned commits. Disable automatic cryptographic commit signing in each
clone (repository-local Git configuration is not copied by cloning):

```sh
git config --local commit.gpgsign false
```

Continue adding the DCO `Signed-off-by` trailer with `git commit -s` as described
in [CONTRIBUTING.md](../CONTRIBUTING.md). Lowercase `-s` adds that trailer;
uppercase `-S` requests a cryptographic signature and should not be used for commits.
Release-tag signing and platform artifact signing remain governed by
[CODE_SIGNING_POLICY.md](../CODE_SIGNING_POLICY.md).

The September 30, 2026 history migration removed commit signatures while
preserving every commit message, author, committer, timestamp, file tree, and merge
relationship. Because the initial commit was signed, all descendant commit IDs
changed. Existing annotated tags retain their annotations and now target the
corresponding rewritten commits.

The migration clone keeps the original signed history in
`.git/unsigned-history-backups/`, alongside the original refs, an old-to-new object
map, and copies of the pre-migration index, configuration, and staged/unstaged
patches. These local recovery files are not included in a clone or push.
The rewrite does not itself update GitHub. Publishing rewritten branches and tags
requires a coordinated force push; after publication, use a fresh clone or migrate
local work onto the rewritten history instead of merging the old history back in.

## Bindings

C ABI entrypoints: `char *yacht_request(const char *, cancel_fn, void *)` and
`void yacht_free(char *)`. See bindings/c/yacht.h for the full lifetime contract.
All operations include version=1 and op. Native errors expose a stable code
(cancelled, invalid_input, io_error, internal_error) and user-readable message.
`ok:null` is a successful void operation, not a missing response.

| Operation | Additional request fields | Result |
|---|---|---|
| info | — | app version and protocol |
| defaults / unstyled | — | full portable style object |
| normalize_style / validate_style / is_unstyled | style | normalized style / null / bool |
| settings | settings (optional) | validated settings and effective initial_style |
| safe_color / escape / numeric | text | bool / string / bool |
| sample | — | retained table metadata |
| parse | bytes (u8 array), delimiter | retained table metadata |
| read | path, delimiter, chunk_size (optional) | retained table metadata |
| records | records (array of arrays of strings) | retained table metadata |
| rows | handle | full rows, compatibility/test use only |
| release | handle | null; one release per acquired handle |
| preview | handle, style, limit | HTML/source and visible bounds metadata |
| html | handle, style, limit (optional) | complete HTML unless explicitly limited |
| export | handle, style, path, overwrite | null after atomic publication |
| convert | input, output, style, delimiter, overwrite | null |
| batch | inputs, style, delimiter, infer_tsv, overwrite | per-input outcomes |
| default_output | path | output path |
| load_presets / save_presets | path; presets for save | map / null |

Required style fields/defaults are defined in Rust; frontend properties are editable
projections, never independent serialization defaults. Never persist a table handle.
Swift uses an immutable Sendable storage owner; C# keeps a managed owner alive across
P/Invoke; Python holds its Table during ctypes calls. Rust clones the Arc before
releasing the registry mutex, so independent UI work can run concurrently.

Cancellation callbacks must be cheap, nonthrowing, and valid on the caller thread.
Do not call UI APIs from callbacks. To cancel, set the native task/token/event;
Rust checks it while reading/generating and before publishing staged files.

## Native frontends and tests

macOS retains its view/store structure and accessibility identifiers. Expensive
Rust calls go through Background.run, which propagates Swift cancellation. Keep
Finder multi-open handling in NSApplicationDelegate. SwiftPM tests now verify the
real Rust ABI, not an independent Swift algorithm. New app/UI files require project
regeneration; the bindings library files are discovered by SwiftPM.

Windows, Linux and the internal Mac reference use the shared implementation in
`platform/avalonia`. Keep presentation state and commands in `MainViewModel`,
platform operations behind `IDesktopServices`, and business behavior in Rust.
P/Invoke loads the library from the application directory. C# integration tests
exercise the same binding and view model without a graphical session; the common
smoke harness runs the actual window/native preview. See [migration details](AVALONIA-MIGRATION.md).

## Feature changes

Platform-independent functionality belongs in Rust by default.
User-facing changes must be evaluated for implementation across all supported native UIs.

Add core behavior/tests first, extend the versioned binding protocol, update all
applicable native controls/workflows and parity documentation, then run the relevant
native suites. A native platform failure blocks release. Keep observable output
changes explicit in COMPATIBILITY.md and retain the Python fixture provenance.

Use native labels, focus order, keyboard accelerators, screen-reader semantics and
scaling; do not use screenshots/pixel equality as a cross-platform contract.
Export colors are content and must not follow UI dark mode. Appearance/layout are
native settings, while remember-style/preview behavior defaults belong in Rust.

## Dependency updates

Cargo.lock and Windows packages.lock.json pin resolution. Dependabot opens updates;
review compiler/OS/runtime minimum changes and rebuild all architectures. Linux
libraries use supported distribution packages so system security updates apply.
Avoid adding dependencies for trivial helpers. No external paid service is required.

## Verification discipline

`cargo bench` reports meaningful parsing, preview and streaming-generation samples;
compare release builds on the same host/data. Do not equate a sink benchmark with
disk export latency. Test huge cells/columns as well as many rows. Preserve atomic
publication and cancellation under errors; never truncate outputs as preparation.

Native runtime smoke tests complement core tests, not replace human accessibility
and platform acceptance. Only mark parity verified after actual execution on the
stated host/architecture. Version/packaging/signing details live in BUILDING.md and RELEASING.md.
