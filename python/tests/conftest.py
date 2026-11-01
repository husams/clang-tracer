from concurrent.futures import ThreadPoolExecutor

import grpc
import pytest

from clang_tracer import Client
from clang_tracer import messages as pb
from clang_tracer.proto import tracer_pb2_grpc as rpc


class TracerService(rpc.TracerServicer):
    """A real binary gRPC peer; it does not simulate native process injection."""

    def __init__(self):
        self.requests = []
        self.cancelled = __import__("threading").Event()

    def Attach(self, request, context):
        self.requests.append(request)
        if request.pid == 999:
            context.abort(grpc.StatusCode.PERMISSION_DENIED, "ptrace denied")
        return pb.Session(id="s1", pid=request.pid, state=pb.Session.ATTACHED)

    def ListSessions(self, request, context):
        self.requests.append(request)
        return pb.ListSessionsResponse(
            sessions=[pb.Session(id="s1", pid=123, state=pb.Session.ATTACHED)]
        )

    def ResolveProbe(self, request, context):
        self.requests.append(request)
        return pb.ProbeSpec(
            symbol="_Z3addii",
            signature=pb.Signature(
                kind=pb.FUNCTION, result=pb.INT32, arguments=[pb.INT32, pb.INT32]
            ),
        )

    def AddProbe(self, request, context):
        self.requests.append(request)
        return pb.Probe(
            id=7,
            session_id=request.session_id,
            symbol=request.spec.symbol,
            signature=request.spec.signature,
            active=True,
        )

    def RemoveProbe(self, request, context):
        self.requests.append(request)
        return pb.Probe(
            id=request.probe_id,
            session_id=request.session_id,
            original_bytes_restored=True,
        )

    def Detach(self, request, context):
        self.requests.append(request)
        return pb.Session(id=request.session_id, state=pb.Session.DETACHED)

    def WatchEvents(self, request, context):
        self.requests.append(request)
        context.add_callback(self.cancelled.set)
        if request.after_sequence == 999:
            context.abort(grpc.StatusCode.OUT_OF_RANGE, "cursor evicted")
        if request.pid == 999:
            while context.is_active():
                self.cancelled.wait(0.01)
            return
        fixtures = [
            ("add(int, int)", "value"),
            ("Calculator::multiply(int, int) const", "multiply.begin"),
            ("add(int, int)", "value"),
            ("other()", "snapshot"),
        ]
        for i, (function_name, event_name) in enumerate(fixtures, 1):
            event = pb.TraceEvent(
                sequence=request.after_sequence + i,
                session_id="s1",
                pid=123,
                function_name=function_name,
                phase=pb.POST,
                call_id=i,
                result=pb.Value(type=pb.INT32, signed_value=42),
            )
            event.stack.frames.add(function="add", line=10)
            event.context.event.name = event_name
            if request.session_ids and event.session_id not in request.session_ids:
                continue
            if request.pid and event.pid != request.pid:
                continue
            if request.names and not any(
                name == event.function_name or name == event.context.event.name
                for name in request.names
            ):
                continue
            yield event

    def ListSymbols(self, request, _context):
        self.requests.append(request)
        candidates = [
            pb.FunctionSymbol(
                selector="add(int, int)",
                linkage_symbol="_Z3addii",
                module_path="/tmp/target",
            ),
            pb.FunctionSymbol(
                selector="Calculator::multiply(int, int) const",
                linkage_symbol="_ZNK10Calculator8multiplyEii",
                module_path="/tmp/target",
            ),
        ]
        return pb.ListSymbolsResponse(
            symbols=[item for item in candidates if request.query in item.selector]
        )


@pytest.fixture
def peer():
    service = TracerService()
    server = grpc.server(ThreadPoolExecutor(max_workers=4))
    rpc.add_TracerServicer_to_server(service, server)
    port = server.add_insecure_port("127.0.0.1:0")
    server.start()
    with Client(f"127.0.0.1:{port}", timeout=2) as client:
        yield client, service
    server.stop(0).wait(2)
