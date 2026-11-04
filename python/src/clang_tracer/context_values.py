"""Decode self-contained shared-state snapshots for console display."""

import json
from typing import Any, cast

from google.protobuf import descriptor_pb2, descriptor_pool, message_factory
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import DecodeError, Message

from .proto import context_transport_pb2 as cpb


def _field_name(field: FieldDescriptor) -> str:
    for option in field.GetOptions().uninterpreted_option:
        if (
            len(option.name) == 1
            and option.name[0].name_part == "tracer_cpp_field_name"
            and option.HasField("string_value")
        ):
            return str(option.string_value.decode("utf-8", errors="replace"))
    return str(field.name)


def state_values(
    envelope: cpb.ContextEventEnvelope, state: cpb.StatePayload
) -> list[tuple[str, str]]:
    """Return bounded field/value rows, or a readable decode diagnostic."""
    matches = [
        schema
        for schema in envelope.schemas
        if schema.epoch_schema_id == state.epoch_schema_id
    ]
    if len(matches) != 1 or not state.epoch_schema_id:
        return [("decode unavailable", "missing or duplicate state schema")]
    schema = matches[0]
    if state.schema_identity != schema.schema_identity:
        return [("decode unavailable", "state schema identity mismatch")]
    if (
        len(schema.serialized_file_descriptor_set) > 4 * 1024 * 1024
        or len(state.payload) > 32 * 1024 * 1024
    ):
        return [("decode unavailable", "state or schema exceeds display limit")]
    try:
        files = descriptor_pb2.FileDescriptorSet.FromString(
            schema.serialized_file_descriptor_set
        )
        if not files.file or len(files.file) > 256:
            raise ValueError("invalid descriptor file count")
        pool = descriptor_pool.DescriptorPool()
        pending = {file.name: file for file in files.file}
        if len(pending) != len(files.file):
            raise ValueError("duplicate descriptor file names")
        loaded: set[str] = set()
        while pending:
            ready = [
                name
                for name, file in pending.items()
                if all(dependency in loaded for dependency in file.dependency)
            ]
            if not ready:
                raise ValueError("missing or cyclic descriptor dependencies")
            for name in ready:
                pool.Add(pending.pop(name))
                loaded.add(name)
        descriptor = pool.FindMessageTypeByName(schema.root_type.lstrip("."))
        message = message_factory.GetMessageClass(descriptor).FromString(state.payload)
    except (DecodeError, KeyError, TypeError, ValueError) as error:
        return [("decode unavailable", str(error)[:512])]

    rows: list[tuple[str, str]] = []
    remaining = 16384

    def add(path: str, value: str) -> None:
        nonlocal remaining
        if len(rows) >= 256 or remaining <= 0:
            return
        path = path[:512]
        text = value[: min(4096, max(0, remaining - len(path)))]
        if len(text) < len(value):
            text += "…<truncated>"
        rows.append((path, text))
        remaining -= len(path) + len(text)

    def scalar(field: FieldDescriptor, value: Any) -> str:
        if field.type == FieldDescriptor.TYPE_STRING:
            text = json.dumps(value[:4096], ensure_ascii=False)
            return text + ("…<truncated>" if len(value) > 4096 else "")
        if field.type == FieldDescriptor.TYPE_BYTES:
            return repr(value[:4096]) + ("…<truncated>" if len(value) > 4096 else "")
        if field.type == FieldDescriptor.TYPE_ENUM:
            enum_type = field.enum_type
            item = enum_type.values_by_number.get(value) if enum_type else None
            return item.name if item else str(value)
        return str(value)

    def walk(message: Message, prefix: str, depth: int) -> None:
        if depth >= 16:
            add(prefix, "<depth limit>")
            return
        for raw_field in message.DESCRIPTOR.fields:
            field = cast(FieldDescriptor, raw_field)
            if len(rows) >= 256 or remaining <= 0:
                break
            path = f"{prefix}.{_field_name(field)}" if prefix else _field_name(field)
            value = getattr(message, field.name)
            repeated = getattr(field, "is_repeated", None)
            if repeated is None:  # Protobuf 5.29 does not expose is_repeated.
                repeated = getattr(field, "label") == FieldDescriptor.LABEL_REPEATED
            if repeated:
                if not value:
                    add(path, "[]")
                elif field.message_type and field.message_type.GetOptions().map_entry:
                    item_field = field.message_type.fields_by_name["value"]
                    for key in value:
                        if len(rows) >= 256 or remaining <= 0:
                            break
                        label = repr(key[:256]) if isinstance(key, str) else repr(key)
                        item_path = f"{path}[{label}]"
                        if item_field.type == FieldDescriptor.TYPE_MESSAGE:
                            walk(value[key], item_path, depth + 1)
                        else:
                            add(item_path, scalar(item_field, value[key]))
                else:
                    for index, item in enumerate(value):
                        if len(rows) >= 256 or remaining <= 0:
                            break
                        item_path = f"{path}[{index}]"
                        if field.type == FieldDescriptor.TYPE_MESSAGE:
                            walk(item, item_path, depth + 1)
                        else:
                            add(item_path, scalar(field, item))
            elif field.has_presence and not message.HasField(field.name):
                add(path, "<unset>")
            elif field.type == FieldDescriptor.TYPE_MESSAGE:
                walk(value, path, depth + 1)
            else:
                add(path, scalar(field, value))

    walk(message, "", 0)
    if len(rows) >= 256 or remaining <= 0:
        rows.append(("display", "<truncated>"))
    return rows
