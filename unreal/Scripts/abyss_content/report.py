"""Build report: JSON (CI contract, Docs/CI.md 8: `"ok": false` fails the ue:content job) + a readable Markdown twin.

No `unreal` import.
"""
from __future__ import annotations

import json
import platform
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class StepResult:
    name: str
    status: str = "pending"          # pending | ok | warnings | failed | skipped
    seconds: float = 0.0
    counts: dict[str, int] = field(default_factory=dict)
    notes: list[str] = field(default_factory=list)


@dataclass
class BuildReport:
    started: float = field(default_factory=time.time)
    engine: str = ""
    args: list[str] = field(default_factory=list)
    steps: list[StepResult] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    assets: dict[str, dict[str, Any]] = field(default_factory=dict)     # content path -> {class, action, ...}
    materials: dict[str, dict[str, Any]] = field(default_factory=dict)  # material -> stats
    verification: list[str] = field(default_factory=list)
    plan: dict[str, Any] = field(default_factory=dict)
    finished: float = 0.0
    _current: StepResult | None = None
    _step_t0: float = 0.0

    # ---- steps ----
    def begin(self, name: str) -> StepResult:
        step = StepResult(name)
        self.steps.append(step)
        self._current = step
        self._step_t0 = time.time()
        return step

    def end(self, status: str | None = None) -> None:
        step = self._current
        if step is None:
            return
        step.seconds = round(time.time() - self._step_t0, 2)
        if status is not None:
            step.status = status
        elif step.status == "pending":
            step.status = "ok"
        self._current = None

    def skip(self, name: str, reason: str) -> None:
        step = StepResult(name, status="skipped", notes=[reason])
        self.steps.append(step)

    def count(self, key: str, n: int = 1) -> None:
        if self._current is not None:
            self._current.counts[key] = self._current.counts.get(key, 0) + n

    def note(self, text: str) -> None:
        if self._current is not None:
            self._current.notes.append(text)

    # ---- problems ----
    def error(self, text: str) -> None:
        self.errors.append(text)
        if self._current is not None:
            self._current.status = "failed"
            self._current.notes.append(f"ERROR {text}")

    def warning(self, text: str) -> None:
        self.warnings.append(text)
        if self._current is not None:
            if self._current.status in ("pending", "ok"):
                self._current.status = "warnings"
            self._current.notes.append(f"warning {text}")

    def asset(self, path: str, cls: str, action: str, **extra: Any) -> None:
        entry = {"class": cls, "action": action}
        entry.update(extra)
        self.assets[path] = entry
        self.count(action)

    @property
    def ok(self) -> bool:
        return not self.errors

    # ---- output ----
    def to_dict(self) -> dict[str, Any]:
        actions: dict[str, int] = {}
        for entry in self.assets.values():
            actions[entry["action"]] = actions.get(entry["action"], 0) + 1
        return {
            "ok": self.ok,
            "schemaVersion": 1,
            "generator": "unreal/Scripts/build_content.py",
            "engine": self.engine,
            "host": f"{platform.system()} {platform.machine()} Python {platform.python_version()}",
            "args": self.args,
            "startedUnix": round(self.started, 3),
            "seconds": round((self.finished or time.time()) - self.started, 2),
            "summary": {"errors": len(self.errors), "warnings": len(self.warnings), "assets": len(self.assets),
                        "actions": actions},
            "errors": self.errors,
            "warnings": self.warnings,
            "steps": [{"name": s.name, "status": s.status, "seconds": s.seconds, "counts": s.counts, "notes": s.notes}
                      for s in self.steps],
            "plan": self.plan,
            "materials": self.materials,
            "verification": self.verification,
            "assets": dict(sorted(self.assets.items())),
        }

    def to_markdown(self) -> str:
        d = self.to_dict()
        lines = [
            "# Abyssfire content build report",
            "",
            f"* Result: **{'OK' if d['ok'] else 'FAILED'}** — {d['summary']['errors']} error(s), "
            f"{d['summary']['warnings']} warning(s), {d['summary']['assets']} asset(s) touched, {d['seconds']} s",
            f"* Engine: {d['engine'] or 'n/a'} · Host: {d['host']}",
            f"* Arguments: `{' '.join(d['args']) or '(none)'}`",
            "",
            "## Steps",
            "",
            "| Step | Status | Seconds | Counts |",
            "|---|---|---|---|",
        ]
        for s in d["steps"]:
            counts = ", ".join(f"{k} {v}" for k, v in sorted(s["counts"].items())) or "—"
            lines.append(f"| {s['name']} | {s['status']} | {s['seconds']} | {counts} |")
        for title, items in (("Errors", d["errors"]), ("Warnings", d["warnings"]),
                             ("Verification", d["verification"])):
            lines += ["", f"## {title}", ""]
            lines += [f"* {x}" for x in items] or ["(none)"]
        if d["materials"]:
            lines += ["", "## Materials", "", "| Material | Status | VS instr. | PS instr. | Samplers |", "|---|---|---|---|---|"]
            for name, m in sorted(d["materials"].items()):
                lines.append(f"| {name} | {m.get('status', '')} | {m.get('vs', '')} | {m.get('ps', '')} | "
                             f"{m.get('samplers', '')} |")
        lines += ["", "## Step notes", ""]
        for s in d["steps"]:
            if s["notes"]:
                lines.append(f"### {s['name']}")
                lines += [f"* {n}" for n in s["notes"][:200]]
                if len(s["notes"]) > 200:
                    lines.append(f"* … {len(s['notes']) - 200} more (see the JSON report)")
                lines.append("")
        return "\n".join(lines) + "\n"

    def write(self, json_path: Path, md_path: Path | None = None) -> None:
        self.finished = time.time()
        json_path.parent.mkdir(parents=True, exist_ok=True)
        tmp = json_path.with_suffix(json_path.suffix + ".tmp")
        tmp.write_text(json.dumps(self.to_dict(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        tmp.replace(json_path)
        if md_path is not None:
            md_path.parent.mkdir(parents=True, exist_ok=True)
            md_path.write_text(self.to_markdown(), encoding="utf-8")
