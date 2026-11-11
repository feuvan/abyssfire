#!/usr/bin/env bash
# shellcheck disable=SC2016 # the stub bodies below are literal scripts, expanded when the stubs run
# Logic test of unreal/Scripts/release_mac.sh on Linux (or a Mac without Unreal): the script runs in a throw-away git
# repository with a github.com origin, against the fake engine and stub Apple tools of fake_mac.sh and a fake `gh`
# that records every call, can be told what exists on "GitHub" (login, the commit, a release, a bare tag) and checks
# the files it is handed. It checks the flow, not Unreal, Apple's tools or GitHub: the order of the child scripts and
# what reaches their environment (the version; no CI_COMMIT_TAG, signing secret or GitHub token), the preflight
# refusals and that nothing is built before them, --from and its stale-leftover guards, the `gh release create`
# arguments, --no-publish / --draft / --replace / --config, local signing versus ad-hoc, the release notes (content,
# nothing of this machine), the failure paths, and that the script keeps to bash 3.2.
#
#   unreal/CI/tools/test_release_mac.sh [WORKDIR]
# Tools: bash, git, python3, jq (the fake gh evaluates the script's --jq expressions with it), zip/unzip. Nothing
# outside WORKDIR is touched (HOME is redirected); no network.
set -euo pipefail
command -v jq >/dev/null 2>&1 || {
  echo "test_release_mac.sh needs jq (apt install jq / brew install jq): the fake gh runs the --jq expressions" >&2
  exit 1
}
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci="$(cd "$here/.." && pwd)"
unreal_src="$(cd "$ci/.." && pwd)"
W="${1:-$(mktemp -d "${TMPDIR:-/tmp}/af-relmac-XXXXXX")}"
mkdir -p "$W"
W="$(cd "$W" && pwd)"
rm -rf "${W:?}/repo" "${W:?}/UE" "${W:?}/bin" "${W:?}/ghbin" "${W:?}/linuxbin" "${W:?}/home" "${W:?}/Volumes" \
  "${W:?}/gh" "${W:?}/calls.log"
mkdir -p "$W/bin" "$W/ghbin" "$W/linuxbin" "$W/home" "$W/Volumes" "$W/gh"
fails=0
ok() { echo "  ok   $*"; }
bad() {
  echo "  FAIL $*"
  fails=$((fails + 1))
}
check() { # check DESCRIPTION COMMAND... -> ok / FAIL
  local what="$1"
  shift
  if "$@"; then ok "$what"; else bad "$what"; fi
}
has_call() { grep -q -- "$1" "$W/calls.log"; }
no_call() { ! grep -q -- "$1" "$W/calls.log"; }
in_log() { grep -q -- "$2" "$W/$1.log"; } # in_log SLUG PATTERN
nothing_built() { no_call '^Build.sh \|^UnrealEditor-Cmd \|^RunUAT \|^hdiutil \|^codesign \|^gh release create'; }

# ------------------------------------------------------------------- fake engine + Apple stubs ($W/UE, $W/bin)
real_python="$(command -v python3)"
# shellcheck source=fake_mac.sh
source "$here/fake_mac.sh"
# The content step is unreal/Scripts/build_content.sh, which starts the editor without the CI DDC variable: an
# editor that only records what it was given.
cat >"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" <<'EOF'
#!/usr/bin/env bash
echo "UnrealEditor-Cmd $*" >>"$AF_TEST_CALLS"
env | sort >"$AF_TEST_ENV_DIR/editor.env"
proj="$(dirname "$1")"
mkdir -p "$proj/Content/Abyssfire/Maps"
echo umap >"$proj/Content/Abyssfire/Maps/L_Main.umap"
for a in "$@"; do case "$a" in -abslog=*) echo "LogPython: content built" >"${a#-abslog=}" ;; esac; done
echo '{"ok": true, "assets": 1}' >"$AF_CONTENT_REPORT"
EOF
# RunUAT: record the environment, refuse what must never reach the engine, then the shared fake.
mv "$UE/Engine/Build/BatchFiles/RunUAT.sh" "$UE/Engine/Build/BatchFiles/RunUAT.fake.sh"
cat >"$UE/Engine/Build/BatchFiles/RunUAT.sh" <<'EOF'
#!/usr/bin/env bash
env | sort >"$AF_TEST_ENV_DIR/uat.env"
if env | grep -Eq '^(CI_COMMIT_TAG|GITLAB_CI|GH_TOKEN|GITHUB_TOKEN|GH_HOST|AF_MAC_SIGN_IDENTITY|AF_NOTARY_KEYCHAIN_PROFILE|AF_REQUIRE_SIGNING|AF_CLEAN_BUILD)='; then
  echo "RunUAT (refused) $*" >>"$AF_TEST_CALLS"
  echo "a CI / signing / GitHub variable reached the engine"
  exit 21
fi
repo_root="$AF_TEST_ENV_DIR/repo"
case "${AF_TEST_UAT_DOES:-}" in
  edit) echo '{"edited": "while the cook ran"}' >"$repo_root/unreal/Data/items.json" ;; # the user keeps working
  commit) git -C "$repo_root" commit -q --allow-empty -m "unreal: committed while the cook ran" ;;
  interrupt)
    # Ctrl-C / a kill of the package step: the signal reaches ue-package.sh (our parent) and us
    echo "RunUAT (interrupted) $*" >>"$AF_TEST_CALLS"
    kill -INT "$PPID"
    kill -INT "$$"
    exit 130
    ;;
esac
exec "$(dirname "$0")/RunUAT.fake.sh" "$@"
EOF
chmod +x "$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$UE/Engine/Build/BatchFiles/RunUAT.sh"
# mac-dmg.sh builds a venv with pinned wheels for the window layout (covered by test_mac_scripts.sh): not here.
printf '#!/usr/bin/env bash\nif [[ "${1:-} ${2:-}" == "-m venv" ]]; then exit 1; fi\nexec "%s" "$@"\n' "$real_python" \
  >"$W/bin/python3"
chmod +x "$W/bin/python3"
printf '#!/usr/bin/env bash\necho Linux\n' >"$W/linuxbin/uname"
chmod +x "$W/linuxbin/uname"

# --------------------------------------------------------------------------------------------------- fake gh
# As strict as gh 2.89 where the script could go wrong: every subcommand takes only the flags the real one has
# ("unknown flag" otherwise), --json only the real field names, --jq expressions are run by jq on the JSON gh would
# print, --target must be a full sha, --hostname github.com. GH_HOST / GH_REPO must not reach it.
# State in $AF_TEST_GH: files `auth`, `commit`, `release` (content: true = draft), `tag`; switches `push-false`,
# `commit-404` (a missing commit answers 404, not 422), `not-prerelease` (the release is a full one), `one-asset`,
# `delete-fails-after` (release delete removes the release, then fails on the tag), `create-fail`,
# `create-leaves-draft`. `release create` saves its arguments (one per line) and the notes. Every call that changes
# something is also logged as "gh-mutation <what> after-summary|BEFORE-summary" (was the summary on screen?).
cat >"$W/ghbin/gh" <<'EOF'
#!/usr/bin/env bash
echo "gh $*" >>"$AF_TEST_CALLS"
s="$AF_TEST_GH"
if [[ -n "${GH_HOST:-}${GH_REPO:-}" ]]; then echo "fake gh: GH_HOST / GH_REPO reached gh" >&2; exit 6; fi
fail() { echo "$*" >&2; exit 1; }
not_found() { fail "gh: Not Found (HTTP 404)"; }
mutation() {
  local when=BEFORE-summary
  if grep -q '^  kind   ' "${AF_TEST_RUN_LOG:-/dev/null}" 2>/dev/null; then when=after-summary; fi
  echo "gh-mutation $1 $when" >>"$AF_TEST_CALLS"
}
jq_expr="" json_fields="" repo="" positional=()
emit() { # emit JSON -> what gh prints: the JSON, or the --jq result (raw strings)
  if [[ -n "$jq_expr" ]]; then
    printf '%s\n' "$1" | jq -r "$jq_expr" || fail "failed to parse jq expression"
  else
    printf '%s\n' "$1"
  fi
}
need_repo() { [[ "$repo" == "acme/abyssfire" ]] || fail "GraphQL: Could not resolve to a Repository with the name '$repo'."; }
release_json() { # the fields --json asked for
  local f pre=true assets='[{"name":"a.dmg"},{"name":"SHA256SUMS.txt"}]'
  [[ -n "$json_fields" ]] || fail "fake gh: release view without --json is not expected"
  for f in ${json_fields//,/ }; do
    case "$f" in
      apiUrl | assets | author | body | createdAt | databaseId | id | isDraft | isImmutable | isPrerelease | name | \
        publishedAt | tagName | tarballUrl | targetCommitish | uploadUrl | url | zipballUrl) ;;
      *) fail "Unknown JSON field: \"$f\"" ;;
    esac
  done
  if [[ -f "$s/not-prerelease" ]]; then pre=false; fi
  if [[ -f "$s/one-asset" ]]; then assets='[{"name":"a.dmg"}]'; fi
  jq -c -n --arg tag "$1" --arg fields "$json_fields" --argjson draft "$(cat "$s/release")" --argjson pre "$pre" \
    --argjson assets "$assets" '
    {url: ("https://github.com/acme/abyssfire/releases/tag/" + $tag), isDraft: $draft, isPrerelease: $pre,
     assets: $assets, name: ("Abyssfire Unreal " + $tag), tagName: $tag}
    | with_entries(select(.key as $k | ($fields | split(",") | index($k))))'
}
cmd="$1 ${2:-}"
case "$cmd" in
  "auth token")
    shift 2
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --hostname | -h) [[ "$2" == "github.com" ]] || fail "no oauth token found for $2"; shift 2 ;;
        *) fail "unknown flag: $1" ;;
      esac
    done
    [[ -f "$s/auth" ]] || fail "no oauth token found for github.com"
    echo "gho_faketokenOUTPUT"
    ;;
  "api "*)
    shift
    method=GET path=""
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --method | -X) method="$2"; shift 2 ;;
        --jq | -q) jq_expr="$2"; shift 2 ;;
        --hostname) [[ "$2" == "github.com" ]] || fail "fake gh: wrong host $2"; shift 2 ;;
        -*) fail "unknown flag: $1" ;;
        *) [[ -z "$path" ]] || fail "accepts 1 arg(s), received 2"; path="$1"; shift ;;
      esac
    done
    [[ -f "$s/auth" ]] || fail "gh: Bad credentials (HTTP 401)"
    case "$method $path" in
      "GET repos/acme/abyssfire")
        if [[ -f "$s/push-false" ]]; then emit '{"full_name":"acme/abyssfire","permissions":{"push":false}}'; else
          emit '{"full_name":"acme/abyssfire","permissions":{"push":true}}'
        fi
        ;;
      "GET repos/acme/abyssfire/commits/"*)
        if [[ ! -f "$s/commit" ]]; then
          if [[ -f "$s/commit-404" ]]; then not_found; fi
          fail "gh: No commit found for SHA: ${path##*/} (HTTP 422)"
        fi
        emit "{\"sha\":\"${path##*/}\"}"
        ;;
      "GET repos/acme/abyssfire/git/ref/tags/"*)
        [[ -f "$s/tag" ]] || not_found
        emit "{\"ref\":\"refs/tags/${path##*/tags/}\"}"
        ;;
      "DELETE repos/acme/abyssfire/git/refs/tags/"*)
        mutation "delete-tag"
        [[ -f "$s/tag" ]] || fail "gh: Reference does not exist (HTTP 422)"
        rm -f "$s/tag"
        ;;
      *) not_found ;;
    esac
    ;;
  "release view" | "release delete" | "release create")
    shift 2
    yes=0 cleanup=0 draft=false pre=false target="" title="" notes=""
    while [[ $# -gt 0 ]]; do
      case "$cmd $1" in
        *" --repo" | *" -R") repo="$2"; shift 2 ;;
        "release view --json") json_fields="$2"; shift 2 ;;
        "release view --jq" | "release view -q") jq_expr="$2"; shift 2 ;;
        "release delete --yes" | "release delete -y") yes=1; shift ;;
        "release delete --cleanup-tag") cleanup=1; shift ;;
        "release create --target") target="$2"; shift 2 ;;
        "release create --title" | "release create -t") title="$2"; shift 2 ;;
        "release create --notes-file" | "release create -F") notes="$2"; shift 2 ;;
        "release create --prerelease" | "release create -p") pre=true; shift ;;
        "release create --draft" | "release create -d") draft=true; shift ;;
        *)
          if [[ "$1" == -* ]]; then fail "unknown flag: $1"; fi
          positional+=("$1")
          shift
          ;;
      esac
    done
    [[ ${#positional[@]} -ge 1 ]] || fail "tag required"
    need_repo
    tag="${positional[0]}"
    case "$cmd" in
      "release view")
        [[ ${#positional[@]} -eq 1 ]] || fail "accepts at most 1 arg(s)"
        [[ -z "$jq_expr" || -n "$json_fields" ]] || fail "cannot use \`--jq\` without specifying \`--json\`"
        [[ -f "$s/release" ]] || fail "release not found"
        emit "$(release_json "$tag")"
        ;;
      "release delete")
        [[ ${#positional[@]} -eq 1 ]] || fail "accepts 1 arg(s)"
        [[ $yes -eq 1 ]] || { echo "gh would prompt" >&2; exit 4; }
        mutation "delete-release"
        [[ -f "$s/release" ]] || fail "release not found"
        rm -f "$s/release"
        if [[ -f "$s/delete-fails-after" ]]; then fail "HTTP 422: Reference does not exist (the draft had no tag)"; fi
        if [[ $cleanup -eq 1 ]]; then rm -f "$s/tag"; fi
        ;;
      "release create")
        mutation "create-release"
        [[ -n "${GH_TOKEN:-}" ]] || { echo "the GitHub token did not reach gh" >&2; exit 5; }
        [[ "$target" =~ ^[0-9a-f]{40}$ ]] || fail "fake gh: --target must be the full commit sha (got '$target')"
        [[ -n "$title" && -s "$notes" ]] || fail "fake gh: --title and a non-empty --notes-file are expected"
        [[ ! -f "$s/release" && ! -f "$s/tag" ]] || fail "HTTP 422: Validation Failed (tag_name already_exists)"
        if [[ -f "$s/create-fail" ]]; then
          if [[ -f "$s/create-leaves-draft" ]]; then echo true >"$s/release"; fi
          fail "HTTP 502: Bad Gateway (upload)"
        fi
        printf '%s\n' release create "$tag" --repo "$repo" --target "$target" --title "$title" --notes-file "$notes" >"$s/create.args"
        if [[ "$pre" == true ]]; then echo --prerelease >>"$s/create.args"; fi
        if [[ "$draft" == true ]]; then echo --draft >>"$s/create.args"; fi
        for a in "${positional[@]:1}"; do
          [[ "$a" == *"#"* && -s "${a%%#*}" ]] || { echo "asset ${a%%#*} does not exist (or has no #label)" >&2; exit 3; }
          echo "$a" >>"$s/create.args"
        done
        cp "$notes" "$s/published-notes.md" || exit 3
        echo "$draft" >"$s/release"
        if [[ "$draft" == false ]]; then : >"$s/tag"; fi
        echo "https://github.com/acme/abyssfire/releases/tag/$tag"
        ;;
    esac
    ;;
  *) echo "fake gh: unexpected call: $*" >&2; exit 9 ;;
esac
EOF
chmod +x "$W/ghbin/gh"

# ---------------------------------------------------------------------------- throw-away repository ($W/repo)
R="$W/repo"
mkdir -p "$R/unreal/CI" "$R/unreal/Scripts" "$R/unreal/Config" "$R/unreal/Data" "$R/unreal/Fonts" "$R/src"
for d in lib scripts installers version.sh .gitignore; do cp -R "$ci/$d" "$R/unreal/CI/"; done
cp "$unreal_src/Scripts/release_mac.sh" "$unreal_src/Scripts/build_content.sh" "$R/unreal/Scripts/"
cp "$unreal_src/.gitignore" "$R/unreal/.gitignore"
echo 'print("build_content")' >"$R/unreal/Scripts/build_content.py"
echo '{}' >"$R/unreal/Data/items.json"
echo 'ttf' >"$R/unreal/Fonts/Test-Regular.ttf"
echo '{ "FileVersion": 3 }' >"$R/unreal/Abyssfire.uproject"
printf '[/Script/EngineSettings.GeneralProjectSettings]\nProjectName=Abyssfire\nProjectVersion=0.1.0\n' \
  >"$R/unreal/Config/DefaultGame.ini"
echo 'web' >"$R/src/main.ts"

# PATH without any real gh; the fake one is added per run (WITH_GH)
base_path=""
IFS=: read -r -a path_dirs <<<"$PATH"
for d in "${path_dirs[@]}"; do
  if [[ -n "$d" && ! -x "$d/gh" ]]; then base_path="${base_path:+$base_path:}$d"; fi
done
export HOME="$W/home" USER=zzlocaluser LOGNAME=zzlocaluser
export PATH="$W/bin:$base_path"
WITH_GH="$W/ghbin:$PATH"
export AF_TEST_CALLS="$W/calls.log" AF_TEST_VOLUMES="$W/Volumes" AF_TEST_ENV_DIR="$W" AF_TEST_GH="$W/gh"
export AF_PLISTBUDDY="$W/bin/PlistBuddy" UE_ROOT="$UE"
unset UE_ROOT_MAC AF_OUT_DIR AF_LOG_DIR AF_VERSION AF_MAC_ARCH AF_PERSISTENT_DDC AF_UE_EXTRA_ARGS
unset AF_RELEASE_MAX_ASSET_BYTES AF_XCODE_DEVELOPER_DIR AF_TEST_UAT_FAIL AF_TEST_DROP AF_TEST_NO_METAL
unset AF_TEST_UAT_DOES AF_TEST_TWO_IDS AF_TEST_RUN_LOG

g() { git -C "$R" "$@"; }
commit() { # commit DATE SUBJECT
  GIT_AUTHOR_DATE="$1" GIT_COMMITTER_DATE="$1" g commit -q -m "$2"
}
g init -q
g symbolic-ref HEAD refs/heads/main
g config user.name "Local Dev"
g config user.email "local.dev@example.com"
g config commit.gpgsign false
g remote add origin git@github.com:acme/abyssfire.git
g add -A
commit "2026-03-01T10:00:00+08:00" "unreal: first import"
g tag unreal-v0.0.9
echo '{"a": 1}' >"$R/unreal/Data/items.json"
g add -A
commit "2026-03-02T10:00:00+08:00" "unreal: sharper telegraphs (thanks dev@example.com, @someone)"
# a subject may name a path: the engine's documented place, and (masked in the notes) this machine's home and clone
echo '{"a": 2}' >"$R/unreal/Data/items.json"
g add -A
commit "2026-03-03T10:00:00+08:00" "unreal: UE_ROOT defaults to /Users/Shared/Epic Games/UE_5.8 (was $W/home/UE, see $R/unreal)"
echo 'web 2' >"$R/src/main.ts"
g add -A
commit "2026-03-04T05:06:07+08:00" "web: not part of the remake"
sha="$(g rev-parse HEAD)"
ver="0.1.0-pre.20260304.${sha:0:8}"
tag="unreal-v$ver"
OUT="$R/unreal/CI/out"
REL="$OUT/release/mac"
dmg="$REL/Abyssfire-$ver-macos-arm64.dmg"
dev_id="Developer ID Application: Test (TEAM123456)"
dev_sha1="ABCDEF0123456789ABCDEF0123456789ABCDEF01" # its SHA-1 in the fake `security find-identity`

# What must never steer or leak into a local run: GitLab's variables, CI signing secrets, kit switches (with
# AF_REQUIRE_SIGNING the ad-hoc DMG step would die after the cook; AF_CLEAN_BUILD reaching the package step would
# delete the editor binaries the content step built), another GitHub host or repository.
export CI_COMMIT_TAG=v9.9.9 CI_COMMIT_SHORT_SHA=deadbeef CI_PIPELINE_IID=7 GITLAB_CI=1
export AF_MAC_DEVID_P12_B64=UDEyREFUQQ== AF_MAC_DEVID_P12_PASSWORD=p12-password AF_SKIP_NOTARIZE=1
export AF_MAC_SIGN_IDENTITY="Apple Distribution: Test (TEAM123456)" GH_TOKEN=ghp_testtoken0000
export AF_REQUIRE_SIGNING=1 AF_CLEAN_BUILD=1 GH_HOST=ghe.example.invalid GH_REPO=other/repo

rel() { # rel SLUG ARGS... -> release_mac.sh with the fake gh on PATH, stdin not a terminal; log $W/SLUG.log
  local slug="$1"
  shift
  : >"$W/calls.log"
  : >"$W/$slug.log"
  # shellcheck disable=SC2094 # the fake gh only looks into the log for the summary (gh-mutation ... after-summary)
  AF_TEST_RUN_LOG="$W/$slug.log" PATH="$WITH_GH" bash "$R/unreal/Scripts/release_mac.sh" "$@" >>"$W/$slug.log" 2>&1 </dev/null
}
rel_nogh() { # the same without any gh on PATH
  local slug="$1"
  shift
  : >"$W/calls.log"
  bash "$R/unreal/Scripts/release_mac.sh" "$@" >"$W/$slug.log" 2>&1 </dev/null
}
runs_ok() { # runs_ok DESCRIPTION SLUG ARGS...
  local what="$1" st=0
  shift
  rel "$@" || st=$?
  if [[ $st -eq 0 ]]; then ok "$what"; else bad "$what: exit $st (see $W/$1.log)"; fi
}
refused() { # refused DESCRIPTION PATTERN SLUG ARGS... -> must fail, say PATTERN, and build / publish nothing
  local what="$1" pattern="$2" st=0
  shift 2
  rel "$@" || st=$?
  if [[ $st -ne 0 ]] && in_log "$1" "$pattern" && nothing_built; then ok "$what"; else
    bad "$what: exit $st, message or side effects wrong (see $W/$1.log)"
  fi
}
gh_state() { # gh_state NAME... -> exactly these state files exist
  rm -rf "${W:?}/gh"
  mkdir -p "$W/gh"
  local n
  for n in "$@"; do echo false >"$W/gh/$n"; done
}
create_arg_after() { awk -v f="$1" 'p { print; exit } $0 == f { p = 1 }' "$W/gh/create.args"; }
create_has() { grep -qxF -- "$1" "$W/gh/create.args"; }
# (|| true: nothing found is an empty answer for the check that follows, not the end of the test under pipefail)
line_of() { grep -n -- "$1" "$W/calls.log" | sed -n 1p | cut -d: -f1 || true; }
notes_have() { grep -qF -- "$1" "$REL/release-notes.md"; }
notes_lack() { ! grep -qF -- "$1" "$REL/release-notes.md"; }
tree_clean() { [[ -z "$(g status --porcelain --untracked-files=no)" ]]; }

echo "=== lint: bash 3.2 / BSD userland (by construction: no bash 3.2 runs here)"
code="$W/release_mac.code"
grep -v '^[[:space:]]*#' "$unreal_src/Scripts/release_mac.sh" >"$code"
lint() { # lint WHAT REGEX -> the construct must not appear in the code lines
  if grep -nE -- "$2" "$code" >"$W/lint.txt"; then bad "$1: $(head -3 "$W/lint.txt" | tr '\n' ' ')"; else ok "no $1"; fi
}
lint "associative arrays" '(declare|local|typeset)[[:space:]]+-[a-zA-Z]*A'
lint "mapfile / readarray" '(^|[^[:alnum:]_])(mapfile|readarray)([^[:alnum:]_]|$)'
lint 'case conversion ${var,,} / ${var^^}' '\$\{[^}]*(,,|\^\^|[^[:space:]\\],\}|\^\})'
lint '&>> or |&' '&>>|\|&'
lint "coproc" '(^|[^[:alnum:]_])coproc([^[:alnum:]_]|$)'
lint "namerefs (local -n / declare -n)" '(declare|local|typeset)[[:space:]]+-[a-zA-Z]*n'
lint '[[ -v ]]' '\[\[[[:space:]]+(![[:space:]]+)?-v[[:space:]]'
lint "wait -n" 'wait[[:space:]]+-n'
lint "negative array indexes" '\[-[0-9]+\]'
lint '${var@Q} style transformations' '\$\{[A-Za-z_][A-Za-z_0-9]*(\[[^]]*\])?@[A-Za-z]\}'
lint "GNU-only sed -i / readlink -f / date -d / stat -c / grep -P / sort -V / mktemp --" \
  '(^|[^[:alnum:]_])(sed[[:space:]]+(-[a-zA-Z]*[[:space:]]+)*-i|readlink[[:space:]]+-f|date[[:space:]]+(-d|--date)|stat[[:space:]]+-c|grep[[:space:]]+-[a-zA-Z]*P|sort[[:space:]]+-[a-zA-Z]*V|mktemp[[:space:]]+--)'
check "set -euo pipefail" grep -qx 'set -euo pipefail' "$code"
check "release_mac.sh is executable" test -x "$unreal_src/Scripts/release_mac.sh"
check "the minimum macOS is the one the GitLab release notes state" bash -c \
  "grep -q \"macOS \$(sed -n 's/^MIN_MACOS=\"\\(.*\\)\"/\\1/p' '$unreal_src/Scripts/release_mac.sh')+ (Apple Silicon)\" '$ci/scripts/release.sh'"

echo "=== --help and option errors (nothing is built, GitHub is not asked)"
if rel help --help && nothing_built && no_call '^gh '; then ok "--help exits 0"; else bad "--help"; fi
for o in --version --tag --repo --config --from --no-publish --draft --replace --notes-file --sign-identity \
  --notary-profile --shaders --allow-dirty --yes --help; do
  check "$o is in --help" grep -q -- "^  $o\b\|^  -h, $o\b" "$W/help.log"
  check "$o is in Scripts/README.md" grep -qF -- "\`$o" "$unreal_src/Scripts/README.md"
done
gh_state auth commit
usage_error() { # usage_error DESCRIPTION PATTERN SLUG ARGS... -> exit 2
  local what="$1" pattern="$2" st=0
  shift 2
  rel "$@" || st=$?
  if [[ $st -eq 2 ]] && in_log "$1" "$pattern" && nothing_built && no_call '^gh '; then ok "$what"; else
    bad "$what: exit $st (see $W/$1.log)"
  fi
}
usage_error "a tag starting with v is refused, naming the web release workflow" 'release.yml' tag-v --tag v1.0.0 --yes
usage_error "a tag git would reject is refused" 'not a usable tag name' tag-bad --tag 'a..b' --yes
usage_error "a tag with a space is refused" 'not a usable tag name' tag-space --tag 'a b' --yes
usage_error "a version that is not X.Y.Z[-pre] is refused" 'is not X.Y.Z' version-bad --version 1.2 --yes
usage_error "--allow-dirty without --no-publish is refused" 'only works with --no-publish' dirty-flag --allow-dirty --yes
usage_error "--notary-profile without --sign-identity is refused" 'needs --sign-identity' notary-alone --notary-profile p --yes
usage_error "an unknown --config is refused" 'Shipping, Development or Test' config-bad --config Debug --yes
usage_error "an unknown --from is refused" 'content, package, dmg or publish' from-bad --from cook --yes
usage_error "a missing --notes-file is refused" 'not a readable file' notes-missing --notes-file "$W/none.md" --yes
usage_error "an unknown option is refused" 'unknown argument' unknown --publish
usage_error "--repo that is not OWNER/NAME is refused" 'is not OWNER/NAME' repo-bad --repo https://github.com/acme/abyssfire
refused "a version outside the kit's range is refused (version.sh)" 'not usable' version-range --version 300.0.0 --yes

echo "=== preflight refusals (before anything is built)"
refused "no terminal and no --yes" 'pass --yes' no-tty
if PATH="$W/linuxbin:$WITH_GH" bash "$R/unreal/Scripts/release_mac.sh" --yes >"$W/linux.log" 2>&1 </dev/null; then
  bad "ran on a host that is not macOS"
else check "a host that is not macOS is refused" in_log linux 'macOS only'; fi
st=0
rel_nogh no-gh --yes || st=$?
if [[ $st -ne 0 ]] && in_log no-gh 'brew install gh' && nothing_built; then ok "gh missing"; else bad "gh missing: exit $st"; fi
gh_state commit
refused "gh not logged in" 'gh auth login' no-auth --yes
check "the login is asked with gh auth token (gh auth status fails on any stale account), github.com named" \
  has_call '^gh auth token --hostname github.com$'
check "the token gh prints is not shown" bash -c "! grep -rq 'gho_faketokenOUTPUT' '$W'/*.log"
gh_state auth
refused "the commit is not on GitHub: push first" 'push first' no-commit --yes
check "GitHub was asked for exactly this commit" has_call "^gh api repos/acme/abyssfire/commits/$sha "
gh_state auth commit-404
refused "... also when GitHub answers 404 for it" 'push first' no-commit-404 --yes
gh_state auth commit push-false
refused "a login without write access" 'cannot write' no-push --yes
gh_state auth commit release
refused "an existing release without --replace" "release '$tag' already exists" release-exists --yes
gh_state auth commit tag
refused "an existing bare tag without --replace" "tag '$tag' already exists" tag-exists --yes
gh_state auth commit release not-prerelease
refused "--replace never deletes a full release (one promoted on github.com)" 'is a full release' replace-full --replace --yes
check "... and no delete was sent" no_call '^gh release delete\|^gh-mutation'
gh_state auth commit
check "a warning when GH_TOKEN replaces the gh login" in_log tag-exists 'GH_TOKEN / GITHUB_TOKEN is set'
echo '; local edit' >>"$R/unreal/Config/DefaultGame.ini"
refused "uncommitted changes to tracked files" 'uncommitted changes' dirty --yes
g checkout -q -- unreal/Config/DefaultGame.ini
echo untracked >"$R/unreal/untracked-file.txt"
AF_TEST_NO_METAL=1 refused "missing Metal toolchain" 'Metal Toolchain missing' no-metal --yes
UE_ROOT="$W/no-engine" refused "no engine at UE_ROOT" 'Unreal Engine not found' no-engine --yes
UE_ROOT_MAC="$W/other" refused "UE_ROOT and UE_ROOT_MAC disagree" 'name different engines' two-engines --yes
AF_MAC_ARCH=x86_64 refused "AF_MAC_ARCH other than arm64" 'Apple Silicon only' arch --yes
refused "--from package without unreal/Content" 'no L_Main' from-package-empty --from package --yes
refused "--from dmg without a packaged app" 're-run with --from package' from-dmg-empty --from dmg --yes
refused "--from publish without a packaged app" 're-run with --from package' from-publish-empty --from publish --yes
refused "a signing identity that is in no keychain" 'no valid code-signing identity' id-unknown \
  --sign-identity "Developer ID Application: Nobody (XXXXXXXXXX)" --yes
refused "a signing identity that is not Developer ID" 'not a "Developer ID Application" identity' id-dist \
  --sign-identity "Apple Distribution: Test (TEAM123456)" --yes
AF_TEST_TWO_IDS=1 refused "a name that fits two certificates (after a renewal) is refused before the build" \
  'fits more than one identity' id-twice --sign-identity "$dev_id" --yes
gh_state auth
AF_TEST_TWO_IDS=1 refused "... the SHA-1 picks one (next refusal: the commit, not the identity)" 'push first' id-sha1 \
  --sign-identity "$dev_sha1" --yes
gh_state auth commit
# a package step killed hard (kill -9, power loss) cannot restore the ini: the next run says how
cp "$R/unreal/Config/DefaultGame.ini" "$R/unreal/Config/DefaultGame.ini.af-ci-backup"
echo "ProjectVersion=$ver" >>"$R/unreal/Config/DefaultGame.ini"
refused "the version overlay of a killed package step: the fix is named" \
  'mv unreal/Config/DefaultGame.ini.af-ci-backup unreal/Config/DefaultGame.ini' killed-overlay --yes
mv "$R/unreal/Config/DefaultGame.ini.af-ci-backup" "$R/unreal/Config/DefaultGame.ini"
check "... and following it leaves the tree clean" tree_clean

echo "=== default run: content -> package -> dmg -> publish"
mkdir -p "$R/unreal/Intermediate" "$R/unreal/Saved"
echo "an older engine" >"$R/unreal/Intermediate/.af-toolchain-stamp"
echo stale >"$R/unreal/Saved/stale.marker"
runs_ok "exit 0 (untracked files do not count as dirty)" default --yes
l_build="$(line_of '^Build.sh AbyssfireEditor Mac Development')"
l_editor="$(line_of '^UnrealEditor-Cmd ')"
l_uat="$(line_of '^RunUAT BuildCookRun')"
l_dmg="$(line_of '^hdiutil convert')"
l_gh="$(line_of '^gh release create')"
check "children in order: content, package, dmg, then gh release create" \
  test "${l_build:-0}" -lt "${l_editor:-0}" -a "${l_editor:-0}" -lt "${l_uat:-0}" -a "${l_uat:-0}" -lt "${l_dmg:-0}" \
  -a "${l_dmg:-0}" -lt "${l_gh:-0}"
check "version is <ProjectVersion>-pre.<commit date>.<sha8>" in_log default "Abyssfire $ver (Shipping) from ${sha:0:8}"
check "the engine saw that version" grep -qx "AF_VERSION=$ver" "$W/uat.env"
check "the content build saw it too" grep -qx "AF_VERSION=$ver" "$W/editor.env"
check "numeric version 0.1.0 for the bundle" grep -qx "AF_VERSION_NUMERIC=0.1.0" "$W/uat.env"
check "ProjectVersion stamped for the cook (the fake RunUAT insists) and restored" tree_clean
check "no CI_COMMIT_TAG / GITLAB_CI in the engine's environment" bash -c "! grep -Eq '^(CI_COMMIT_TAG|GITLAB_CI|CI_COMMIT_SHORT_SHA|CI_PIPELINE_IID)=' '$W/uat.env' '$W/editor.env'"
check "no signing secret or signing name in the engine's environment" bash -c \
  "! grep -Eq '^AF_[A-Z_]*(_B64|PASSWORD)=|^AF_MAC_SIGN_IDENTITY=|^AF_NOTARY|^AF_SKIP_NOTARIZE=' '$W/uat.env' '$W/editor.env'"
check "no GitHub token in the engine's environment" bash -c "! grep -q 'ghp_testtoken' '$W/uat.env' '$W/editor.env'"
check "both script families got the same engine (UE_ROOT = UE_ROOT_MAC)" bash -c \
  "grep -qx 'UE_ROOT_MAC=$UE' '$W/uat.env' && grep -qx 'UE_ROOT=$UE' '$W/editor.env'"
check "Shipping, arm64" has_call "-clientconfig=Shipping .*-specifiedarchitecture=arm64 -NoCodeSign"
check "stale Saved wiped by the toolchain stamp" test ! -e "$R/unreal/Saved/stale.marker"
check "... before the content step, not between content and package" test -f "$R/unreal/Saved/ContentBuild/build_report.json"
check "ad-hoc signature, no notarisation" bash -c "grep -q -- '--sign - ' '$W/calls.log' && ! grep -q notarytool '$W/calls.log'"
check "nothing on GitHub was changed before the summary was shown" bash -c \
  "grep -q '^gh-mutation create-release after-summary' '$W/calls.log' && ! grep -q 'BEFORE-summary' '$W/calls.log'"
check "GH_HOST / GH_REPO of the caller's shell did not reach gh (the fake refuses them)" has_call '^gh release create'
check "content-info written by the finished content step" grep -qx "commit: $sha" "$OUT/content-info.txt"
check "the DMG artwork is marked with its version" test "$(cat "$OUT/installer-assets/.release-version")" = "$ver"
check "after a public release: how to get the new tag into this clone" in_log default 'git fetch --tags origin'
check "gh release create <tag>" test "$(sed -n 3p "$W/gh/create.args")" = "$tag"
check "--repo from the origin remote (scp-like form)" test "$(create_arg_after --repo)" = "acme/abyssfire"
check "--target is the full commit sha" test "$(create_arg_after --target)" = "$sha"
check "--prerelease, not --draft" bash -c "grep -qx -- --prerelease '$W/gh/create.args' && ! grep -qx -- --draft '$W/gh/create.args'"
check "--title names the version" test "$(create_arg_after --title)" = "Abyssfire Unreal $ver (macOS, pre-release)"
check "--notes-file is the generated notes" test "$(create_arg_after --notes-file)" = "$REL/release-notes.md"
check "the DMG as a labelled asset" create_has "$dmg#macOS (Apple Silicon) - .dmg"
check "SHA256SUMS.txt as a labelled asset" create_has "$REL/SHA256SUMS.txt#SHA-256 checksums"
check "the checksum file matches the DMG" test "$(cut -d' ' -f1 "$REL/SHA256SUMS.txt")" = "$(sha256sum "$dmg" | cut -d' ' -f1)"
check "the release was read back and its URL printed" in_log default "released: https://github.com/acme/abyssfire/releases/tag/$tag"
check "each step reports its elapsed time" test "$(grep -c 'step [a-z]* done in [0-9]*m [0-9]*s' "$W/default.log")" = 4
check "per-step logs" test -s "$OUT/logs/release-mac/content.log" -a -s "$OUT/logs/release-mac/package.log" -a -s "$OUT/logs/release-mac/dmg.log"
check "release-info next to the zip" grep -qx "commit: $sha" "$OUT/package/mac/release-info.txt"
plist_ok() {
  unzip -p "$dmg" Abyssfire.app/Contents/Info.plist | python3 -c '
import plistlib, sys
d = plistlib.loads(sys.stdin.buffer.read())
assert d["CFBundleShortVersionString"] == "0.1.0" and d["CFBundleVersion"] == "4", d'
}
check "Info.plist: version 0.1.0, build = commit count (not CI_PIPELINE_IID)" plist_ok

echo "=== release notes of the default (ad-hoc) build"
N="$W/gh/published-notes.md"
check "what was published is the notes file" cmp -s "$N" "$REL/release-notes.md"
check "pre-release banner first" bash -c "head -1 '$N' | grep -q 'Pre-release / 预发布版本'"
check "download table: file, size, sha256" grep -q "| macOS (Apple Silicon) - .dmg | \[\`Abyssfire-$ver-macos-arm64.dmg\`\](https://github.com/acme/abyssfire/releases/download/$tag/Abyssfire-$ver-macos-arm64.dmg) | [0-9.]* [KM]iB | \`$(sha256sum "$dmg" | cut -d' ' -f1)\` |" "$N"
check "minimum macOS, drag to Applications" grep -q 'macOS 14.5+ (Apple Silicon).*drag Abyssfire to Applications' "$N"
check "not notarised: Open Anyway, Control-click on macOS 14 and earlier only, xattr" bash -c \
  "grep -q 'Privacy & Security.*Open Anyway' '$N' && grep -q 'macOS 14 and earlier only: Control-click' '$N' && grep -q 'xattr -dr com.apple.quarantine /Applications/Abyssfire.app' '$N' && grep -q 'expected for a test build' '$N'"
check "changes since the previous unreal-v* tag: unreal/ commits only" bash -c \
  "grep -q 'Changes since \`unreal-v0.0.9\`' '$N' && grep -q '^unreal: sharper telegraphs' '$N' && ! grep -q 'web: not part' '$N' && ! grep -q 'first import' '$N'"
check "footer: full sha, engine version, configuration" grep -q "Built from \`$sha\` with Unreal Engine 5.8.3 (Shipping, arm64)" "$N"
check "a subject that names a path does not stop the run; this machine's home and clone are masked" \
  grep -qx 'unreal: UE_ROOT defaults to /Users/Shared/Epic Games/UE_5.8 (was <home>/UE, see <repo>/unreal)' "$N"
check "no absolute path of this machine" bash -c \
  "! grep -q -e '$W' -e '/home/' -e '/tmp/' '$N' && ! grep -v '^unreal: UE_ROOT defaults to /Users/Shared/' '$N' | grep -q '/Users/'"
check "no user name, host name or e-mail address" bash -c \
  "! grep -q -e zzlocaluser -e 'Local Dev' -e \"\$(hostname)\" '$N' && ! grep -Eq '[[:alnum:]._-]+@[[:alnum:]-]+\.[[:alpha:]]' '$N'"
check "no Development-only claims in a Shipping build" bash -c "! grep -q 'abyssautotest' '$N'"

echo "=== existing release / tag, --replace, --draft, --notes-file"
refused "the release now exists: a second run stops in preflight" 'already exists' again --yes
usage_error "--notes-file inside the release directory (rebuilt on every run) is refused" 'must not be inside' notes-inside \
  --from publish --notes-file "$REL/release-notes.md" --yes
check "... and the file is still there" test -s "$REL/release-notes.md"
runs_ok "--replace --from publish" replace --replace --from publish --yes
check "nothing rebuilt" no_call '^Build.sh \|^RunUAT \|^hdiutil '
check "the summary says what --replace deletes" in_log replace \
  "^  replaces    the existing pre-release 'Abyssfire Unreal $tag' (2 files, their download counts) and its tag are DELETED first"
check "the deletion comes after the summary, never before" bash -c \
  "grep -q '^gh-mutation delete-release after-summary' '$W/calls.log' && ! grep -q 'BEFORE-summary' '$W/calls.log'"
check "release deleted with its tag, then created" bash -c \
  "test \"\$(grep -n '^gh release delete $tag --repo acme/abyssfire --yes --cleanup-tag' '$W/calls.log' | cut -d: -f1)\" -lt \"\$(grep -n '^gh release create' '$W/calls.log' | cut -d: -f1)\""
gh_state auth commit tag
runs_ok "--replace with a bare tag" replace-tag --replace --from publish --yes
check "bare tag deleted through the API, no release delete" bash -c \
  "grep -q '^gh api --method DELETE repos/acme/abyssfire/git/refs/tags/$tag' '$W/calls.log' && ! grep -q '^gh release delete' '$W/calls.log'"
check "the summary names the tag" in_log replace-tag "^  replaces    the existing tag '$tag' (it has no release) is DELETED first"
gh_state auth commit release delete-fails-after
echo true >"$W/gh/release" # a draft left by a failed upload: no tag for --cleanup-tag to remove
runs_ok "--replace of a draft: gh release delete failing on the missing tag after deleting the release is no failure" \
  replace-draft --replace --from publish --yes
check "... the summary called it a draft, the new release was created" bash -c \
  "grep -q '^  replaces    the existing draft ' '$W/replace-draft.log' && grep -q '^gh-mutation create-release' '$W/calls.log'"
gh_state auth commit release not-prerelease
refused "--replace --from publish with a full release: nothing deleted" 'is a full release' replace-full-publish \
  --replace --from publish --yes
check "... no mutating call" no_call '^gh-mutation'
gh_state auth commit
printf 'Playtest build for the weekend.\n' >"$W/my-notes.md"
runs_ok "--draft --notes-file --tag" draft --from publish --draft --notes-file "$W/my-notes.md" --tag unreal-draft-1 --yes
check "--draft and --prerelease" bash -c "grep -qx -- --draft '$W/gh/create.args' && grep -qx -- --prerelease '$W/gh/create.args'"
check "custom tag used, also in the download link" bash -c \
  "test \"\$(sed -n 3p '$W/gh/create.args')\" = unreal-draft-1 && grep -q 'releases/download/unreal-draft-1/' '$N'"
check "the user's notes right after the banner" test "$(sed -n 3p "$N")" = "Playtest build for the weekend."
check "the log says it is a draft" in_log draft 'DRAFT'
check "a draft has no tag yet: no fetch hint" bash -c "! grep -q 'git fetch --tags' '$W/draft.log'"
gh_state auth commit not-prerelease one-asset
runs_ok "the read-back is checked" readback --from publish --tag unreal-readback --yes
check "... a release GitHub does not show as a pre-release is reported" in_log readback "does not show 'unreal-readback' as a pre-release"
check "... and a missing file too" in_log readback "GitHub shows 1 files for 'unreal-readback', 2 were sent"

echo "=== --no-publish"
gh_state auth commit
runs_ok "--no-publish --from publish" nopub --no-publish --from publish
check "no gh call at all" no_call '^gh '
check "the gh command is printed" in_log nopub "gh release create $tag --repo acme/abyssfire --target $sha"
check "notes and checksums written" test -s "$REL/release-notes.md" -a -s "$REL/SHA256SUMS.txt" -a -s "$dmg"
# (the set-up lines between the cases never end the test: when an earlier run failed, the checks say so)
mkdir -p "$OUT/installer-assets"
echo kept >"$OUT/installer-assets/kept.marker"
st=0
rel_nogh nopub-nogh --no-publish --from dmg || st=$?
check "works without gh on PATH (from dmg), stdin not a terminal" test "$st" = 0
check "the DMG artwork of the same version is kept" test -f "$OUT/installer-assets/kept.marker"
check "DMG rebuilt, gh command printed" bash -c "grep -q '^hdiutil convert' '$W/calls.log' && grep -q 'gh release create $tag' '$W/nopub-nogh.log'"

echo "=== origin forms and --repo"
for url in https://github.com/acme/abyssfire.git https://github.com/acme/abyssfire \
  https://x-access-token:ghs_secret@github.com/acme/abyssfire.git ssh://git@github.com/acme/abyssfire.git \
  git@github.com:acme/abyssfire; do
  g remote set-url origin "$url"
  if rel origin --no-publish --from publish && in_log origin "gh release create $tag --repo acme/abyssfire " &&
    ! grep -q 'ghs_secret' "$W/origin.log" "$REL/release-notes.md"; then ok "origin ${url/ghs_secret/***}"; else
    bad "origin ${url/ghs_secret/***} (see $W/origin.log)"
  fi
done
g remote set-url origin https://gitlab.com/acme/abyssfire.git
refused "an origin that is not github.com needs --repo" 'pass --repo OWNER/NAME' origin-gitlab --no-publish --from publish
runs_ok "... and works with --repo" origin-repo --no-publish --from publish --repo acme/abyssfire
g remote set-url origin git@github.com:acme/abyssfire.git

echo "=== the question before publishing (stdin is a terminal)"
gh_state auth commit
cat >"$W/with_tty.py" <<'EOF'
import os, pty, sys
answer, argv = sys.argv[1], sys.argv[2:]
pid, fd = pty.fork()
if pid == 0:
    os.execvp(argv[0], argv)
os.write(fd, (answer + "\n").encode())
out = b""
while True:
    try:
        data = os.read(fd, 4096)
    except OSError:
        break
    if not data:
        break
    out += data
status = os.waitpid(pid, 0)[1]
sys.stdout.buffer.write(out)
sys.exit(os.WEXITSTATUS(status) if os.WIFEXITED(status) else 99)
EOF
ask() { # ask SLUG ANSWER ARGS... -> release_mac.sh on a pseudo-terminal, ANSWER typed at the prompt
  local slug="$1" answer="$2"
  shift 2
  : >"$W/calls.log"
  PATH="$WITH_GH" "$real_python" "$W/with_tty.py" "$answer" bash "$R/unreal/Scripts/release_mac.sh" "$@" >"$W/$slug.log" 2>&1
}
if "$real_python" -c 'import os, pty; os.close(pty.openpty()[0])' 2>/dev/null; then
  st=0
  ask ask-no n --from publish --tag unreal-ask-1 || st=$?
  check "answer n: not published, exit non-zero" bash -c \
    "test $st != 0 && grep -q 'Publish this to https://github.com/acme/abyssfire/releases' '$W/ask-no.log' && grep -q 'not published' '$W/ask-no.log' && ! grep -q '^gh release create' '$W/calls.log'"
  st=0
  ask ask-empty "" --from publish --tag unreal-ask-1 || st=$?
  check "just Enter: not published" bash -c "test $st != 0 && ! grep -q '^gh release create' '$W/calls.log'"
  st=0
  ask ask-yes y --from publish --tag unreal-ask-1 || st=$?
  check "answer y: published" bash -c "test $st = 0 && grep -q '^gh release create unreal-ask-1 ' '$W/calls.log'"
  # nothing irreversible before the confirmation: --replace deletes only after a yes
  gh_state auth commit release
  st=0
  ask ask-replace-no n --from publish --replace --tag unreal-ask-1 || st=$?
  check "--replace, answer n: the summary named the deletion, nothing was deleted" bash -c \
    "test $st != 0 && grep -q 'replaces    the existing pre-release' '$W/ask-replace-no.log' && ! grep -q '^gh release delete\|^gh api --method DELETE\|^gh-mutation' '$W/calls.log' && test -f '$W/gh/release'"
  st=0
  ask ask-replace-yes y --from publish --replace --tag unreal-ask-1 || st=$?
  check "--replace, answer y: deleted, then created" bash -c \
    "test $st = 0 && test \"\$(grep -n '^gh release delete unreal-ask-1 ' '$W/calls.log' | cut -d: -f1)\" -lt \"\$(grep -n '^gh release create unreal-ask-1 ' '$W/calls.log' | cut -d: -f1)\""
else
  echo "  skip no pseudo-terminal available here"
fi

echo "=== size limit"
gh_state auth commit
AF_RELEASE_MAX_ASSET_BYTES=10 refused "an asset at or over the limit is not uploaded" 'GitHub refuses release files of 2 GiB' \
  too-big --from publish --yes
st=0
AF_RELEASE_MAX_ASSET_BYTES=10 rel too-big-local --from publish --no-publish || st=$?
check "--no-publish only warns about it" bash -c "test $st = 0 && grep -q 'WARNING.*2 GiB' '$W/too-big-local.log'"

echo "=== stale leftovers (--from)"
refused "--from dmg with another version" "its version is '$ver', this run's is '9.9.9-rc.1'" stale-version \
  --from dmg --version 9.9.9-rc.1 --yes
refused "--from dmg with another configuration" "its config is 'Shipping', this run's is 'Development'.*--from package" \
  stale-config --from dmg --config Development --yes
refused "--from publish with another configuration" "--from package" stale-config-publish --from publish --config Test --yes
mkdir -p "$OUT/installers/mac"
cp "$OUT/installers/mac/Abyssfire-$ver-macos-arm64.dmg" "$W/good.dmg" || true
echo tampered >>"$OUT/installers/mac/Abyssfire-$ver-macos-arm64.dmg"
refused "--from publish with a DMG this script did not build" 'checksum differs.*--from dmg' stale-dmg --from publish --yes
cp "$W/good.dmg" "$OUT/installers/mac/Abyssfire-$ver-macos-arm64.dmg" || true
mv "$OUT/installers/mac/release-info.txt" "$W/dmg-info.txt" || true
refused "--from publish without the DMG's release-info" 're-run with --from dmg' stale-noinfo --from publish --yes
mv "$W/dmg-info.txt" "$OUT/installers/mac/release-info.txt" || true
refused "--from publish --sign-identity with an ad-hoc DMG" 'ad-hoc signed.*--from dmg' stale-adhoc \
  --from publish --sign-identity "$dev_id" --yes
runs_ok "the untouched leftovers still publish" stale-none --from publish --tag unreal-leftover --yes
mv "$OUT/content-info.txt" "$W/content-info.txt" || echo "commit: none" >"$W/content-info.txt"
refused "--from package with content no finished content step of this script built" 'was not built by a finished content step' \
  stale-content-none --from package --yes
sed 's/^commit: .*/commit: 0123456789abcdef0123456789abcdef01234567/' "$W/content-info.txt" >"$OUT/content-info.txt"
refused "--from package with content of another commit" "built from commit 01234567, this run is ${sha:0:8}" \
  stale-content-commit --from package --yes
mv "$W/content-info.txt" "$OUT/content-info.txt"

echo "=== --config Development"
gh_state auth commit
runs_ok "--from package --config Development" dev --from package --config Development --tag unreal-dev-1 --yes
check "content not rebuilt" no_call '^Build.sh \|^UnrealEditor-Cmd '
check "BuildCookRun Development" has_call '^RunUAT BuildCookRun .*-clientconfig=Development '
check "asset file name says development" create_has "$REL/Abyssfire-$ver-development-macos-arm64.dmg#macOS (Apple Silicon) - .dmg (Development build)"
check "title says Development build" test "$(create_arg_after --title)" = "Abyssfire Unreal $ver (macOS, Development build, pre-release)"
check "notes: Development build, autotest switches" bash -c \
  "grep -q 'Development build.*-abyssautotest=smoke.*-abyssclass=warrior|mage|rogue' '$N' && grep -q '(Development, arm64)' '$N'"
check "notes: the autotest command waits for the game (open -W), no exit status promised through open" bash -c \
  "grep -q 'open -n -W /Applications/Abyssfire.app --args -abyssautotest=smoke' '$N' && ! grep -q 'exit status' '$N'"
check "the checksum file names the published file" grep -q "  Abyssfire-$ver-development-macos-arm64.dmg\$" "$REL/SHA256SUMS.txt"
refused "a later Shipping --from publish refuses the Development leftovers" "its config is 'Development'" dev-stale --from publish --yes

echo "=== local signing (--sign-identity, --notary-profile)"
gh_state auth commit
runs_ok "--from package --sign-identity --notary-profile --shaders" signed --from package --shaders \
  --sign-identity "$dev_id" --notary-profile af-notary --tag unreal-signed-1 --yes
check "the profile is checked in preflight" has_call '^xcrun notarytool history --keychain-profile af-notary'
check "the engine still got no signing name" bash -c "! grep -q 'refused' '$W/calls.log' && ! grep -Eq '^AF_MAC_SIGN|^AF_NOTARY' '$W/uat.env'"
check "codesign with the identity, hardened runtime, no --keychain" bash -c \
  "grep -q -- 'codesign --force --options runtime --timestamp --sign $dev_sha1 ' '$W/calls.log' && ! grep -q -- '--keychain ' '$W/calls.log'"
check "DMG signed with the identity (by its SHA-1: one certificate, whatever else carries the name)" \
  has_call "codesign --force --timestamp --sign $dev_sha1 .*\.dmg\$"
check "no temporary keychain" no_call '^security create-keychain\|^security import\|^security unlock-keychain'
check "notarytool submit --keychain-profile, twice (AF_SKIP_NOTARIZE of the caller ignored)" \
  test "$(grep -c 'notarytool submit .* --keychain-profile af-notary ' "$W/calls.log")" = 2
check "the ticket is validated before publishing" has_call '^xcrun stapler validate .*installers/mac/Abyssfire-'
check "notes say signed and notarised" grep -q 'signed with Developer ID and notarised' "$N"
check "notes carry no Gatekeeper workaround" bash -c "! grep -q -e 'Open Anyway' -e 'xattr' -e 'Control-click' '$N'"
check "no identity or profile name in the notes" bash -c "! grep -q -e 'TEAM123456' -e 'af-notary' -e '$dev_sha1' '$N'"
check "summary says so" in_log signed 'signature   signed with Developer ID, notarised'
gh_state auth commit
runs_ok "--from dmg --sign-identity alone" signed-only --from dmg --sign-identity "$dev_id" --tag unreal-signed-2 --yes
check "signed, not notarised: the notes keep the workaround" bash -c \
  "grep -q 'signed with Developer ID but \*\*not notarised\*\*' '$N' && grep -q 'Open Anyway' '$N' && ! grep -q notarytool '$W/calls.log'"
refused "--from publish --notary-profile with a DMG that is not notarised" 'not notarised.*--from dmg' stale-notary \
  --from publish --sign-identity "$dev_id" --notary-profile af-notary --yes

echo "=== --shaders, --allow-dirty"
check "--shaders is for the content step only (the signed run started at package)" no_call 'AllowCommandletRendering'
echo '; local edit' >>"$R/unreal/Config/DefaultGame.ini"
runs_ok "--no-publish --allow-dirty --shaders builds a dirty tree" dirty-build --no-publish --allow-dirty --shaders
check "--shaders passed to build_content.sh and on to the editor" has_call '^UnrealEditor-Cmd .*build_content.py --shaders .*-AllowCommandletRendering'
check "the local edit survived the version overlay" grep -q '^; local edit$' "$R/unreal/Config/DefaultGame.ini"
g checkout -q -- unreal/Config/DefaultGame.ini
refused "a build made with --allow-dirty cannot be published later" 'built with uncommitted changes' dirty-publish \
  --from publish --yes

echo "=== failure paths"
gh_state auth commit
st=0
AF_TEST_UAT_FAIL=1 rel uat-fail --yes || st=$?
check "a failing RunUAT ends the run with the child's status" test "$st" = 25
check "... names the step and its log" in_log uat-fail "step package failed with status 25 .*log: $OUT/logs/release-mac/package.log"
check "... and the option to resume with" in_log uat-fail 're-run with --from package'
check "... before the DMG and before gh release create" no_call '^hdiutil \|^gh release create'
check "... and leaves the ini restored" tree_clean
refused "the half-built state is not publishable" 're-run with --from package' uat-fail-resume --from dmg --yes
st=0
AF_TEST_UAT_DOES=interrupt rel uat-int --from package --yes || st=$?
check "an interrupted package step ends the run" bash -c "test $st != 0 && grep -q '^RunUAT (interrupted)' '$W/calls.log'"
check "... with the ini restored, no overlay backup and no release-info left" bash -c \
  "test -z \"\$(git -C '$R' status --porcelain --untracked-files=no)\" && test ! -e '$R/unreal/Config/DefaultGame.ini.af-ci-backup' && test ! -e '$OUT/package/mac/release-info.txt'"
check "... and nothing after it ran" no_call '^hdiutil \|^gh release create'
refused "... its leftovers are not publishable" 're-run with --from package' uat-int-resume --from dmg --yes
# the build must be the commit it is published as: an edit or a commit made while a step ran stops the run
st=0
AF_TEST_UAT_DOES=edit rel edit-during --from package --yes || st=$?
check "a tracked file edited during the package step stops the run" bash -c \
  "test $st != 0 && grep -q 'tracked files changed during the package step' '$W/edit-during.log' && grep -q ' M unreal/Data/items.json' '$W/edit-during.log'"
check "... before the DMG and before gh release create, and without a release-info" bash -c \
  "! grep -q '^hdiutil \|^gh release create\|^gh-mutation' '$W/calls.log' && test ! -e '$OUT/package/mac/release-info.txt'"
g checkout -q -- unreal/Data/items.json
st=0
AF_TEST_UAT_DOES=commit rel commit-during --from package --yes || st=$?
check "a commit made during the package step stops the run" bash -c \
  "test $st != 0 && grep -q 'HEAD moved from ${sha:0:8} to ' '$W/commit-during.log' && ! grep -q '^hdiutil \|^gh-mutation' '$W/calls.log'"
g reset -q --hard "$sha"
runs_ok "rebuild after the failure" rebuild --no-publish
touch "$W/gh/create-fail"
st=0
rel create-fail --from publish --tag unreal-x --notes-file "$W/my-notes.md" --yes || st=$?
check "a failing gh release create exits non-zero" test "$st" != 0
check "... and prints how to resume" in_log create-fail \
  "resume with: unreal/Scripts/release_mac.sh --tag unreal-x --notes-file .*my-notes.md --yes --from publish \$"
touch "$W/gh/create-leaves-draft"
st=0
rel create-fail-draft --from publish --replace --yes || st=$?
check "... with --replace when a release was left behind" bash -c \
  "test $st != 0 && grep -q 'left a release' '$W/create-fail-draft.log' && grep -q 'resume with: unreal/Scripts/release_mac.sh --yes --from publish --replace' '$W/create-fail-draft.log'"

echo "=== a new commit, no earlier tag"
gh_state auth commit
echo '{"a": 3}' >"$R/unreal/Data/items.json"
g add -A
commit "2026-03-05T01:00:00+08:00" "unreal: next"
sha2="$(g rev-parse HEAD)"
refused "leftovers of another commit" "re-run with --from package" stale-commit --from publish --version "$ver" --yes
check "... named as such" in_log stale-commit "its commit is '$sha', this run's is '$sha2'"
refused "--from package with the content of the previous commit" "built from commit ${sha:0:8}, this run is ${sha2:0:8}" \
  stale-content-next --from package --yes
g tag -d unreal-v0.0.9 >/dev/null
mkdir -p "$OUT/installer-assets"
echo stale >"$OUT/installer-assets/stale.marker"
runs_ok "the new commit builds under its own version" next --no-publish
check "the DMG artwork of the earlier version is drawn again" bash -c \
  "test ! -e '$OUT/installer-assets/stale.marker' && test -s '$OUT/installer-assets/dmg-background.png' && test \"\$(cat '$OUT/installer-assets/.release-version')\" = '0.1.0-pre.20260305.${sha2:0:8}'"
check "the log says why there is no list of changes" in_log next 'no earlier unreal-v\* tag in this clone'
check "version follows the commit" in_log next "Abyssfire 0.1.0-pre.20260305.${sha2:0:8} (Shipping)"
check "no 'changes' section without an earlier unreal-v* tag" bash -c "! grep -q 'Changes since' '$REL/release-notes.md'"
check "a resumed run computes the same names" bash -c \
  "PATH='$WITH_GH' bash '$R/unreal/Scripts/release_mac.sh' --no-publish --from publish </dev/null 2>&1 | grep -q 'gh release create unreal-v0.1.0-pre.20260305.${sha2:0:8} '"

g rm -q --cached unreal/Config/DefaultGame.ini
commit "2026-03-06T01:00:00+08:00" "unreal: the ini is no longer committed"
refused "no committed DefaultGame.ini: a message, not a silent exit" 'pass --version X.Y.Z' no-ini --no-publish --from publish

echo
if [[ $fails -gt 0 ]]; then
  echo "FAILED: $fails (logs in $W)"
  exit 1
fi
echo "OK: $W"
