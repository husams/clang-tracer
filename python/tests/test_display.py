import pytest
from google.protobuf import descriptor_pb2, descriptor_pool, message_factory

from clang_tracer import context
from clang_tracer import messages as pb
from clang_tracer.client import ServerStatus
from clang_tracer.display import render


def plain(value) -> str:
    return "".join(text for _, text in render(value))


def string_state_event():
    files = descriptor_pb2.FileDescriptorSet()
    file = files.file.add(name="snapshot.proto", package="test", syntax="proto3")
    root = file.message_type.add(name="Snapshot")
    field = root.field.add(name="f1", number=1, type=9, label=1)
    option = field.options.uninterpreted_option.add(string_value=b"message")
    option.name.add(name_part="tracer_cpp_field_name", is_extension=False)
    root.field.add(name="empty", number=2, type=9, label=1)
    root.field.add(name="count", number=3, type=5, label=1)
    root.field.add(name="tags", number=4, type=9, label=3)
    root.field.add(name="child", number=5, type=11, type_name=".test.Child", label=1)
    child = file.message_type.add(name="Child")
    child.field.add(name="text", number=1, type=9, label=1)
    pool = descriptor_pool.DescriptorPool()
    pool.Add(file)
    value = message_factory.GetMessageClass(
        pool.FindMessageTypeByName("test.Snapshot")
    )()
    value.f1 = "request-42\x1b[2J"
    value.tags.extend(["book", "café"])
    value.child.text = "nested"
    event = pb.TraceEvent()
    event.context.schemas.add(
        epoch_schema_id=1,
        schema_identity=b"revision",
        root_type="test.Snapshot",
        serialized_file_descriptor_set=files.SerializeToString(),
    )
    event.context.event.states.add(
        epoch_schema_id=1,
        schema_identity=b"revision",
        payload=value.SerializeToString(),
    )
    return event


def test_state_payload_shows_strings_defaults_repeated_and_nested_values():
    output = plain(string_state_event())
    assert 'message = "request-42\\u001b[2J"' in output
    assert 'empty = ""' in output
    assert "count = 0" in output
    assert 'tags[0] = "book"' in output
    assert 'tags[1] = "café"' in output
    assert 'child.text = "nested"' in output
    assert "\x1b" not in output


@pytest.mark.parametrize("damage", ["schema", "payload", "root", "identity"])
def test_invalid_state_payload_is_reported_without_crashing(damage):
    event = string_state_event()
    if damage == "schema":
        event.context.schemas[0].serialized_file_descriptor_set = b"\xff"
    elif damage == "payload":
        event.context.event.states[0].payload = b"\xff"
    elif damage == "root":
        event.context.schemas[0].root_type = "Missing"
    else:
        event.context.event.states[0].schema_identity = b"other"
    assert "decode unavailable" in plain(event)


def test_large_state_display_is_bounded():
    event = string_state_event()
    # A valid length-delimited string, larger than the console's value budget.
    event.context.event.states[0].payload = b"\x0a\x80\x80\x04" + b"x" * 65536
    output = plain(event)
    assert "<truncated>" in output
    assert len(output) < 20000


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


def test_validation_report_displays_status_options_and_diagnostic_location():
    report = pb.ValidationReport(
        success=False,
        level=pb.ValidateProbeRequest.SYNTAX,
        effective_options=["-std=c++17"],
    )
    report.diagnostics.add(
        stage="parse",
        severity="error",
        file="callback.cpp",
        line=3,
        column=8,
        message="expected ;",
    )
    output = plain(report)
    assert "validation failed level=syntax" in output
    assert "-std=c++17" in output
    assert "error [parse] callback.cpp:3:8" in output
    assert "expected ;" in output
