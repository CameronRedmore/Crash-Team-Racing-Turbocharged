#!/usr/bin/env bash
# Check clang-format compliance for the lines a branch changed.
#
# The decompiled upstream tree is not clang-format clean, so checking whole
# files would fail forever and would bury real changes under reindentation of
# code nobody touched. This uses git clang-format, which formats only the lines
# in the diff: editing an unformatted upstream file no longer obliges you to
# reformat it, but the lines you add still have to conform.
#
# Files this fork authored outright are formatted in full instead; see
# --whole-files below.

set -euo pipefail

# Pin the version: clang-format's output changes between releases, so a check
# that passes locally can fail in CI purely because of the tool version. If
# this needs to change, reformat the affected files in the same commit that
# bumps it, so that diff stays reviewable.
#
# The pin lives in .clang-format-version because three separate things have to
# agree on it: this script, .githooks/pre-commit, and the CI job that installs
# the tool. Three copies kept in sync by a comment is how they drift.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
clang_format_version_file="${script_dir}/.clang-format-version"
if [[ ! -s "${clang_format_version_file}" ]]; then
    echo "check-format: ${clang_format_version_file} is missing or empty" >&2
    exit 2
fi
CLANG_FORMAT_VERSION="$(tr -d '[:space:]' < "${clang_format_version_file}")"

# Files this fork wrote from scratch. Derived from the diff rather than a
# hand-kept list, so it cannot drift: a file counts as authored when the fork
# added it, and as merely edited when the fork only modified it. For authored
# files whole-file formatting is the standard, because there is no upstream
# formatting to preserve.

usage() {
    cat <<'EOF'
usage: ./check-format.sh [--fix] [--whole-files] [<base-ref>]

Checks the C sources changed between <base-ref> and HEAD.

  <base-ref>     defaults to the merge base with origin/vita, where this fork
                 diverges from upstream. That audits the whole fork, including
                 the ~150 upstream files it edits, so it is expected to report
                 the debt accumulated before this check existed. CI should pass
                 the pull request base instead, which checks only new lines.
                 Pass HEAD~1 to check a single commit.
  --fix          reformats instead of reporting.
  --whole-files  additionally requires the files this fork authored to be
                 clang-format clean end to end, not just on changed lines.

Requires clang-format ${CLANG_FORMAT_VERSION}.
EOF
}

fix=0
whole_files=0
base_ref=""
for arg in "$@"; do
    case "$arg" in
        -h|--help)
            usage
            exit 0
            ;;
        --fix)
            fix=1
            ;;
        --whole-files)
            whole_files=1
            ;;
        *)
            base_ref="$arg"
            ;;
    esac
done

if [[ -n "${CLANG_FORMAT_OVERRIDE:-}" ]]; then
    clang_format="$CLANG_FORMAT_OVERRIDE"
else
    clang_format="$(command -v clang-format || true)"
fi
if [[ -z "$clang_format" ]]; then
    echo "check-format: clang-format not found; install clang-format ${CLANG_FORMAT_VERSION}" >&2
    exit 2
fi

# Skip the version gate when the caller deliberately pointed at another build.
if [[ -z "${CLANG_FORMAT_OVERRIDE:-}" ]]; then
    found_version="$("$clang_format" --version | sed -n 's/.*version \([0-9][0-9.]*\).*/\1/p')"
    if [[ "$found_version" != "$CLANG_FORMAT_VERSION" ]]; then
        echo "check-format: need clang-format ${CLANG_FORMAT_VERSION}, found ${found_version:-unknown}" >&2
        echo "  Output differs between clang-format releases, so the check is pinned." >&2
        echo "  Set CLANG_FORMAT_OVERRIDE=/path/to/clang-format-${CLANG_FORMAT_VERSION} to use another install." >&2
        exit 2
    fi
fi

if ! command -v git-clang-format >/dev/null 2>&1; then
    echo "check-format: git-clang-format is not installed (it ships with clang-tools)" >&2
    exit 2
fi

if [[ -z "$base_ref" ]]; then
    if ! git rev-parse --verify -q origin/vita >/dev/null; then
        echo "check-format: origin/vita not found; fetch it or pass <base-ref>" >&2
        exit 2
    fi
    base_ref="$(git merge-base HEAD origin/vita)"
fi

if ! git rev-parse --verify -q "$base_ref" >/dev/null; then
    echo "check-format: '$base_ref' is not a revision" >&2
    exit 2
fi

# externals/ is vendored (SDL, stb) and is never ours to reformat.
mapfile -t files < <(
    git diff --name-only --diff-filter=AM "$base_ref" -- '*.c' '*.h' |
        grep -v '^externals/' || true
)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "check-format: no C sources changed since $(git describe --tags --always "$base_ref")"
    exit 0
fi

echo "check-format: ${#files[@]} changed C source(s) since $(git describe --tags --always "$base_ref")"

status=0

# 1. Changed lines must be formatted, for every file in the diff.
if [[ "$fix" -eq 1 ]]; then
    git clang-format --style=file "$base_ref" -- "${files[@]}"
else
    # git clang-format prints status prose on stdout when it changed nothing,
    # so only treat the output as a finding if it actually contains a patch.
    format_diff="$(git clang-format --style=file --diff "$base_ref" -- "${files[@]}" 2>/dev/null || true)"
    if printf '%s\n' "$format_diff" | grep -q '^diff --git '; then
        echo "check-format: changed lines are not clang-format clean:" >&2
        echo "$format_diff" >&2
        status=1
    fi
fi

# 2. Files this fork authored must be formatted end to end. Files it merely
#    edited are held to the changed-lines standard above instead.
if [[ "$whole_files" -eq 1 ]]; then
    mapfile -t owned < <(
        git diff --name-only --diff-filter=A "$base_ref" -- '*.c' '*.h' |
            grep -v '^externals/' || true
    )
    if [[ "$fix" -eq 1 ]]; then
        [[ ${#owned[@]} -gt 0 ]] && "$clang_format" -i "${owned[@]}"
    elif [[ ${#owned[@]} -gt 0 ]]; then
        echo "check-format: requiring whole-file formatting for ${#owned[@]} file(s) this fork added"
        "$clang_format" --dry-run -Werror "${owned[@]}" 2>&1 || status=1
    fi
fi

if [[ "$status" -ne 0 ]]; then
    echo "check-format: reformat with './check-format.sh --fix'" >&2
    exit 1
fi

echo "check-format: changed C sources match .clang-format"
