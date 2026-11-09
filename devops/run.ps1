param([switch]$Example, [Parameter(ValueFromRemainingArguments = $true)][string[]]$ApplicationArguments)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildCandidates = @('build/windows-msvc/Release/smarttank.exe', 'build/windows-preview/Release/smarttank.exe')
$newestBuild = $buildCandidates | ForEach-Object {
    $candidatePath = Join-Path $projectRoot $_
    if (Test-Path -LiteralPath $candidatePath) { Get-Item -LiteralPath $candidatePath }
} | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $newestBuild) {
    throw 'Build SmartTank first: cmake --preset windows-msvc; cmake --build --preset release'
}
$applicationPath = $newestBuild.FullName
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
