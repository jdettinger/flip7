#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -x "$project_dir/build/flip7-sweep" ]; then
    "$project_dir/scripts/build.sh"
fi
exec "$project_dir/build/flip7-sweep" "$@"
