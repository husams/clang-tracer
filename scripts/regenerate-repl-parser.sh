#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $# -lt 1 || $# -gt 2 || (${2:-} != '' && ${2:-} != --check) ]]; then
  echo 'Usage: scripts/regenerate-repl-parser.sh /path/to/antlr-4.13.2-complete.jar [--check]' >&2
  exit 2
fi
jar_path=$(cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
expected=eae2dfa119a64327444672aff63e9ec35a20180dc5b8090b7a6ab85125df4d76
if command -v sha256sum >/dev/null; then
  digest=$(sha256sum "$jar_path")
else
  digest=$(shasum -a 256 "$jar_path")
fi
if [[ ${digest%% *} != "$expected" ]]; then
  echo 'ANTLR generator checksum mismatch; use the exact 4.13.2 complete jar.' >&2
  exit 1
fi
cd -- "$repo_dir"
output_dir=src/repl/generated
if [[ ${2:-} == --check ]]; then
  output_dir=$(mktemp -d)
  trap 'rm -rf -- "$output_dir"' EXIT
fi
"${TRACER_REPL_JAVA:-java}" -jar "$jar_path" -Dlanguage=Cpp -visitor -no-listener \
  -Xexact-output-dir -o "$output_dir" src/repl/TracerCommands.g4
if [[ ${2:-} == --check ]]; then
  diff -ru -- src/repl/generated "$output_dir"
fi
