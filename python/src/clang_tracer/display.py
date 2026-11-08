"""Small, readable prompt-toolkit renderers for public tracer messages."""

from typing import Any

from google.protobuf.message import Message
from prompt_toolkit.formatted_text import FormattedText
from prompt_toolkit.formatted_text.utils import fragment_list_to_text

from .client import ServerStatus
from .context_values import state_values
from .proto import context_transport_pb2 as cpb
from .proto import tracer_pb2 as pb


def _safe(value: object) -> str:
    """Escape controls in data that may originate in a traced process."""
    text = str(value)
    escaped = []
    for character in text:
        if character.isprintable():
            escaped.append(character)
        elif ord(character) <= 0xFF:
            escaped.append(f"\\x{ord(character):02x}")
        else:
            escaped.append(f"\\u{ord(character):04x}")
    return "".join(escaped)


def _safe_block(value: str) -> str:
    """Keep intentional help-text line breaks while escaping terminal controls."""
    return "\n".join(_safe(line) for line in value.split("\n"))


def _enum(enum_type: Any, value: int) -> str:
    try:
        return str(enum_type.Name(value))
    except ValueError:
        return f"UNKNOWN({value})"


def _hex(value: int) -> str:
    return f"0x{value:x}"


def _short_bytes(value: bytes) -> str:
    encoded = value.hex()
    return encoded if len(encoded) <= 16 else f"{encoded[:16]}…"


class _Lines:
    def __init__(self) -> None:
        self.fragments: list[tuple[str, str]] = []

    def add(
        self,
        label: str,
        detail: str = "",
        *,
        indent: int = 0,
        style: str = "",
    ) -> None:
        self.fragments.append(("class:label", "  " * indent + _safe(label)))
        if detail:
            self.fragments.append((style, " " + _safe(detail)))
        self.fragments.append(("", "\n"))

    def finish(self) -> FormattedText:
        if self.fragments and self.fragments[-1][1] == "\n":
            self.fragments.pop()
        return FormattedText(self.fragments)


def _value(value: pb.Value) -> str:
    kind = _enum(pb.ValueType, value.type)
    arm = value.WhichOneof("data")
    if arm is None:
        payload = "void" if value.type == pb.VOID else "<unset>"
    elif value.type in (pb.POINTER, pb.OBJECT) and arm == "unsigned_value":
        payload = _hex(value.unsigned_value)
    else:
        payload = _safe(getattr(value, arm))
    return f"{kind.lower()} {payload}"


def _typed_context_value(value: cpb.TypedTraceValue) -> str:
    kind = _enum(pb.ValueType, value.value_type)
    arm = value.WhichOneof("value")
    if arm is None:
        payload = "void" if value.value_type == pb.VOID else "<unset>"
    elif value.value_type in (pb.POINTER, pb.OBJECT) and arm == "unsigned_value":
        payload = _hex(value.unsigned_value)
    else:
        payload = _safe(getattr(value, arm))
    return f"{kind.lower()} {payload}"


def _signature(signature: pb.Signature) -> str:
    kind = _enum(pb.CallableKind, signature.kind).lower()
    result = _enum(pb.ValueType, signature.result).lower()
    arguments = ", ".join(
        _enum(pb.ValueType, item).lower() for item in signature.arguments
    )
    return f"{kind} ({arguments}) -> {result}"


def _probe(lines: _Lines, probe: pb.Probe, *, indent: int) -> None:
    state = "active" if probe.active else "inactive"
    lines.add(
        f"probe {probe.id}",
        f"{state} symbol={probe.symbol!r} session={probe.session_id} "
        f"entry={_hex(probe.entry_address)} callback={_hex(probe.callback_address)} "
        f"restored={probe.original_bytes_restored}",
        indent=indent,
        style="class:ok" if probe.active else "class:warning",
    )
    if probe.agent_dropped_events or probe.callback_errors:
        lines.add(
            "diagnostics",
            f"agent_dropped={probe.agent_dropped_events} "
            f"callback_errors={probe.callback_errors}",
            indent=indent + 1,
            style="class:warning",
        )


def _session(lines: _Lines, session: pb.Session) -> None:
    lines.add(
        "session",
        f"id={session.id} pid={session.pid} "
        f"state={_enum(pb.Session.State, session.state)}",
        style="class:heading",
    )
    if session.error:
        lines.add("error", session.error, indent=1, style="class:error")
    for module in session.application_modules:
        lines.add("module", module, indent=1, style="class:muted")
    for probe in session.probes:
        _probe(lines, probe, indent=1)


def _probe_spec(lines: _Lines, spec: pb.ProbeSpec) -> None:
    selector = spec.debug_info.selector or spec.symbol
    lines.add("probe spec", f"selector={selector!r}", style="class:heading")
    lines.add("signature", _signature(spec.signature), indent=1)
    if spec.receiver.record_name:
        lines.add(
            "receiver",
            f"{spec.receiver.record_name} size={spec.receiver.size} "
            f"alignment={spec.receiver.alignment}",
            indent=1,
        )
        for field in spec.receiver.fields:
            lines.add(
                "field",
                f"{field.name}: {field.canonical_type} "
                f"access={field.access} bit_offset={field.bit_offset}",
                indent=2,
            )
    if spec.receiver_unavailable_reason:
        lines.add("receiver unavailable", spec.receiver_unavailable_reason, indent=1)
    if spec.debug_info.module:
        lines.add(
            "debug info",
            f"module={spec.debug_info.module} "
            f"entry={_hex(spec.debug_info.runtime_entry)}",
            indent=1,
        )
    depth = "full" if spec.stack_depth == (1 << 32) - 1 else spec.stack_depth
    lines.add(
        "options",
        f"stack_depth={depth} "
        f"internal_frames={spec.include_internal_stack_frames} "
        f"print_arguments={spec.print_arguments} print_result={spec.print_result}",
        indent=1,
    )
    if spec.include_headers:
        lines.add("include", ", ".join(spec.include_headers), indent=1)
    if spec.header_context.inference:
        lines.add("compile command", spec.header_context.inference, indent=1)
    if spec.callback_package_path:
        lines.add("callback package", spec.callback_package_path, indent=1)


def _stack(lines: _Lines, stack: pb.StackTrace, *, indent: int) -> None:
    status = _enum(pb.StackTrace.Status, stack.status)
    depth = "full" if stack.requested_depth == (1 << 32) - 1 else stack.requested_depth
    lines.add(
        "stack",
        f"{status} captured={stack.captured_depth}/{depth} "
        f"hidden={stack.hidden_frames} application_only={stack.application_only}",
        indent=indent,
        style="class:warning"
        if stack.status in (pb.StackTrace.PARTIAL, pb.StackTrace.TRUNCATED)
        else "",
    )
    if stack.detail:
        lines.add("detail", stack.detail, indent=indent + 1)
    for frame in stack.frames:
        location = f"{frame.file}:{frame.line}" if frame.file else "unknown location"
        module = (
            f"{frame.module}+{_hex(frame.module_offset)}"
            if frame.module
            else "unknown module"
        )
        lines.add(
            _enum(pb.StackFrame.Kind, frame.kind),
            f"{frame.function or '<unknown>'} {module} {location} "
            f"pc={_hex(frame.pc)} sp={_hex(frame.stack_pointer)}",
            indent=indent + 1,
        )


def _context(lines: _Lines, envelope: cpb.ContextEventEnvelope) -> None:
    lines.add(
        "shared context",
        f"epoch={envelope.epoch_id} "
        f"producer={_short_bytes(envelope.producer_session_id)} "
        f"schemas={len(envelope.schemas)}",
        style="class:heading",
    )
    for schema in envelope.schemas:
        lines.add(
            f"schema #{schema.epoch_schema_id}",
            f"root={schema.root_type or '<unknown>'} "
            f"state={schema.cpp_state_type or '<unknown>'} "
            f"scope={schema.cpp_scope_type or '<unknown>'} "
            f"descriptor_bytes={len(schema.serialized_file_descriptor_set)}",
            indent=1,
        )
    if not envelope.HasField("event"):
        return
    event = envelope.event
    lines.add(
        "context event",
        f"name={event.name!r} source_sequence={event.source_sequence} "
        f"id={_short_bytes(event.event_id)} phase={_enum(pb.TracePhase, event.phase)}",
        indent=1,
    )
    for argument in event.arguments:
        name = argument.name or f"arg{argument.index}"
        lines.add(
            "argument",
            f"{name}={_typed_context_value(argument.value)}",
            indent=2,
        )
    for named in event.named_values:
        label = {1: "named value", 2: "object field"}.get(
            named.kind, f"named value kind={named.kind}"
        )
        lines.add(
            label,
            f"{named.name}={_typed_context_value(named.value)}",
            indent=2,
        )
    if event.HasField("result"):
        lines.add("context result", _typed_context_value(event.result), indent=2)
    for state in event.states:
        lines.add(
            "state payload",
            f"scope={state.scope_id} schema={state.epoch_schema_id} "
            f"bytes={len(state.payload)} truncations={len(state.truncations)}",
            indent=2,
        )
        for name, value in state_values(envelope, state):
            lines.add(name, f"= {value}", indent=3)
        for truncation in state.truncations:
            lines.add(
                "truncation",
                f"captured={truncation.captured_items}/{truncation.total_items} "
                f"path_steps={len(truncation.path)}",
                indent=3,
            )


def _trace_event(lines: _Lines, event: pb.TraceEvent) -> None:
    phase = _enum(pb.TracePhase, event.phase)
    lines.add(
        f"trace {phase}",
        f"sequence={event.sequence} probe_sequence={event.probe_sequence} "
        f"probe={event.probe_id} pid={event.pid} "
        f"tid={event.tid} call={event.call_id} session={event.session_id} "
        f"object={_hex(event.object_address)}",
        style="class:heading",
    )
    if event.function_name:
        lines.add("function", event.function_name, indent=1)
    named = {item.argument_index: item for item in event.named_arguments}
    for index in sorted(set(range(len(event.arguments))) | set(named)):
        item = named.get(index)
        if item is not None:
            name = item.name or f"arg{index}"
            value = item.value
        else:
            name = f"arg{index}"
            value = event.arguments[index]
        lines.add("argument", f"{name}={_value(value)}", indent=1)
    for field in event.object_fields:
        name = field.name or f"field{field.argument_index}"
        lines.add("field", f"{name}={_value(field.value)}", indent=1)
    if phase == "POST" and event.HasField("result"):
        lines.add("result", _value(event.result), indent=1, style="class:ok")
    if not event.HasField("context") or event.callback_value:
        lines.add("callback", str(event.callback_value), indent=1)
    if event.agent_dropped_events or event.server_dropped_events:
        lines.add(
            "dropped events",
            f"agent={event.agent_dropped_events} server={event.server_dropped_events}",
            style="class:warning",
        )
    if event.HasField("stack") and event.stack.status != pb.StackTrace.DISABLED:
        _stack(lines, event.stack, indent=1)
    if event.HasField("context"):
        _context(lines, event.context)


def _symbols(lines: _Lines, page: pb.ListSymbolsResponse) -> None:
    if not page.symbols:
        lines.add("symbols", "no matching functions or methods", style="class:muted")
    for item in page.symbols:
        lines.add(
            item.selector,
            f"{item.linkage_symbol} in {item.module_path}",
            style="class:heading",
        )
    if page.next_page_token:
        lines.add("more", "use the next-page command printed by the console")


def _validation_report(lines: _Lines, report: pb.ValidationReport) -> None:
    level = "syntax" if report.level == pb.ValidateProbeRequest.SYNTAX else "compile"
    lines.add(
        "validation",
        f"{'success' if report.success else 'failed'} level={level}",
        style="class:ok" if report.success else "class:error",
    )
    for option in report.effective_options:
        lines.add("option", option, indent=1)
    for diagnostic in report.diagnostics:
        location = diagnostic.file
        if diagnostic.line:
            location += f":{diagnostic.line}"
            if diagnostic.column:
                location += f":{diagnostic.column}"
        label = f"{diagnostic.severity} [{diagnostic.stage}]"
        lines.add(label, location, indent=1, style="class:error")
        lines.add("message", diagnostic.message, indent=2)


def render(value: object, *, error: bool = False) -> FormattedText:
    """Render known tracer objects without exposing Protobuf debug reprs."""
    if isinstance(value, str):
        style = "class:error" if error else ""
        return FormattedText([(style, _safe_block(value))])

    lines = _Lines()
    if isinstance(value, ServerStatus):
        lines.add(
            "server",
            f"endpoint={value.endpoint} running={value.running} "
            f"managed={value.managed} pid={value.pid or 'none'}",
            style="class:ok" if value.running else "class:warning",
        )
        if value.detail:
            lines.add("detail", value.detail, indent=1, style="class:warning")
    elif isinstance(value, pb.Session):
        _session(lines, value)
    elif isinstance(value, pb.Probe):
        _probe(lines, value, indent=0)
    elif isinstance(value, pb.ProbeSpec):
        _probe_spec(lines, value)
    elif isinstance(value, pb.TraceEvent):
        _trace_event(lines, value)
    elif isinstance(value, pb.ListSymbolsResponse):
        _symbols(lines, value)
    elif isinstance(value, pb.ValidationReport):
        _validation_report(lines, value)
    elif isinstance(value, Message):
        lines.add("message", value.DESCRIPTOR.full_name)
    else:
        return FormattedText(
            [("class:error" if error else "", _safe_block(str(value)))]
        )

    rendered = lines.finish()
    if error:
        return FormattedText([("class:error", fragment_list_to_text(rendered))])
    return rendered
