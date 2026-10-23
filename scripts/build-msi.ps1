param([string]$Wix='wix')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try{
 & $Wix build installer\Package.wxs -arch x64 -ext WixToolset.UI.wixext/6.0.2 -o dist\WindowsFolderSize-1.0.1-x64.msi
 if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
 $hash=(Get-FileHash dist\WindowsFolderSize-1.0.1-x64.msi -Algorithm SHA256).Hash.ToLower()
 [IO.File]::WriteAllText((Join-Path $root 'dist\SHA256SUMS-1.0.1.txt'),($hash+'  WindowsFolderSize-1.0.1-x64.msi'+"`n"),[Text.UTF8Encoding]::new($false))
}finally{Pop-Location}
