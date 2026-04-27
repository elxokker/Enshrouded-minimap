param(
  [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Enshrouded",
  [string]$Configuration = "Release",
  [string]$Platform = "x64",
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not $SkipBuild) {
  & (Join-Path $root "build-release.ps1") -Configuration $Configuration -Platform $Platform
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

$sourceDll = Join-Path $root "build\$Platform\$Configuration\mods\minimap_mod.dll"
$assetsDir = Join-Path $root "assets"
$targetDir = Join-Path $GameDir "mods\minimap_mod"

if (-not (Test-Path -LiteralPath $sourceDll)) {
  throw "Built DLL not found: $sourceDll"
}

New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupDir = Join-Path $targetDir "backup-$stamp"
if (Test-Path -LiteralPath (Join-Path $targetDir "minimap_mod.dll")) {
  New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
  Get-ChildItem -LiteralPath $targetDir -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $backupDir $_.Name) -Force
  }
}

Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $targetDir "minimap_mod.dll") -Force
Get-ChildItem -LiteralPath $assetsDir -File | ForEach-Object {
  Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $targetDir $_.Name) -Force
}

Write-Host "Installed minimap_mod to $targetDir"
if (Test-Path -LiteralPath $backupDir) {
  Write-Host "Backup: $backupDir"
}
