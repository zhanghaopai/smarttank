param(
    [switch]$Example,
    [switch]$Preview,
    [switch]$SkipBuild,
    [switch]$NoLaunch,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ApplicationArguments
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$configurePreset = 'windows-msvc'
$buildPreset = 'release'
if ($Preview) {
    $configurePreset = 'windows-preview'
    $buildPreset = 'preview'
}
$applicationPath = Join-Path $projectRoot "build/$configurePreset/Release/smarttank.exe"
if (-not $SkipBuild) {
    Push-Location -LiteralPath $projectRoot
    try {
        & cmake --preset $configurePreset
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with code $LASTEXITCODE" }
        & cmake --build --preset $buildPreset
        if ($LASTEXITCODE -ne 0) { throw "Build failed with code $LASTEXITCODE" }
        & ctest --preset $buildPreset
        if ($LASTEXITCODE -ne 0) { throw "Tests failed with code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}
if ($NoLaunch) { return }
if (-not (Test-Path -LiteralPath $applicationPath)) {
    throw 'Application not found. Run ./devops/run.ps1 without -SkipBuild first.'
}
$previousPath = $env:PATH
try {
    $qtRuntime = Join-Path $projectRoot '.deps/Qt/6.8.3/msvc2022_64/bin'
    $occtRuntime = Join-Path $projectRoot '.deps/vcpkg/installed/x64-windows/bin'
    $env:PATH = "$qtRuntime;$occtRuntime;$previousPath"
    $launchArguments = @()
    if ($Example) { $launchArguments += '--preview-example' }
    $launchArguments += $ApplicationArguments
    & $applicationPath @launchArguments
    if ($LASTEXITCODE -ne 0) { throw "SmartTank exited with code $LASTEXITCODE" }
} finally {
    $env:PATH = $previousPath
}
