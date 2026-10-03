#!/usr/bin/env bash
# Package the current Linux x86 build for release.

exec "$(dirname "$0")/package.sh" linux "$@"
