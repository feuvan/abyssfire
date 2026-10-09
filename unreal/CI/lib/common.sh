# shellcheck shell=bash
# Shared helpers for the Abyssfire CI scripts (unreal/CI/scripts/*.sh). Sourced, never executed.
# Works with bash 3.2 (the macOS system bash on the self-hosted runner) and bash 5 (Linux images).
#
#   AF_CI_DIR      unreal/CI
#   AF_UNREAL_DIR  unreal
#   AF_REPO_DIR    repository root
#   AF_OUT_DIR     everything the jobs produce (artifacts are collected from here); default unreal/CI/out
#   AF_LOG_DIR     this job's logs: $AF_OUT_DIR/logs/$CI_JOB_NAME_SLUG (artifact path unreal/CI/out/logs/<slug>/)

if [[ -n "${AF_COMMON_LOADED:-}" ]]; then return 0; fi
AF_COMMON_LOADED=1

AF_CI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AF_UNREAL_DIR="$(cd "$AF_CI_DIR/.." && pwd)"
AF_REPO_DIR="$(cd "$AF_UNREAL_DIR/.." && pwd)"
AF_OUT_DIR="${AF_OUT_DIR:-$AF_CI_DIR/out}"
# per job, so a job's log artifact does not re-upload the logs of the jobs it downloaded artifacts from
AF_LOG_DIR="${AF_LOG_DIR:-$AF_OUT_DIR/logs/${CI_JOB_NAME_SLUG:-local}}"
AF_PRODUCT_NAME="Abyssfire"
AF_UPROJECT="$AF_UNREAL_DIR/Abyssfire.uproject"
export AF_CI_DIR AF_UNREAL_DIR AF_REPO_DIR AF_OUT_DIR AF_LOG_DIR AF_PRODUCT_NAME AF_UPROJECT
# macOS tool with an absolute path (overridable only for unreal/CI/tools/test_mac_scripts.sh)
AF_PLISTBUDDY="${AF_PLISTBUDDY:-/usr/libexec/PlistBuddy}"

af_log() { printf '\033[1;36m[af]\033[0m %s\n' "$*" >&2; }
af_warn() { printf '\033[1;33m[af] WARNING:\033[0m %s\n' "$*" >&2; }
af_die() {
  printf '\033[1;31m[af] ERROR:\033[0m %s\n' "$*" >&2
  exit 1
}

# Collapsible sections in the GitLab job log (plain lines elsewhere).
af_section_start() {
  local name="$1"; shift
  if [[ -n "${GITLAB_CI:-}" ]]; then
    printf '\033[0Ksection_start:%s:%s[collapsed=false]\r\033[0K\033[1m%s\033[0m\n' "$(date +%s)" "$name" "$*"
  else
    printf '=== %s\n' "$*"
  fi
}
af_section_end() {
  if [[ -n "${GITLAB_CI:-}" ]]; then printf '\033[0Ksection_end:%s:%s\r\033[0K\n' "$(date +%s)" "$1"; fi
}

af_require_cmd() {
  local c
  for c in "$@"; do
    command -v "$c" >/dev/null 2>&1 || af_die "required command '$c' not found on this machine (see unreal/Docs/CI.md)"
  done
}

# af_is_true VALUE -> success for 1/true/yes/on (any case)
af_is_true() {
  case "$(printf '%s' "${1:-}" | tr '[:upper:]' '[:lower:]')" in
    1 | true | yes | on) return 0 ;;
    *) return 1 ;;
  esac
}

# af_have VAR -> success when the variable named VAR is set and non-empty (CI variables may be absent).
af_have() {
  local name="$1"
  [[ -n "${!name:-}" ]]
}

# af_require_vars VAR... -> die listing every missing variable (names only, never values)
af_require_vars() {
  local missing=() v
  for v in "$@"; do af_have "$v" || missing+=("$v"); done
  if [[ ${#missing[@]} -gt 0 ]]; then
    af_die "missing CI/CD variable(s): ${missing[*]} (Settings > CI/CD > Variables, see unreal/Docs/CI.md)"
  fi
}

# Signing is mandatory on release tags unless AF_REQUIRE_SIGNING=0; elsewhere only when AF_REQUIRE_SIGNING=1.
af_signing_required() {
  local v="${AF_REQUIRE_SIGNING:-auto}"
  if [[ "$v" == "auto" ]]; then
    [[ -n "${CI_COMMIT_TAG:-}" ]]
    return
  fi
  af_is_true "$v"
}

# af_secret_to_file VAR OUT
# Writes the binary secret held by CI variable VAR to OUT (mode 600). The variable is either base64 text
# (a masked "Variable" type) or a GitLab "File" type variable (its value is the path of a file that holds
# the base64 text). Whitespace and line breaks in the base64 are ignored.
af_secret_to_file() {
  local name="$1" out="$2" val
  val="${!name:-}"
  [[ -n "$val" ]] || af_die "CI/CD variable $name is empty"
  mkdir -p "$(dirname "$out")"
  (
    umask 077
    if [[ -f "$val" ]]; then
      tr -d ' \t\r\n' <"$val" | base64 --decode >"$out" 2>/dev/null
    else
      printf '%s' "$val" | tr -d ' \t\r\n' | base64 --decode >"$out" 2>/dev/null
    fi
  ) || af_die "CI/CD variable $name is not valid base64 (create it with: base64 < file | tr -d '\\n')"
  [[ -s "$out" ]] || af_die "CI/CD variable $name decoded to an empty file"
}

# Every signing secret this CI knows (unreal/Docs/CI.md 4.2).
AF_SECRET_VARS="AF_MAC_DEVID_P12_B64 AF_MAC_DEVID_P12_PASSWORD AF_ASC_API_KEY_P8_B64 AF_ASC_API_KEY_ID
AF_ASC_API_ISSUER_ID AF_ANDROID_KEYSTORE_B64 AF_ANDROID_KEYSTORE_PASSWORD AF_ANDROID_KEY_ALIAS AF_ANDROID_KEY_PASSWORD
AF_IOS_DIST_P12_B64 AF_IOS_DIST_P12_PASSWORD AF_IOS_PROVISION_PROFILE_B64 AF_WIN_CODESIGN_PFX_B64
AF_WIN_CODESIGN_PFX_PASSWORD AF_RELEASE_API_TOKEN"

# af_unset_secrets [KEEP...] -> removes the signing secrets from this shell, so no child process (the engine, its
# crash dumps, pip, Python helpers) inherits them. Called before handing control to anything that does not need them;
# KEEP names the ones this step still uses. (Secrets are also scoped to their job's environment, CI.md 4.2.)
af_unset_secrets() {
  local v k keep
  for v in $AF_SECRET_VARS; do
    keep=0
    for k in "$@"; do if [[ "$k" == "$v" ]]; then keep=1; fi; done
    if [[ $keep -eq 0 ]]; then unset "$v"; fi
  done
}

# af_secret_value VAR -> prints the value; a "File" type variable is read from its file (trailing newline dropped)
af_secret_value() {
  local name="$1" val
  val="${!name:-}"
  if [[ -n "$val" && -f "$val" ]]; then
    tr -d '\r\n' <"$val"
  else
    printf '%s' "$val"
  fi
}

af_sha256() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; else shasum -a 256 "$1" | awk '{print $1}'; fi
}

af_file_size() {
  if stat -c %s "$1" >/dev/null 2>&1; then stat -c %s "$1"; else stat -f %z "$1"; fi
}

# Random string for throw-away keychain / keystore passwords.
af_random_password() {
  LC_ALL=C tr -dc 'A-Za-z0-9' </dev/urandom 2>/dev/null | head -c 32 || true
}

# Loads AF_VERSION & co: from the `version` job's dotenv (already in the environment), else computes them.
af_load_version() {
  if [[ -z "${AF_VERSION:-}" || -z "${AF_MSI_VERSION:-}" ]]; then
    local line
    while IFS= read -r line; do
      [[ "$line" == AF_*=* ]] || continue
      export "${line?}"
    done < <("$AF_CI_DIR/version.sh")
  fi
  [[ -n "${AF_VERSION:-}" ]] || af_die "could not determine the version (unreal/CI/version.sh)"
  af_log "version $AF_VERSION (numeric $AF_VERSION_NUMERIC, MSI $AF_MSI_VERSION, build $AF_BUILD_NUMBER)"
}

# Writes a small JSON-free manifest line per output file: "<sha256>  <name>" (sha256sum format).
af_checksum_into() {
  local list="$1"; shift
  local f
  for f in "$@"; do printf '%s  %s\n' "$(af_sha256 "$f")" "$(basename "$f")" >>"$list"; done
}
