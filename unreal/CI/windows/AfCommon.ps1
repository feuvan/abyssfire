# Shared helpers for the Abyssfire CI PowerShell scripts (self-hosted Windows runner). Dot-source it:
#   . (Join-Path $PSScriptRoot 'AfCommon.ps1')
# Compatible with Windows PowerShell 5.1 and PowerShell 7 (no ternary / null-coalescing / pipeline-chain operators).
Set-StrictMode -Version 3.0

$script:AfCiDir = Split-Path -Parent $PSScriptRoot
$script:AfUnrealDir = Split-Path -Parent $script:AfCiDir
$script:AfRepoDir = Split-Path -Parent $script:AfUnrealDir
if ($env:AF_OUT_DIR) { $script:AfOutDir = $env:AF_OUT_DIR } else { $script:AfOutDir = Join-Path $script:AfCiDir 'out' }
$script:AfUproject = Join-Path $script:AfUnrealDir 'Abyssfire.uproject'
# per job, so a job's log artifact does not re-upload the logs of the jobs it downloaded artifacts from
if ($env:AF_LOG_DIR) { $script:AfLogDir = $env:AF_LOG_DIR }
elseif ($env:CI_JOB_NAME_SLUG) { $script:AfLogDir = Join-Path $script:AfOutDir "logs/$($env:CI_JOB_NAME_SLUG)" }
else { $script:AfLogDir = Join-Path $script:AfOutDir 'logs/local' }
$script:AfRestore = New-Object System.Collections.ArrayList

function Write-AfLog([string]$Message) { Write-Host "[af] $Message" -ForegroundColor Cyan }
function Write-AfWarn([string]$Message) { Write-Host "[af] WARNING: $Message" -ForegroundColor Yellow }

function Start-AfSection([string]$Name, [string]$Title) {
    if ($env:GITLAB_CI) {
        $t = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        Write-Host ("$([char]27)[0Ksection_start:{0}:{1}`r$([char]27)[0K{2}" -f $t, $Name, $Title)
    } else { Write-Host "=== $Title" }
}
function Stop-AfSection([string]$Name) {
    if ($env:GITLAB_CI) {
        $t = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        Write-Host ("$([char]27)[0Ksection_end:{0}:{1}`r$([char]27)[0K" -f $t, $Name)
    }
}

function Test-AfTrue([string]$Value) {
    if (-not $Value) { return $false }
    return @('1', 'true', 'yes', 'on') -contains $Value.ToLowerInvariant()
}

# Every signing secret this CI knows (unreal/Docs/CI.md 4.2; same list as AF_SECRET_VARS in lib/common.sh).
$script:AfSecretVars = @('AF_MAC_DEVID_P12_B64', 'AF_MAC_DEVID_P12_PASSWORD', 'AF_ASC_API_KEY_P8_B64', 'AF_ASC_API_KEY_ID',
    'AF_ASC_API_ISSUER_ID', 'AF_ANDROID_KEYSTORE_B64', 'AF_ANDROID_KEYSTORE_PASSWORD', 'AF_ANDROID_KEY_ALIAS',
    'AF_ANDROID_KEY_PASSWORD', 'AF_IOS_DIST_P12_B64', 'AF_IOS_DIST_P12_PASSWORD', 'AF_IOS_PROVISION_PROFILE_B64',
    'AF_WIN_CODESIGN_PFX_B64', 'AF_WIN_CODESIGN_PFX_PASSWORD', 'AF_RELEASE_API_TOKEN')

# Removes the signing secrets from this process, so the engine and its crash dumps never inherit them.
function Clear-AfSecrets {
    foreach ($n in $script:AfSecretVars) { Remove-Item -LiteralPath "Env:$n" -ErrorAction SilentlyContinue }
}

# Port of unreal/CI/version.sh (same rules, same variable names). Values already in the environment win
# (the `version` job's dotenv report).
function Get-AfVersion {
    if ($env:AF_VERSION -and $env:AF_MSI_VERSION) {
        return [pscustomobject]@{
            Version = $env:AF_VERSION; Numeric = $env:AF_VERSION_NUMERIC; Quad = $env:AF_VERSION_QUAD
            Build = $env:AF_BUILD_NUMBER; AndroidCode = $env:AF_ANDROID_VERSION_CODE; MsiVersion = $env:AF_MSI_VERSION
        }
    }
    $sha = $env:CI_COMMIT_SHORT_SHA
    if (-not $sha) {
        $ErrorActionPreference = 'Continue'   # Windows PowerShell 5.1: native stderr under 'Stop' would throw
        $sha = (& git -C $script:AfRepoDir rev-parse --short=8 HEAD 2>$null)
        if (-not $sha) { $sha = 'unknown' }
    }
    $build = 0
    if ($env:CI_PIPELINE_IID -match '^\d+$') { $build = [int]$env:CI_PIPELINE_IID }
    $tag = $env:CI_COMMIT_TAG
    if ($tag -and $tag -match '^v(\d+)\.(\d+)\.(\d+)(-([0-9A-Za-z.-]+))?$') {
        $major = [int]$Matches[1]; $minor = [int]$Matches[2]; $patch = [int]$Matches[3]
        if ($major -gt 209 -or $minor -gt 99 -or $patch -gt 99) { throw "tag $tag is outside the supported version range" }
        $numeric = "$major.$minor.$patch"
        $version = $numeric
        $suffix = 99
        if ($Matches[5]) {
            $version = "$numeric-$($Matches[5])"
            $suffix = 0
            if ($Matches[5] -match '(\d+)$') { $suffix = [Math]::Min([int]$Matches[1], 98) }
        }
        $code = $major * 10000000 + $minor * 100000 + $patch * 100 + $suffix
        $msi = "$major.$minor.$($patch * 100 + $suffix)"
    } else {
        $numeric = '0.0.0'
        $version = "0.0.0-$sha"
        $code = [Math]::Max($build, 1)
        $msi = '0.0.0'
    }
    $v = [pscustomobject]@{
        Version = $version; Numeric = $numeric; Quad = "$numeric.$($build % 65536)"; Build = "$build"
        AndroidCode = "$code"; MsiVersion = $msi
    }
    $env:AF_VERSION = $v.Version; $env:AF_VERSION_NUMERIC = $v.Numeric; $env:AF_VERSION_QUAD = $v.Quad
    $env:AF_BUILD_NUMBER = $v.Build; $env:AF_ANDROID_VERSION_CODE = $v.AndroidCode; $env:AF_MSI_VERSION = $v.MsiVersion
    return $v
}

# Appends a section to an ini of the CI working copy (backed up, restored by Restore-AfFiles; never committed).
function Add-AfIniOverlay([string]$Path, [string]$Section, [string[]]$Lines) {
    $backup = "$Path.af-ci-backup"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $Path -Destination $backup
        [void]$script:AfRestore.Add(@{ Kind = 'restore'; Path = $Path })
    }
    $text = "`r`n; --- added by unreal/CI for this build only (restored afterwards) ---`r`n$Section`r`n" +
        (($Lines | ForEach-Object { "$_`r`n" }) -join '')
    [System.IO.File]::AppendAllText($Path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

# Copies a file into the working copy for this build only (removed by Restore-AfFiles).
function Add-AfTempFile([string]$Source, [string]$Destination) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    [void]$script:AfRestore.Add(@{ Kind = 'delete'; Path = $Destination })
}

function Restore-AfFiles {
    foreach ($r in $script:AfRestore) {
        try {
            if ($r.Kind -eq 'restore') {
                Move-Item -LiteralPath "$($r.Path).af-ci-backup" -Destination $r.Path -Force
            } else {
                Remove-Item -LiteralPath $r.Path -Force -ErrorAction SilentlyContinue
            }
        } catch { Write-AfWarn "could not restore $($r.Path): $_" }
    }
    $script:AfRestore.Clear()
}

# Runs a native tool, mirrors its output to a log file and throws on a non-zero exit code.
function Invoke-AfNative([string]$Exe, [string[]]$Arguments, [string]$LogName) {
    $logDir = $script:AfLogDir
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $log = Join-Path $logDir "$LogName.console.log"
    Write-AfLog ("{0} {1}" -f $Exe, ($Arguments -join ' '))
    # Windows PowerShell 5.1 turns native stderr lines into terminating errors under 'Stop' when redirected.
    $ErrorActionPreference = 'Continue'
    & $Exe @Arguments 2>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath $log -Append | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) { throw "$LogName failed with exit code $code (log: $log)" }
}
