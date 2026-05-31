param(
    [string]$GafferVersion = $(if ($env:GAFFER_VERSION) { $env:GAFFER_VERSION } else { "1.6.18.0" }),
    [string]$GafferRuntimeDirName = $(if ($env:GAFFER_RUNTIME_DIR_NAME) { $env:GAFFER_RUNTIME_DIR_NAME } else { "gaffer-$GafferVersion-windows" }),
    [string]$TargetOS = $(if ($env:TARGET_OS) { $env:TARGET_OS } else { "windows" })
)

$ErrorActionPreference = "Stop"

$rootDir = Split-Path -Parent $PSScriptRoot
$distRoot = Join-Path $rootDir "dist\gaffer"
$versionFile = Join-Path $rootDir "VERSION"

if (-not (Test-Path $versionFile)) {
    throw "Missing VERSION file at $versionFile"
}

$version = (Get-Content $versionFile -Raw).Trim()
if (-not $version) {
    throw "VERSION file is empty"
}

if (-not (Test-Path $distRoot)) {
    throw "Expected combined build payload at $distRoot"
}

$archiveBaseName = "scatter-paint-tools-gaffer-v$version-$TargetOS-gaffer-$GafferVersion"
$stagingRoot = Join-Path $rootDir "dist\release"
$packageRoot = Join-Path $stagingRoot $archiveBaseName
$payloadRoot = Join-Path $packageRoot "scatter-paint-tools-gaffer"
$setupGuide = Join-Path $packageRoot "how-to-setup.md"

function Copy-TreeContents($Source, $Destination) {
    if (Test-Path $Source) {
        New-Item -ItemType Directory -Force -Path $Destination | Out-Null
        Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Recurse -Force
    }
}

if (Test-Path $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $payloadRoot | Out-Null

foreach ($pluginDir in @("gaffer_scatter_paint", "gaffer_scatter_plus", "gaffer_pointcloud_plus")) {
    $pluginPayloadRoot = Join-Path $payloadRoot $pluginDir
    Copy-TreeContents (Join-Path $rootDir "$pluginDir\python") (Join-Path $pluginPayloadRoot "python")
    Copy-TreeContents (Join-Path $rootDir "$pluginDir\startup") (Join-Path $pluginPayloadRoot "startup")

    if ($pluginDir -eq "gaffer_scatter_paint") {
        Copy-TreeContents (Join-Path $rootDir "$pluginDir\graphics") (Join-Path $pluginPayloadRoot "graphics")
        Copy-TreeContents (Join-Path $rootDir "$pluginDir\demo") (Join-Path $pluginPayloadRoot "demo")
    }
}

@"
# How To Setup

This package contains the Scatter Paint Tools for Gaffer release for Gaffer $GafferVersion.

Archive layout:

- $archiveBaseName/
- $archiveBaseName/scatter-paint-tools-gaffer/gaffer_scatter_paint/
- $archiveBaseName/scatter-paint-tools-gaffer/gaffer_scatter_plus/
- $archiveBaseName/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/

## Windows setup

1. Unpack the archive.
2. Point Gaffer at the plugin folders inside:

   $archiveBaseName/scatter-paint-tools-gaffer/

3. Use the packaged Gaffer $GafferVersion Windows runtime this release was built against.

Example PowerShell launch setup:

````powershell
`$TOOL_ROOT = "C:\path\to\$archiveBaseName\scatter-paint-tools-gaffer"
`$GAFFER_ROOT = "C:\path\to\$GafferRuntimeDirName"

`$env:PYTHONNOUSERSITE = "1"
`$env:IECORE_FONT_PATHS = "`$GAFFER_ROOT\fonts"
`$env:IECORE_DLL_DIRECTORIES = "`$GAFFER_ROOT\lib"
`$env:PATH = "`$GAFFER_ROOT\bin;`$GAFFER_ROOT\lib;`$env:PATH"
`$env:PYTHONPATH = "`$TOOL_ROOT\gaffer_scatter_paint\python;`$TOOL_ROOT\gaffer_scatter_plus\python;`$TOOL_ROOT\gaffer_pointcloud_plus\python;`$GAFFER_ROOT\python"
`$env:GAFFER_STARTUP_PATHS = "`$TOOL_ROOT\gaffer_scatter_paint\startup;`$TOOL_ROOT\gaffer_scatter_plus\startup;`$TOOL_ROOT\gaffer_pointcloud_plus\startup"

& "`$GAFFER_ROOT\bin\gaffer.cmd"
````

Quick validation:

````powershell
& "`$GAFFER_ROOT\bin\python.exe" -c "import GafferScatterPaint, GafferScatterPaintUI, GafferScatterPlus, GafferScatterPlusUI, GafferPointCloudPlus, GafferPointCloudPlusUI"
````
"@ | Set-Content -LiteralPath $setupGuide -Encoding UTF8

Write-Host "Prepared release package staging at $packageRoot"
