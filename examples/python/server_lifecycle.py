#!/usr/bin/env python3
"""Start a local native server with ServerProcess and shut it down cleanly."""

import argparse
import shutil
import sys
import tempfile

from clang_tracer import Client, ServerProcess


def main() -> None:
    argparse.ArgumentParser(description=__doc__).parse_args()
    temporary = tempfile.TemporaryDirectory(prefix="clang-tracer-python-", delete=False)
    endpoint = f"unix://{temporary.name}/control.sock"
    server = None
    try:
        server = ServerProcess(endpoint)
        server.start()
        print(f"Managed server PID: {server.pid}; running={server.running}.")
        with Client(endpoint) as client:
            status = client.status()
            print(
                f"Client status: running={status.running}, "
                f"managed-by-client={status.managed}."
            )
            sessions = client.list_sessions()
            print(f"Sessions on this server: {len(sessions)}.")
        server.shutdown()
        print(f"Graceful shutdown complete; running={server.running}.")
    finally:
        shutdown_error = None
        if server is not None and server.running:
            try:
                server.shutdown()
            except BaseException as error:
                shutdown_error = error
        if server is not None and server.running:
            print(
                f"Managed server is still running (PID {server.pid}); "
                f"recovery endpoint: {endpoint}; "
                f"directory: {temporary.name}",
                file=sys.stderr,
            )
        else:
            shutil.rmtree(temporary.name)
        if shutdown_error is not None:
            raise shutdown_error


if __name__ == "__main__":
    main()
