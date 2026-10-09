# unreal/CI — GitLab CI/CD scripts

Job scripts, installer templates and self-tests for [`/.gitlab-ci.yml`](../../.gitlab-ci.yml).
Read **[`unreal/Docs/CI.md`](../Docs/CI.md)** (runner setup, every CI/CD variable, releases, troubleshooting).

| Path | What |
|---|---|
| `version.sh` | version stamping from the tag (`vX.Y.Z[-pre]`, else `0.0.0-<sha>`) → dotenv |
| `lib/` | shared bash helpers: `common.sh` (logging, secrets, signing policy), `ue.sh` (engine, DDC, ini overlays, BuildCookRun), `macos.sh` (keychain, codesign, notarytool) |
| `scripts/` | one script per job (bash; macOS system bash 3.2 compatible, shellcheck-clean) |
| `windows/` | PowerShell 5.1 / 7 scripts for the Windows UE runner |
| `installers/` | NSIS + WiX (wixl) templates, DMG layout, entitlements, artwork generator |
| `tools/` | `check_gitlab_ci.py` (pipeline lint + rule simulation) and local self-tests without Unreal |

Everything runs the same locally: e.g. `bash unreal/CI/scripts/core-tests.sh gcc`, `unreal/CI/tools/test_windows_installers.sh --sign`.
Outputs go to `unreal/CI/out/` (git-ignored).
