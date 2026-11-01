from clang_tracer import context
from clang_tracer import messages as pb
from clang_tracer.client import ServerStatus
from clang_tracer.display import render


def plain(value) -> str:
    return "".join(text for _, text in render(value))


def test_status_sessions_probes_and_specs_are_concise():
    status_text = plain(ServerStatus("unix:///tmp/control.sock", True, True, 42))
    assert (
        "server endpoint=unix:///tmp/control.sock running=True managed=True pid=42"
        in status_text
    )
    assert "ServerStatus" not in status_text

    session_text = plain(
        pb.Session(
            id="s-1",
            pid=42,
            state=pb.Session.ATTACHED,
            probes=[
                pb.Probe(
                    id=3,
                    session_id="s-1",
                    symbol="Calculator::multiply",
                    active=True,
                    entry_address=0x1234,
                    agent_dropped_events=2,
                )
            ],
        )
    )
    assert "session id=s-1 pid=42 state=ATTACHED" in session_text
    assert "probe 3 active symbol='Calculator::multiply'" in session_text
    assert "entry=0x1234" in session_text and "agent_dropped=2" in session_text

    spec_text = plain(
        pb.ProbeSpec(
            symbol="_Z3addii",
            signature=pb.Signature(
                kind=pb.FUNCTION,
                result=pb.INT32,
                arguments=[pb.INT32, pb.INT32],
            ),
            debug_info=pb.DebugInfo(selector="add(int, int)"),
        )
    )
    assert "selector='add(int, int)'" in spec_text
    assert "function (int32, int32) -> int32" in spec_text
    assert "callback_source" not in spec_text


def test_trace_events_show_named_and_fallback_values_result_stack_and_context():
    event = pb.TraceEvent(
        session_id="s\x1b[2J",
        probe_id=7,
        pid=42,
        tid=43,
        sequence=9,
        probe_sequence=21,
        function_name="Calculator::multiply(int, int) const",
        phase=pb.POST,
        call_id=12,
        object_address=0xABC,
        arguments=[
            pb.Value(type=pb.INT32, signed_value=0),
            pb.Value(type=pb.POINTER, unsigned_value=0x1234),
        ],
        named_arguments=[
            pb.NamedValue(
                name="value",
                argument_index=0,
                value=pb.Value(type=pb.INT32, signed_value=0),
            )
        ],
        object_fields=[
            pb.NamedValue(
                name="bias",
                argument_index=0,
                value=pb.Value(type=pb.INT32, signed_value=7),
            )
        ],
        result=pb.Value(type=pb.INT32, signed_value=0),
        agent_dropped_events=1,
        server_dropped_events=2,
        stack=pb.StackTrace(
            status=pb.StackTrace.COMPLETE,
            requested_depth=4,
            captured_depth=2,
            frames=[
                pb.StackFrame(
                    kind=pb.StackFrame.TARGET_ENTRY,
                    function="add",
                    module="target",
                    module_offset=0x30,
                    pc=0x1000,
                    stack_pointer=0x2000,
                    file="target.cpp",
                    line=8,
                )
            ],
        ),
    )
    context_event = event.context.event
    context_event.name = "update\x1b[31m"
    context_event.phase = pb.PRE
    context_event.source_sequence = 5
    context_event.arguments.add(
        index=3,
        name="factor",
        value=context.TypedTraceValue(value_type=pb.INT32, signed_value=2),
    )
    context_event.named_values.add(
        kind=1,
        name="named_total",
        value=context.TypedTraceValue(value_type=pb.INT64, signed_value=11),
    )
    context_event.named_values.add(
        kind=2,
        name="bias",
        value=context.TypedTraceValue(value_type=pb.INT32, signed_value=7),
    )
    schema = event.context.schemas.add(
        epoch_schema_id=2,
        root_type="State",
        cpp_state_type="Counter",
        cpp_scope_type="request",
        serialized_file_descriptor_set=b"abc",
    )
    assert schema.epoch_schema_id == 2
    state = context_event.states.add(
        scope_id=1,
        epoch_schema_id=2,
        payload=b"\x00state",
    )
    state.truncations.add(total_items=10, captured_items=3)

    output = plain(event)
    assert "sequence=9 probe_sequence=21" in output
    assert "function Calculator::multiply(int, int) const" in output
    assert "object=0xabc" in output
    assert "value=int32 0" in output
    assert "arg1=pointer 0x1234" in output
    assert "bias=int32 7" in output
    assert "result int32 0" in output
    assert "agent=1 server=2" in output
    assert "COMPLETE captured=2/4" in output
    assert "add target+0x30 target.cpp:8" in output
    assert "root=State state=Counter scope=request descriptor_bytes=3" in output
    assert "factor=int32 2" in output
    assert "named value named_total=int64 11" in output
    assert "object field bias=int32 7" in output
    assert "bytes=6 truncations=1" in output
    assert "captured=3/10 path_steps=0" in output
    assert "\\x1b" in output and "\x1b" not in output


def test_sparse_named_argument_index_and_pre_event_do_not_dump_proto_repr():
    event = pb.TraceEvent(
        phase=pb.PRE,
        named_arguments=[
            pb.NamedValue(
                name="far",
                argument_index=0xFFFFFFFF,
                value=pb.Value(type=pb.UINT64, unsigned_value=5),
            )
        ],
        result=pb.Value(type=pb.INT32, signed_value=0),
    )
    output = plain(event)
    assert "far=uint64 5" in output
    assert "result" not in output
    assert "sequence" in output and "TraceEvent" not in output

    legacy = plain(pb.TraceEvent(phase=pb.PRE, callback_value=0))
    assert "callback 0" in legacy

    legacy_named = plain(
        pb.TraceEvent(
            phase=pb.PRE,
            callback_value=17,
            named_arguments=[
                pb.NamedValue(
                    name="left",
                    argument_index=0,
                    value=pb.Value(type=pb.INT32, signed_value=2),
                )
            ],
        )
    )
    assert "left=int32 2" in legacy_named
    assert "callback 17" in legacy_named


def test_display_sanitizes_source_strings_and_colors_error_output():
    session = pb.Session(id="x\x1b[31m", state=pb.Session.FAILED)
    rendered = render(session)
    output = "".join(text for _, text in rendered)
    assert "\\x1b[31m" in output
    assert "\x1b" not in output
    assert any(style == "class:heading" for style, _ in rendered)

    error = render("bad\x1b[2J\nnext", error=True)
    assert "".join(text for _, text in error) == "bad\\x1b[2J\nnext"
    assert all(style == "class:error" for style, _ in error)


def test_symbol_page_has_readable_names_and_pagination_notice():
    page = pb.ListSymbolsResponse(next_page_token="opaque")
    page.symbols.add(
        selector="Calculator::multiply(int, int) const",
        linkage_symbol="_ZNK10Calculator8multiplyEii",
        module_path="/tmp/target",
    )
    output = plain(page)
    assert "Calculator::multiply(int, int) const" in output
    assert "_ZNK10Calculator8multiplyEii" in output
    assert "/tmp/target" in output
    assert "next-page command" in output
