$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force "$root\build\scan-fixture\empty","$root\build\scan-fixture\nested" | Out-Null
[IO.File]::WriteAllBytes("$root\build\scan-fixture\one.bin",[byte[]]::new(123))
[IO.File]::WriteAllBytes("$root\build\scan-fixture\nested\two.bin",[byte[]]::new(4096))
[IO.File]::WriteAllBytes("$root\build\scan-fixture\.hidden",[byte[]]::new(17))
$info=[Diagnostics.ProcessStartInfo]::new();$info.FileName="$root\build\bin\WindowsFolderSize.exe";$info.Arguments="--resolve";$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$p=[Diagnostics.Process]::Start($info);if(-not $p.WaitForExit(180000)){throw "Symbol preparation timed out"};if($p.ExitCode -ne 0){throw "Symbol preparation failed (exit $($p.ExitCode))"}
