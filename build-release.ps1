param(
  [string]$Configuration = "Release",
  [string]$Platform = "x64",
  [string]$Toolset = "v142",
  [string]$VCToolsVersion = "14.29.30133"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$project = Join-Path $root "minimap_mod.vcxproj"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = $null
if (Test-Path -LiteralPath $vswhere) {
  $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
}

if (-not $installPath) {
  $known = "C:\Program Files\Microsoft Visual Studio\18\Insiders"
  if (Test-Path -LiteralPath $known) {
    $installPath = $known
  }
}

if (-not $installPath) {
  throw "Visual Studio/MSBuild not found."
}

$vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvarsall.bat"
$msbuild = Join-Path $installPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path -LiteralPath $vcvars)) { throw "vcvarsall.bat not found: $vcvars" }
if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }

$vcvarsArgs = $Platform
if ($VCToolsVersion) {
  $vcvarsArgs = "$vcvarsArgs -vcvars_ver=$VCToolsVersion"
}

$cmd = "`"$vcvars`" $vcvarsArgs && `"$msbuild`" `"$project`" /p:Configuration=$Configuration /p:Platform=$Platform /p:PlatformToolset=$Toolset /m"
& cmd.exe /c $cmd
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
