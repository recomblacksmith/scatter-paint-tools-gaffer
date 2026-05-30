param(
    [ValidateSet("scatter_paint", "scatter_plus", "pointcloud_plus", "all")]
    [string]$Plugin = "all",
    [switch]$Tests,
    [string]$GafferVersion = $(if ($env:GAFFER_VERSION) { $env:GAFFER_VERSION } else { "1.6.18.0" }),
    [string]$GafferUrl = $env:GAFFER_URL,
    [string]$GafferRoot = $env:GAFFER_ROOT,
    [string]$CacheRoot = $(if ($env:GAFFER_CACHE_ROOT) { $env:GAFFER_CACHE_ROOT } else { Join-Path $PSScriptRoot ".cache\gaffer" }),
    [int]$Jobs = $(if ($env:JOBS) { [int]$env:JOBS } else { [Environment]::ProcessorCount })
)

$ErrorActionPreference = "Stop"

function Resolve-VsDevCmd {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Unable to find vswhere.exe. Install Visual Studio 2022 with the C++ build tools."
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        throw "Unable to find Visual Studio C++ build tools."
    }

    $devCmd = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $devCmd)) {
        throw "Unable to find VsDevCmd.bat at $devCmd."
    }

    return $devCmd
}

function Resolve-SConsCommand {
    $scons = Get-Command scons -ErrorAction SilentlyContinue
    if ($scons) {
        return "scons"
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        return "py -m SCons"
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return "python -m SCons"
    }

    throw "Unable to find SCons. Install it with 'py -m pip install scons' or make 'scons' available on PATH."
}

function Resolve-GafferRuntime {
    if ($GafferRoot) {
        $root = Resolve-Path $GafferRoot
        return $root.Path
    }

    $archiveName = "gaffer-$GafferVersion-windows.zip"
    $url = if ($GafferUrl) { $GafferUrl } else { "https://github.com/GafferHQ/gaffer/releases/download/$GafferVersion/$archiveName" }
    $archivePath = Join-Path $CacheRoot $archiveName
    $extractRoot = Join-Path $CacheRoot $GafferVersion
    $runtimeRoot = Join-Path $extractRoot "gaffer-$GafferVersion-windows"

    New-Item -ItemType Directory -Force -Path $CacheRoot, $extractRoot | Out-Null

    if (-not (Test-Path $archivePath)) {
        Write-Host "Downloading $url"
        Invoke-WebRequest -Uri $url -OutFile $archivePath
    }

    $requiredRuntimeFiles = @(
        "bin\python.exe",
        "lib\Gaffer.lib",
        "lib\boost_python311.lib",
        "python\Gaffer\__init__.py"
    )
    $runtimeComplete = $true
    foreach ($relativePath in $requiredRuntimeFiles) {
        if (-not (Test-Path (Join-Path $runtimeRoot $relativePath))) {
            $runtimeComplete = $false
            break
        }
    }

    if (-not $runtimeComplete) {
        if (Test-Path $extractRoot) {
            Remove-Item -LiteralPath $extractRoot -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
        $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
        if ($tar) {
            & $tar.Source -xf $archivePath -C $extractRoot
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to extract $archivePath with tar.exe."
            }
        } else {
            Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot -Force
        }
    }

    foreach ($relativePath in $requiredRuntimeFiles) {
        if (-not (Test-Path (Join-Path $runtimeRoot $relativePath))) {
            throw "Downloaded archive did not contain the expected runtime file $relativePath under $runtimeRoot."
        }
    }

    if (-not (Test-Path (Join-Path $runtimeRoot "bin\python.exe"))) {
        throw "Downloaded archive did not contain the expected runtime at $runtimeRoot."
    }

    return $runtimeRoot
}

function Get-PluginDirectories {
    switch ($Plugin) {
        "scatter_paint" { @("gaffer_scatter_paint") }
        "scatter_plus" { @("gaffer_scatter_plus") }
        "pointcloud_plus" { @("gaffer_pointcloud_plus") }
        "all" { @("gaffer_scatter_paint", "gaffer_scatter_plus", "gaffer_pointcloud_plus") }
    }
}

function Copy-TreeContents($Source, $Destination) {
    if (Test-Path $Source) {
        New-Item -ItemType Directory -Force -Path $Destination | Out-Null
        Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Recurse -Force
    }
}

$repoRoot = $PSScriptRoot
$runtimeRoot = Resolve-GafferRuntime
$devCmd = Resolve-VsDevCmd
$sconsCommand = Resolve-SConsCommand
$plugins = Get-PluginDirectories

$distRoot = Join-Path $repoRoot "dist\gaffer"
if ($Plugin -eq "all" -and (Test-Path $distRoot)) {
    Remove-Item -LiteralPath $distRoot -Recurse -Force
}

foreach ($pluginDir in $plugins) {
    Write-Host "Building $pluginDir against $runtimeRoot"
    $pluginPath = Join-Path $repoRoot $pluginDir
    $buildCmd = Join-Path ([System.IO.Path]::GetTempPath()) "scatter-paint-tools-build-$PID.cmd"
    @(
        "@echo off",
        "call `"$devCmd`" -arch=x64 -host_arch=x64",
        "if errorlevel 1 exit /b %errorlevel%",
        "$sconsCommand -C `"$pluginPath`" GAFFER_ROOT=`"$runtimeRoot`" -j$Jobs",
        "exit /b %errorlevel%"
    ) | Set-Content -LiteralPath $buildCmd -Encoding ASCII
    cmd.exe /d /s /c "`"$buildCmd`""
    Remove-Item -LiteralPath $buildCmd -Force -ErrorAction SilentlyContinue
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $pluginDir."
    }
}

New-Item -ItemType Directory -Force -Path (Join-Path $distRoot "python"), (Join-Path $distRoot "startup") | Out-Null
foreach ($pluginDir in $plugins) {
    Copy-TreeContents (Join-Path $repoRoot "$pluginDir\python") (Join-Path $distRoot "python")
    Copy-TreeContents (Join-Path $repoRoot "$pluginDir\startup") (Join-Path $distRoot "startup")
    if ($pluginDir -eq "gaffer_scatter_paint") {
        Copy-TreeContents (Join-Path $repoRoot "$pluginDir\graphics") (Join-Path $distRoot "graphics")
        Copy-TreeContents (Join-Path $repoRoot "$pluginDir\demo") (Join-Path $distRoot "demo")
    }
}

$env:PYTHONNOUSERSITE = "1"
$env:QT_QPA_PLATFORM = "offscreen"
$env:IECORE_FONT_PATHS = Join-Path $runtimeRoot "fonts"
$env:IECORE_DLL_DIRECTORIES = Join-Path $runtimeRoot "lib"
$env:PATH = "$(Join-Path $runtimeRoot "bin");$(Join-Path $runtimeRoot "lib");$env:PATH"
$env:PYTHONPATH = "$(Join-Path $distRoot "python");$(Join-Path $runtimeRoot "python")"
$env:GAFFER_STARTUP_PATHS = Join-Path $distRoot "startup"

$imports = switch ($Plugin) {
    "scatter_paint" { "import GafferScatterPaint, GafferScatterPaintUI" }
    "scatter_plus" { "import GafferScatterPlus, GafferScatterPlusUI" }
    "pointcloud_plus" { "import GafferPointCloudPlus, GafferPointCloudPlusUI" }
    "all" { "import GafferScatterPaint, GafferScatterPaintUI, GafferScatterPlus, GafferScatterPlusUI, GafferPointCloudPlus, GafferPointCloudPlusUI" }
}

& (Join-Path $runtimeRoot "bin\python.exe") -c $imports
if ($LASTEXITCODE -ne 0) {
    throw "Import smoke test failed."
}

if ($Tests) {
    foreach ($testModule in @("GafferScatterPaintTest", "GafferScatterPlusTest", "GafferPointCloudPlusTest")) {
        if ($Plugin -eq "all" -or $testModule -like "*$(($Plugin -replace '_', '') -replace 'pointcloud', 'PointCloud')*") {
            & (Join-Path $runtimeRoot "bin\python.exe") -m unittest $testModule
            if ($LASTEXITCODE -ne 0) {
                throw "Tests failed for $testModule."
            }
        }
    }
}

Write-Host "Windows plugin bundle written to $distRoot"
