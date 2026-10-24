param([string]$Wix='wix')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
 New-Item -ItemType Directory -Path (Join-Path $root 'dist') -Force | Out-Null
 & $Wix build installer\Package.wxs -arch x64 -ext WixToolset.UI.wixext/6.0.2 -pdb build\WindowsFolderSize-1.0.11-x64.wixpdb -o dist\WindowsFolderSize-1.0.11-x64.msi
 if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
 $patchFile=Join-Path ([IO.Path]::GetTempPath()) ('WindowsFolderSize-finish-'+[Guid]::NewGuid().ToString('N')+'.msi')
 Copy-Item -LiteralPath (Join-Path $root 'dist\WindowsFolderSize-1.0.11-x64.msi') -Destination $patchFile
 & (Join-Path $root 'build\msi_finish_patch.exe') $patchFile
 if($LASTEXITCODE -ne 0){throw 'MSI Finish dialog patch failed'}
 Copy-Item -LiteralPath $patchFile -Destination (Join-Path $root 'dist\WindowsFolderSize-1.0.11-x64.msi') -Force
 Remove-Item -LiteralPath $patchFile
 $stream=[IO.File]::OpenRead((Join-Path $root 'dist\WindowsFolderSize-1.0.11-x64.msi'))
 $sha=[Security.Cryptography.SHA256]::Create()
 try {$hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLower()} finally {$stream.Dispose();$sha.Dispose()}
 [IO.File]::WriteAllText((Join-Path $root 'dist\SHA256SUMS-1.0.11.txt'),$hash+'  WindowsFolderSize-1.0.11-x64.msi'+"`n",[Text.UTF8Encoding]::new($false))
 $archive=[IO.Path]::GetFullPath((Join-Path $root 'build\old-installers'))
 if(-not $archive.StartsWith([IO.Path]::GetFullPath($root)+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Invalid archive path'}
 New-Item -ItemType Directory -Path $archive -Force | Out-Null
 Get-ChildItem -LiteralPath (Join-Path $root 'dist') -File | Where-Object {$_.Name -notin @('WindowsFolderSize-1.0.11-x64.msi','SHA256SUMS-1.0.11.txt')} | ForEach-Object {Move-Item -LiteralPath $_.FullName -Destination (Join-Path $archive $_.Name) -Force}
} finally {Pop-Location}
