#!/usr/bin/env bash
# Static analysis over the C sources this fork changed, using the repo's
# .clang-tidy config.
#
# Scoped to the diff (files and changed lines) for the same reason check-format.sh is: the decompiled
# upstream tree is enormous and was never written to survive analysis, so
# running over the whole tree is slow and produces noise nobody can act on.
#
# Non-blocking by default. The point of the first runs is to see what the
# configured checks report on this codebase, not to gate merges. Pass --strict
# once the set is understood; .clang-tidy already quarantines the families
# judged too noisy to enable.
#
# usage: ./check-tidy.sh [--strict] [--build <dir>] [<base-ref>]

set -euo pipefail

usage() {
    cat <<'EOF'
usage: ./check-tidy.sh [--strict] [--build <dir>] [<base-ref>]

  --strict        exit non-zero if any check reports anything
  --build <dir>   build directory holding compile_commands.json
                  (default: ./build, then the current directory)
  <base-ref>      analyse files changed since this ref
                  (default: merge-base with origin/turbocharged)

The build directory must come from a configured and built tree: clang-tidy
reads the compile database for the real include paths and defines. Configure
with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON (the CMakePresets 'common' preset
already sets it).

Note on the unity build: game/game_unity.h #includes 261 .c files and main.c
includes that, so the game is one translation unit. Most changed files are not
translation units and cannot be handed to clang-tidy on their own -- it would
guess the compile command, fail to find common.h and report a wall of bogus
errors. Those files are analysed through the TU that includes them instead,
and the script says which ones it did.
EOF
}

strict=0
build_dir=""
base_ref=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict) strict=1; shift ;;
        --build)  build_dir="${2:-}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        -*) echo "check-tidy: unknown option '$1'" >&2; usage >&2; exit 2 ;;
        *)  base_ref="$1"; shift ;;
    esac
done

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "check-tidy: clang-tidy is not installed" >&2
    exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "check-tidy: python3 is needed to read compile_commands.json" >&2
    exit 2
fi

# Prefer the build directory the caller named, then the conventional ones.
compile_db=""
for candidate in "${build_dir}" build . ; do
    [[ -z "$candidate" ]] && continue
    if [[ -f "${candidate}/compile_commands.json" ]]; then
        compile_db="$candidate"
        break
    fi
done
if [[ -z "$compile_db" ]]; then
    echo "check-tidy: no compile_commands.json found; configure with" >&2
    echo "  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON and build the tree first." >&2
    exit 2
fi

if [[ -z "$base_ref" ]]; then
    if ! git rev-parse --verify -q origin/turbocharged >/dev/null; then
        echo "check-tidy: origin/turbocharged not found; fetch it or pass <base-ref>" >&2
        exit 2
    fi
    base_ref="$(git merge-base HEAD origin/turbocharged)"
fi

if ! git rev-parse --verify -q "$base_ref" >/dev/null; then
    echo "check-tidy: '$base_ref' is not a revision" >&2
    exit 2
fi

repo_root="$(git rev-parse --show-toplevel)"

# The translation units the build actually compiles. Only these may be passed
# to clang-tidy; anything else makes it invent a compile command.
mapfile -t db_tus < <(
    python3 -c '
import json, os, sys
with open(sys.argv[1]) as handle:
    for entry in json.load(handle):
        print(os.path.normpath(entry["file"]))
' "${compile_db}/compile_commands.json"
)

is_tu() {
    local wanted="$1" entry
    for entry in "${db_tus[@]}"; do
        [[ "$entry" == "$wanted" ]] && return 0
    done
    return 1
}

# externals/ is vendored (SDL, stb) and is never ours to analyse.
mapfile -t changed < <(
    git diff --name-only --diff-filter=AM "$base_ref" -- '*.c' |
        grep -v '^externals/' || true
)
# Headers are not TUs either; a changed header is analysed through main.c.
mapfile -t changed_headers < <(
    git diff --name-only --diff-filter=AM "$base_ref" -- '*.h' |
        grep -v '^externals/' || true
)

if [[ ${#changed[@]} -eq 0 && ${#changed_headers[@]} -eq 0 ]]; then
    echo "check-tidy: no C sources changed since $(git describe --tags --always "$base_ref")"
    exit 0
fi

direct=()
covered=()
for file in "${changed[@]}" "${changed_headers[@]}"; do
    if is_tu "${repo_root}/${file}"; then
        direct+=("$file")
    else
        covered+=("$file")
    fi
done

# Files that are not their own TU get analysed through the unity TU that
# includes them. .clang-tidy's HeaderFilterRegex is what makes the findings
# inside them show up.
unity_tus=()
if [[ ${#covered[@]} -gt 0 ]] && is_tu "${repo_root}/main.c"; then
    unity_tus+=("main.c")
fi

analysis=("${direct[@]}")
for tu in "${unity_tus[@]}"; do
    # Do not analyse main.c twice if it was itself in the changed set.
    duplicate=0
    for existing in "${direct[@]}"; do
        [[ "$existing" == "$tu" ]] && duplicate=1 && break
    done
    [[ "$duplicate" -eq 0 ]] && analysis+=("$tu")
done

echo "check-tidy: $((${#changed[@]} + ${#changed_headers[@]})) changed C source/header(s) since $(git describe --tags --always "$base_ref")"
echo "check-tidy: clang-tidy $(clang-tidy --version | sed -n 's/^[[:space:]]*LLVM version //p' | head -1), compile database in '${compile_db}'"
echo "check-tidy: analysing ${#analysis[@]} translation unit(s): ${#direct[@]} directly, ${#unity_tus[@]} covering the other ${#covered[@]} changed file(s)"

if [[ ${#analysis[@]} -eq 0 ]]; then
    echo "check-tidy: nothing analysable"
    exit 0
fi

findings_file="$(mktemp)"
trap 'rm -f "$findings_file"' EXIT

# Report only findings on lines this branch added or changed. The unity build
# pulls the whole decompiled tree into the analysed TUs, so without a line
# filter every pre-existing upstream finding would be attributed to the diff.
line_filter="$(
    git diff -U0 --diff-filter=AM "$base_ref" -- '*.c' '*.h' ':!externals' |
        python3 -c '
import json, re, sys
files, name = {}, None
for line in sys.stdin:
    if line.startswith("+++ b/"):
        name = line[6:].strip()
    elif line.startswith("@@") and name:
        m = re.match(r"@@ -\S+ \+(\d+)(?:,(\d+))? @@", line)
        start, count = int(m.group(1)), int(m.group(2) or 1)
        if count:
            files.setdefault(name, []).append([start, start + count - 1])
print(json.dumps([{"name": n, "lines": l} for n, l in files.items()]))
'
)"

for file in "${analysis[@]}"; do
    clang-tidy -p "$compile_db" --quiet --line-filter="$line_filter" "$file" >> "$findings_file" 2>&1 || true
done

total="$(grep -cE '(warning|error): .*\[[a-z][a-z0-9.-]*\]$' "$findings_file" || true)"
total="${total:-0}"

if [[ "$total" -eq 0 ]]; then
    echo "check-tidy: no findings"
    exit 0
fi

# Group by check name so the output says what to triage rather than dumping
# thousands of lines into a CI log.
echo "check-tidy: ${total} finding(s), by check:"
sed -n 's/.*\[\([a-z][a-z0-9-]*\)\]$/\1/p' "$findings_file" | sort | uniq -c | sort -rn

# A clang-diagnostic-error means a TU did not compile, so the findings from it
# are not trustworthy. Say so rather than reporting a confident wrong number.
compile_errors="$(grep -c 'clang-diagnostic-error' "$findings_file" || true)"
if [[ "${compile_errors:-0}" -gt 0 ]]; then
    echo
    echo "check-tidy: ${compile_errors} TU(s) failed to compile under clang-tidy;"
    echo "  findings from those files are unreliable."
fi

echo
echo "check-tidy: re-run for detail with:"
echo "  clang-tidy -p ${compile_db} --quiet <file>"

if [[ "$strict" -eq 1 ]]; then
    exit 1
fi
