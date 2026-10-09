#!/usr/bin/env bash
# Version stamping for every build and installer (unreal/Docs/CI.md "Versions").
#
#   unreal/CI/version.sh                 print KEY=VALUE lines (dotenv format, no quotes)
#   unreal/CI/version.sh --dotenv FILE   also write them to FILE (the `version` job's artifacts:reports:dotenv)
#
# Source of truth: the tag of a tag pipeline (CI_COMMIT_TAG = vX.Y.Z or vX.Y.Z-pre.N); outside CI an exact
# `git describe` tag. Anything else is a development build: 0.0.0-<shortsha>.
#
#   AF_VERSION               X.Y.Z[-pre]    | 0.0.0-<shortsha>     display / file names / package registry version
#   AF_VERSION_NUMERIC       X.Y.Z          | 0.0.0                CFBundleShortVersionString, iOS VersionInfo
#   AF_MSI_VERSION           X.Y.(Z*100+S)  | 0.0.0                MSI ProductVersion: S = min(N,98) for -pre.N, 99 for
#                            the final release, so 1.2.3-rc.1 (1.2.301) < 1.2.3 (1.2.399) < 1.2.4-rc.1 (1.2.401)
#                            (Windows Installer compares only 3 fields; the 3rd is <= 9999 < 65535)
#   AF_VERSION_QUAD          X.Y.Z.B                               Windows VIProductVersion / file version (B = build % 65536)
#   AF_BUILD_NUMBER          CI_PIPELINE_IID (else commit count)   CFBundleVersion
#   AF_ANDROID_VERSION_CODE  X*10^7 + Y*10^5 + Z*100 + (pre ? min(N,98) : 99) | build number
#                            (monotonic: 1.2.3-rc.1 < 1.2.3 < 1.2.4; Play's limit 2100000000 -> X <= 209)
#   AF_IS_RELEASE            1 on a vX.Y.Z[-pre] tag, else 0
#   AF_IS_PRERELEASE         1 when the tag has a -pre suffix
#   AF_GIT_SHA               short commit sha
#   AF_TAG                   the tag (absent for development builds)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"

dotenv=""
if [[ "${1:-}" == "--dotenv" ]]; then
  dotenv="${2:?--dotenv needs a file}"
elif [[ $# -gt 0 ]]; then
  echo "usage: $0 [--dotenv FILE]" >&2
  exit 2
fi

sha="${CI_COMMIT_SHORT_SHA:-}"
if [[ -z "$sha" ]]; then sha="$(git -C "$repo" rev-parse --short=8 HEAD 2>/dev/null || echo unknown)"; fi

tag="${CI_COMMIT_TAG:-}"
if [[ -z "$tag" && -z "${GITLAB_CI:-}" ]]; then
  tag="$(git -C "$repo" describe --tags --exact-match 2>/dev/null || true)"
fi

build="${CI_PIPELINE_IID:-}"
if [[ -z "$build" ]]; then build="$(git -C "$repo" rev-list --count HEAD 2>/dev/null || echo 0)"; fi
[[ "$build" =~ ^[0-9]+$ ]] || build=0
build=$((10#$build))

re='^v([0-9]+)\.([0-9]+)\.([0-9]+)(-([0-9A-Za-z.-]+))?$'
if [[ -n "$tag" && "$tag" =~ $re ]]; then
  major=$((10#${BASH_REMATCH[1]}))
  minor=$((10#${BASH_REMATCH[2]}))
  patch=$((10#${BASH_REMATCH[3]}))
  pre="${BASH_REMATCH[5]}"
  # MSI ProductVersion limits (major.minor.build = 255.255.65535) and Play's versionCode limit.
  if ((major > 209 || minor > 99 || patch > 99)); then
    echo "version.sh: $tag is outside the supported range (major <= 209, minor <= 99, patch <= 99:" \
      "MSI ProductVersion and the Android versionCode scheme)" >&2
    exit 1
  fi
  numeric="$major.$minor.$patch"
  if [[ -n "$pre" ]]; then
    version="$numeric-$pre"
    is_pre=1
    n="$(printf '%s' "$pre" | grep -oE '[0-9]+$' || true)"
    n=$((10#${n:-0}))
    if ((n > 98)); then n=98; fi
    suffix=$n
  else
    version="$numeric"
    is_pre=0
    suffix=99
  fi
  code=$((major * 10000000 + minor * 100000 + patch * 100 + suffix))
  msi="$major.$minor.$((patch * 100 + suffix))"
  is_release=1
else
  if [[ -n "$tag" ]]; then echo "version.sh: tag '$tag' is not vX.Y.Z[-pre]; building as a development version" >&2; fi
  numeric="0.0.0"
  version="0.0.0-$sha"
  is_pre=0
  is_release=0
  code=$((build > 0 ? build : 1))
  msi="0.0.0"
  tag=""
fi

out="$(
  cat <<EOF
AF_VERSION=$version
AF_VERSION_NUMERIC=$numeric
AF_MSI_VERSION=$msi
AF_VERSION_QUAD=$numeric.$((build % 65536))
AF_BUILD_NUMBER=$build
AF_ANDROID_VERSION_CODE=$code
AF_IS_RELEASE=$is_release
AF_IS_PRERELEASE=$is_pre
AF_GIT_SHA=$sha
EOF
)"
# GitLab's dotenv parser rejects empty values: AF_TAG only exists in tag builds.
if [[ -n "$tag" ]]; then
  out="$out
AF_TAG=$tag"
fi
printf '%s\n' "$out"
if [[ -n "$dotenv" ]]; then
  mkdir -p "$(dirname "$dotenv")"
  printf '%s\n' "$out" >"$dotenv"
fi
