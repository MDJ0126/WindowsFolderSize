param([string]$Wix='wix')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try{
 New-Item -ItemType Directory -Path (Join-Path $root 'dist') -Force | Out-Null
 & $Wix build installer\Package.wxs -arch x64 -ext WixToolset.UI.wixext/6.0.2 -pdb build\WindowsFolderSize-1.0.4-x64.wixpdb -o dist\WindowsFolderSize-1.0.4-x64.msi
 if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
 $stream=[IO.File]::OpenRead((Join-Path $root 'dist\WindowsFolderSize-1.0.4-x64.msi'));$sha=[Security.Cryptography.SHA256]::Create();try{
 New-Item -ItemType Directory -Path (Join-Path $root 'dist') -Force | Out-Null$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLower()}finally{$stream.Dispose();$sha.Dispose()}
 [IO.File]::WriteAllText((Join-Path $root 'dist\SHA256SUMS-1.0.4.txt'),($hash+'  WindowsFolderSize-1.0.4-x64.msi'+"`n"),[Text.UTF8Encoding]::new($false))
}finally{Pop-Location}
