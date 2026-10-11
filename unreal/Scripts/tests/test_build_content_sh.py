"""build_content.sh against a stand-in engine (bash on Linux / macOS; no Unreal needed).

Covers SKIP_EDITOR_BUILD with a missing / stale / current editor module binary (a stale one is rebuilt anyway, with a
warning, and still used when that build fails; SKIP_EDITOR_BUILD=force never builds), --shaders (adds -AllowCommandletRendering and still reaches build_content.py) and the shader-error scan of the
editor log (printed always, fails the run only with --shaders).
"""
from __future__ import annotations

import json
import os
import platform
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE.parent / "build_content.sh"
HOST = {"Darwin": "Mac", "Linux": "Linux"}.get(platform.system(), "")

BUILD_STUB = """#!/usr/bin/env bash
echo "$*" >> "$FAKE_RECORD/build_calls"
exit "${FAKE_BUILD_STATUS:-0}"
"""
EDITOR_STUB = """#!/usr/bin/env bash
log=""
for a in "$@"; do
  printf '%s\\n' "$a" >> "$FAKE_RECORD/editor_args"
  case "$a" in -abslog=*) log="${a#-abslog=}" ;; esac
done
printf '%b' "${FAKE_LOG_LINES:-LogPython: content build OK\\n}" > "$log"
printf '{"ok": true}\\n' > "$AF_CONTENT_REPORT"
"""


@unittest.skipUnless(HOST and shutil.which("bash"), "build_content.sh runs on macOS / Linux")
class BuildContentShTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.unreal = root / "unreal"
        (self.unreal / "Scripts").mkdir(parents=True)
        shutil.copy2(SCRIPT, self.unreal / "Scripts" / "build_content.sh")
        self.source = self.unreal / "Source" / "Abyssfire" / "Private" / "ContentBuild" / "Helper.cpp"
        self.source.parent.mkdir(parents=True)
        self.source.write_text("// helper\n")
        self.binaries = self.unreal / "Binaries" / HOST
        self.module = self.binaries / ("UnrealEditor-Abyssfire.dylib" if HOST == "Mac"
                                       else "libUnrealEditor-Abyssfire.so")
        ue = root / "UE"
        for rel, text in ((f"Engine/Build/BatchFiles/{HOST}/Build.sh", BUILD_STUB),
                          (f"Engine/Binaries/{HOST}/UnrealEditor-Cmd", EDITOR_STUB)):
            path = ue / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
            path.chmod(0o755)
        self.record = root / "record"
        self.record.mkdir()
        self.env = {k: v for k, v in os.environ.items() if k not in ("SKIP_EDITOR_BUILD", "FAKE_LOG_LINES",
                                                                   "FAKE_BUILD_STATUS")}
        self.env.update(UE_ROOT=str(ue), FAKE_RECORD=str(self.record))

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def module_binary(self, age_s: float) -> None:
        """The editor module, built `age_s` seconds after (> 0) or before (< 0) the newest source file."""
        self.binaries.mkdir(parents=True, exist_ok=True)
        self.module.write_bytes(b"\x7fELF")
        now = time.time()
        os.utime(self.source, (now - 1000, now - 1000))
        os.utime(self.module, (now - 1000 + age_s, now - 1000 + age_s))

    def run_sh(self, *args: str, **env: str) -> subprocess.CompletedProcess:
        for f in ("build_calls", "editor_args"):
            (self.record / f).unlink(missing_ok=True)
        return subprocess.run(["bash", str(self.unreal / "Scripts" / "build_content.sh"), "--only", "materials", *args],
                              env={**self.env, **env}, capture_output=True, text=True, timeout=60)

    def built(self) -> bool:
        return (self.record / "build_calls").is_file()

    def editor_args(self) -> list[str]:
        return (self.record / "editor_args").read_text().splitlines()

    def test_skip_with_a_current_binary(self):
        self.module_binary(+10)
        r = self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.built())
        self.assertNotIn("WARNING", r.stderr)

    def test_skip_with_a_stale_binary_builds_anyway(self):
        self.module_binary(-10)
        r = self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(self.built())
        self.assertIn("WARNING: SKIP_EDITOR_BUILD is set but the editor binary is stale", r.stderr)
        self.assertIn("is older than Source/Abyssfire/Private/ContentBuild/Helper.cpp", r.stderr)
        # the build this script ran is stamped: an up-to-date (untouched) binary is not "stale" on the next run
        r = self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertFalse(self.built(), r.stderr)
        self.assertNotIn("WARNING", r.stderr)

    def test_force_skips_a_stale_binary(self):
        self.module_binary(-10)
        r = self.run_sh(SKIP_EDITOR_BUILD="force")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.built())
        self.assertIn("SKIP_EDITOR_BUILD=force: not building AbyssfireEditor", r.stderr)
        self.assertIn("STALE EDITOR BINARY", r.stderr)
        self.assertIn("-run=pythonscript", self.editor_args())
        self.module_binary(+10)
        r = self.run_sh(SKIP_EDITOR_BUILD="force")
        self.assertFalse(self.built())
        self.assertNotIn("WARNING", r.stderr)

    def test_failed_rebuild_of_a_stale_binary_still_runs_the_content_build(self):
        self.module_binary(-10)
        r = self.run_sh(SKIP_EDITOR_BUILD="1", FAKE_BUILD_STATUS="6")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(self.built())
        self.assertIn("the AbyssfireEditor build failed (status 6", r.stderr)
        self.assertIn("-run=pythonscript", self.editor_args())
        # not stamped: the next run still sees the binary as stale and tries again
        self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertTrue(self.built())

    def test_failed_build_ends_the_run_without_a_binary_to_fall_back_on(self):
        for env in ({"SKIP_EDITOR_BUILD": "1"}, {}):
            r = self.run_sh(FAKE_BUILD_STATUS="6", **env)
            self.assertEqual(r.returncode, 6, r.stderr)
            self.assertIn("the AbyssfireEditor build failed (status 6)", r.stderr)
            self.assertFalse((self.record / "editor_args").exists(), env)
        self.module_binary(+10)
        r = self.run_sh(FAKE_BUILD_STATUS="6")
        self.assertEqual(r.returncode, 6, r.stderr)
        self.assertFalse((self.record / "editor_args").exists())

    def test_skip_without_a_binary_builds(self):
        r = self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertTrue(self.built())
        self.assertIn("no editor module binary", r.stderr)

    def test_default_always_builds(self):
        self.module_binary(+10)
        self.run_sh()
        self.assertTrue(self.built())

    def test_shaders_flag(self):
        self.module_binary(+10)
        self.run_sh(SKIP_EDITOR_BUILD="1")
        self.assertNotIn("-AllowCommandletRendering", self.editor_args())
        r = self.run_sh("--shaders", SKIP_EDITOR_BUILD="1")
        self.assertEqual(r.returncode, 0, r.stderr)
        args = self.editor_args()
        self.assertIn("-AllowCommandletRendering", args)
        script = next(a for a in args if a.startswith("-script="))
        self.assertTrue(script.endswith("build_content.py --only materials --shaders"), script)
        self.assertIn("-run=pythonscript", args)

    def test_shader_errors_in_the_log(self):
        self.module_binary(+10)
        lines = ("LogShaderCompilers: Warning: Failed to compile Material /Game/Abyssfire/Materials/M_AF_Toon for "
                 "platform SF_METAL_SM5\\n/Engine/Generated/Material.ush(77,5): error: undeclared identifier 'x'\\n"
                 "LogPython: done\\n")
        r = self.run_sh(SKIP_EDITOR_BUILD="1", FAKE_LOG_LINES=lines)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("shader compile errors in the log", r.stderr)
        self.assertIn("Failed to compile Material /Game/Abyssfire/Materials/M_AF_Toon", r.stderr)
        r = self.run_sh("--shaders", SKIP_EDITOR_BUILD="1", FAKE_LOG_LINES=lines)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn("content build FAILED", r.stderr)
        report = json.loads((self.unreal / "Saved" / "ContentBuild" / "build_report.json").read_text())
        self.assertTrue(report["ok"])


if __name__ == "__main__":
    unittest.main()
