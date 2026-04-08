param(
    [switch]$CommitManifest,
    [switch]$CreateGitHubRelease,
    [switch]$Push
)

$ErrorActionPreference = "Stop"

function Get-SemVerParts([string]$Version) {
    $match = [regex]::Match($Version.Trim(), '^v?(\d+)\.(\d+)\.(\d+)$')
    if (-not $match.Success) {
        return $null
    }

    return @{
        Major = [int]$match.Groups[1].Value
        Minor = [int]$match.Groups[2].Value
        Patch = [int]$match.Groups[3].Value
    }
}

function Compare-SemVer([string]$Left, [string]$Right) {
    $leftParts = Get-SemVerParts $Left
    $rightParts = Get-SemVerParts $Right

    if (-not $leftParts -or -not $rightParts) {
        return [string]::Compare($Left, $Right, $true)
    }

    foreach ($field in @("Major", "Minor", "Patch")) {
        if ($leftParts[$field] -gt $rightParts[$field]) { return 1 }
        if ($leftParts[$field] -lt $rightParts[$field]) { return -1 }
    }

    return 0
}

function Increment-PatchVersion([string]$Version) {
    $parts = Get-SemVerParts $Version
    if (-not $parts) {
        throw "Version '$Version' is not in v<major>.<minor>.<patch> format."
    }

    return "v{0}.{1}.{2}" -f $parts.Major, $parts.Minor, ($parts.Patch + 1)
}

function Get-ManifestVersion([string]$Path) {
    if (-not (Test-Path $Path)) {
        return $null
    }

    $firstLine = Get-Content -Path $Path -TotalCount 1 -ErrorAction SilentlyContinue
    if (-not $firstLine) {
        return $null
    }

    return $firstLine.Trim()
}

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

$manifestPaths = @(
    (Join-Path $repoRoot "ota\michael.txt"),
    (Join-Path $repoRoot "ota\trevor.txt")
)
$manifestVersions = @($manifestPaths | ForEach-Object { Get-ManifestVersion $_ } | Where-Object { $_ })
$latestManifestVersion = $null
foreach ($manifestVersion in $manifestVersions) {
    if (-not $latestManifestVersion -or (Compare-SemVer $manifestVersion $latestManifestVersion) -gt 0) {
        $latestManifestVersion = $manifestVersion
    }
}

if ($latestManifestVersion -and $version -eq $latestManifestVersion) {
    $nextVersion = Increment-PatchVersion $version
    $updatedSourceText = [regex]::Replace(
        $sourceText,
        'CURRENT_FW_VERSION\[\]\s*=\s*"([^"]+)"',
        "CURRENT_FW_VERSION[] = `"$nextVersion`"",
        1
    )

    Set-Content -Path $sourceFile -Value $updatedSourceText -Encoding UTF8
    $sourceText = $updatedSourceText
    $version = $nextVersion
    Write-Host "[ota] Auto-bumped CURRENT_FW_VERSION to $version"
} elseif ($latestManifestVersion) {
    Write-Host "[ota] CURRENT_FW_VERSION already updated since last run; keeping $version"
} else {
    Write-Host "[ota] No prior manifest version found; keeping $version"
}

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
    Write-Host "[ota] Committing release metadata files..."
    git -C $repoRoot add PlatformIO/src/Useless_Boxes.cpp ota/michael.txt ota/trevor.txt

    $staged = git -C $repoRoot diff --cached --name-only
    if ($staged) {
        git -C $repoRoot commit -m "Prepare OTA release $version"
    } else {
        Write-Host "[ota] Release metadata unchanged; skipping commit."
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
