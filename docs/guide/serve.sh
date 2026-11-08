#!/usr/bin/env sh
set -eu
bind=127.0.0.1
port=8000

usage() {
  printf '%s\n' 'Usage: serve.sh [--bind ADDRESS] [--port PORT]' \
    'Serve the Clang Tracer guide (default: 127.0.0.1:8000).'
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --bind|--port)
      if [ "$#" -lt 2 ] || [ -z "$2" ]; then
        printf 'Missing value for %s\n' "$1" >&2
        exit 2
      fi
      if [ "$1" = --bind ]; then bind=$2; else port=$2; fi
      shift 2 ;;
    *) usage >&2; exit 2 ;;
  esac
done

guide_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 -m http.server --bind "$bind" --directory "$guide_dir" "$port"
