#!/bin/sh
# Apply the stat shim only to the SDK-built REPL and its server, not to the
# host Python runner (which may require a newer glibc than the SDK).
export LD_PRELOAD="$TRACER_TEST_IDENTITY_SHIM"
exec "$TRACER_TEST_REPL" "$@"
