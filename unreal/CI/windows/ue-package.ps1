#Requires -Version 5.1
<#
.SYNOPSIS
    CI jobs `package:win64` and `package:android-win` (self-hosted Windows runner, PowerShell executor):
    RunUAT BuildCookRun.

.DESCRIPTION
    -Platform Win64 (default)
        Shipping build, cook, stage, pak (IoStore), package, archive, UE prerequisites installer (-prereqs).
        Output: unreal/CI/out/package/win64/Windows/   staged build, input of the Linux `installer:windows` job
                unreal/CI/out/package/win64/symbols/   this build's PDB, zipped (only when AF_SYMBOLS_DIR is not set;
                                                       with it the PDB goes to that runner folder, not the artifact)
    -Platform Android
        Only when UE_ANDROID_RUNNER_TAG points at a Windows runner (UE_ANDROID_RUNNER_SHELL=pwsh). ASTC, arm64,
        .aab + universal .apk, release Gradle build signed with a throw-away per-job key (same contract as
        unreal/CI/scripts/ue-package.sh: installer:android applies the upload key).
        Output: unreal/CI/out/package/android/Abyssfire.apk, Abyssfire.aab

    Input: unreal/Content from the `ue:content` job (artifact); unreal/CI/out/installer-assets (icons, optional).
    Logs:  unreal/CI/out/logs/<job>/ (UAT / UBT / cook). After BuildCookRun every runtime Data/ + Fonts/ file must
    be in the pak (pak lists PakList_*.txt that UAT writes to uebp_LogFolder).

    Variables (unreal/Docs/CI.md):
      UE_ROOT_WIN          engine root (default C:\Program Files\Epic Games\UE_5.8)
      AF_UE_CLIENT_CONFIG  Shipping (default) | Development
      AF_PERSISTENT_DDC    persistent local DerivedDataCache directory on the runner (UE-LocalDataCachePath);
                           mandatory in CI (runner config.toml)
      AF_ZEN_DATA_DIR      optional Zen storage data directory (UE-ZenDataPath) [Verify on 5.8.3]
      AF_SYMBOLS_DIR       runner folder that keeps the PDBs (recommended); unset = zipped PDB in the artifact
      AF_CLEAN_BUILD       1 = delete Binaries / Intermediate / Saved first (automatic when the engine or MSVC changed)
      AF_ANDROID_DISTRIBUTION  auto (tags and protected refs in CI) | 1 | 0 (debug build)
      AF_UE_EXTRA_ARGS     extra BuildCookRun arguments
      AF_UAT_COMMAND       testing hook: a script that stands in for RunUAT.bat (never set in real pipelines)
#>
[CmdletBinding()]
param(
    [ValidateSet('Win64', 'Android')]
    [string]$Platform = 'Win64'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AfCommon.ps1')

function Test-AfSigningRequired {
    $v = $env:AF_REQUIRE_SIGNING
    if (-not $v -or $v -eq 'auto') { return [bool]$env:CI_COMMIT_TAG }
    return (Test-AfTrue $v)
}
function Test-AfAndroidDistribution {
    $v = $env:AF_ANDROID_DISTRIBUTION
    if (-not $v -or $v -eq 'auto') { return ((Test-AfSigningRequired) -or ($env:CI_COMMIT_REF_PROTECTED -eq 'true')) }
    return (Test-AfTrue $v)
}
function Get-AfNewest([string[]]$Roots, [string]$Filter, [datetime]$Since) {
    $all = foreach ($r in $Roots) {
        if (Test-Path -LiteralPath $r) {
            Get-ChildItem -Recurse -File -Path $r -Filter $Filter -ErrorAction SilentlyContinue |
                Where-Object { $_.LastWriteTime -ge $Since }
        }
    }
    return ($all | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
}

# Incremental builds reuse Binaries / Intermediate on the persistent runner; after an engine hotfix or a new MSVC their
# PCHs / UHT output are stale (ue58-platform.md 12.1). The stamp records what built them; a change wipes them.
function Update-AfToolchainStamp([string]$UeRoot) {
    $ErrorActionPreference = 'Continue'
    $parts = @()
    $bv = Join-Path $UeRoot 'Engine/Build/Build.version'
    if (Test-Path -LiteralPath $bv) { $parts += (Get-Content -Raw -LiteralPath $bv) }
    if (${env:ProgramFiles(x86)}) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $vs = & $vswhere -latest -products * -property installationPath 2>$null | Select-Object -First 1
            $msvc = $null
            if ($vs -and (Test-Path -LiteralPath (Join-Path $vs 'VC\Tools\MSVC'))) {
                $msvc = Get-ChildItem -Directory -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') |
                    Sort-Object { try { [version]$_.Name } catch { [version]'0.0' } } | Select-Object -Last 1
            }
            $parts += "vs=$vs msvc=$(if ($msvc) { $msvc.Name } else { '?' })"
        }
    }
    $parts += "host=Win64 ndk=$($env:NDKROOT)"
    $stamp = $parts -join "`n"
    $stampFile = Join-Path $script:AfUnrealDir 'Intermediate/.af-toolchain-stamp'
    $clean = Test-AfTrue $env:AF_CLEAN_BUILD
    if (-not $clean -and (Test-Path -LiteralPath $stampFile)) {
        $clean = ([System.IO.File]::ReadAllText($stampFile) -ne $stamp)
    }
    if ($clean) {
        Write-AfWarn 'engine / toolchain changed (or AF_CLEAN_BUILD=1): clean build, deleting Binaries, Intermediate, Saved'
        foreach ($d in @('Binaries', 'Intermediate', 'Saved')) {
            Remove-Item -Recurse -Force -LiteralPath (Join-Path $script:AfUnrealDir $d) -ErrorAction SilentlyContinue
        }
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $stampFile) | Out-Null
    [System.IO.File]::WriteAllText($stampFile, $stamp, (New-Object System.Text.UTF8Encoding($false)))
}

# Runtime files staged by Abyssfire.Build.cs RuntimeDependencies (Data/..., Fonts/...): the data tables and fonts.
function Get-AfRuntimeDependencyFiles {
    $ErrorActionPreference = 'Continue'
    $files = @()
    $top = $null
    if (Get-Command git -ErrorAction SilentlyContinue) { $top = & git -C $script:AfUnrealDir rev-parse --show-toplevel 2>$null }
    $inRepo = $false
    if ($LASTEXITCODE -eq 0 -and $top) {
        $inRepo = ((Resolve-Path -LiteralPath $top).Path.TrimEnd('\', '/') -eq (Resolve-Path -LiteralPath $script:AfRepoDir).Path.TrimEnd('\', '/'))
    }
    if ($inRepo) {
        $files = @(& git -C $script:AfUnrealDir ls-files -- Data Fonts)
    } else {
        $root = (Resolve-Path -LiteralPath $script:AfUnrealDir).Path
        foreach ($d in @('Data', 'Fonts')) {
            $p = Join-Path $root $d
            if (Test-Path -LiteralPath $p) {
                $files += @(Get-ChildItem -Recurse -File -LiteralPath $p |
                    ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') })
            }
        }
    }
    return @($files | Where-Object { $_ -match '\.(json|ttf|otf|ttc)$' } | Sort-Object)
}

# Every runtime Data/ + Fonts/ file must be in the pak (Shipping only logs a missing data file, ue58-platform.md 10.2).
function Test-AfRuntimeDependencies([string]$UeRoot, [string[]]$PakSearch) {
    $lists = @(Get-ChildItem -Recurse -File -Path $env:uebp_LogFolder -Filter 'PakList_*.txt' -ErrorAction SilentlyContinue)
    if ($lists.Count -eq 0) {
        $unrealPak = Join-Path $UeRoot 'Engine/Binaries/Win64/UnrealPak.exe'
        $pak = foreach ($r in $PakSearch) {
            if (Test-Path -LiteralPath $r) { Get-ChildItem -Recurse -File -Path $r -Filter 'pakchunk0*.pak' -ErrorAction SilentlyContinue }
        }
        $pak = $pak | Select-Object -First 1
        if (-not $pak -or -not (Test-Path -LiteralPath $unrealPak)) {
            throw "no PakList_*.txt in $($env:uebp_LogFolder) and no pakchunk0*.pak to list: cannot verify the staged Data/ + Fonts/"
        }
        Invoke-AfNative -Exe $unrealPak -Arguments @($pak.FullName, '-List') -LogName 'UnrealPak-List'
        $lists = @(Get-Item -LiteralPath (Join-Path $script:AfLogDir 'UnrealPak-List.console.log'))
    }
    $files = @(Get-AfRuntimeDependencyFiles)
    if ($files.Count -eq 0) { throw "no Data/*.json or Fonts/ files under $($script:AfUnrealDir) (RuntimeDependencies)" }
    $missing = @($files | Where-Object {
            -not (Select-String -LiteralPath $lists.FullName -SimpleMatch -Pattern "Abyssfire/$_`"" -Quiet)
        })
    foreach ($m in $missing) { Write-AfWarn "not staged: $m" }
    if ($missing.Count -gt 0) {
        throw 'RuntimeDependencies (Data/, Fonts/) missing from the pak: check Abyssfire.Build.cs (ue58-platform.md 10.1); AF_CLEAN_BUILD=1 rebuilds a stale receipt'
    }
    Write-AfLog "pak holds all $($files.Count) runtime Data/ + Fonts/ files ($($lists.Count) pak list(s))"
}

$exitCode = 0
$tempFiles = New-Object System.Collections.ArrayList
try {
    # The engine (and its crash dumps) gets no signing secret: none is needed here (Android uses a throw-away key).
    Clear-AfSecrets
    $started = (Get-Date).AddSeconds(-5)
    $ueRoot = $env:UE_ROOT_WIN
    if (-not $ueRoot) { $ueRoot = 'C:\Program Files\Epic Games\UE_5.8' }
    $runUat = Join-Path $ueRoot 'Engine/Build/BatchFiles/RunUAT.bat'
    if ($env:AF_UAT_COMMAND) {
        $runUat = $env:AF_UAT_COMMAND
        Write-AfWarn "AF_UAT_COMMAND set: using $runUat instead of RunUAT.bat (test mode)"
    } elseif (-not (Test-Path -LiteralPath $runUat)) {
        throw "Unreal Engine not found: $runUat (set UE_ROOT_WIN, see unreal/Docs/CI.md)"
    }

    $buildVersion = Join-Path $ueRoot 'Engine/Build/Build.version'
    if (Test-Path -LiteralPath $buildVersion) {
        $bv = Get-Content -Raw -LiteralPath $buildVersion | ConvertFrom-Json
        Write-AfLog ("Unreal Engine {0}.{1}.{2} at {3}" -f $bv.MajorVersion, $bv.MinorVersion, $bv.PatchVersion, $ueRoot)
        if ("$($bv.MajorVersion).$($bv.MinorVersion)" -ne '5.8') { throw "the project pins UE 5.8 (DECISIONS P1)" }
        if ([int]$bv.PatchVersion -lt 3) { Write-AfWarn "UE 5.8.$($bv.PatchVersion) is older than the tested 5.8.3" }
    } elseif (-not $env:AF_UAT_COMMAND) {
        Write-AfWarn "$buildVersion not found; engine version not checked"
    }
    Update-AfToolchainStamp -UeRoot $ueRoot

    $v = Get-AfVersion
    Write-AfLog "version $($v.Version) (numeric $($v.Numeric), build $($v.Build))"

    $map = Join-Path $script:AfUnrealDir 'Content/Abyssfire/Maps/L_Main.umap'
    if (-not (Test-Path -LiteralPath $map)) { throw "unreal/Content is missing L_Main: this job needs the ue:content artifact" }

    Add-AfIniOverlay -Path (Join-Path $script:AfUnrealDir 'Config/DefaultGame.ini') `
        -Section '[/Script/EngineSettings.GeneralProjectSettings]' -Lines @("ProjectVersion=$($v.Version)")

    # DerivedDataCache: the runner's persistent directory (mandatory in CI: the DDC is not a GitLab cache).
    $ddc = $env:AF_PERSISTENT_DDC
    if (-not $ddc) {
        if ($env:GITLAB_CI) { throw 'AF_PERSISTENT_DDC is not set: give the runner a persistent DerivedDataCache directory in its config.toml environment (unreal/Docs/CI.md 3.2)' }
        $ddc = Join-Path $script:AfUnrealDir 'DerivedDataCache'
    }
    New-Item -ItemType Directory -Force -Path $ddc | Out-Null
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $ddc, 'Process')
    if ($env:AF_ZEN_DATA_DIR) { [Environment]::SetEnvironmentVariable('UE-ZenDataPath', $env:AF_ZEN_DATA_DIR, 'Process') }
    $env:uebp_LogFolder = Join-Path $script:AfLogDir 'uat'
    New-Item -ItemType Directory -Force -Path $env:uebp_LogFolder | Out-Null
    Write-AfLog "local DerivedDataCache: $ddc"

    $archive = Join-Path $script:AfOutDir "work/$Platform-archive"
    if (Test-Path -LiteralPath $archive) { Remove-Item -Recurse -Force -LiteralPath $archive }
    New-Item -ItemType Directory -Force -Path $archive | Out-Null

    $config = $env:AF_UE_CLIENT_CONFIG
    if (-not $config) { $config = 'Shipping' }
    $uatArgs = @(
        'BuildCookRun',
        "-project=$($script:AfUproject)",
        "-platform=$Platform",
        "-clientconfig=$config",
        '-build', '-cook', '-stage', '-pak', '-iostore', '-compressed', '-package', '-archive',
        "-archivedirectory=$archive",
        '-nop4', '-utf8output', '-unattended'
    )

    $dist = 'no'
    if ($Platform -eq 'Win64') {
        $uatArgs += @('-prereqs', '-nodebuginfo')
        # Generated app icon for the executable when the project commits none (unreal/Build/Windows/Application.ico).
        $projectIcon = Join-Path $script:AfUnrealDir 'Build/Windows/Application.ico'
        $generatedIcon = Join-Path $script:AfOutDir 'installer-assets/ue/Windows/Application.ico'
        if (-not (Test-Path -LiteralPath $projectIcon) -and (Test-Path -LiteralPath $generatedIcon)) {
            Add-AfTempFile -Source $generatedIcon -Destination $projectIcon
            Write-AfLog 'exe icon: generated Application.ico (installer-assets)'
        }
    } else {
        $flavor = $env:AF_ANDROID_COOK_FLAVOR
        if (-not $flavor) { $flavor = 'ASTC' }
        $uatArgs += "-cookflavor=$flavor"
        $androidIni = Join-Path $script:AfUnrealDir 'Config/Android/AndroidEngine.ini'
        $section = '[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]'
        # Bundle mode builds only the .aab; the .apk is bundletool's universal APK (bEnableUniversalAPK=True).
        Add-AfIniOverlay -Path $androidIni -Section $section `
            -Lines @("StoreVersion=$($v.AndroidCode)", "VersionDisplayName=$($v.Version)", 'bEnableBundle=True', 'bEnableUniversalAPK=True')
        if (Test-AfAndroidDistribution) {
            # -distribution = release (non-debuggable) Gradle build; its keystore is a throw-away one, never the upload key.
            $keytool = 'keytool'
            if ($env:JAVA_HOME) {
                foreach ($k in @('bin/keytool.exe', 'bin/keytool')) {
                    $p = Join-Path $env:JAVA_HOME $k
                    if (Test-Path -LiteralPath $p) { $keytool = $p; break }
                }
            }
            $ksName = 'abyssfire-ci-throwaway.keystore'
            $ksPath = Join-Path $script:AfUnrealDir "Build/Android/$ksName"
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ksPath) | Out-Null
            Remove-Item -LiteralPath $ksPath -Force -ErrorAction SilentlyContinue
            [void]$tempFiles.Add($ksPath)
            $pass = [guid]::NewGuid().ToString('N')
            $env:AF__CI_PASS = $pass
            try {
                Invoke-AfNative -Exe $keytool -LogName 'keytool' -Arguments @('-genkeypair', '-keystore', $ksPath,
                    '-storetype', 'PKCS12', '-storepass:env', 'AF__CI_PASS', '-keypass:env', 'AF__CI_PASS', '-alias', 'ci',
                    '-keyalg', 'RSA', '-keysize', '2048', '-validity', '2', '-dname', 'CN=Abyssfire CI throw-away')
            } finally { Remove-Item Env:AF__CI_PASS -ErrorAction SilentlyContinue }
            Add-AfIniOverlay -Path $androidIni -Section $section -Lines @(
                "KeyStore=$ksName", 'KeyAlias=ci', "KeyStorePassword=$pass", "KeyPassword=$pass")
            $uatArgs += '-distribution'
            $dist = 'yes'
            Write-AfLog 'Android: distribution build, signed with a throw-away key (installer:android applies the upload key)'
        } else {
            Write-AfWarn 'Android: development build (debug key, debuggable); AF_ANDROID_DISTRIBUTION=1 for a release build'
        }
        $res = Join-Path $script:AfUnrealDir 'Build/Android/res'
        $generatedRes = Join-Path $script:AfOutDir 'installer-assets/ue/Android/res'
        if (-not (Test-Path -LiteralPath $res) -and (Test-Path -LiteralPath $generatedRes)) {
            Copy-Item -Recurse -LiteralPath $generatedRes -Destination $res
            [void]$tempFiles.Add($res)
            Write-AfLog 'Android: generated launcher icons'
        }
    }
    if ($env:AF_UE_EXTRA_ARGS) { $uatArgs += ($env:AF_UE_EXTRA_ARGS -split '\s+' | Where-Object { $_ }) }

    Start-AfSection 'bcr' "BuildCookRun $Platform $config"
    Invoke-AfNative -Exe $runUat -Arguments $uatArgs -LogName "RunUAT-$Platform"
    Stop-AfSection 'bcr'
    Test-AfRuntimeDependencies -UeRoot $ueRoot -PakSearch @($archive, (Join-Path $script:AfUnrealDir 'Saved/StagedBuilds'))

    if ($Platform -eq 'Win64') {
        # UE 5 archives the Windows build under <archive>\Windows (UE 4: WindowsNoEditor).
        $staged = $null
        foreach ($name in @('Windows', 'WindowsNoEditor', 'Win64')) {
            $p = Join-Path $archive $name
            if (Test-Path -LiteralPath (Join-Path $p 'Abyssfire.exe')) { $staged = $p; break }
        }
        if (-not $staged) { throw "no Abyssfire.exe under $archive (Windows / WindowsNoEditor / Win64)" }
        $paks = Get-ChildItem -Path (Join-Path $staged 'Abyssfire/Content/Paks') -Filter '*.pak' -ErrorAction SilentlyContinue
        if (-not $paks) { throw "no .pak in $staged/Abyssfire/Content/Paks" }

        $dst = Join-Path $script:AfOutDir 'package/win64'
        if (Test-Path -LiteralPath $dst) { Remove-Item -Recurse -Force -LiteralPath $dst }
        New-Item -ItemType Directory -Force -Path $dst | Out-Null
        Move-Item -LiteralPath $staged -Destination (Join-Path $dst 'Windows')
        # Symbols: only this build's PDB (Binaries survives between jobs: older PDBs of other configs stay there), kept on
        # the runner (AF_SYMBOLS_DIR) or zipped into the artifact (a monolithic Shipping PDB is 0.5-1.5 GB).
        $symbols = Join-Path $dst 'symbols'
        New-Item -ItemType Directory -Force -Path $symbols | Out-Null
        $pdbName = if ($config -eq 'Development') { 'Abyssfire.pdb' } else { "Abyssfire-Win64-$config.pdb" }
        $pdb = Get-Item -LiteralPath (Join-Path $script:AfUnrealDir "Binaries/Win64/$pdbName") -ErrorAction SilentlyContinue
        $pdbInfo = 'none'
        if ($pdb -and $pdb.LastWriteTime -ge $started) {
            if ($env:AF_SYMBOLS_DIR) {
                New-Item -ItemType Directory -Force -Path $env:AF_SYMBOLS_DIR | Out-Null
                $kept = Join-Path $env:AF_SYMBOLS_DIR "$($v.Version)-$pdbName"
                Copy-Item -LiteralPath $pdb.FullName -Destination $kept -Force
                $pdbInfo = "kept on the runner: $kept"
            } else {
                Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
                $zipPath = Join-Path $symbols "Abyssfire-$($v.Version)-win64-pdb.zip"
                $zip = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
                try {
                    [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $pdb.FullName, $pdb.Name,
                        [System.IO.Compression.CompressionLevel]::Optimal)
                } finally { $zip.Dispose() }
                $pdbInfo = "artifact: symbols/$(Split-Path -Leaf $zipPath)"
            }
        } else {
            Write-AfWarn "no $pdbName written by this build: no symbols kept"
        }
        $files = Get-ChildItem -Recurse -File -Path (Join-Path $dst 'Windows')
        $size = ($files | Measure-Object -Sum Length).Sum
        $info = @("version: $($v.Version)", "config: $config", "files: $($files.Count)", ("size: {0:N1} MiB" -f ($size / 1MB)),
            "symbols: $pdbInfo")
    } else {
        $roots = @($archive, (Join-Path $script:AfUnrealDir 'Binaries/Android'), (Join-Path $script:AfUnrealDir 'Intermediate/Android'))
        $apk = Get-AfNewest -Roots $roots -Filter '*.apk' -Since $started
        $aab = Get-AfNewest -Roots $roots -Filter '*.aab' -Since $started
        if (-not $apk) { throw "no .apk produced (archive $archive): the universal APK needs bEnableUniversalAPK=True (overlaid by this script; see logs/<job>/uat for bundletool errors)" }
        if (-not $aab) { throw 'no .aab produced: check bEnableBundle=True (DefaultEngine.ini, overlaid by this script) and the Gradle log' }
        $dst = Join-Path $script:AfOutDir 'package/android'
        if (Test-Path -LiteralPath $dst) { Remove-Item -Recurse -Force -LiteralPath $dst }
        New-Item -ItemType Directory -Force -Path $dst | Out-Null
        Copy-Item -LiteralPath $apk.FullName -Destination (Join-Path $dst 'Abyssfire.apk')
        Copy-Item -LiteralPath $aab.FullName -Destination (Join-Path $dst 'Abyssfire.aab')
        $info = @("apk: $($apk.Name)", "aab: $($aab.Name)", "version: $($v.Version)", "versionCode: $($v.AndroidCode)",
            "distribution: $dist")
    }
    Set-Content -LiteralPath (Join-Path $dst 'package-info.txt') -Value $info -Encoding ASCII
    $info | ForEach-Object { Write-AfLog $_ }
} catch {
    Write-Host "[af] ERROR: $_" -ForegroundColor Red
    $exitCode = 1
} finally {
    Restore-AfFiles
    foreach ($t in $tempFiles) { Remove-Item -Recurse -Force -LiteralPath $t -ErrorAction SilentlyContinue }
    if ($Platform -eq 'Android') {
        # UEDeployAndroid copies Build/Android (keystore included) and writes the signing passwords into the generated
        # gradle.properties under Intermediate/Android: never leave them on the persistent runner.
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -LiteralPath (Join-Path $script:AfUnrealDir 'Intermediate/Android')
    }
    $logs = Join-Path $script:AfLogDir 'project'
    $saved = Join-Path $script:AfUnrealDir 'Saved/Logs'
    if (Test-Path -LiteralPath $saved) {
        New-Item -ItemType Directory -Force -Path $logs | Out-Null
        Copy-Item -Recurse -Force -Path (Join-Path $saved '*') -Destination $logs -ErrorAction SilentlyContinue
    }
}
exit $exitCode
