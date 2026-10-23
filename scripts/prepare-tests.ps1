$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force "$root\build\scan-fixture\empty","$root\build\scan-fixture\nested" | Out-Null
[IO.File]::WriteAllBytes("$root\build\scan-fixture\one.bin",[byte[]]::new(123))
[IO.File]::WriteAllBytes("$root\build\scan-fixture\nested\two.bin",[byte[]]::new(4096))
[IO.File]::WriteAllBytes("$root\build\scan-fixture\.hidden",[byte[]]::new(17))
& "$root\dist\Symbols\WindowsFolderSizeSymbols.exe"
if($LASTEXITCODE -ne 0){throw 'Symbol preparation failed'}
