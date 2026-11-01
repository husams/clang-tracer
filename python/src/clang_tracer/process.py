"""Own a local server child; SIGTERM requests the native server's full cleanup."""

import os
import platform
import subprocess
import tempfile
import time
from typing import BinaryIO, Self

import grpc

from .client import Client, ServerStatus, positive_timeout


class ServerProcessError(RuntimeError):
    """A managed server failed to start or shut down cleanly."""


class ServerProcess:
    def __init__(self, endpoint: str = "127.0.0.1:50051") -> None:
        self.endpoint = endpoint
        self._process: subprocess.Popen[bytes] | None = None
        self._log: BinaryIO | None = None
        self._termination_requested = False

    @property
    def pid(self) -> int | None:
        return self._process.pid if self._process is not None else None

    @property
    def running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    def status(self, *, timeout: float = 1) -> ServerStatus:
        with Client(self.endpoint) as client:
            status = client.status(timeout=timeout)
        return ServerStatus(
            self.endpoint,
            status.running,
            self.running,
            self.pid if self.running else None,
            status.detail,
        )

    def diagnostics(self) -> str:
        if self._log is None:
            return ""
        descriptor = self._log.fileno()
        size = os.fstat(descriptor).st_size
        # Do not move the inherited file offset while the child writes its log.
        return os.pread(descriptor, 8192, max(0, size - 8192)).decode(errors="replace")

    def start(
        self,
        executable: str = "clang-tracer-server",
        *,
        agent: str | None = None,
        timeout: float = 30,
        operation_timeout: int = 10,
        durable_directory: str | None = None,
    ) -> Self:
        positive_timeout(timeout)
        if isinstance(operation_timeout, bool) or not isinstance(
            operation_timeout, int
        ):
            raise ValueError("operation_timeout must be an integer between 1 and 300")
        if not 1 <= operation_timeout <= 300:
            raise ValueError("operation_timeout must be between 1 and 300")
        deadline = time.monotonic() + timeout
        if platform.system() != "Linux" or platform.machine() not in {
            "x86_64",
            "AMD64",
        }:
            raise ServerProcessError(
                "launch requires Linux x86_64; connect to a remote server"
            )
        if self.running:
            raise ServerProcessError("server already launched by this manager")
        endpoint_status = self.status(timeout=min(timeout, 0.5))
        if endpoint_status.running:
            raise ServerProcessError(
                "endpoint already serves a tracer; refusing to adopt it"
            )
        last_rpc_status = endpoint_status.detail or "UNAVAILABLE: endpoint not ready"
        if time.monotonic() >= deadline:
            raise ServerProcessError(
                f"server readiness timed out; last RPC status: {last_rpc_status}"
            )
        command = [
            executable,
            "--listen",
            self.endpoint,
            "--timeout",
            str(operation_timeout),
        ]
        if agent:
            command.extend(["--agent", agent])
        if durable_directory:
            command.extend(["--context-durable-dir", durable_directory])
        if self._log is not None:
            self._log.close()
        self._log = tempfile.TemporaryFile()
        self._termination_requested = False
        try:
            self._process = subprocess.Popen(
                command,
                stdin=subprocess.DEVNULL,
                stdout=self._log,
                stderr=self._log,
                shell=False,
                start_new_session=True,
            )
        except OSError:
            self._log.close()
            self._log = None
            raise
        try:
            with Client(self.endpoint) as client:
                while time.monotonic() < deadline:
                    if not self.running:
                        raise ServerProcessError(
                            f"server exited during startup: {self.diagnostics()}"
                        )
                    remaining = deadline - time.monotonic()
                    try:
                        client._wait_for_ready(timeout=max(0.001, min(0.2, remaining)))
                    except grpc.RpcError as error:
                        last_rpc_status = f"{error.code().name}: {error.details()}"
                    else:
                        last_rpc_status = "OK"
                        if self.running:
                            return self
                    time.sleep(min(0.05, max(0, deadline - time.monotonic())))
            detail = self.diagnostics()
            diagnostic = f"; {detail}" if detail else ""
            raise ServerProcessError(
                f"server readiness timed out; last RPC status: "
                f"{last_rpc_status}{diagnostic}"
            )
        except BaseException:
            self.shutdown(timeout=max(6, operation_timeout + 6))
            raise

    def shutdown(self, *, timeout: float = 30) -> None:
        positive_timeout(timeout)
        process = self._process
        if process is None:
            return
        if process.poll() is None and not self._termination_requested:
            self._termination_requested = True
            try:
                process.terminate()
            except ProcessLookupError:
                pass
        try:
            result = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise ServerProcessError(
                "graceful shutdown timed out; child retained for retry (no forced kill)"
            ) from error
        detail = self.diagnostics()
        self._process = None
        self._termination_requested = False
        if self._log is not None:
            self._log.close()
            self._log = None
        if result != 0:
            raise ServerProcessError(f"server exited with status {result}: {detail}")

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_: object) -> None:
        self.shutdown()
