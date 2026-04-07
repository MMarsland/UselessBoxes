param(
    [switch]$CommitManifest,
    [switch]$CreateGitHubRelease,
    [switch]$Push
)

$ErrorActionPreference = "Stop"

$projectDir = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $projectDir
$sourceFile = Join-Path $projectDir "src\Useless_Boxes.cpp"

if (-not (Test-Path $sourceFile)) {
    throw "Could not find firmware source file: $sourceFile"
}

$sourceText = Get-Content -Path $sourceFile -Raw -Encoding UTF8
$versionMatch = [regex]::Match($sourceText, 'CURRENT_FW_VERSION\[\]\s*=\s*"([^"]+)"')
if (-not $versionMatch.Success) {
    throw "Could not find CURRENT_FW_VERSION in $sourceFile"
}

$version = $versionMatch.Groups[1].Value.Trim()
Write-Host "[ota] Release version: $version"

$pioPath = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
if (-not (Test-Path $pioPath)) {
    $pioPath = "platformio"
}

$buildTargets = @(
    @{ Env = "arduino_nano_esp32_michael"; OutputName = "michael-firmware.bin" },
    @{ Env = "arduino_nano_esp32_trevor"; OutputName = "trevor-firmware.bin" }
)

$artifactDir = Join-Path $repoRoot "ota\artifacts\$version"
New-Item -ItemType Directory -Path $artifactDir -Force | Out-Null

foreach ($target in $buildTargets) {
    $envName = $target.Env
    $outputName = $target.OutputName

    Write-Host "[ota] Building $envName..."
    & $pioPath run -d $projectDir -e $envName
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for $envName"
    }

    $firmwarePath = Join-Path $projectDir ".pio\build\$envName\firmware.bin"
    if (-not (Test-Path $firmwarePath)) {
        throw "Expected firmware not found: $firmwarePath"
    }

    $artifactPath = Join-Path $artifactDir $outputName
    Copy-Item -Path $firmwarePath -Destination $artifactPath -Force
    Write-Host "[ota] Created artifact: $artifactPath"
}

if ($CommitManifest) {
    Write-Host "[ota] Committing manifest files..."
    git -C $repoRoot add ota/michael.txt ota/trevor.txt

    $staged = git -C $repoRoot diff --cached --name-only
    if ($staged) {
        git -C $repoRoot commit -m "Update OTA manifests for $version"
    } else {
        Write-Host "[ota] Manifest files unchanged; skipping commit."
    }

    if ($Push) {
        git -C $repoRoot push
    }
}

if ($CreateGitHubRelease) {
    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        throw "GitHub CLI (gh) is not installed or not on PATH."
    }

    cmd /c "gh auth status >NUL 2>NUL"
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI is installed but not authenticated. Run: gh auth login"
    }

    $assetPaths = @(
        (Join-Path $artifactDir "michael-firmware.bin"),
        (Join-Path $artifactDir "trevor-firmware.bin")
    )

    Write-Host "[ota] Creating or updating GitHub release $version..."

    cmd /c "gh release view $version >NUL 2>NUL"
    if ($LASTEXITCODE -eq 0) {
        & gh release upload $version $assetPaths --clobber
    } else {
        & gh release create $version $assetPaths --title $version --notes "OTA firmware release $version"
    }

    if ($LASTEXITCODE -ne 0) {
        throw "GitHub release command failed."
    }
}

Write-Host "[ota] Done. Artifacts are in: $artifactDir"
Write-Host "[ota] Run with -CommitManifest and/or -CreateGitHubRelease to automate Git and GitHub steps."
