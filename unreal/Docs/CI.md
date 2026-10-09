# Abyssfire CI/CD（GitLab）— 多平台安装包流水线

> 中文为主，关键术语保留英文。Pipeline definition: [`/.gitlab-ci.yml`](../../.gitlab-ci.yml) · scripts: [`unreal/CI/`](../CI/) ·
> 平台背景：`Docs/spec/ue58-platform.md` §3（工具链）/ §12（打包），`Docs/DECISIONS.md` P1–P12。
> 原 Web 版仍由 `.github/workflows/deploy.yml` 部署到 GitHub Pages，本流水线不涉及它。

产物：**macOS `.dmg`**（Apple Silicon，Developer ID 签名 + 公证 + staple，带背景与 Applications 快捷方式）、
**Windows `.exe`（NSIS）与 `.msi`（wixl）**（另附免安装 `.zip`）、**Android `.apk` + `.aab`**、可选手动 **iOS `.ipa`**。

---

## 1. 总览 Overview

```
verify ──────────── assets ─────────── content ───────── package ─────────────── installers ─────────── release
version             art:blender-smoke   ue:content        package:mac      ──►   installer:mac-dmg  ─┐
ci:lint, ci:selftest installer-assets   (Mac runner,      package:win64    ──►   installer:windows  ─┼► release (tag)
core:gcc / clang                         build_content.py) package:android ──►   installer:android  ─┘
core:gcc-sanitize                                          package:ios (manual) ───────────────────────► release:ios (manual)
core:clang-shared                                          package-all (manual child-pipeline trigger)
data:check
web:vitest (optional)
```

| 流水线 Pipeline | 运行内容 |
|---|---|
| Merge request | `verify`；`unreal/Art/blender/**` 相对目标分支有改动时跑 `art:blender-smoke`（否则手动） |
| 普通分支 push | `verify`；`unreal/Art/blender/**` 有改动时跑 `art:blender-smoke`（否则手动） |
| 受保护分支 push（非默认，如 `release/*`） | 同上 + 手动按钮 `package-all` |
| 默认分支 push / 定时（schedule） | 全部（content → package → installers），不发布 |
| 受保护 Tag `vX.Y.Z` / `vX.Y.Z-rc.N` | 全部（工作副本完全清理后构建）+ `release`（上传 Package Registry + 创建 GitLab Release）；iOS 为手动 |
| 其他受保护 tag（如 `nightly-…`） | 全部，不发布 |
| `package-all`（受保护分支上手动） | 以 `AF_PACKAGE_ALL=1` 运行同一份 `.gitlab-ci.yml` 的子流水线：content + 全部 package + installers，不重复 verify，不发布 |
| 受保护分支上 Run pipeline + 变量 `AF_PACKAGE_ALL=1` | 同上（需要项目允许 pipeline variables，见 3.4） |
| 已开 MR 的分支 push | 被 `workflow` 去重，只跑 MR 流水线 |

**UE 作业只在受保护的 ref 上创建**（`$CI_COMMIT_REF_PROTECTED == "true"`）：自建 UE runner 必须是 *Protected*（3.1），
它不接非保护 ref 的作业；这种作业会 pending 1 小时直到被判 stuck，且 pending 期间一直占着 `resource_group`
（`ue-<tag>`），所有 main / tag 的 UE 作业都得排在它后面。因此 MR、普通分支、非保护 tag 上没有任何 UE 作业，也没有 `package-all`。

**为什么 UE 作业必须自建 runner**：Epic EULA 不允许把引擎打进公共镜像，GitLab SaaS runner 上没有 UE。
所有调用引擎的作业（`ue:content`、`package:*`、`installer:mac-dmg`）通过 tag 选择自建 runner；
其余无引擎作业（核心测试、数据校验、Blender 冒烟、Windows 安装包、Android 签名、发布）在 GitLab SaaS Linux runner 的 Docker 镜像里运行。

| Job | Runner | 说明 | 超时 |
|---|---|---|---|
| `version` | SaaS `bash:5.2` | `unreal/CI/version.sh` → dotenv（`AF_VERSION` 等），后续作业通过 `needs` 继承 | 5m |
| `ci:lint` | SaaS `ubuntu:24.04` | `check_gitlab_ci.py`（流水线结构 + 规则模拟）+ shellcheck | 10m |
| `ci:selftest` | SaaS `ubuntu:24.04` | 仅 `.gitlab-ci.yml` / `unreal/CI/**` 有改动时：真实构建 NSIS + MSI（含自签名证书签名）、APK/AAB 签名、用桩工具跑 Mac 作业脚本 | 20m |
| `ci:selftest-pwsh` | SaaS `mcr.microsoft.com/powershell` | 同上条件：PowerShell 脚本解析 + 假 RunUAT 端到端（Win64 / Android） | 10m |
| `core:gcc` / `core:clang` | SaaS `ubuntu:24.04` | `CoreTests/run.sh gcc|clang`，JUnit 报告 | 45m |
| `core:gcc-sanitize` | SaaS | `ABYSS_SANITIZE=1`（ASan + UBSan + float-cast-overflow） | 60m |
| `core:clang-shared` | SaaS | `ABYSS_SHARED=1`（隐藏可见性共享库：缺 `ABYSS_API` 即链接失败） | 45m |
| `data:check` | SaaS `node:22` | `npm ci` + `export-data/run.mjs --check`（`unreal/Data` 必须与 TS 源一致） | 20m |
| `web:vitest` | SaaS `node:22` | 原 Web 版 Vitest，`allow_failure`；源码改动时自动，否则手动 | 30m |
| `art:blender-smoke` | SaaS `python:3.11` + `bpy==5.0.*` | `Art/blender/tests/smoke_test.py`，预览图为 artifact | 30m |
| `installer-assets` | SaaS `python:3.12` + Pillow | 图标（.ico/.icns/png）、DMG 背景、NSIS 位图 | 10m |
| `ue:content` | **Mac** | 编译 `AbyssfireEditor`，无头运行 `Scripts/build_content.py` → `unreal/Content/`（artifact，后续所有 package 复用） | 3h |
| `package:mac` | **Mac** | BuildCookRun Mac Shipping arm64 → `Abyssfire.app.zip` | 4h |
| `package:win64` | **Windows** | BuildCookRun Win64 Shipping `-prereqs` → staged `Windows/` 目录（PDB 留在 runner 的 `AF_SYMBOLS_DIR`，或压缩进 artifact） | 4h |
| `package:android` | **Mac**（默认，可改） | BuildCookRun Android ASTC arm64 `-package` → `.aab` + 通用 `.apk`（bundletool），一次性 CI 密钥签名的 release 构建 | 4h |
| `package:android-win` | **Windows** | 同上的 PowerShell 版（`UE_ANDROID_RUNNER_SHELL=pwsh` 时替代上一个） | 4h |
| `package:ios` | **Mac**，手动 | BuildCookRun IOS `-distribution`，分发证书 + 描述文件 → `.ipa` | 4h |
| `installer:mac-dmg` | **Mac** | codesign（hardened runtime）→ notarytool → staple → hdiutil 样式化 DMG → 签名/公证/staple DMG（两次公证各最多 `AF_NOTARY_TIMEOUT` = 1h） | 3h |
| `installer:windows` | SaaS `ubuntu:24.04` | makensis（.exe）+ wixl（.msi）+ zip，可选 osslsigncode 签名 | 1h |
| `installer:android` | SaaS `eclipse-temurin:21` | build-tools 36：`zipalign -P 16` + `apksigner`；AAB 用 `jarsigner`。**唯一拿到上传密钥的作业**（environment `android-signing`） | 1h |
| `release` | SaaS `registry.gitlab.com/gitlab-org/cli`（glab） | 仅受保护的版本 tag：上传 generic package registry，`glab release create`（每个文件一个链接 + SHA256SUMS；`CI_JOB_TOKEN` 认证） | 1h |
| `release:ios` | SaaS，手动 | 把 `.ipa` 上传并追加到已有 Release（标注为 App Store Connect 上传包，不能直接安装） | 30m |

通用策略：所有作业 `interruptible: true`（发布作业除外；`package-all` 触发作业也是，否则新的 push 取消不了它的子流水线），
新提交会取消旧流水线（`auto_cancel: on_new_commit: interruptible`）；`retry` 仅针对 `runner_system_failure`（最多 2 次）；
所有 artifact 都有 `expire_in`；自建 runner 上的作业用 `resource_group: ue-<runner tag>` 串行化（同一台机器同一时间只跑一个 UE 作业，也保护钥匙串）。
签名作业用 `environment: {name: <x>-signing, action: prepare}`（不产生部署记录），密钥变量的 environment scope 设为对应名字，
其它作业（包括在同一台 Mac 上跑引擎的作业）拿不到这些密钥（4.2）。

---

## 2. 产物 Artifacts

| 作业 | 路径（相对仓库根） | 保留 |
|---|---|---|
| `ue:content` | `unreal/Content/`、`unreal/CI/out/content/{report.json,build_content.log,summary.txt}` | 3 天 |
| `package:mac` | `unreal/CI/out/package/mac/Abyssfire.app.zip`（未签名，ditto 打包保留符号链接） | 1 周 |
| `package:win64` | `unreal/CI/out/package/win64/Windows/`（staged 构建）；未设 `AF_SYMBOLS_DIR` 时另有 `…/symbols/Abyssfire-<ver>-win64-pdb.zip`（只含本次构建的 PDB） | 1 周 |
| `package:android` | `unreal/CI/out/package/android/Abyssfire.{apk,aab}`（一次性 CI 密钥签名，未用于分发）、`package-info.txt` | 1 周 |
| `package:ios` | `unreal/CI/out/package/ios/Abyssfire-<ver>-ios.ipa` | 4 周 |
| `installer:mac-dmg` | `unreal/CI/out/installers/mac/Abyssfire-<ver>-macos-arm64.dmg` + `SHA256SUMS-mac.txt` | 4 周 |
| `installer:windows` | `…/installers/windows/Abyssfire-<ver>-win64-setup.exe`、`-win64.msi`、`-win64-portable.zip`、`SHA256SUMS-windows.txt`、`SIGNING-windows.txt`（`signed=0/1`） | 4 周 |
| `installer:android` | `…/installers/android/Abyssfire-<ver>-android-arm64.apk`、`-android.aab`、`apk-certs.txt` | 4 周 |
| `release` | Package Registry：`abyssfire/<ver>/…`（永久）、`release-notes.md` | 永久 / 4 周 |
| 所有 UE 作业 | `unreal/CI/out/logs/<job>/`（UAT/UBT/cook 日志、pak 清单 `uat/PakList_*.txt`、notarytool 日志、崩溃报告；引擎进程不继承任何签名密钥） | 同作业 |

安装包行为：

* **DMG**：窗口 660×420，渊火图标在左、Applications 链接在右、背景图（Retina 双分辨率 TIFF），卷图标；
  App 与 DMG 都签名 + 公证 + staple，离线也能通过 Gatekeeper。Info.plist 的 `CFBundleShortVersionString` / `CFBundleVersion` 由 CI 写入。
* **setup.exe（NSIS 3, MUI2）**：简体中文 / English（随系统语言），安装到 `Program Files\Abyssfire`，开始菜单（游戏 + 卸载），
  可选桌面快捷方式，可选 UE 运行库（VC++/DirectX），控制面板“程序和功能”条目，升级时先静默卸载旧版（沿用旧安装目录，`/D=` 显式指定时以 `/D` 为准）；
  卸载器逐个删除安装过的文件（从不 `RMDir /r` 用户选择的目录），存档在 `%LOCALAPPDATA%` 不受影响。
  完成页“启动渊火”经 `explorer.exe` 以当前用户（非管理员）权限启动游戏。
  静默安装：`setup.exe /S [/D=D:\Games\Abyssfire]`。NSIS 单文件上限 2 GB（超过时只发布 .msi / .zip）。
* **setup.exe 与 .msi 互斥**：两者装到同一目录和开始菜单，各自登记卸载项。setup.exe 用 MSI 的 UpgradeCode（`MsiEnumRelatedProductsW`）
  检测到 MSI 安装就提示先卸载并退出；MSI 用 `LaunchCondition` 检测 setup.exe 的卸载项（`Uninstall\Abyssfire`）同样拒绝。
  setup.exe 自己的注册表键是 `HKLM\Software\feuvan\Abyssfire\Setup`（MSI 的组件 KeyPath 在上一级）。
* **MSI（wixl，WiX 3 子集）**：固定 `UpgradeCode`、每次构建新 `ProductCode` + `MajorUpgrade`（同版本也可覆盖；版本号见第 5 节 `AF_MSI_VERSION`），
  开始菜单（游戏 + “Uninstall Abyssfire” = `msiexec /x [ProductCode]`），桌面快捷方式为独立 feature
  （`msiexec /i x.msi ADDLOCAL=Main` 可不装），ARP 图标；字符串为 ASCII（MSI 不支持可靠的 UTF-8 代码页）。
  静默：`msiexec /i Abyssfire-x.y.z-win64.msi /qn [INSTALLDIR=…]`。VC++ 运行库由 UE 引导程序 `Abyssfire.exe` 首次启动时安装。
* **APK**：UE 以 bundle 模式构建，`.apk` 是 bundletool 生成的通用 APK（`bEnableUniversalAPK=True`，CI 追加到 ini）；
  16 KB 页对齐（Play 对 Android 15+ 的要求），v2 + v3 签名；**AAB** 用上传密钥（upload key）签名，供 Play App Signing。
  两者在 `installer:android` 里重新签名：UE 打包时只用每个作业临时生成的一次性密钥（真正的上传密钥从不交给引擎、也不落到自建 runner 上）。
* **iOS `.ipa`**：App Store 分发描述文件签名，只能上传 App Store Connect / TestFlight，**不能直接装到设备上**（Release 链接也这样标注）。

---

## 3. Runner 准备

### 3.1 macOS runner（`ue5-mac`：内容构建、Mac / Android / iOS 打包、DMG）

硬件：Apple Silicon（M2 Pro 以上推荐），≥ 32 GB 内存，≥ 500 GB SSD 空闲（引擎 ~60 GB、DDC、Android NDK、每个 job 的构建目录）。

1. **系统**：macOS Sequoia 15.x（最低 Sonoma 14.5）。关闭睡眠：`sudo pmset -a sleep 0 disksleep 0`；为 runner 用户开启自动登录
   （签名需要已登录用户会话里的钥匙串；DMG 的 Finder 回退方案也需要图形会话）。
2. **Xcode 26.1.1**（26.0 最低，**26.4 不兼容 UE 5.8**）：从 developer.apple.com 下载 `.xip` 安装到 `/Applications/Xcode.app`，然后
   ```bash
   sudo xcode-select -s /Applications/Xcode.app && sudo xcodebuild -license accept && xcodebuild -runFirstLaunch
   xcodebuild -downloadComponent MetalToolchain   # Xcode 26 不再自带 Metal 工具链：没有它 cook 时 metal / metallib 失败
   xcodebuild -downloadPlatform iOS               # iOS 平台自 Xcode 15 起按需下载（package:ios 需要）
   ```
   UE 作业开头会检查 `xcrun -sdk macosx metal --version`（`package:ios` 还检查 `xcrun --sdk iphoneos --show-sdk-path`），缺失时立即失败。
   多个 Xcode 并存时可用变量 `AF_XCODE_DEVELOPER_DIR` 指定。Developer ID 中间证书（Developer ID Certification Authority G2）
   随 Xcode 安装；缺失时从 <https://www.apple.com/certificateauthority/> 导入 System 钥匙串。
3. **Unreal Engine 5.8.3**：Epic Games Launcher 安装到默认的 `/Users/Shared/Epic Games/UE_5.8`（与 `UE_ROOT_MAC` 默认值一致），
   勾选 *Mac*、*iOS*、*Android* 目标平台支持。源码版亦可（把 `UE_ROOT_MAC` 指向源码根目录，需先 `Setup.sh` + 编译）。
   CI 会读取 `Engine/Build/Build.version`，不是 5.8 则失败，低于 5.8.3 给出警告。
4. **Android 工具链**（Mac 上打 Android 包时）：Android Studio（或命令行工具）安装
   SDK Platform 36、Build-Tools 36.0.0、Platform-Tools、**NDK r27c（27.2.12479018）**、CMake；**JDK 21**（Temurin 21 或 Android Studio 自带 JBR 21）。
   然后跑一次 `"$UE_ROOT_MAC/Engine/Build/BatchFiles/RunUAT.sh" Turnkey -command=VerifySdk -platform=Android -UpdateIfNeeded`。
   环境变量写进 runner 的 `config.toml`（见下）：`ANDROID_HOME`、`ANDROID_SDK_ROOT`、`NDKROOT`、`NDK_ROOT`、`JAVA_HOME`。
5. **其它**：`git`（Xcode 自带即可）、`python3`（Xcode CLT 自带，3.9 即可）。DMG 样式脚本每个作业在 `unreal/CI/out/work/dmg/dmg-venv`
   新建 venv，按 `unreal/CI/installers/mac/dmg-requirements.txt` 以 `--require-hashes --only-binary :all:` 安装固定版本的
   `ds_store` / `mac_alias`（需要能访问 PyPI），并以 `env -i` + `python -I` 运行（看不到任何密钥）；不复用共享的 venv。
6. **gitlab-runner**（shell executor）：
   ```bash
   brew install gitlab-runner            # 或下载官方二进制到 /usr/local/bin/gitlab-runner
   # GitLab: Settings > CI/CD > Runners > New project runner：
   #   Tags: ue5-mac   Run untagged jobs: 关   Protected: 必须开（只跑受保护分支/tag）   Maximum job timeout: 6h
   gitlab-runner register --non-interactive --url https://gitlab.com --token glrt-xxxxxxxx \
     --executor shell --description "abyssfire-mac"
   gitlab-runner install && gitlab-runner start    # 以登录用户身份运行（LaunchAgent），不要用 sudo
   ```
   `~/.gitlab-runner/config.toml` 要点：
   ```toml
   concurrent = 1
   [[runners]]
     name = "abyssfire-mac"
     executor = "shell"
     shell = "bash"
     limit = 1
     builds_dir = "/Users/ci/gl/builds"     # 短路径
     cache_dir  = "/Users/ci/gl/cache"
     environment = [
       "ANDROID_HOME=/Users/ci/Library/Android/sdk",
       "ANDROID_SDK_ROOT=/Users/ci/Library/Android/sdk",
       "NDKROOT=/Users/ci/Library/Android/sdk/ndk/27.2.12479018",
       "NDK_ROOT=/Users/ci/Library/Android/sdk/ndk/27.2.12479018",
       "JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-21.jdk/Contents/Home",
       "AF_PERSISTENT_DDC=/Users/ci/gl/ddc",
     ]
   ```
   **Protected 是硬性要求，不是建议。** shell executor 的工作副本（`builds_dir`）、保留下来的 `unreal/Binaries`、`unreal/Intermediate`、
   DDC、用户目录（`~/Library`、UBT 的 `BuildConfiguration.xml`、Zen 数据）以及引擎安装目录对这台机器上的每个作业都是同一份、可写的。
   若非保护分支 / MR 的代码能在这台 runner 上运行，它就能植入被 main / tag 构建当作“最新”复用、随后被签名公证的 dylib / UBT makefile / DDC 条目，
   或读取前一个作业留下的东西。所以：runner 设为 Protected，流水线只在受保护 ref 上创建 UE 作业（第 1 节），
   同一台机器上**不要**再注册一个不受保护的 runner（只换 `builds_dir` 不够：引擎目录和用户目录仍然共享；要给分支打包就用另一台机器 / 另一个系统用户）。
   另外：release tag 的 UE 作业用 `GIT_CLEAN_FLAGS: -ffdx` 从完全干净的工作副本构建；引擎 / Xcode / NDK 变化时
   `af_ue_init` 自动清掉 `Binaries` / `Intermediate` / `Saved`（`Intermediate/.af-toolchain-stamp`，也可设 `AF_CLEAN_BUILD=1`）。

   **`AF_PERSISTENT_DDC` 必须设置**（CI 中未设置时 UE 作业直接失败）：本机常驻的 DerivedDataCache 目录，不放在工作副本里，
   也不是 GitLab `cache:`（那样每个作业都要解压、再把数 GB 打包上传，`git clean -ffdx` 还会先删掉它）。
   脚本把它作为环境变量 `UE-LocalDataCachePath` 传给每个引擎进程（名字带 `-`，bash 不能 `export`，用 `env` 传递，子进程原样继承）；
   不在 CI 中时默认 `unreal/DerivedDataCache`。UE 5.4 起本地 DDC 可能走 **Zen 存储服务（zenserver）**，其数据目录默认在
   runner 用户的用户目录下（同样跨作业保留，只此一个受保护 runner 使用）；要放到别处可设 `AF_ZEN_DATA_DIR`
   （脚本以 `UE-ZenDataPath` 传入，**[Verify]** 5.8.3 上的变量名，ue58-platform.md §17）。

### 3.2 Windows runner（`ue5-win`：Win64 打包）

1. **Windows 11 x64**，≥ 32 GB 内存，≥ 300 GB SSD；把构建目录和 DDC 加入 Defender 排除项。
2. **Visual Studio 2026（18.x）** + “使用 C++ 的游戏开发”工作负载，组件：**MSVC v14.50**（**不要 14.51**：`<hash_map>` 被移除，UE 5.8 编译失败）、
   **Windows SDK 10.0.26100**、.NET 10 SDK；建议同时装 VS 2022 17.14 的 v143 生成工具作为后备（ue58-platform.md §3.1）。
3. **Unreal Engine 5.8.3**：Epic Launcher 安装到 `C:\Program Files\Epic Games\UE_5.8`（= `UE_ROOT_WIN` 默认值），勾选 Windows 平台支持。
4. **Git for Windows**，并 `git config --system core.longpaths true`；**PowerShell 7**：`winget install Microsoft.PowerShell`
   （脚本也兼容 Windows PowerShell 5.1）。
5. **gitlab-runner**（shell executor，PowerShell）：
   ```powershell
   mkdir C:\GitLab-Runner; cd C:\GitLab-Runner
   Invoke-WebRequest https://gitlab-runner-downloads.s3.amazonaws.com/latest/binaries/gitlab-runner-windows-amd64.exe -OutFile gitlab-runner.exe
   # UI 里新建 runner：Tags ue5-win，Run untagged jobs 关，Protected 必须开（同 3.1），Maximum job timeout 6h
   .\gitlab-runner.exe register --non-interactive --url https://gitlab.com --token glrt-xxxxxxxx --executor shell --shell pwsh --description abyssfire-win
   .\gitlab-runner.exe install --user ".\ci" --password "<ci 用户密码>"   # 用普通用户而非 LocalSystem（用户目录下的 DDC / UE 配置）
   .\gitlab-runner.exe start
   ```
   `config.toml`：`concurrent = 1`、`shell = "pwsh"`、`builds_dir = "C:/gl/b"`（短路径，避免 260 字符限制）、`cache_dir = "C:/gl/c"`，
   `environment = ["AF_PERSISTENT_DDC=C:/gl/ddc", "AF_SYMBOLS_DIR=C:/gl/symbols"]`。
   `AF_PERSISTENT_DDC` 必须设置（同 3.1）。`AF_SYMBOLS_DIR`（推荐）：本次构建的 PDB（`Abyssfire-Win64-Shipping.pdb`，
   单体 Shipping PDB 常有 0.5–1.5 GB）复制到这里，命名 `<版本>-<pdb 名>`；不设时 PDB 压缩成 zip 放进 artifact（会占 artifact 配额，
   `installer:windows` 也要多下载）。引擎 / MSVC 版本变化时同样自动清理 `Binaries` / `Intermediate` / `Saved`。
6. （可选）若要在 Windows 上打 Android：装 Android SDK/NDK r27c/JDK 21，设置同名环境变量，
   并把 CI 变量 `UE_ANDROID_RUNNER_TAG=ue5-win`、`UE_ANDROID_RUNNER_SHELL=pwsh`。

### 3.3 可选：Linux UE runner（Android）

`unreal/CI/scripts/ue-package.sh Android` 也支持 Linux 主机（源码编译的 UE 5.8.3，`UE_ROOT_LINUX`，默认 `~/UnrealEngine`）。
注册一个 **Protected** shell runner（tag 自定，`config.toml` 里设 `AF_PERSISTENT_DDC`），设置 `UE_ANDROID_RUNNER_TAG=<该 tag>`
（`UE_ANDROID_RUNNER_SHELL` 保持 `bash`）。

### 3.4 GitLab 项目设置

* **Settings > CI/CD > General pipelines > Timeout：6h**（作业的 `timeout` 不能超过项目和 runner 的上限）。
* **Settings > Repository > Protected branches**：`main`（以及想用 `package-all` 的发布 / 热修分支，如 `release/*`）；**Protected tags**：`v*`。
  受保护变量只在这些 ref 上可见 → 签名密钥不会泄露给普通分支/MR；UE 作业也只在这些 ref 上创建（第 1 节）。
  能推送受保护分支的人就能在 UE runner 上运行代码，只给可信的角色推送权限。
* **Settings > CI/CD > Variables > Minimum role to use pipeline variables**：手动 *Run pipeline* 并带 `AF_PACKAGE_ALL=1`
  需要 `developer`（或 `maintainer`）。GitLab.com 自 17.7 起新命名空间里的新项目默认 `no_one_allowed`
  （报错 `Insufficient permissions to set pipeline variables`），只有 Owner 能改。不想改就用 `package-all` 按钮
  （同项目的父子流水线不受这个设置限制）。
* **Environments**：签名作业用 `android-signing`（`installer:android`）、`mac-signing`（`installer:mac-dmg`）、`ios-signing`（`package:ios`）、
  `windows-signing`（`installer:windows`）。作业第一次运行时 GitLab 自动创建它们；把 4.2 的密钥变量的 *Environment scope* 设成对应名字。
* **Settings > CI/CD > Artifacts**：GitLab.com 单个 artifact 上限约 1 GB。`unreal/Content/`、Win64 staged 目录较大时需要提高实例上限
  （自管 GitLab：Admin > Settings > CI/CD > Maximum artifacts size），或在 Windows runner 上直接生成安装包（把 `installer:windows` 的 `tags` 改成 Windows runner 并预装 NSIS/WiX——需自行改作业）。
  Win64 的 PDB 不要进 artifact：在 Windows runner 上设 `AF_SYMBOLS_DIR`（3.2），否则 PDB zip（数百 MB）也计入 `package:win64` 的 artifact。
* **Resource group** 默认 `unordered`；如需按提交顺序执行：`PUT /projects/:id/resource_groups/ue-ue5-mac`，`process_mode=oldest_first`。
* SaaS runner 消耗计算分钟：verify 一轮约 4 个核心构建 + Node，作业均无 tag，使用默认 small Linux runner。

---

## 4. CI/CD 变量（Settings > CI/CD > Variables）

### 4.1 配置变量（`.gitlab-ci.yml` 里有默认值，可在项目变量里覆盖）

覆盖 runner tag、引擎路径等配置变量时，项目变量**不要勾 Protected**：受保护变量只注入受保护 ref，其余流水线会悄悄用回 yml 默认值
（这些值也不是机密）。`package-all` 子流水线用的同样是项目变量（触发作业 `inherit: variables: false`，不把 yml 默认值当作高优先级的上游变量传下去）。

| 变量 | 默认 | 作用 |
|---|---|---|
| `UE_MAC_RUNNER_TAG` | `ue5-mac` | Mac runner 的 tag（内容构建、Mac / iOS 打包、DMG） |
| `UE_WIN_RUNNER_TAG` | `ue5-win` | Windows runner 的 tag |
| `UE_ANDROID_RUNNER_TAG` | `ue5-mac` | Android 打包所用 runner（默认就是 Mac）。**必须是字面值**：GitLab 对 `tags` 只做一遍变量展开，写成 `$UE_MAC_RUNNER_TAG` 会变成字面 tag，作业永远 pending。改 `UE_MAC_RUNNER_TAG` 时**两个一起改** |
| `UE_ANDROID_RUNNER_SHELL` | `bash` | `pwsh` = Android runner 是 Windows（启用 `package:android-win`） |
| `UE_ROOT_MAC` | `/Users/Shared/Epic Games/UE_5.8` | Mac 上的引擎根目录 |
| `UE_ROOT_WIN` | `C:\Program Files\Epic Games\UE_5.8` | Windows 上的引擎根目录 |
| `UE_ROOT_LINUX` | `~/UnrealEngine` | Linux UE runner（仅 Android 可选） |
| `AF_UE_CLIENT_CONFIG` | `Shipping` | 打包配置（`Development` 便于调试） |
| `AF_MAC_ARCH` | `arm64` | Mac 架构（P2：仅 Apple Silicon） |
| `AF_ANDROID_COOK_FLAVOR` | `ASTC` | Android 纹理格式 |
| `AF_ANDROID_BUILD_TOOLS` | `36.0.0` | `installer:android` 使用的 build-tools（≥ 35 才有 `zipalign -P 16`） |
| `AF_ANDROID_CMDLINE_TOOLS_ZIP` | Google 官方 commandlinetools-linux zip | Android 命令行工具下载地址（镜像/新版时覆盖） |
| `AF_REQUIRE_SIGNING` | `auto` | `auto`：tag 上 Mac / Android 必须签名（缺密钥即失败），Windows 可选；`1`：总是必须，**包括 Windows**；`0`：允许未签名 |
| `AF_WIN_REQUIRE_CODESIGN` | （空） | `1` = 没有 Windows 证书时失败（默认 Windows 签名可选；未签名时 Release 说明会注明 SmartScreen 警告） |
| `AF_SKIP_NOTARIZE` | （空） | `1` = 只签名不公证（调试用；Gatekeeper 会拦截下载的 App） |
| `AF_NOTARY_TIMEOUT` | `1h` | notarytool `--wait` 超时（每次提交；`installer:mac-dmg` 公证两次，作业超时 3h） |
| `AF_MAC_SIGN_IDENTITY` | 自动 | 指定签名身份（名称或 SHA-1），默认取 p12 中第一个 “Developer ID Application” |
| `AF_MAC_APP_ICON` | `auto` | `auto`：项目没有提交 Mac 图标时用生成的图标；`always` / `never` |
| `AF_DMG_FINDER` | （空） | `1` = 允许用 Finder AppleScript 设置 DMG 窗口（需图形会话；默认用 ds_store 无头写入） |
| `AF_PERSISTENT_DDC` | （无，**CI 中必填**） | runner 本机持久 DDC 目录，写进 runner `config.toml`（3.1 / 3.2） |
| `AF_ZEN_DATA_DIR` | （空） | Zen 存储（UE 5.4+ 本地 DDC）数据目录，以 `UE-ZenDataPath` 传入；空 = 引擎默认（runner 用户目录） |
| `AF_CLEAN_BUILD` | （空） | `1` = 删除 `Binaries` / `Intermediate` / `Saved` 后全量构建（引擎 / Xcode / MSVC / NDK 变化时自动执行） |
| `AF_SYMBOLS_DIR` | （空） | Windows runner 上保存 PDB 的目录（3.2）；空 = PDB 压缩进 `package:win64` 的 artifact |
| `AF_ANDROID_DISTRIBUTION` | `auto` | `auto`：tag 与受保护 ref 上做 distribution（release、不可调试）构建，用一次性 CI 密钥；`1` 总是；`0` debug 构建 |
| `AF_UE_EXTRA_ARGS` | （空） | 追加给每个 BuildCookRun 的参数 |
| `AF_CONTENT_ARGS` / `AF_EDITOR_EXTRA_ARGS` | （空） | 传给 `build_content.py` / 编辑器的额外参数 |
| `AF_XCODE_DEVELOPER_DIR` | （空） | 指定 Xcode（导出为 `DEVELOPER_DIR`） |
| `AF_WIN_TIMESTAMP_URL` | `http://timestamp.digicert.com` | Authenticode RFC 3161 时间戳服务器（`none` = 不加时间戳） |
| `AF_ASAN_DETECT_LEAKS` | `1` | 容器不允许 LeakSanitizer 时设 `0` |
| `AF_PACKAGE_ALL` | （空） | `1` = 在**受保护**分支上跑完整打包线（`package-all` 子流水线自动设置；手动 Run pipeline 时需要 3.4 的 pipeline variables 权限） |
| `AF_IMAGE_*` | 见 yml | 各 SaaS 作业的 Docker 镜像（可固定到 digest）；`AF_IMAGE_RELEASE` = `registry.gitlab.com/gitlab-org/cli:v1.118.0`（glab，固定版本） |

### 4.2 密钥（全部 **Masked + Protected**；类型 Variable，除非注明 File；**Environment scope** 见每组）

Environment scope：`AF_MAC_DEVID_*`、`AF_ASC_API_*` → `mac-signing`；`AF_ANDROID_*` → `android-signing`；
`AF_APPLE_TEAM_ID`、`AF_IOS_*` → `ios-signing`；`AF_WIN_CODESIGN_*` → `windows-signing`。这样只有对应的签名作业拿得到它们
（scope 留 `*` 也能工作，但每个受保护 ref 上的作业都会收到全部密钥）。脚本另外在启动引擎、pip、辅助 Python 之前清除所有签名变量（`af_unset_secrets`）。

| 变量 | 用途 | 如何生成 |
|---|---|---|
| `AF_MAC_DEVID_P12_B64` | Developer ID Application 证书 + 私钥（.p12 的 base64） | 钥匙串访问 → “我的证书” → 右键 *Developer ID Application: …* → 导出 .p12（设密码）；`base64 -i DevID.p12 \| tr -d '\n' \| pbcopy` |
| `AF_MAC_DEVID_P12_PASSWORD` | 上面 .p12 的密码 | — |
| `AF_ASC_API_KEY_P8_B64` | App Store Connect API 私钥（notarytool） | App Store Connect → 用户和访问 → 集成 → App Store Connect API → 团队密钥 → 生成（角色 Developer 即可）；下载 `AuthKey_XXXXXX.p8`（只能下载一次）；`base64 -i AuthKey_XXXXXX.p8 \| tr -d '\n'` |
| `AF_ASC_API_KEY_ID` | 上面密钥的 Key ID（10 位） | 同一页面 |
| `AF_ASC_API_ISSUER_ID` | Issuer ID（UUID） | 同一页面顶部 |
| `AF_ANDROID_KEYSTORE_B64` | Android 上传密钥库（keystore 的 base64）；只有 `installer:android`（SaaS、临时容器）使用，从不交给 UE | `keytool -genkeypair -v -keystore abyssfire-upload.keystore -alias abyssfire -keyalg RSA -keysize 4096 -validity 10000`；`base64 -w0 abyssfire-upload.keystore`（macOS：`base64 -i … \| tr -d '\n'`）。在 Play Console 启用 Play App Signing，此密钥为 upload key。**离线备份密钥库！** |
| `AF_ANDROID_KEYSTORE_PASSWORD` | 密钥库密码 | — |
| `AF_ANDROID_KEY_ALIAS` | 密钥别名（如 `abyssfire`） | 别名不满 8 字符无法 Mask：此变量可只设 Protected |
| `AF_ANDROID_KEY_PASSWORD` | 密钥密码（可省略 = 同密钥库密码；PKCS12 密钥库两者必须相同） | — |
| `AF_APPLE_TEAM_ID` | Apple 团队 ID（10 位，iOS 打包；DECISIONS P12 让 `DefaultEngine.ini` 的 `CodeSigningTeam` 留空，构建时由此注入） | developer.apple.com → Membership。团队 ID 不算机密，可只设 Protected |
| `AF_IOS_DIST_P12_B64` | Apple Distribution 证书 + 私钥（iOS） | 同 Developer ID 的导出方法 |
| `AF_IOS_DIST_P12_PASSWORD` | 上面 .p12 的密码 | — |
| `AF_IOS_PROVISION_PROFILE_B64` | App Store 描述文件（`com.feuvan.abyssfire`）；用它签名的 `.ipa` 只能上传 App Store Connect / TestFlight，不能直接安装（要给测试机侧载需另建 Ad Hoc 描述文件与导出路径，目前不在 CI 中） | developer.apple.com → Profiles → App Store Connect 类型；`base64 -i Abyssfire_AppStore.mobileprovision`。文件常超过 Mask 长度：用 **File** 类型变量存 base64 文本（仅 Protected） |
| `AF_WIN_CODESIGN_PFX_B64` | （可选）Authenticode 证书 + 私钥（.pfx base64） | `base64 -w0 codesign.pfx`。注意：2023 年 6 月起新签发的 OV/EV 代码签名证书私钥必须在硬件（HSM/USB token）里，无法导出 .pfx；这种情况下留空（安装包不签名，SmartScreen 会提示），或改用支持 PKCS#11 / 云签名的方案 |
| `AF_WIN_CODESIGN_PFX_PASSWORD` | .pfx 密码 | — |

规则：

* **Masked** 要求单行且 ≥ 8 字符：base64 必须去掉换行（`tr -d '\n'` / `base64 -w0`）。过长或不满足条件的用 **File** 类型变量（内容仍是 base64 文本），
  脚本自动识别 “变量值是文件路径” 的情况（`af_secret_to_file`）。
* **Protected**：只在受保护分支 / tag 上注入（见 3.4）。MR 和普通分支拿不到签名密钥 → 产物为调试签名 / ad-hoc 签名，tag 上则强制要求签名（`AF_REQUIRE_SIGNING=auto`）。
* 不需要额外的 API 令牌：发布（`glab`、package registry、Release links API）只用 `CI_JOB_TOKEN`。不要定义 `GITLAB_TOKEN` 之类的变量（glab 会优先用它）。
* 密钥只在作业运行期间落盘：临时钥匙串（私钥导入为不可导出 `-x`，只授权 codesign / productbuild；不签名时锁定，闲置 30 分钟自动锁定；
  作业结束删除并恢复原搜索列表，`after_script` 的 `mac-keychain-reset.sh` 再清理被杀掉的作业遗留的钥匙串和搜索列表项）、
  解码一次的 `.p8`（`work/dmg/secrets/`，作业结束删除）、对 `Config/*.ini` 的追加段（作业开始前备份，结束后还原）。
  Android 打包只用每个作业生成的一次性密钥库（`Build/Android/abyssfire-ci-throwaway.keystore`），作业结束连同
  `Intermediate/Android`（UE 复制的密钥库和写有口令的 `gradle.properties`）一起删除——**上传密钥从不出现在自建 runner 上**。
  **任何密钥都不会提交到仓库**。`ci:lint` 会拒绝在 `.gitlab-ci.yml` 中给密钥类变量赋值。
* 残余风险：`security create-keychain/unlock-keychain -p`、`security import -P` 的口令出现在进程参数里，同一系统用户的进程在那一瞬间可见；
  钥匙串口令每个作业随机生成，且 runner 为 Protected、只跑受保护 ref 的作业。

---

## 5. 版本号 Versions（`unreal/CI/version.sh`，PowerShell 版 `Get-AfVersion` 规则相同）

| 来源 | `AF_VERSION` | plist 数字版本 | MSI `ProductVersion`（`AF_MSI_VERSION`） | Android versionCode |
|---|---|---|---|---|
| tag `v1.2.3` | `1.2.3` | `1.2.3` | `1.2.399` | `10200399` |
| tag `v1.2.3-rc.4` | `1.2.3-rc.4` | `1.2.3` | `1.2.304`（rc < 正式版） | `10200304`（rc < 正式版） |
| 其它（分支、非版本 tag） | `0.0.0-<短 sha>` | `0.0.0` | `0.0.0` | 流水线 IID |

* versionCode = `X·10⁷ + Y·10⁵ + Z·100 + (预发布 ? min(N,98) : 99)`，单调递增；限制 X ≤ 209、Y ≤ 99、Z ≤ 99（超出时 `version` 作业失败）。
* MSI `ProductVersion` = `X.Y.(Z·100 + S)`，S 同上（预发布 min(N,98)，正式版 99）：Windows Installer 只比较前三段，
  若用 `X.Y.Z`，`1.2.3-rc.4` 与 `1.2.3` 无法区分，rc 的 MSI 会悄悄替换正式版。代价是“设置 > 应用”里显示 `1.2.399` 这样的版本号
  （MSI 的描述与注册表 `Version` 值仍是 `1.2.3`）。
* `AF_VERSION_QUAD` = `X.Y.Z.<流水线 IID>`（NSIS 版本资源），`AF_BUILD_NUMBER` = 流水线 IID（`CFBundleVersion`）。
* 写入位置：`DefaultGame.ini ProjectVersion`、Android `StoreVersion` / `VersionDisplayName`、iOS `VersionInfo`（构建时追加到工作副本的 ini，作业后还原）、
  Mac `Info.plist`、NSIS / MSI、文件名、Package Registry 版本。

---

## 6. 如何发布 How to cut a release

1. 确认 `main` 的流水线全绿（verify + 全部 installers）。需要时先在受保护的发布分支上点 `package-all` 预演。
2. 打带注释的 tag 并推送（tag 说明会出现在 Release 描述最上方）：
   ```bash
   git tag -a v0.2.0 -m "第一章完整可玩 / Chapter 1 playable"
   git push origin v0.2.0
   ```
   预发布用 `v0.2.0-rc.1`（Release 描述会标注 Pre-release）。
3. tag 流水线：verify → content → package（Mac / Win64 / Android）→ installers（签名 + 公证）→ `release`：
   * 每个文件上传到 **Deploy > Package Registry > abyssfire / 0.2.0**；
   * `glab release create` 创建 **Deploy > Releases > v0.2.0**，含 .dmg / .exe / .msi / .zip / .apk / .aab / SHA256SUMS.txt 链接与安装说明
     （Windows 未签名时，链接名和说明会注明 SmartScreen 警告）。
4. iOS（可选）：在该 tag 流水线里手动运行 `package:ios`，成功后运行 `release:ios`（.ipa 追加到 Release，标注 “App Store Connect upload (not installable)”）。
   上架 App Store：用 Transporter 或 `xcrun altool --upload-app` 上传 .ipa（目前不在 CI 中自动化）。
5. Google Play：把 `.aab` 上传到 Play Console 的内部测试轨道（目前手动；可后续加 fastlane `supply`）。
6. 重跑：`release` 可重试——文件会重新上传；Release 已存在时保持不变（需要重建就先在 UI 删除 Release）。

---

## 7. 本地验证 Local checks（无需 UE）

```bash
python3 unreal/CI/tools/check_gitlab_ci.py                       # 结构 + 规则模拟（需要 PyYAML）
python3 unreal/CI/tools/check_gitlab_ci.py --schema ci.json      # + GitLab 官方 JSON schema（pip install jsonschema；
#   schema: https://gitlab.com/gitlab-org/gitlab/-/raw/master/app/assets/javascripts/editor/schema/ci.json）
shellcheck -x -P SCRIPTDIR unreal/CI/version.sh unreal/CI/lib/*.sh unreal/CI/scripts/*.sh unreal/CI/tools/*.sh
unreal/CI/tools/test_windows_installers.sh [--sign]              # 假 staged 构建 → 真实 NSIS .exe + .msi + 检查（apt: nsis msitools wixl osslsigncode 7zip）
                                                                 #   MSI 版本 X.Y.(Z*100+S)、exe/msi 互斥、SIGNING-windows.txt、AF_REQUIRE_SIGNING=1 拒绝未签名
unreal/CI/tools/test_android_sign.sh                             # 一次性密钥签过的假 APK/AAB → 重新签名后只剩上传密钥（release / debug / tag 拒绝）
pwsh -NoProfile -File unreal/CI/tools/test_powershell.ps1        # 解析 + 假 RunUAT 端到端跑 ue-package.ps1（Win64 / Android）：pak 清单检查、
                                                                 #   PDB zip、工具链戳、一次性 keystore、通用 APK、Intermediate/Android 清理、密钥不进引擎
unreal/CI/tools/test_mac_scripts.sh                              # 假引擎 + 桩 Apple 工具跑 ue-content / ue-package (Mac/Android/IOS) / mac-dmg /
                                                                 #   mac-keychain-reset：版本写入、由内向外签名、ad-hoc 不带 hardened runtime、两次公证 + staple、
                                                                 #   钥匙串加锁 / 不可导出 / 搜索列表清理、pak 清单、一次性 Android 密钥、DDC 必填、Metal 检查、失败路径
python3 unreal/CI/installers/assets/make_installer_assets.py --out /tmp/af-assets   # 图标 / DMG 背景 / NSIS 位图
bash unreal/CI/scripts/core-tests.sh gcc                          # 与 CI 完全相同的作业脚本
bash unreal/CI/scripts/data-check.sh
AF_RELEASE_DRY_RUN=1 CI_COMMIT_TAG=v1.2.3 bash unreal/CI/scripts/release.sh          # 发布预演（需 unreal/CI/out/installers）
```

Mac 上可直接在仓库里跑 UE 作业脚本做冒烟：`bash unreal/CI/scripts/ue-content.sh`、`bash unreal/CI/scripts/ue-package.sh Mac`、
`bash unreal/CI/scripts/mac-dmg.sh`（无签名变量时生成 ad-hoc 签名、未公证的 DMG）。所有输出在 `unreal/CI/out/`（已被 git 忽略）。

---

## 8. 与其它部分的约定 Contracts

* **`Scripts/build_content.py`**：CI 以 `-run=pythonscript` 无头运行，并导出环境变量 `AF_CI=1`、`AF_CONTENT_REPORT=<路径>`。
  脚本应把 JSON 报告写到 `AF_CONTENT_REPORT`，失败时包含 `"ok": false`（CI 也会扫描日志中的 `LogPython: Error` / Traceback，
  并检查 `Content/Abyssfire/Maps/L_Main.umap` 是否生成）。pythonscript commandlet 在 Python 异常后可能仍返回 0，所以这些检查是必要的。
* **图标**：放 `unreal/Art/Export/Icons/T_UI_AppIcon.png`（≥ 1024² 正方形）即作为全部安装包图标的来源；没有时用
  `Portraits/T_UI_Portrait_Hero_Warrior.png` 合成，再没有就用程序生成的余烬徽记。项目一旦提交自己的
  `unreal/Build/Windows/Application.ico`、`unreal/Build/Android/res/drawable*/icon.png`、Mac 的 `.icns` / `AppIcon.appiconset`，CI 就不再覆盖。
  iOS 图标需提交到 `unreal/Build/IOS/`（Xcode 资源目录；CI 不生成）。
* **RuntimeDependencies 检查**：每次 BuildCookRun 之后，`Data/` 与 `Fonts/` 下每个运行时文件（`*.json`、`*.ttf`、`*.otf`、`*.ttc`；
  README / 字体许可证不查）必须出现在 UAT 写到 `uebp_LogFolder` 的 pak 清单 `PakList_*.txt` 里（`"../../../Abyssfire/<文件>"`），
  没有清单时退而用 `UnrealPak <pakchunk0*.pak> -List`。Shipping 只在日志里报数据加载错误（ue58-platform.md §10.2），
  而通配符在 UBT receipt 里展开、`Intermediate` 跨作业保留，缺文件否则会被静默打包。**[Verify]** 5.8.3 上 pak 清单的文件名与格式（§17 第 8 项）。
* **签名配置注入**：Android `KeyStore/KeyAlias/KeyStorePassword/KeyPassword`（一次性 CI 密钥库）、`bEnableBundle` / `bEnableUniversalAPK` 与 iOS
  `[/Script/MacTargetPlatform.XcodeProjectSettings] CodeSigningTeam / bUseAutomaticCodeSigning / IOSSigningIdentity / IOSProvisioningProfile`
  （以及旧版 `IOSRuntimeSettings` 键）只追加到 CI 工作副本的 `Config/Android/AndroidEngine.ini`、`Config/IOS/IOSEngine.ini`，
  作业结束还原。**[Verify]** 5.8.3 上 Modern Xcode 的键名（ue58-platform.md §17 第 4 项）。
* **打包输出名**：脚本不假设 UE 的具体文件名——Mac 取归档目录里的 `.app`，Android 取本作业新生成的最新 `.apk`（通用 APK）/ `.aab`
  （归档目录、`Binaries/Android`、`Intermediate/Android`），Windows 取 `Windows/Abyssfire.exe` 所在目录。

---

## 9. 故障排查 Troubleshooting

| 现象 | 原因 / 处理 |
|---|---|
| UE 作业一直 *pending* | 没有在线的 runner 带该 tag：检查 `UE_*_RUNNER_TAG` 与 runner 的 tag 是否一致（`UE_ANDROID_RUNNER_TAG` 要跟着 `UE_MAC_RUNNER_TAG` 一起改，且必须是字面值）、覆盖这些变量的项目变量没有勾 Protected、runner 是否在线。流水线只在受保护 ref 上创建 UE 作业；若手工改 yml 让非保护 ref 产生 UE 作业，Protected runner 不会接，它会 pending 1 小时并一直占着 `resource_group`，main / tag 的 UE 作业显示 *Waiting for resource: ue-ue5-mac* 直到它被判 stuck——取消它即可 |
| `Waiting for resource: ue-…` | 同一台 runner 上另一个 UE 作业在跑（或 pending）；见上一行 |
| `AF_PERSISTENT_DDC is not set` | runner `config.toml` 的 `environment` 缺 `AF_PERSISTENT_DDC`（3.1 / 3.2） |
| `Metal Toolchain missing` / cook 报 `cannot execute tool 'metal' due to missing Metal Toolchain` | Xcode 26 需单独下载：`xcodebuild -downloadComponent MetalToolchain`（3.1） |
| `iOS platform not installed` | `xcodebuild -downloadPlatform iOS`（3.1） |
| `PCH file built from a different branch` / UHT 或 BuildId 不匹配 | 引擎或工具链升级后的陈旧中间文件：作业会按 `Intermediate/.af-toolchain-stamp` 自动全量重建；手动用 `AF_CLEAN_BUILD=1` 重跑 |
| `RuntimeDependencies (Data/, Fonts/) missing from the pak` / `not staged: …` | `Abyssfire.Build.cs` 的 `RuntimeDependencies` 或 staging 有问题；新加的 `Data/*.json` 没进 UBT receipt 时用 `AF_CLEAN_BUILD=1` 重跑 |
| `the job exceeded the maximum timeout` | 项目 Timeout 或 runner “Maximum job timeout” < 作业 `timeout`（3.4，设 6h） |
| `Unreal Engine not found at …` | `UE_ROOT_MAC` / `UE_ROOT_WIN` 指向错误；Launcher 版本路径为 `UE_5.8` |
| `the project pins UE 5.8` | runner 上装的不是 5.8.x |
| Win64 编译报 `<hash_map>` 缺失 | MSVC 14.51；在 VS Installer 里装 14.50 组件（或 VS 2022 v143） |
| `content build failed` / Python 错误 | 看 `unreal/CI/out/content/build_content.log`（artifact）；先在本机 `UnrealEditor -run=pythonscript` 复现 |
| `unreal/Content is missing L_Main` | package 作业没拿到 `ue:content` 的 artifact（过期 3 天 / artifact 过大被拒）：重跑 `ue:content` |
| artifact 上传 `413 Request Entity Too Large` | 超过实例 artifact 上限（3.4） |
| `security: SecKeychainItemImport … MAC verification failed` | `.p12` 密码错误，或 OpenSSL 3 导出的 p12 用了 macOS 不认的算法：`openssl pkcs12 -export -legacy …` 重新导出 |
| `errSecInternalComponent` / codesign 卡住 | runner 不在已登录的用户会话里运行（LaunchDaemon / SSH）：改为 LaunchAgent + 自动登录 |
| notarytool `Invalid` | 日志在 `unreal/CI/out/logs/<job>/notary-*-log.json`：常见为某个 dylib 未签名 / 无 hardened runtime / 无时间戳——`af_codesign_bundle` 逐个签名所有 Mach-O；第三方库需要特殊 entitlement 时编辑 `unreal/CI/installers/mac/entitlements.plist` |
| 开发版（ad-hoc 签名）DMG 里的 App 启动即崩溃，`mapped file has no Team ID` | ad-hoc 签名不能带 hardened runtime（库验证拒绝无 Team ID 的 dylib）；`af_codesign_bundle` 对 identity `-` 已不加 `--options runtime`，确认脚本是最新的 |
| notarytool 等待超时 | 苹果队列慢于 `AF_NOTARY_TIMEOUT`（1h）：提交仍在苹果那边继续，重跑作业即可；`installer:mac-dmg` 超时 3h |
| `spctl … rejected` | DMG 未 staple 或证书不是 Developer ID Application |
| DMG 打开后没有背景 / 图标位置 | 看 `logs/<job>/dmg-pip.log`：runner 无法从 PyPI 安装固定版本的 `ds_store` / `mac_alias`（`dmg-requirements.txt`，哈希校验）——联网，或设 `AF_DMG_FINDER=1`（需图形会话） |
| `zipalign without -P support` | build-tools < 35；保持 `AF_ANDROID_BUILD_TOOLS=36.0.0`，或删除缓存 `.android-sdk` 后重跑 |
| `commandlinetools … 404` | Google 更新了下载文件名：设置 `AF_ANDROID_CMDLINE_TOOLS_ZIP` 为 developer.android.com 上的新链接 |
| `Failed to obtain key with alias` | `AF_ANDROID_KEY_PASSWORD` 错误；PKCS12 密钥库的 key 密码必须等于 store 密码（不设 `AF_ANDROID_KEY_PASSWORD` 即可） |
| `no .apk produced` | bundle 模式下 `.apk` 只由 bundletool 的通用 APK 产生：需要 `bEnableUniversalAPK=True`（脚本已追加到 `AndroidEngine.ini`）；看 `logs/<job>/uat/` 里 bundletool 的错误 |
| `no .aab produced` | `bEnableBundle=True`（脚本已追加）未生效，或 Gradle 失败（看 `logs/<job>/uat/`） |
| `keytool could not create the throw-away keystore` | runner 上没有 JDK 21 / `JAVA_HOME`（3.1 第 4 步） |
| Android 打包找不到 SDK/NDK | runner `config.toml` 的 `ANDROID_HOME` / `NDKROOT` / `JAVA_HOME`；先跑 Turnkey `VerifySdk` |
| `release … already exists` | Release 已存在（`glab release view` 找到了它）：文件已重新上传，Release 未改；需要重建就先在 UI 删除 |
| `glab release create failed` / 401 | 作业变量 `GLAB_ENABLE_CI_AUTOLOGIN=true` 让 glab 用 `CI_JOB_TOKEN`；项目里若定义了 `GITLAB_TOKEN` / `GITLAB_ACCESS_TOKEN` / `OAUTH_TOKEN`，glab 会优先用它——删除 |
| `Insufficient permissions to set pipeline variables` | 3.4：Minimum role to use pipeline variables；或用 `package-all` 按钮 |
| setup.exe 提示已通过 .msi 安装 / MSI 提示已由 setup.exe 安装 | 两种安装包互斥（第 2 节）：先在“设置 > 应用”卸载另一个 |
| `release` 报 `missing installers` | 某个 installer 作业未成功或 artifact 过期；发布绝不会缺平台 |
| `core:gcc-sanitize` 报 LeakSanitizer 致命错误 | 容器禁止 ptrace：设 `AF_ASAN_DETECT_LEAKS=0` |
| `data:check` 报 STALE | `unreal/Data` 与 TS 源不一致：本地 `node unreal/Tools/export-data/run.mjs` 后提交 `unreal/Data/`（及 golden） |
| `ci:lint` 失败 | 按输出修 `.gitlab-ci.yml`；它模拟每种流水线并检查 `needs` 是否都存在 |
| 分支上看不到 `package-all` | 只在受保护的非默认分支上提供（3.4）；默认分支和 tag 不需要它（本来就跑全部）；MR / 普通分支没有（Protected runner 不会接这些作业） |

---

## 10. 文件清单 Files

```
.gitlab-ci.yml                                  流水线（GitLab 只认仓库根目录）
unreal/CI/version.sh                            版本号 → dotenv
unreal/CI/lib/common.sh                         日志、折叠段、密钥解码、签名策略、版本加载
unreal/CI/lib/ue.sh                             UE 路径 / 版本检查、DDC、ini 覆盖与还原、BuildCookRun
unreal/CI/lib/macos.sh                          临时钥匙串、inside-out codesign、notarytool
unreal/CI/scripts/core-tests.sh                 core:*        CoreTests/run.sh + JUnit
unreal/CI/scripts/data-check.sh                 data:check    export-data --check
unreal/CI/scripts/web-test.sh                   web:vitest
unreal/CI/scripts/blender-smoke.sh              art:blender-smoke
unreal/CI/scripts/ue-content.sh                 ue:content    编辑器编译 + build_content.py
unreal/CI/scripts/ue-package.sh                 package:mac / android / ios
unreal/CI/scripts/mac-dmg.sh                    installer:mac-dmg
unreal/CI/scripts/mac-keychain-reset.sh         installer:mac-dmg / package:ios 的 after_script（清理遗留的签名钥匙串）
unreal/CI/scripts/windows-installers.sh         installer:windows（NSIS + wixl + zip）
unreal/CI/scripts/sign-pe.sh                    osslsigncode 原地签名（也被 NSIS !finalize 调用）
unreal/CI/scripts/android-sdk-setup.sh          build-tools 安装（缓存）
unreal/CI/scripts/android-sign.sh               installer:android
unreal/CI/scripts/release.sh, release-ios.sh    release, release:ios
unreal/CI/windows/AfCommon.ps1, ue-package.ps1  package:win64 / package:android-win（PowerShell 5.1 / 7）
unreal/CI/installers/windows/abyssfire.nsi      NSIS 模板        gen_file_lists.py  安装/卸载文件清单
unreal/CI/installers/windows/abyssfire.wxs      WiX 3（wixl）模板
unreal/CI/installers/mac/dmg_style.py           .DS_Store（窗口、图标位置、背景）  entitlements.plist
unreal/CI/installers/mac/dmg-requirements.txt   dmg_style.py 的依赖（固定版本 + 哈希）
unreal/CI/installers/assets/make_installer_assets.py   图标 / DMG 背景 / NSIS 位图（Pillow 或纯标准库）
unreal/CI/tools/check_gitlab_ci.py              ci:lint 流水线检查
unreal/CI/tools/test_*.sh / .ps1                本地自测（第 7 节）
```
