# Installer security remediation — 1.0.1 candidate

## Confirmed incident

Microsoft Defender detected the public 1.0.0 self-extracting setup executable as `Trojan:Win32/Bearfoos.B!ml` on 2026-10-24 (KST). That release is held as a draft. A false-positive determination has **not** been obtained; do not allowlist the old executable or disable protection.

The original installer SHA-256 was `e8cf362d3bfdb36e8e0b26b4618f238225ddb9f2a84fe7d433506ae90e6b0eab`.

## Design changes

- Replace the custom self-extracting EXE with a standard WiX 6.0.2 / Windows Installer MSI.
- Windows Installer owns file installation, product registration, rollback and removal. The package does not copy an uninstaller into Temp, create a RunOnce cleanup command, or force-terminate the broker.
- Install code to administrator-protected Program Files; keep mutable symbols/logs in the invoking user's Local AppData. Machine-wide COM registration no longer points at a user-writable DLL.
- Use Windows Restart Manager, without `RmForceShutdown`, for the explicit optional Explorer restart. No `TerminateProcess` import is needed by the new maintenance utility.
- Register a non-visible broker lifecycle window so Restart Manager can request a normal shutdown.
- Do not change Defender settings, exclusions, signatures, trust stores, or detection decisions. No packing, obfuscation, or certificate trust bypass was added.

## Validation performed

| Check | Result |
| --- | --- |
| Defender custom scan of original broker/extension files | No threats found |
| Defender custom scan of the 1.0.1 MSI | No threats found; exit 0 |
| Windows Installer administrative extraction of production MSI | Exit 0 |
| Standard WiX ICE validation of production MSI | Exit 0 |
| Actual install of a separate per-user regression MSI with the same payload binaries | Exit 0 |
| Installed broker versus production payload hash | Equal |
| Resumable scan, cancellation, exact fixture bytes/files/directories | Passed |
| Native shell property canonicalization and COM/exported Size formatters | Passed |
| Monotonic progress and dot animation frames | Passed |
| Repeated file changes, retained completed size, final updates and deletion | Passed |
| Actual regression MSI uninstall while its broker was running | MSI log: removal completed successfully, engine return 0 |
| Regression folder, startup value, product registration after uninstall | Absent; MSI product state -1 |

The disposable per-user regression package changes installation scope/registry root; it is not the production per-machine package. Production ICE validation was run without suppressing ICEs. Neither package required an antivirus exception. Defender signatures during these checks: **1.461.161.0**; real-time protection remained enabled.

## Remaining limits

- The production per-machine install/elevation path has not been executed in this session.
- Computer Use was used to inspect File Explorer, but an unrelated Windows cryptographic-key permission prompt blocked further UI input. The prompt was not approved or bypassed. Actual on-screen folder-size acceptance and the optional Explorer-restart UI remain unverified for this candidate.
- Microsoft submission was attempted through the official portal; both available customer/developer paths required account authentication. No submission was completed. The login page was left for a later manual handoff; no authentication dialog was automated.
- These local test results are not a Microsoft false-positive decision and do not guarantee the absence of future detections on other devices. The candidate remains unpublished pending remaining acceptance checks.
