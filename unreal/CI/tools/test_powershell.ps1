#Requires -Version 7.0
<#
.SYNOPSIS
    Self-test of the Windows-runner PowerShell scripts on any OS with PowerShell 7 (no Unreal Engine needed).

.DESCRIPTION
    1. Parses every unreal/CI/windows/*.ps1 with the PowerShell AST parser (syntax errors fail).
    2. Runs PSScriptAnalyzer when the module is installed (errors and warnings fail).
    3. Runs ue-package.ps1 end to end in a scratch copy of the project skeleton, with a fake RunUAT
       (AF_UAT_COMMAND) that checks the arguments, the ProjectVersion overlay, the DDC variable and that no signing
       secret reaches it, then writes a staged Windows build, this build's PDB and the pak list (PakList_*.txt);
       afterwards the outputs (PDB zip, not a stale PDB) must exist and every overlaid file must be restored. A pak
       list without one of the Data/ files must fail the job; a changed toolchain stamp must wipe Intermediate.
       Same for -Platform Android: throw-away keystore (keytool; a stub when no JDK is installed), the .apk only
       when bEnableUniversalAPK=True is overlaid, Intermediate/Android removed afterwards.

    pwsh -NoProfile -File unreal/CI/tools/test_powershell.ps1 [-WorkDir <dir>]
#>
param([string]$WorkDir = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$ci = Split-Path -Parent $PSScriptRoot
$failures = 0
function Assert-All($Checks) {
    foreach ($c in $Checks) {
        if ($c.ok) { Write-Host "  ok   $($c.what)" } else { $script:failures++; Write-Host "  FAIL $($c.what)" }
    }
}

Write-Host '=== 1. parse'
foreach ($f in Get-ChildItem -Path (Join-Path $ci 'windows') -Filter '*.ps1') {
    $tokens = $null; $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($f.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors) {
        $failures++
        $errors | ForEach-Object { Write-Host "  FAIL $($f.Name):$($_.Extent.StartLineNumber) $($_.Message)" }
    } else { Write-Host "  ok   $($f.Name) ($($tokens.Count) tokens)" }
}

Write-Host '=== 2. PSScriptAnalyzer'
if (Get-Module -ListAvailable -Name PSScriptAnalyzer) {
    $issues = Invoke-ScriptAnalyzer -Path (Join-Path $ci 'windows') -Severity Error, Warning `
        -ExcludeRule PSAvoidUsingWriteHost
    if ($issues) { $failures++; $issues | Format-Table -AutoSize | Out-String | Write-Host } else { Write-Host '  ok' }
} else { Write-Host '  skipped (Install-Module PSScriptAnalyzer to enable)' }

Write-Host '=== 3. ue-package.ps1 with a fake RunUAT'
if (-not $WorkDir) { $WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ("af-ps-" + [guid]::NewGuid().ToString('N').Substring(0, 8)) }
if (Test-Path $WorkDir) { Remove-Item -Recurse -Force $WorkDir }
$u = Join-Path $WorkDir 'unreal'
New-Item -ItemType Directory -Force -Path (Join-Path $u 'CI/windows'), (Join-Path $u 'Config'),
    (Join-Path $u 'Content/Abyssfire/Maps'), (Join-Path $u 'Binaries/Win64'), (Join-Path $u 'Data'), (Join-Path $u 'Fonts'),
    (Join-Path $u 'CI/out/installer-assets/ue/Windows') | Out-Null
Copy-Item (Join-Path $ci 'windows/*.ps1') (Join-Path $u 'CI/windows')
Set-Content (Join-Path $u 'Abyssfire.uproject') '{ "FileVersion": 3 }'
$ini = Join-Path $u 'Config/DefaultGame.ini'
Set-Content $ini "[/Script/EngineSettings.GeneralProjectSettings]`r`nProjectVersion=0.1.0"
$iniBefore = Get-Content -Raw $ini
Set-Content (Join-Path $u 'Content/Abyssfire/Maps/L_Main.umap') 'umap'
Set-Content (Join-Path $u 'Data/items.json') '{}'
Set-Content (Join-Path $u 'Data/README.md') 'not a runtime file'
Set-Content (Join-Path $u 'Fonts/Test-Regular.ttf') 'ttf'
# a stale PDB of an earlier Development build must not end up in the symbols
$stalePdb = Join-Path $u 'Binaries/Win64/Abyssfire.pdb'
Set-Content $stalePdb 'old pdb'
(Get-Item $stalePdb).LastWriteTime = (Get-Date).AddDays(-3)
Set-Content (Join-Path $u 'CI/out/installer-assets/ue/Windows/Application.ico') 'ico'

# fake RunUAT helpers shared by every variant: the pak list UAT writes into uebp_LogFolder
$pakListHelper = @'
function Write-FakePakList([string]$Project, [string]$Drop) {
    $root = Split-Path -Parent $Project
    $lines = foreach ($d in 'Data', 'Fonts') {
        Get-ChildItem -Recurse -File (Join-Path $root $d) | ForEach-Object {
            $rel = $_.FullName.Substring($root.Length + 1).Replace('\', '/')
            if ($rel -ne $Drop) { "`"$($_.FullName)`" `"../../../Abyssfire/$rel`" -compress" }
        }
    }
    Set-Content (Join-Path $env:uebp_LogFolder 'PakList_pakchunk0-Test.txt') $lines
}
foreach ($n in 'AF_ANDROID_KEYSTORE_B64', 'AF_ANDROID_KEYSTORE_PASSWORD', 'AF_WIN_CODESIGN_PFX_B64', 'AF_MAC_DEVID_P12_B64') {
    if ([Environment]::GetEnvironmentVariable($n)) { Write-Host "fake UAT: secret $n reached the engine"; exit 9 }
}
'@

$fake = Join-Path $WorkDir 'fake-uat.ps1'
($pakListHelper + @'
$a = $args
function Arg([string]$name) { ($a | Where-Object { $_ -like "-$name=*" } | Select-Object -First 1) -replace "^-$name=", '' }
foreach ($need in @('BuildCookRun', '-platform=Win64', '-clientconfig=Shipping', '-cook', '-stage', '-pak', '-iostore',
                    '-package', '-archive', '-prereqs', '-unattended')) {
    if ($a -notcontains $need) { Write-Host "fake UAT: missing $need"; exit 3 }
}
$proj = Arg 'project'
$ini = Join-Path (Split-Path -Parent $proj) 'Config/DefaultGame.ini'
if (-not ((Get-Content -Raw $ini) -match 'ProjectVersion=1\.2\.3')) { Write-Host 'fake UAT: no ProjectVersion overlay'; exit 4 }
if (-not [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath')) { Write-Host 'fake UAT: no DDC path'; exit 5 }
if (-not (Test-Path (Join-Path (Split-Path -Parent $proj) 'Build/Windows/Application.ico'))) { Write-Host 'fake UAT: no icon'; exit 6 }
$w = Join-Path (Arg 'archivedirectory') 'Windows'
New-Item -ItemType Directory -Force -Path (Join-Path $w 'Abyssfire/Content/Paks'), (Join-Path $w 'Engine/Extras/Redist/en-us') | Out-Null
Set-Content (Join-Path $w 'Abyssfire.exe') 'MZ'
Set-Content (Join-Path $w 'Abyssfire/Content/Paks/Abyssfire-Windows.pak') 'pak'
Set-Content (Join-Path $w 'Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe') 'MZ'
New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path -Parent $proj) 'Binaries/Win64') | Out-Null
Set-Content (Join-Path (Split-Path -Parent $proj) 'Binaries/Win64/Abyssfire-Win64-Shipping.pdb') 'shipping pdb'
Write-FakePakList -Project $proj -Drop $env:AF_TEST_DROP
Write-Host 'fake UAT: BUILD SUCCESSFUL'
Write-Error 'a line on stderr must not fail the job' -ErrorAction Continue
exit 0
'@) | Set-Content $fake

$env:AF_UAT_COMMAND = $fake
$env:UE_ROOT_WIN = (Join-Path $WorkDir 'no-engine')
$env:CI_COMMIT_TAG = 'v1.2.3'; $env:CI_PIPELINE_IID = '42'; $env:CI_COMMIT_SHORT_SHA = 'abcdef12'
$env:AF_WIN_CODESIGN_PFX_B64 = 'c2VjcmV0'   # must be cleared before the engine runs
foreach ($n in 'AF_VERSION', 'AF_MSI_VERSION', 'AF_OUT_DIR', 'AF_SYMBOLS_DIR', 'GITLAB_CI', 'AF_TEST_DROP') { Remove-Item "Env:$n" -ErrorAction SilentlyContinue }
& (Join-Path $u 'CI/windows/ue-package.ps1')
$code = $LASTEXITCODE
$out = Join-Path $u 'CI/out/package/win64'
$pdbZip = Join-Path $out 'symbols/Abyssfire-1.2.3-win64-pdb.zip'
$zipEntries = @()
if (Test-Path $pdbZip) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [System.IO.Compression.ZipFile]::OpenRead($pdbZip); try { $zipEntries = @($z.Entries | ForEach-Object Name) } finally { $z.Dispose() }
}
$stampFile = Join-Path $u 'Intermediate/.af-toolchain-stamp'
Assert-All @(
    @{ ok = ($code -eq 0); what = "exit code 0 (got $code)" },
    @{ ok = (Test-Path (Join-Path $out 'Windows/Abyssfire.exe')); what = 'staged Windows/Abyssfire.exe' },
    @{ ok = (Test-Path (Join-Path $out 'Windows/Abyssfire/Content/Paks/Abyssfire-Windows.pak')); what = 'staged .pak' },
    @{ ok = (($zipEntries -join ',') -eq 'Abyssfire-Win64-Shipping.pdb'); what = "symbols zip holds only this build's PDB ($($zipEntries -join ','))" },
    @{ ok = (-not (Get-ChildItem (Join-Path $out 'symbols') -Filter '*.pdb')); what = 'no loose PDB in the artifact' },
    @{ ok = (Test-Path (Join-Path $out 'package-info.txt')); what = 'package-info.txt' },
    @{ ok = (Test-Path $stampFile); what = 'toolchain stamp written' },
    @{ ok = ((Get-Content -Raw $ini) -eq $iniBefore); what = 'DefaultGame.ini restored' },
    @{ ok = (-not (Test-Path "$ini.af-ci-backup")); what = 'no backup left behind' },
    @{ ok = (-not (Test-Path (Join-Path $u 'Build/Windows/Application.ico'))); what = 'temporary icon removed' }
)

Write-Host '=== 3b. failing UAT must fail the script and still restore files'
$good = Get-Content -Raw $fake
Set-Content $fake 'Write-Host "fake UAT: ERROR"; exit 7'
& (Join-Path $u 'CI/windows/ue-package.ps1')
$code = $LASTEXITCODE
if ($code -ne 0 -and ((Get-Content -Raw $ini) -eq $iniBefore)) { Write-Host "  ok   exit $code, files restored" }
else { $failures++; Write-Host "  FAIL exit $code" }

Write-Host '=== 3c. a runtime Data/ file missing from the pak fails the job'
Set-Content $fake $good
$env:AF_TEST_DROP = 'Data/items.json'
& (Join-Path $u 'CI/windows/ue-package.ps1')
$code = $LASTEXITCODE
Remove-Item Env:AF_TEST_DROP
if ($code -ne 0) { Write-Host "  ok   exit $code" } else { $failures++; Write-Host '  FAIL a pak without Data/items.json was accepted' }

Write-Host '=== 3d. a changed toolchain stamp wipes Intermediate first'
Set-Content $stampFile 'an older engine'
Set-Content (Join-Path $u 'Intermediate/stale.marker') 'x'
& (Join-Path $u 'CI/windows/ue-package.ps1')
$code = $LASTEXITCODE
Assert-All @(
    @{ ok = ($code -eq 0); what = "exit code 0 (got $code)" },
    @{ ok = (-not (Test-Path (Join-Path $u 'Intermediate/stale.marker'))); what = 'stale Intermediate removed' }
)

Write-Host '=== 3e. -Platform Android (distribution build, throw-away keystore)'
New-Item -ItemType Directory -Force -Path (Join-Path $u 'Config/Android') | Out-Null
$aini = Join-Path $u 'Config/Android/AndroidEngine.ini'
Set-Content $aini '[/Script/Engine.UserInterfaceSettings]'
$aBefore = Get-Content -Raw $aini
($pakListHelper + @'
$a = $args
function Arg([string]$name) { ($a | Where-Object { $_ -like "-$name=*" } | Select-Object -First 1) -replace "^-$name=", '' }
foreach ($need in @('-platform=Android', '-cookflavor=ASTC', '-distribution', '-package')) {
    if ($a -notcontains $need) { Write-Host "fake UAT: missing $need"; exit 3 }
}
$u = Split-Path -Parent (Arg 'project')
$ini = Get-Content -Raw (Join-Path $u 'Config/Android/AndroidEngine.ini')
foreach ($k in @('KeyStore=abyssfire-ci-throwaway.keystore', 'KeyAlias=ci', 'KeyStorePassword=', 'StoreVersion=10200399',
                 'VersionDisplayName=1.2.3', 'bEnableBundle=True')) {
    if (-not $ini.Contains($k)) { Write-Host "fake UAT: ini lacks $k"; exit 4 }
}
if ((Get-Item (Join-Path $u 'Build/Android/abyssfire-ci-throwaway.keystore')).Length -le 0) { Write-Host 'fake UAT: keystore'; exit 5 }
# what UEDeployAndroid leaves behind: the copied keystore and the generated gradle.properties with the passwords
$g = Join-Path $u 'Intermediate/Android/arm64/gradle'
New-Item -ItemType Directory -Force -Path $g | Out-Null
Set-Content (Join-Path $g 'gradle.properties') 'STORE_PASSWORD=...'
$d = Join-Path (Arg 'archivedirectory') 'Android_ASTC'
New-Item -ItemType Directory -Force -Path $d | Out-Null
Set-Content (Join-Path $d 'Abyssfire.aab') 'aab'
# like UE: bundle mode makes an .apk only through bundletool's universal APK
if ($ini.Contains('bEnableUniversalAPK=True')) { Set-Content (Join-Path $d 'Abyssfire-arm64-universal.apk') 'apk' }
Write-FakePakList -Project (Arg 'project') -Drop ''
exit 0
'@) | Set-Content $fake
# keytool: the real one when a JDK is installed, else a stub (the ps image has no Java)
$fakeJdk = $null
if (-not (Get-Command keytool -ErrorAction SilentlyContinue) -and -not $IsWindows) {
    $fakeJdk = Join-Path $WorkDir 'fake-jdk'
    New-Item -ItemType Directory -Force -Path (Join-Path $fakeJdk 'bin') | Out-Null
    $stub = Join-Path $fakeJdk 'bin/keytool'
    Set-Content $stub "#!/bin/sh`nwhile [ `$# -gt 0 ]; do [ `"`$1`" = -keystore ] && out=`"`$2`"; shift; done`n[ -n `"`$AF__CI_PASS`" ] || exit 3`necho KS >`"`$out`"`n"
    & chmod +x $stub
    $env:JAVA_HOME = $fakeJdk
}
$env:AF_ANDROID_KEYSTORE_B64 = 'S0VZU1RPUkU='; $env:AF_ANDROID_KEYSTORE_PASSWORD = 'store-pw'   # must never reach UE
$env:CI_COMMIT_TAG = 'v1.2.3'; Remove-Item Env:AF_VERSION -ErrorAction SilentlyContinue
& (Join-Path $u 'CI/windows/ue-package.ps1') -Platform Android
$code = $LASTEXITCODE
$ao = Join-Path $u 'CI/out/package/android'
Assert-All @(
    @{ ok = ($code -eq 0); what = "exit code 0 (got $code)" },
    @{ ok = (Test-Path (Join-Path $ao 'Abyssfire.apk')); what = 'Abyssfire.apk (universal APK)' },
    @{ ok = (Test-Path (Join-Path $ao 'Abyssfire.aab')); what = 'Abyssfire.aab' },
    @{ ok = ((Get-Content -Raw (Join-Path $ao 'package-info.txt')) -match 'distribution: yes'); what = 'distribution build' },
    @{ ok = ((Get-Content -Raw $aini) -eq $aBefore); what = 'AndroidEngine.ini restored' },
    @{ ok = (-not (Test-Path (Join-Path $u 'Build/Android/abyssfire-ci-throwaway.keystore'))); what = 'throw-away keystore removed' },
    @{ ok = (-not (Test-Path (Join-Path $u 'Intermediate/Android'))); what = 'Intermediate/Android removed' }
)
foreach ($n in @('AF_ANDROID_KEYSTORE_B64', 'AF_ANDROID_KEYSTORE_PASSWORD', 'AF_WIN_CODESIGN_PFX_B64')) { Remove-Item "Env:$n" -ErrorAction SilentlyContinue }
if ($fakeJdk) { Remove-Item Env:JAVA_HOME }

Write-Host '=== 4. Get-AfVersion matches version.sh rules'
. (Join-Path $ci 'windows/AfCommon.ps1')
$cases = @(
    @{ tag = 'v1.2.3'; iid = '77'; v = '1.2.3'; q = '1.2.3.77'; c = '10200399'; m = '1.2.399' },
    @{ tag = 'v1.2.3-rc.4'; iid = '5'; v = '1.2.3-rc.4'; q = '1.2.3.5'; c = '10200304'; m = '1.2.304' },
    @{ tag = ''; iid = '133'; v = '0.0.0-abcdef12'; q = '0.0.0.133'; c = '133'; m = '0.0.0' }
)
foreach ($t in $cases) {
    Remove-Item Env:AF_VERSION -ErrorAction SilentlyContinue
    Remove-Item Env:AF_MSI_VERSION -ErrorAction SilentlyContinue
    $env:CI_COMMIT_TAG = $t.tag; $env:CI_PIPELINE_IID = $t.iid
    $r = Get-AfVersion
    if ($r.Version -eq $t.v -and $r.Quad -eq $t.q -and $r.AndroidCode -eq $t.c -and $r.MsiVersion -eq $t.m) {
        Write-Host "  ok   '$($t.tag)' -> $($r.Version) $($r.Quad) $($r.AndroidCode) MSI $($r.MsiVersion)"
    } else { $failures++; Write-Host "  FAIL '$($t.tag)' -> $($r.Version) $($r.Quad) $($r.AndroidCode) MSI $($r.MsiVersion)" }
}

Remove-Item -Recurse -Force $WorkDir
if ($failures) { Write-Host "FAILED: $failures"; exit 1 }
Write-Host 'OK'
