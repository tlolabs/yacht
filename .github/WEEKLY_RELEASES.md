# Weekly direct downloads

The Sunday 16:17 UTC schedule processes only unattempted, publication-ready version tags. No push,
PR, tag-event, manual dispatch or automatic retry trigger is added. Existing
baseline and durable claims remain unchanged. Moved baseline tags fail closed. AVID Core has its own Saturday
workflow; this repository does not schedule it.

Each Linux/Windows x64/ARM64 job packages its exact immutable tagged source,
checks the final package and launches the extracted/installed app briefly.
Windows ZIP and installer, Linux tar and Debian package are checked independently
within their target job. This is a basic smoke check, not full regression,
accessibility or minimum-OS qualification. Linux Debian packages currently
require glibc 2.39; no broader distribution compatibility is implied.

Only a successful target job with its run-specific artifact can publish. Missing, expired or duplicate artifacts and failed native jobs cause explicit publisher failure, never a successful publication status. A failed
target does not suppress another successful target. The publisher loads control
code from the workflow commit, verifies source/run/target/version/checksums and
the remote tag, and requires an existing public release with the matching
channel. Before any new claim, selection requires an existing nondraft release with the exact
tag and stable/prerelease channel, plus an uploaded, nonempty matching-release Mac
ZIP with GitHub SHA256 digest evidence. Accepted names include the existing
canonical `YACHT-macos-arm64.zip` / `YACHT-macos-x64.zip`, bound to that
exact release tag/channel, or the versioned alternatives
`yacht-VERSION-macos-arm64.zip` / `yacht-VERSION-macos-x64.zip` (also `YACHT`);
VERSION is the exact source version; beta/RC releases may also include their
full tag suffix, matching the publisher’s source-version/tag convention. Development and explicitly
unnotarized/unsigned filenames are not qualification signals. The owner's
published Mac archive signals local qualification; CI does not independently
attest Apple signing or notarization. Missing prerequisites are reported as
skipped/ineligible, without consuming attempts, so a later weekly schedule can
select them once ready. Any existing native target archive, installer, Debian
package or target checksum skips that target before a claim, including incomplete
uploads; other targets remain independent. Immediately before the first claim,
selection refreshes every selected tag/SHA and release/asset/hold prerequisite.
Any change fails the whole preflight without writing claims. API failures are
errors, never empty release lists.

The trusted `.github/weekly-release-holds.json` blocks known held tags regardless
of available assets. It currently holds `v2.1.3` for the unresolved macOS UI
regression. Its hold must only be removed explicitly after resolution, not by
uploading a Mac artifact. An existing release can additionally carry the exact
body marker `<!-- weekly-publication: hold -->`. The publisher rechecks the
same release/asset/hold prerequisites and exact remote tag immediately before each asset upload. If readiness
changes after a genuine attempt, publication fails and that attempt stays
consumed; it does not silently retry. No release or tag is created,
no existing bytes are overwritten, and GitHub Latest is never changed.

Publication uploads direct-download archives and checksums only. Existing
Windows signing remains optional for these direct downloads; no certificate is
enrolled by this workflow and unsigned downloads are not signed update packages.
Authenticated update feeds, keys and client verification remain untouched.
No macOS artifacts are built or changed here.

The workflow retains the smoke gate's attempt records even if publication fails.
It does not silently retry an attempted tag. Failure is visible in its target's
publisher job and the durable ledger. Finalization waits for publication and records success only when both that target's smoke and publisher jobs succeeded; missing, skipped, failed, or ambiguous publication cannot become success. Correct the cause and obtain an explicitly authorized recovery
rather than clearing attempt records or moving a tag.

Local verification: `actionlint .github/workflows/weekly-release-smoke.yml`,
`python3 -m unittest discover -s .github/scripts -p 'test_weekly*.py'`, and
`python3 -m unittest discover -s script -p test_publish_release.py`.
Native Windows/Linux package execution remains to be verified by the scheduled
runners; local static checks do not assert those platforms passed.
