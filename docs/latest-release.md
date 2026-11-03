WindowsFolderSize 1.0.16 reduces per-file index memory and calculation CPU load.

- Store file records in the Windows SQLite disk B-tree, with a 2 MiB page-cache target, mmap disabled and disk-backed temporary storage.
- Limit active scans to two, bound pending changes, yield between scan slices, and lower the scheduler cap from 2% to 1% of system CPU.
- Preserve arithmetic updates for file edits, subtree deletion and renames.
- Isolated 150,000-record test: peak working set 116,002,816 -> 9,302,016 bytes (about 92% lower). New-case average system CPU: 0.729%. These fixture measurements are not a total-process memory ceiling or live Explorer performance guarantees.
- Persistence, rollback, rename, subtree deletion, actual file creation/resize/deletion and the production scanner worker passed native tests. MSI validation passed.

The new database requires one initial validation scan. Existing binary indexes are preserved. Installation and live Explorer UI QA of this version remain pending because the user requested no screen interaction. The earlier automatic Size-column display issue has not been declared resolved.

Install WindowsFolderSize-1.0.16-x64.msi. The broker executable alone is not the installer.

SHA-256: dfa898faac89b84c6aefe43cbe2ad5d9268b31cb0c6a1ad2e55b2fb8869642ed
