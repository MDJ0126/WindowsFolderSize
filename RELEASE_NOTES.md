# 1.0.10

- Skip inaccessible child directories and keep the sum of readable files. Mark partial totals as approximate; an inaccessible root remains unavailable.
- Localize approximation and failure text using the Windows display language (19 languages; English fallback). Preserve Windows numeric/unit formatting.
- Persist the approximation flag in cache format v2, with v1 migration. Keep partial status during incremental updates.
- Version the Explorer DLL filename so the new formatter can load after the optional final Explorer restart without overwriting the mapped DLL.
- Actual ACL regression: partial 1234 bytes, inaccessible root rejected, restored permissions exact 1334 bytes. Cache roundtrip, KO/EN words, canonicalization and COM/exported display paths passed. MSI validation and Defender scan passed.

# 1.0.8

- Start the broker after repair/reinstallation as well as first installation. Previously repair stopped the broker but excluded it from the Finish action.
- Restore parent-directory refresh notifications, coalesced to once per second, and invalidate restored cache entries on broker startup.
- Disable Restart Manager application shutdown/restart and system reboot prompts in the MSI. Remove the old product after installing shared components. Restart Explorer only from the final optional checkbox, including repair.
- Production broker updated without restarting Explorer. MSI validation and Defender scan passed. Interactive final-dialog behavior is not yet visually verified because Explorer is minimized; no window was activated.

# 1.0.7

- Index individual file sizes during the initial enumeration. Apply old/new byte deltas for file growth, addition, deletion and rename notifications.
- For a newly added or moved-in directory, enumerate only that subtree. Remove deleted subtrees directly from the index.
- Coalesce notifications, retain completed totals, persist updated totals atomically, and retain the 2% broker CPU hard cap.
- Recover with full validation if notifications overflow or file metadata cannot be read. After process restart the file index is rebuilt while the saved total remains visible; offline journal replay is not implemented.
- Actual broker regression: growth/add/delete/file rename/subtree move/rename/delete all matched expected bytes; full root enumeration counter stayed 1 -> 1.

# 1.0.6

- Persist completed folder sizes atomically in the current user's Local AppData cache file.
- Restore the previous size immediately on the first request after restart; validate in the background to catch changes while the broker was offline.
- Recalculate watched folders when they change. This version does not implement per-file delta accounting or offline USN replay; a changed or restored folder is re-enumerated.
- Raise the broker hard CPU cap to 2% and remove the additional 30 ms pause. Same 367-file sample: previous policy 469 ms, new policy 15 ms; this small cached sample is not a general speed guarantee.
- Restart/offline edit regression passed: saved 42 bytes restored immediately, then updated to 99 bytes.

# 1.0.4

- Enforce a 1% system CPU hard cap for the broker using Windows Job Objects; stop rather than silently run uncapped if enforcement fails.
- Scan at most 64 entries / 2 ms per slice, with a 30 ms pause and idle process priority.
- Keep animated progress, limit percentage-change notifications to once per second, and notify only the changed item rather than refreshing the whole parent folder.
- Refresh continuously changing folders at most once per minute.
- CPU saturation test: 1.09% measured over 5 seconds. Scanner, cache, canonicalization and display regression tests passed.

# 1.0.2

- Restore the desktop even when Restart Manager cannot restart Explorer.
- Arm an independent recovery guard before closing Explorer; verify the new desktop remains available.
- Add recovery regression tests and portable MSI checksum generation.
- Verified installed 1.0.2 restart, real Explorer folder sizes, scanner/cache/display regression tests, MSI validation, and Defender custom scan (no threats found).

# WindowsFolderSize 1.0.0

First public release for Windows 11 x64.

- Folder totals in File Explorer's existing Size column.
- Animated directory-traversal progress and cooperative scan scheduling.
- Completed sizes stay visible while background refreshes run.
- Native English installer with a new app icon, progress, inline results and optional Explorer restart.
- Automatic startup at sign-in and an uninstaller.

Download **WindowsFolderSizeSetup-1.0.0-x64.exe**. Approve extension registration, then choose **Restart Explorer** or **Later**.

The installer is unsigned. Verify the download against **SHA256SUMS.txt**. Windows 11 x64 is tested; Windows 10 and ARM64 are not validated. First symbol preparation requires access to Microsoft's symbol server. See the README for progress semantics and scan limitations.

GPL-3.0-or-later; complete project source is available at tag **v1.0.0**. Third-party notices are included in the installer and repository.
