# Third-party components and references

- Explorer hook ABI and cached property extraction behavior: m417z, Better file
  sizes in Explorer details 1.6, GPL-3.0. Source:
  https://github.com/ramensoftware/windhawk-mods/blob/main/mods/explorer-details-better-file-sizes.wh.cpp
  This project is GPL-3.0-or-later and includes complete source.
- MinHook 1.3.4, Tsuda Kageyu and contributors, BSD-2-Clause (including HDE notices).
  Exact revision c3fcafdc10146beb5919319d0683e44e3c30d537.
  Its license is included as MinHook-LICENSE.txt in the installed application.
- Microsoft Debugging Tools (SDK debugger redistributables, not System32 copies): dbghelp.dll and symsrv.dll are included alongside the
  symbol helper. They run only when preparing symbols for a Windows version,
  not as part of the resident application. Microsoft public symbols are fetched
  from https://msdl.microsoft.com/download/symbols and cached locally.

WindowsFolderSize does not include, launch, or require Windhawk or Everything.
The broker does not use remote process injection. Explorer loads its COM shell
extension through ShellIconOverlayIdentifiers. IsMemberOf always returns S_FALSE,
so the extension never draws an overlay. It uses one of Windows' limited overlay
handler slots. Extension registration is machine-wide and points to the current user installation.
