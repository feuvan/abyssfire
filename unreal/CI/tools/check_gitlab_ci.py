#!/usr/bin/env python3
"""Static checks for /.gitlab-ci.yml (CI job `ci:lint`, and locally before pushing).

    python3 unreal/CI/tools/check_gitlab_ci.py [--file .gitlab-ci.yml] [--schema ci.json] [--verbose]

GitLab only validates a pipeline when it is created; this catches the mistakes earlier, offline:

* YAML: parses with a loader that rejects duplicate keys and understands `!reference`; `extends` (multi-level,
  deep-merged like GitLab: hashes merge, arrays replace), `!reference` and `default:` / `inherit:` are resolved.
* Optional JSON-schema validation against GitLab's own schema (`--schema` file; app/assets/javascripts/editor/
  schema/ci.json in gitlab-org/gitlab) when the `jsonschema` package is installed.
* Structure: every job has a known stage; `needs` / `dependencies` name existing jobs in the same or an earlier
  stage, no cycles; rules use known keys and `when` values; `if:` expressions parse; manual rules on trigger jobs.
* Policy (unreal/Docs/CI.md): retry only on runner_system_failure; every job (trigger jobs too) sets interruptible;
  artifacts expire; self-hosted Unreal jobs have a resource_group and a timeout and no GitLab `cache:` (the DDC is
  the runner's AF_PERSISTENT_DDC); jobs on SaaS runners have an image; no secret-looking variable has a value in the
  file; every script referenced under unreal/CI exists (bash ones executable, with a shebang).
* Pipeline simulation: the workflow + job rules are evaluated for merge-request, branch, protected branch,
  default-branch, tag, pre-release tag, non-version tag, unprotected tag, package-all child, scheduled, "Run pipeline"
  and branch-with-open-MR pipelines, the way GitLab does it: rules:if sees the job's own `variables` and the global
  ones it inherits (`inherit:variables`), pipeline variables override both. The expected job set of each is asserted
  (MR = verify only, default branch = everything but release, tags = release, no Unreal job on an unprotected ref...)
  and every non-optional `needs` must exist in each simulated pipeline (GitLab refuses to create it otherwise).
* Runner tags are expanded like GitLab does: ONE pass over the raw variable values (a nested `$VAR` inside a
  variable stays literal there), so a tag that still contains `$` or is empty is an error.

Exit code 0 = clean. Requires PyYAML.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import re
import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[3]
RESERVED = {"default", "include", "stages", "variables", "workflow", "image", "services", "cache", "before_script",
            "after_script", "spec"}
RULE_KEYS = {"if", "changes", "exists", "when", "allow_failure", "variables", "needs", "interruptible"}
WHEN_VALUES = {"on_success", "on_failure", "always", "manual", "never", "delayed"}
UE_TAG_VARS = ("$UE_MAC_RUNNER_TAG", "$UE_WIN_RUNNER_TAG", "$UE_ANDROID_RUNNER_TAG")
SECRET_NAME = re.compile(r"(PASSWORD|SECRET|TOKEN|_B64|_P12|_PFX|_P8|KEYSTORE|PRIVATE)", re.I)

errors: list[str] = []
warnings: list[str] = []


def err(msg: str) -> None:
    errors.append(msg)


def warn(msg: str) -> None:
    warnings.append(msg)


# ── YAML loading ───────────────────────────────────────────────────────────────────────────────────────────
class Reference(list):
    """`!reference [job, key, ...]`"""


class StrictLoader(yaml.SafeLoader):
    pass


def _construct_mapping(loader, node, deep=False):
    seen = set()
    for key_node, _ in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in seen:
            raise yaml.constructor.ConstructorError(None, None, f"duplicate key {key!r}", key_node.start_mark)
        seen.add(key)
    return yaml.SafeLoader.construct_mapping(loader, node, deep)


StrictLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _construct_mapping)
StrictLoader.add_constructor("!reference", lambda l, n: Reference(l.construct_sequence(n)))


def deep_merge(base, over):
    if isinstance(base, dict) and isinstance(over, dict):
        out = dict(base)
        for k, v in over.items():
            out[k] = deep_merge(base[k], v) if k in base else copy.deepcopy(v)
        return out
    return copy.deepcopy(over)


def resolve_extends(doc: dict) -> dict:
    cache: dict[str, dict] = {}

    def resolve(name: str, stack: tuple) -> dict:
        if name in cache:
            return cache[name]
        if name in stack:
            err(f"extends cycle: {' -> '.join(stack + (name,))}")
            return {}
        if name not in doc or not isinstance(doc[name], dict):
            err(f"extends: unknown job/template {name!r} (from {stack[-1] if stack else '?'})")
            return {}
        job = doc[name]
        parents = job.get("extends", [])
        parents = [parents] if isinstance(parents, str) else parents
        merged: dict = {}
        for p in parents:
            merged = deep_merge(merged, resolve(p, stack + (name,)))
        own = {k: v for k, v in job.items() if k != "extends"}
        merged = deep_merge(merged, own)
        cache[name] = merged
        return merged

    return {k: (resolve(k, ()) if isinstance(v, dict) and k not in RESERVED else v) for k, v in doc.items()}


def resolve_references(node, doc, depth=0):
    if depth > 10:
        err("!reference nesting deeper than 10")
        return node
    if isinstance(node, Reference):
        target = doc
        for key in node:
            if not isinstance(target, dict) or key not in target:
                err(f"!reference {list(node)} does not resolve")
                return None
            target = target[key]
        return resolve_references(copy.deepcopy(target), doc, depth + 1)
    if isinstance(node, dict):
        return {k: resolve_references(v, doc, depth) for k, v in node.items()}
    if isinstance(node, list):
        out = []
        for item in node:
            r = resolve_references(item, doc, depth)
            # GitLab flattens a referenced list inside rules / script arrays
            if isinstance(item, Reference) and isinstance(r, list):
                out.extend(r)
            else:
                out.append(r)
        return out
    return node


# ── rules: if-expression parser / evaluator ──────────────────────────────────────────────────────────────────
TOKEN = re.compile(r"""\s*(?:
    (?P<op>==|!=|=~|!~|&&|\|\|)|
    (?P<lp>\()|(?P<rp>\))|
    (?P<var>\$\{?[A-Za-z_][A-Za-z0-9_]*\}?)|
    (?P<str>"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')|
    (?P<re>/(?:[^/\\]|\\.)+/[a-z]*)|
    (?P<null>null)
)""", re.X)


def tokenize(expr: str):
    pos, out = 0, []
    while pos < len(expr):
        if expr[pos:].strip() == "":
            break
        m = TOKEN.match(expr, pos)
        if not m or m.end() == pos:
            raise ValueError(f"cannot parse at {expr[pos:]!r}")
        kind = m.lastgroup
        out.append((kind, m.group(kind)))
        pos = m.end()
    return out


class Expr:
    def __init__(self, expr: str):
        self.src = expr
        self.toks = tokenize(expr)
        self.i = 0
        self.tree = self.parse_or()
        if self.i != len(self.toks):
            raise ValueError(f"trailing tokens in {expr!r}")

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def take(self):
        t = self.peek()
        self.i += 1
        return t

    def parse_or(self):
        node = self.parse_and()
        while self.peek() == ("op", "||"):
            self.take()
            node = ("or", node, self.parse_and())
        return node

    def parse_and(self):
        node = self.parse_cmp()
        while self.peek() == ("op", "&&"):
            self.take()
            node = ("and", node, self.parse_cmp())
        return node

    def parse_cmp(self):
        left = self.parse_primary()
        kind, val = self.peek()
        if kind == "op" and val in ("==", "!=", "=~", "!~"):
            self.take()
            right = self.parse_primary()
            if val in ("=~", "!~") and right[0] not in ("re", "var"):
                raise ValueError(f"{val} needs a /regex/ in {self.src!r}")
            return ("cmp", val, left, right)
        return ("truthy", left)

    def parse_primary(self):
        kind, val = self.take()
        if kind == "lp":
            node = self.parse_or()
            if self.take()[0] != "rp":
                raise ValueError(f"missing ) in {self.src!r}")
            return node
        if kind == "var":
            return ("var", val.strip("${}"))
        if kind == "str":
            return ("str", bytes(val[1:-1], "utf-8").decode("unicode_escape"))
        if kind == "re":
            body, flags = val[1:].rsplit("/", 1)
            re.compile(body)  # RE2 is stricter, but Python catches the gross errors
            return ("re", body, flags)
        if kind == "null":
            return ("null",)
        raise ValueError(f"unexpected token {val!r} in {self.src!r}")

    def eval(self, env: dict) -> bool:
        return bool(self._eval(self.tree, env))

    def _value(self, node, env):
        t = node[0]
        if t == "var":
            return env.get(node[1])
        if t == "str":
            return node[1]
        if t == "null":
            return None
        if t == "re":
            return node
        return self._eval(node, env)

    def _eval(self, node, env):
        t = node[0]
        if t == "or":
            return self._eval(node[1], env) or self._eval(node[2], env)
        if t == "and":
            return self._eval(node[1], env) and self._eval(node[2], env)
        if t == "truthy":
            v = self._value(node[1], env)
            return bool(v) if not isinstance(v, bool) else v
        if t == "cmp":
            op, a, b = node[1], self._value(node[2], env), self._value(node[3], env)
            if op in ("==", "!="):
                return (a == b) == (op == "==")
            pattern = b if isinstance(b, tuple) else ("re", (b or "").strip("/"), "")
            flags = re.I if "i" in pattern[2] else 0
            matched = a is not None and re.search(pattern[1], str(a), flags) is not None
            return matched == (op == "=~")
        if t in ("var", "str", "null"):
            return self._value(node, env)
        raise ValueError(node)


def expand_tag(value: str, raw_env: dict) -> str:
    """A runner tag as GitLab expands it: ExpandVariables.expand_existing over the job's *unexpanded* variables
    (lib/gitlab/ci/pipeline/seed/build.rb evaluate_runner_tags), one pass, unknown variables kept literally."""
    return re.sub(r"\$\{?([A-Za-z_]\w*)\}?", lambda m: raw_env.get(m.group(1), m.group(0)), value)


def job_env(job: dict, global_vars: dict, pipeline_env: dict) -> dict:
    """Variables visible to a job's rules:if (seed-time context): the inherited global YAML variables, the job's own
    `variables`, then predefined / pipeline variables (higher precedence than YAML)."""
    inherit = (job.get("inherit") or {}).get("variables", True)
    if inherit is True:
        env = dict(global_vars)
    elif isinstance(inherit, list):
        env = {k: v for k, v in global_vars.items() if k in inherit}
    else:
        env = {}
    for k, v in (job.get("variables") or {}).items():
        env[k] = v["value"] if isinstance(v, dict) else str(v)
    env.update(pipeline_env)
    return env


def evaluate_rules(rules, env, changes: bool, job_when="on_success"):
    """Returns (when, allow_failure, rule_variables) or None when the job is not added."""
    if rules is None:
        return (job_when, job_when == "manual", {})
    for rule in rules:
        if "if" in rule and not Expr(rule["if"]).eval(env):
            continue
        if "changes" in rule and not changes:
            continue
        if "exists" in rule:
            paths = rule["exists"] if isinstance(rule["exists"], list) else rule["exists"].get("paths", [])
            if not any(list(REPO.glob(p)) for p in paths):
                continue
        when = rule.get("when", "on_success")
        if when == "never":
            return None
        return (when, rule.get("allow_failure", when == "manual"), dict(rule.get("variables") or {}))
    return None


# ── checks ───────────────────────────────────────────────────────────────────────────────────────────────────
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", default=str(REPO / ".gitlab-ci.yml"))
    ap.add_argument("--schema", help="GitLab CI JSON schema (ci.json) for an extra jsonschema validation")
    ap.add_argument("--verbose", "-v", action="store_true")
    args = ap.parse_args()

    path = Path(args.file)
    try:
        raw = yaml.load(path.read_text(encoding="utf-8"), Loader=StrictLoader)
    except yaml.YAMLError as e:
        print(f"ERROR: {path}: {e}")
        return 1
    if not isinstance(raw, dict):
        print("ERROR: top level is not a mapping")
        return 1

    doc = resolve_extends(raw)
    doc = resolve_references(doc, raw if False else doc)
    stages = [".pre"] + doc.get("stages", ["build", "test", "deploy"]) + [".post"]
    global_vars = {k: (v["value"] if isinstance(v, dict) else str(v)) for k, v in (doc.get("variables") or {}).items()}
    default = doc.get("default", {}) or {}

    # jobs (hidden templates and reserved keys excluded); apply `default:` unless inherited off
    jobs: dict[str, dict] = {}
    for name, job in doc.items():
        if name in RESERVED or name.startswith(".") or not isinstance(job, dict):
            continue
        inherit = job.get("inherit", {}).get("default", True)
        merged = dict(job)
        for k, v in default.items():
            if k in merged:
                continue
            if inherit is True or (isinstance(inherit, list) and k in inherit):
                # keys a trigger job cannot have (`interruptible` it can: Entry::Processable allows it)
                if "trigger" in job and k in ("retry", "timeout", "image", "before_script"):
                    continue
                merged[k] = copy.deepcopy(v)
        jobs[name] = merged

    # ---------------------------------------------------------------------------------------- JSON schema
    if args.schema:
        try:
            import jsonschema  # noqa: PLC0415
            schema = json.loads(Path(args.schema).read_text(encoding="utf-8"))
            resolved_raw = resolve_references(copy.deepcopy(raw), raw)
            validator = jsonschema.Draft7Validator(schema)
            for e in sorted(validator.iter_errors(json.loads(json.dumps(resolved_raw))), key=lambda e: list(e.path)):
                err(f"schema: {'/'.join(map(str, e.path)) or '<root>'}: {e.message[:300]}")
        except ImportError:
            warn("--schema given but the jsonschema package is not installed: schema validation skipped")

    # ---------------------------------------------------------------------------------------- structure
    for name, job in jobs.items():
        stage = job.get("stage", "test")
        if stage not in stages:
            err(f"{name}: stage {stage!r} is not in stages {stages[1:-1]}")
        is_trigger = "trigger" in job
        if not is_trigger and "script" not in job:
            err(f"{name}: no script")
        for key in ("needs", "dependencies"):
            for n in job.get(key, []) or []:
                target = n["job"] if isinstance(n, dict) else n
                if target not in jobs:
                    err(f"{name}: {key} references unknown job {target!r}")
                    continue
                ts = jobs[target].get("stage", "test")
                if stages.index(ts) > stages.index(stage):
                    err(f"{name}: {key} {target!r} is in a later stage ({ts} > {stage})")
        for rule in job.get("rules", []) or []:
            if not isinstance(rule, dict):
                err(f"{name}: rule is not a mapping: {rule!r}")
                continue
            unknown = set(rule) - RULE_KEYS
            if unknown:
                err(f"{name}: unknown rule keys {sorted(unknown)}")
            if rule.get("when", "on_success") not in WHEN_VALUES:
                err(f"{name}: bad rule when {rule.get('when')!r}")
            if "if" in rule:
                try:
                    Expr(rule["if"])
                except (ValueError, re.error) as e:
                    err(f"{name}: rule if {rule['if']!r}: {e}")
        # policy
        retry = job.get("retry")
        if not is_trigger:
            whens = retry.get("when", []) if isinstance(retry, dict) else []
            whens = [whens] if isinstance(whens, str) else whens
            if retry is None or whens != ["runner_system_failure"]:
                err(f"{name}: retry must be limited to runner_system_failure (got {retry!r})")
        # a non-interruptible trigger keeps its whole child pipeline running after a newer push (auto_cancel)
        if "interruptible" not in job:
            err(f"{name}: interruptible not set")
        if is_trigger:
            fwd = (job.get("trigger") or {}).get("forward", {}) if isinstance(job.get("trigger"), dict) else {}
            inherit_vars = (job.get("inherit") or {}).get("variables", True)
            if fwd.get("yaml_variables", True) and inherit_vars is True and doc.get("variables"):
                err(f"{name}: trigger job forwards every global YAML variable to the child as a pipeline variable "
                    "(outranks project variables): set inherit:variables false (or list the ones to pass)")
        arts = job.get("artifacts")
        if arts and (arts.get("paths") or arts.get("reports")) and "expire_in" not in arts:
            err(f"{name}: artifacts without expire_in")
        tags = job.get("tags", []) or []
        on_ue = any(t in UE_TAG_VARS for t in tags)
        if on_ue:
            if not job.get("resource_group"):
                err(f"{name}: self-hosted UE job without resource_group")
            if not job.get("timeout"):
                err(f"{name}: self-hosted UE job without timeout")
            if job.get("cache"):
                err(f"{name}: self-hosted UE job with a GitLab cache: (the DDC is the runner's AF_PERSISTENT_DDC)")
            if "image" in job:
                warn(f"{name}: image on a shell-executor UE runner is ignored")
        elif not is_trigger and "image" not in job and "image" not in doc:
            err(f"{name}: SaaS Docker job without image")
        # scripts referenced under unreal/CI must exist
        for line in ((job.get("before_script", []) or []) + (job.get("script", []) or [])
                     + (job.get("after_script", []) or [])):
            for ref in re.findall(r"unreal/CI/[A-Za-z0-9_./-]+\.(?:sh|py|ps1)", str(line)):
                p = REPO / ref
                if not p.is_file():
                    err(f"{name}: script {ref} does not exist")
                elif p.suffix == ".sh":
                    if not os.access(p, os.X_OK):
                        err(f"{ref} is not executable (chmod +x)")
                    if not p.read_text(encoding="utf-8").startswith("#!/usr/bin/env bash"):
                        err(f"{ref} has no bash shebang")

    # needs cycles
    graph = {n: [(x["job"] if isinstance(x, dict) else x) for x in (j.get("needs") or [])] for n, j in jobs.items()}
    state: dict[str, int] = {}

    def visit(n, path):
        if state.get(n) == 1:
            err(f"needs cycle: {' -> '.join(path + [n])}")
            return
        if state.get(n) == 2:
            return
        state[n] = 1
        for m in graph.get(n, []):
            if m in graph:
                visit(m, path + [n])
        state[n] = 2

    for n in graph:
        visit(n, [])

    # secrets must never be written into the file
    for scope, vars_ in [("variables", doc.get("variables") or {})] + [
            (n, j.get("variables") or {}) for n, j in jobs.items()]:
        for k, v in vars_.items():
            val = v["value"] if isinstance(v, dict) else v
            if SECRET_NAME.search(k) and val not in (None, "") and not str(val).startswith("$"):
                err(f"{scope}: variable {k} has a value in the file; secrets belong in masked CI/CD variables")

    # ---------------------------------------------------------------------------------------- simulation
    predefined = {"CI_DEFAULT_BRANCH": "main", "CI_PROJECT_ID": "1", "GITLAB_CI": "true"}
    prot = {"CI_COMMIT_REF_PROTECTED": "true"}
    unprot = {"CI_COMMIT_REF_PROTECTED": "false"}
    contexts = {
        "merge_request": {"CI_PIPELINE_SOURCE": "merge_request_event", "CI_MERGE_REQUEST_IID": "7",
                          "CI_COMMIT_REF_NAME": "feature/x", **unprot},
        "branch": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_BRANCH": "feature/x", "CI_COMMIT_REF_NAME": "feature/x",
                   **unprot},
        "branch_protected": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_BRANCH": "release/1.2",
                             "CI_COMMIT_REF_NAME": "release/1.2", **prot},
        "branch_with_mr": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_BRANCH": "feature/x",
                           "CI_OPEN_MERGE_REQUESTS": "group/abyssfire!7", **unprot},
        "default_branch": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_BRANCH": "main", "CI_COMMIT_REF_NAME": "main",
                           **prot},
        "schedule": {"CI_PIPELINE_SOURCE": "schedule", "CI_COMMIT_BRANCH": "main", "CI_COMMIT_REF_NAME": "main", **prot},
        "tag": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_TAG": "v1.2.3", "CI_COMMIT_REF_NAME": "v1.2.3", **prot},
        "tag_prerelease": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_TAG": "v1.3.0-rc.1", **prot},
        "tag_other": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_TAG": "nightly-2026", **prot},
        "tag_unprotected": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_TAG": "v9.9.9", **unprot},
        "package_all_child": {"CI_PIPELINE_SOURCE": "parent_pipeline", "CI_COMMIT_BRANCH": "release/1.2",
                              "AF_PACKAGE_ALL": "1", **prot},
        "web_package_all": {"CI_PIPELINE_SOURCE": "web", "CI_COMMIT_BRANCH": "release/1.2", "AF_PACKAGE_ALL": "1",
                            **prot},
        "web_package_all_unprotected": {"CI_PIPELINE_SOURCE": "web", "CI_COMMIT_BRANCH": "feature/x",
                                        "AF_PACKAGE_ALL": "1", **unprot},
        "android_on_windows": {"CI_PIPELINE_SOURCE": "push", "CI_COMMIT_TAG": "v1.2.3",
                               "UE_ANDROID_RUNNER_SHELL": "pwsh", "UE_ANDROID_RUNNER_TAG": "ue5-win", **prot},
    }
    verify_jobs = {n for n, j in jobs.items() if j.get("stage") == "verify"}
    ue_jobs = {n for n, j in jobs.items() if any(t in UE_TAG_VARS for t in j.get("tags", []) or [])}
    results: dict[str, dict[str, tuple]] = {}
    for cname, cvars in contexts.items():
        pipeline_env = {**predefined, **cvars}
        wf = evaluate_rules((doc.get("workflow") or {}).get("rules"), {**global_vars, **pipeline_env}, changes=True)
        if wf is None:
            results[cname] = {}
            continue
        run: dict[str, tuple] = {}
        for name, job in jobs.items():
            try:
                r = evaluate_rules(job.get("rules"), job_env(job, global_vars, pipeline_env), changes=True,
                                   job_when=job.get("when", "on_success"))
            except (ValueError, re.error) as e:
                err(f"{name}: {e}")
                continue
            if r is not None:
                run[name] = r
        results[cname] = run
        # needs closure: GitLab rejects a pipeline whose job needs a job that is not in it
        for name in run:
            needs = jobs[name].get("needs") or []
            for n in needs:
                target, optional = (n["job"], n.get("optional", False)) if isinstance(n, dict) else (n, False)
                if target not in run and not optional:
                    err(f"[{cname}] {name} needs {target!r}, which is not in this pipeline")
            if name == "installer:android":
                if not ({"package:android", "package:android-win"} & set(run)):
                    err(f"[{cname}] installer:android has neither package:android nor package:android-win")
        # tags resolve to non-empty literal strings (one non-recursive pass over the raw variables, like GitLab)
        for name in run:
            raw_env = {**job_env(jobs[name], global_vars, pipeline_env), **run[name][2]}
            for t in jobs[name].get("tags", []) or []:
                value = expand_tag(t, raw_env)
                if not value.strip() or "$" in value:
                    err(f"[{cname}] {name}: tag {t!r} expands to {value!r} (GitLab expands tags in one pass: "
                        "a variable used as a tag must hold a literal value)")

    def auto(c):
        return {n for n, (w, _, _) in results[c].items() if w != "manual"}

    def manual(c):
        return {n for n, (w, _, _) in results[c].items() if w == "manual"}

    def rule_vars(c, job):
        return results[c].get(job, (None, None, {}))[2]

    full_auto = {"ue:content", "package:mac", "package:win64", "package:android", "installer-assets",
                 "installer:mac-dmg", "installer:windows", "installer:android"}
    unprotected = [c for c, v in contexts.items() if v.get("CI_COMMIT_REF_PROTECTED") != "true"]
    expectations = [
        ("merge_request", auto("merge_request") <= verify_jobs | {"art:blender-smoke"},
         "runs only verify-stage jobs (and the Blender smoke test) automatically"),
        ("merge_request", "art:blender-smoke" in auto("merge_request"), "smoke-tests Blender changes before merging"),
        ("branch", "package-all" not in results["branch"], "offers no package-all on an unprotected branch"),
        ("branch_protected", not (auto("branch_protected") & ue_jobs), "starts no Unreal job automatically"),
        ("branch_protected", "package-all" in manual("branch_protected"), "offers the manual package-all trigger"),
        ("branch_protected", rule_vars("branch_protected", "package-all").get("AF_PACKAGE_ALL") == "1",
         "passes AF_PACKAGE_ALL=1 to the package-all child"),
        ("branch_with_mr", results["branch_with_mr"] == {}, "is suppressed by workflow (the MR pipeline runs)"),
        ("default_branch", full_auto <= auto("default_branch"), "builds every package and installer"),
        ("default_branch", "release" not in results["default_branch"], "does not release"),
        ("default_branch", "package:ios" in manual("default_branch"), "offers iOS as a manual job"),
        ("default_branch", "package-all" not in results["default_branch"], "has no package-all trigger"),
        ("schedule", full_auto <= auto("schedule"), "builds everything (nightly)"),
        ("tag", full_auto | {"release"} <= auto("tag"), "builds everything and releases"),
        ("tag", {"package:ios", "release:ios"} <= manual("tag"), "offers iOS build + upload as manual jobs"),
        ("tag_prerelease", "release" in auto("tag_prerelease"), "releases a -pre tag"),
        ("tag_other", "release" not in results["tag_other"], "does not release a non-version tag"),
        ("package_all_child", full_auto <= auto("package_all_child"), "builds everything"),
        ("package_all_child", not ((verify_jobs - {"version"}) & set(results["package_all_child"])),
         "does not repeat the verify jobs"),
        ("package_all_child", not ({"release", "release:ios", "package-all"} & set(results["package_all_child"])),
         "neither releases nor recurses"),
        ("web_package_all", full_auto <= auto("web_package_all"), "builds everything"),
        ("web_package_all", "package-all" not in results["web_package_all"], "offers no redundant package-all"),
        ("tag_unprotected", not ({"release", "release:ios"} & set(results["tag_unprotected"])),
         "does not release (no installers without the Protected runners)"),
        ("tag", rule_vars("tag", "package:mac").get("GIT_CLEAN_FLAGS") == "-ffdx",
         "builds release tags from a fully clean working copy"),
        ("android_on_windows", "package:android-win" in auto("android_on_windows")
         and "package:android" not in results["android_on_windows"], "switches Android to the PowerShell job"),
    ]
    for ctx in unprotected:
        expectations.append((ctx, not (set(results[ctx]) & ue_jobs),
                             "creates no job for the Protected Unreal runners (it would stay pending and hold the "
                             "resource group)"))
        expectations.append((ctx, "package-all" not in results[ctx], "offers no package-all"))
    for ctx, ok, what in expectations:
        if not ok:
            err(f"pipeline '{ctx}' {what}: FAILED (jobs: {sorted(results[ctx])})")

    # ---------------------------------------------------------------------------------------- report
    print(f"{path}: {len(jobs)} jobs, stages {stages[1:-1]}")
    short = {"merge_request": "MR", "branch": "branch", "branch_protected": "br-prot", "branch_with_mr": "br+MR",
             "default_branch": "main", "schedule": "sched", "tag": "tag", "tag_prerelease": "tag-rc",
             "tag_other": "tag-oth", "tag_unprotected": "tag-unp", "package_all_child": "child",
             "web_package_all": "web-all", "web_package_all_unprotected": "web-unp",
             "android_on_windows": "and-win"}
    width = max(len(n) for n in jobs)
    header = "job".ljust(width) + "  stage       " + "  ".join(short.get(c, c)[:7].ljust(7) for c in contexts)
    if args.verbose or not errors:
        print(header)
        for name in sorted(jobs, key=lambda n: (stages.index(jobs[n].get("stage", "test")), n)):
            cells = []
            for c in contexts:
                r = results[c].get(name)
                cells.append(("-" if r is None else ("manual" if r[0] == "manual" else "auto")).ljust(7))
            print(name.ljust(width) + "  " + jobs[name].get("stage", "test").ljust(10) + "  " + "  ".join(cells))
        tagsets = sorted({(n, ", ".join(expand_tag(t, job_env(jobs[n], global_vars, predefined))
                                         for t in jobs[n].get("tags", []))) for n in ue_jobs})
        print("self-hosted runner tags (defaults): " + "; ".join(f"{n}=[{t}]" for n, t in tagsets))
    for w in warnings:
        print(f"WARNING: {w}")
    for e in errors:
        print(f"ERROR: {e}")
    print("OK" if not errors else f"{len(errors)} error(s)")
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
