from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class DeliveryProfile(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    DELIVERY_PROFILE_UNSPECIFIED: _ClassVar[DeliveryProfile]
    BEST_EFFORT: _ClassVar[DeliveryProfile]
    DURABLE: _ClassVar[DeliveryProfile]

class RecordingOperation(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    RECORDING_OPERATION_UNSPECIFIED: _ClassVar[RecordingOperation]
    CREATE: _ClassVar[RecordingOperation]
    RESUME: _ClassVar[RecordingOperation]
DELIVERY_PROFILE_UNSPECIFIED: DeliveryProfile
BEST_EFFORT: DeliveryProfile
DURABLE: DeliveryProfile
RECORDING_OPERATION_UNSPECIFIED: RecordingOperation
CREATE: RecordingOperation
RESUME: RecordingOperation

class ExportOpen(_message.Message):
    __slots__ = ("producer_session_id", "epoch_id", "protocol_version", "profile", "operation", "recording_id", "sink_id", "saved_ack_position", "max_packet_bytes", "max_event_bytes", "max_epoch_schemas", "max_epoch_schema_bytes", "native_pid", "process_start_time")
    PRODUCER_SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    EPOCH_ID_FIELD_NUMBER: _ClassVar[int]
    PROTOCOL_VERSION_FIELD_NUMBER: _ClassVar[int]
    PROFILE_FIELD_NUMBER: _ClassVar[int]
    OPERATION_FIELD_NUMBER: _ClassVar[int]
    RECORDING_ID_FIELD_NUMBER: _ClassVar[int]
    SINK_ID_FIELD_NUMBER: _ClassVar[int]
    SAVED_ACK_POSITION_FIELD_NUMBER: _ClassVar[int]
    MAX_PACKET_BYTES_FIELD_NUMBER: _ClassVar[int]
    MAX_EVENT_BYTES_FIELD_NUMBER: _ClassVar[int]
    MAX_EPOCH_SCHEMAS_FIELD_NUMBER: _ClassVar[int]
    MAX_EPOCH_SCHEMA_BYTES_FIELD_NUMBER: _ClassVar[int]
    NATIVE_PID_FIELD_NUMBER: _ClassVar[int]
    PROCESS_START_TIME_FIELD_NUMBER: _ClassVar[int]
    producer_session_id: bytes
    epoch_id: int
    protocol_version: int
    profile: DeliveryProfile
    operation: RecordingOperation
    recording_id: bytes
    sink_id: bytes
    saved_ack_position: int
    max_packet_bytes: int
    max_event_bytes: int
    max_epoch_schemas: int
    max_epoch_schema_bytes: int
    native_pid: int
    process_start_time: int
    def __init__(self, producer_session_id: _Optional[bytes] = ..., epoch_id: _Optional[int] = ..., protocol_version: _Optional[int] = ..., profile: _Optional[_Union[DeliveryProfile, str]] = ..., operation: _Optional[_Union[RecordingOperation, str]] = ..., recording_id: _Optional[bytes] = ..., sink_id: _Optional[bytes] = ..., saved_ack_position: _Optional[int] = ..., max_packet_bytes: _Optional[int] = ..., max_event_bytes: _Optional[int] = ..., max_epoch_schemas: _Optional[int] = ..., max_epoch_schema_bytes: _Optional[int] = ..., native_pid: _Optional[int] = ..., process_start_time: _Optional[int] = ...) -> None: ...

class SchemaRecord(_message.Message):
    __slots__ = ("schema_identity", "root_type", "serialized_file_descriptor_set", "source_lineage_id", "cpp_state_type", "cpp_scope_type", "revision_id", "container_roles", "epoch_schema_id")
    SCHEMA_IDENTITY_FIELD_NUMBER: _ClassVar[int]
    ROOT_TYPE_FIELD_NUMBER: _ClassVar[int]
    SERIALIZED_FILE_DESCRIPTOR_SET_FIELD_NUMBER: _ClassVar[int]
    SOURCE_LINEAGE_ID_FIELD_NUMBER: _ClassVar[int]
    CPP_STATE_TYPE_FIELD_NUMBER: _ClassVar[int]
    CPP_SCOPE_TYPE_FIELD_NUMBER: _ClassVar[int]
    REVISION_ID_FIELD_NUMBER: _ClassVar[int]
    CONTAINER_ROLES_FIELD_NUMBER: _ClassVar[int]
    EPOCH_SCHEMA_ID_FIELD_NUMBER: _ClassVar[int]
    schema_identity: bytes
    root_type: str
    serialized_file_descriptor_set: bytes
    source_lineage_id: bytes
    cpp_state_type: str
    cpp_scope_type: str
    revision_id: bytes
    container_roles: _containers.RepeatedScalarFieldContainer[str]
    epoch_schema_id: int
    def __init__(self, schema_identity: _Optional[bytes] = ..., root_type: _Optional[str] = ..., serialized_file_descriptor_set: _Optional[bytes] = ..., source_lineage_id: _Optional[bytes] = ..., cpp_state_type: _Optional[str] = ..., cpp_scope_type: _Optional[str] = ..., revision_id: _Optional[bytes] = ..., container_roles: _Optional[_Iterable[str]] = ..., epoch_schema_id: _Optional[int] = ...) -> None: ...

class TruncationPathStep(_message.Message):
    __slots__ = ("field_number", "list_index", "string_key", "signed_key", "unsigned_key", "bool_key")
    FIELD_NUMBER_FIELD_NUMBER: _ClassVar[int]
    LIST_INDEX_FIELD_NUMBER: _ClassVar[int]
    STRING_KEY_FIELD_NUMBER: _ClassVar[int]
    SIGNED_KEY_FIELD_NUMBER: _ClassVar[int]
    UNSIGNED_KEY_FIELD_NUMBER: _ClassVar[int]
    BOOL_KEY_FIELD_NUMBER: _ClassVar[int]
    field_number: int
    list_index: int
    string_key: str
    signed_key: int
    unsigned_key: int
    bool_key: bool
    def __init__(self, field_number: _Optional[int] = ..., list_index: _Optional[int] = ..., string_key: _Optional[str] = ..., signed_key: _Optional[int] = ..., unsigned_key: _Optional[int] = ..., bool_key: bool = ...) -> None: ...

class TruncationRecord(_message.Message):
    __slots__ = ("path", "total_items", "captured_items")
    PATH_FIELD_NUMBER: _ClassVar[int]
    TOTAL_ITEMS_FIELD_NUMBER: _ClassVar[int]
    CAPTURED_ITEMS_FIELD_NUMBER: _ClassVar[int]
    path: _containers.RepeatedCompositeFieldContainer[TruncationPathStep]
    total_items: int
    captured_items: int
    def __init__(self, path: _Optional[_Iterable[_Union[TruncationPathStep, _Mapping]]] = ..., total_items: _Optional[int] = ..., captured_items: _Optional[int] = ...) -> None: ...

class ContextStackFrame(_message.Message):
    __slots__ = ("pc", "stack_pointer", "kind", "function", "module", "file", "line", "module_offset", "application_frame")
    class Kind(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        NATIVE: _ClassVar[ContextStackFrame.Kind]
        TARGET_ENTRY: _ClassVar[ContextStackFrame.Kind]
        JIT_BRIDGE: _ClassVar[ContextStackFrame.Kind]
    NATIVE: ContextStackFrame.Kind
    TARGET_ENTRY: ContextStackFrame.Kind
    JIT_BRIDGE: ContextStackFrame.Kind
    PC_FIELD_NUMBER: _ClassVar[int]
    STACK_POINTER_FIELD_NUMBER: _ClassVar[int]
    KIND_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    FILE_FIELD_NUMBER: _ClassVar[int]
    LINE_FIELD_NUMBER: _ClassVar[int]
    MODULE_OFFSET_FIELD_NUMBER: _ClassVar[int]
    APPLICATION_FRAME_FIELD_NUMBER: _ClassVar[int]
    pc: int
    stack_pointer: int
    kind: ContextStackFrame.Kind
    function: str
    module: str
    file: str
    line: int
    module_offset: int
    application_frame: bool
    def __init__(self, pc: _Optional[int] = ..., stack_pointer: _Optional[int] = ..., kind: _Optional[_Union[ContextStackFrame.Kind, str]] = ..., function: _Optional[str] = ..., module: _Optional[str] = ..., file: _Optional[str] = ..., line: _Optional[int] = ..., module_offset: _Optional[int] = ..., application_frame: bool = ...) -> None: ...

class ContextStackTrace(_message.Message):
    __slots__ = ("status", "requested_depth", "frames", "detail", "application_only", "hidden_frames", "captured_depth")
    class Status(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        DISABLED: _ClassVar[ContextStackTrace.Status]
        COMPLETE: _ClassVar[ContextStackTrace.Status]
        TRUNCATED: _ClassVar[ContextStackTrace.Status]
        PARTIAL: _ClassVar[ContextStackTrace.Status]
    DISABLED: ContextStackTrace.Status
    COMPLETE: ContextStackTrace.Status
    TRUNCATED: ContextStackTrace.Status
    PARTIAL: ContextStackTrace.Status
    STATUS_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_DEPTH_FIELD_NUMBER: _ClassVar[int]
    FRAMES_FIELD_NUMBER: _ClassVar[int]
    DETAIL_FIELD_NUMBER: _ClassVar[int]
    APPLICATION_ONLY_FIELD_NUMBER: _ClassVar[int]
    HIDDEN_FRAMES_FIELD_NUMBER: _ClassVar[int]
    CAPTURED_DEPTH_FIELD_NUMBER: _ClassVar[int]
    status: ContextStackTrace.Status
    requested_depth: int
    frames: _containers.RepeatedCompositeFieldContainer[ContextStackFrame]
    detail: str
    application_only: bool
    hidden_frames: int
    captured_depth: int
    def __init__(self, status: _Optional[_Union[ContextStackTrace.Status, str]] = ..., requested_depth: _Optional[int] = ..., frames: _Optional[_Iterable[_Union[ContextStackFrame, _Mapping]]] = ..., detail: _Optional[str] = ..., application_only: bool = ..., hidden_frames: _Optional[int] = ..., captured_depth: _Optional[int] = ...) -> None: ...

class StatePayload(_message.Message):
    __slots__ = ("scope_id", "schema_identity", "payload", "truncations", "epoch_schema_id")
    SCOPE_ID_FIELD_NUMBER: _ClassVar[int]
    SCHEMA_IDENTITY_FIELD_NUMBER: _ClassVar[int]
    PAYLOAD_FIELD_NUMBER: _ClassVar[int]
    TRUNCATIONS_FIELD_NUMBER: _ClassVar[int]
    EPOCH_SCHEMA_ID_FIELD_NUMBER: _ClassVar[int]
    scope_id: int
    schema_identity: bytes
    payload: bytes
    truncations: _containers.RepeatedCompositeFieldContainer[TruncationRecord]
    epoch_schema_id: int
    def __init__(self, scope_id: _Optional[int] = ..., schema_identity: _Optional[bytes] = ..., payload: _Optional[bytes] = ..., truncations: _Optional[_Iterable[_Union[TruncationRecord, _Mapping]]] = ..., epoch_schema_id: _Optional[int] = ...) -> None: ...

class EventRecord(_message.Message):
    __slots__ = ("event_id", "source_id", "source_sequence", "task_id", "parent_task_id", "name", "states", "recording_position", "opaque_metadata", "effective_max_items", "invocation_id", "probe_id", "arguments", "named_values", "result", "phase", "parent_source_id", "parent_submission_sequence", "print_arguments", "print_result", "entry_stack")
    EVENT_ID_FIELD_NUMBER: _ClassVar[int]
    SOURCE_ID_FIELD_NUMBER: _ClassVar[int]
    SOURCE_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    TASK_ID_FIELD_NUMBER: _ClassVar[int]
    PARENT_TASK_ID_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    STATES_FIELD_NUMBER: _ClassVar[int]
    RECORDING_POSITION_FIELD_NUMBER: _ClassVar[int]
    OPAQUE_METADATA_FIELD_NUMBER: _ClassVar[int]
    EFFECTIVE_MAX_ITEMS_FIELD_NUMBER: _ClassVar[int]
    INVOCATION_ID_FIELD_NUMBER: _ClassVar[int]
    PROBE_ID_FIELD_NUMBER: _ClassVar[int]
    ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    NAMED_VALUES_FIELD_NUMBER: _ClassVar[int]
    RESULT_FIELD_NUMBER: _ClassVar[int]
    PHASE_FIELD_NUMBER: _ClassVar[int]
    PARENT_SOURCE_ID_FIELD_NUMBER: _ClassVar[int]
    PARENT_SUBMISSION_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    PRINT_ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    PRINT_RESULT_FIELD_NUMBER: _ClassVar[int]
    ENTRY_STACK_FIELD_NUMBER: _ClassVar[int]
    event_id: bytes
    source_id: bytes
    source_sequence: int
    task_id: bytes
    parent_task_id: bytes
    name: str
    states: _containers.RepeatedCompositeFieldContainer[StatePayload]
    recording_position: int
    opaque_metadata: bytes
    effective_max_items: int
    invocation_id: bytes
    probe_id: bytes
    arguments: _containers.RepeatedCompositeFieldContainer[TraceArgument]
    named_values: _containers.RepeatedCompositeFieldContainer[TraceNamedValue]
    result: TypedTraceValue
    phase: int
    parent_source_id: bytes
    parent_submission_sequence: int
    print_arguments: bool
    print_result: bool
    entry_stack: ContextStackTrace
    def __init__(self, event_id: _Optional[bytes] = ..., source_id: _Optional[bytes] = ..., source_sequence: _Optional[int] = ..., task_id: _Optional[bytes] = ..., parent_task_id: _Optional[bytes] = ..., name: _Optional[str] = ..., states: _Optional[_Iterable[_Union[StatePayload, _Mapping]]] = ..., recording_position: _Optional[int] = ..., opaque_metadata: _Optional[bytes] = ..., effective_max_items: _Optional[int] = ..., invocation_id: _Optional[bytes] = ..., probe_id: _Optional[bytes] = ..., arguments: _Optional[_Iterable[_Union[TraceArgument, _Mapping]]] = ..., named_values: _Optional[_Iterable[_Union[TraceNamedValue, _Mapping]]] = ..., result: _Optional[_Union[TypedTraceValue, _Mapping]] = ..., phase: _Optional[int] = ..., parent_source_id: _Optional[bytes] = ..., parent_submission_sequence: _Optional[int] = ..., print_arguments: bool = ..., print_result: bool = ..., entry_stack: _Optional[_Union[ContextStackTrace, _Mapping]] = ...) -> None: ...

class ContextEventEnvelope(_message.Message):
    __slots__ = ("producer_session_id", "epoch_id", "schemas", "event")
    PRODUCER_SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    EPOCH_ID_FIELD_NUMBER: _ClassVar[int]
    SCHEMAS_FIELD_NUMBER: _ClassVar[int]
    EVENT_FIELD_NUMBER: _ClassVar[int]
    producer_session_id: bytes
    epoch_id: int
    schemas: _containers.RepeatedCompositeFieldContainer[SchemaRecord]
    event: EventRecord
    def __init__(self, producer_session_id: _Optional[bytes] = ..., epoch_id: _Optional[int] = ..., schemas: _Optional[_Iterable[_Union[SchemaRecord, _Mapping]]] = ..., event: _Optional[_Union[EventRecord, _Mapping]] = ...) -> None: ...

class TypedTraceValue(_message.Message):
    __slots__ = ("value_type", "signed_value", "unsigned_value", "real_value")
    VALUE_TYPE_FIELD_NUMBER: _ClassVar[int]
    SIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    UNSIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    REAL_VALUE_FIELD_NUMBER: _ClassVar[int]
    value_type: int
    signed_value: int
    unsigned_value: int
    real_value: float
    def __init__(self, value_type: _Optional[int] = ..., signed_value: _Optional[int] = ..., unsigned_value: _Optional[int] = ..., real_value: _Optional[float] = ...) -> None: ...

class TraceArgument(_message.Message):
    __slots__ = ("index", "name", "value")
    INDEX_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    index: int
    name: str
    value: TypedTraceValue
    def __init__(self, index: _Optional[int] = ..., name: _Optional[str] = ..., value: _Optional[_Union[TypedTraceValue, _Mapping]] = ...) -> None: ...

class TraceNamedValue(_message.Message):
    __slots__ = ("kind", "argument_index", "name", "value")
    KIND_FIELD_NUMBER: _ClassVar[int]
    ARGUMENT_INDEX_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    kind: int
    argument_index: int
    name: str
    value: TypedTraceValue
    def __init__(self, kind: _Optional[int] = ..., argument_index: _Optional[int] = ..., name: _Optional[str] = ..., value: _Optional[_Union[TypedTraceValue, _Mapping]] = ...) -> None: ...

class ContextFrame(_message.Message):
    __slots__ = ("schema", "event")
    SCHEMA_FIELD_NUMBER: _ClassVar[int]
    EVENT_FIELD_NUMBER: _ClassVar[int]
    schema: SchemaRecord
    event: EventRecord
    def __init__(self, schema: _Optional[_Union[SchemaRecord, _Mapping]] = ..., event: _Optional[_Union[EventRecord, _Mapping]] = ...) -> None: ...

class ContextBatch(_message.Message):
    __slots__ = ("schemas", "event")
    SCHEMAS_FIELD_NUMBER: _ClassVar[int]
    EVENT_FIELD_NUMBER: _ClassVar[int]
    schemas: _containers.RepeatedCompositeFieldContainer[SchemaRecord]
    event: EventRecord
    def __init__(self, schemas: _Optional[_Iterable[_Union[SchemaRecord, _Mapping]]] = ..., event: _Optional[_Union[EventRecord, _Mapping]] = ...) -> None: ...

class ExportCounters(_message.Message):
    __slots__ = ("capture_failed", "queue_dropped", "transport_uncertain", "journal_dropped")
    CAPTURE_FAILED_FIELD_NUMBER: _ClassVar[int]
    QUEUE_DROPPED_FIELD_NUMBER: _ClassVar[int]
    TRANSPORT_UNCERTAIN_FIELD_NUMBER: _ClassVar[int]
    JOURNAL_DROPPED_FIELD_NUMBER: _ClassVar[int]
    capture_failed: int
    queue_dropped: int
    transport_uncertain: int
    journal_dropped: int
    def __init__(self, capture_failed: _Optional[int] = ..., queue_dropped: _Optional[int] = ..., transport_uncertain: _Optional[int] = ..., journal_dropped: _Optional[int] = ...) -> None: ...

class ProducerPacket(_message.Message):
    __slots__ = ("open", "data", "counters")
    OPEN_FIELD_NUMBER: _ClassVar[int]
    DATA_FIELD_NUMBER: _ClassVar[int]
    COUNTERS_FIELD_NUMBER: _ClassVar[int]
    open: ExportOpen
    data: ContextFrame
    counters: ExportCounters
    def __init__(self, open: _Optional[_Union[ExportOpen, _Mapping]] = ..., data: _Optional[_Union[ContextFrame, _Mapping]] = ..., counters: _Optional[_Union[ExportCounters, _Mapping]] = ...) -> None: ...

class StreamReady(_message.Message):
    __slots__ = ("protocol_version", "max_packet_bytes", "max_event_bytes", "max_epoch_schemas", "max_epoch_schema_bytes", "accepted_profile")
    PROTOCOL_VERSION_FIELD_NUMBER: _ClassVar[int]
    MAX_PACKET_BYTES_FIELD_NUMBER: _ClassVar[int]
    MAX_EVENT_BYTES_FIELD_NUMBER: _ClassVar[int]
    MAX_EPOCH_SCHEMAS_FIELD_NUMBER: _ClassVar[int]
    MAX_EPOCH_SCHEMA_BYTES_FIELD_NUMBER: _ClassVar[int]
    ACCEPTED_PROFILE_FIELD_NUMBER: _ClassVar[int]
    protocol_version: int
    max_packet_bytes: int
    max_event_bytes: int
    max_epoch_schemas: int
    max_epoch_schema_bytes: int
    accepted_profile: DeliveryProfile
    def __init__(self, protocol_version: _Optional[int] = ..., max_packet_bytes: _Optional[int] = ..., max_event_bytes: _Optional[int] = ..., max_epoch_schemas: _Optional[int] = ..., max_epoch_schema_bytes: _Optional[int] = ..., accepted_profile: _Optional[_Union[DeliveryProfile, str]] = ...) -> None: ...

class StreamFlow(_message.Message):
    __slots__ = ("paused",)
    PAUSED_FIELD_NUMBER: _ClassVar[int]
    paused: bool
    def __init__(self, paused: bool = ...) -> None: ...

class DurableControl(_message.Message):
    __slots__ = ("result", "operation", "recording_id", "durable_prefix", "detail")
    class Result(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        RESULT_UNSPECIFIED: _ClassVar[DurableControl.Result]
        ACCEPTED: _ClassVar[DurableControl.Result]
        REFUSED: _ClassVar[DurableControl.Result]
    RESULT_UNSPECIFIED: DurableControl.Result
    ACCEPTED: DurableControl.Result
    REFUSED: DurableControl.Result
    RESULT_FIELD_NUMBER: _ClassVar[int]
    OPERATION_FIELD_NUMBER: _ClassVar[int]
    RECORDING_ID_FIELD_NUMBER: _ClassVar[int]
    DURABLE_PREFIX_FIELD_NUMBER: _ClassVar[int]
    DETAIL_FIELD_NUMBER: _ClassVar[int]
    result: DurableControl.Result
    operation: RecordingOperation
    recording_id: bytes
    durable_prefix: int
    detail: str
    def __init__(self, result: _Optional[_Union[DurableControl.Result, str]] = ..., operation: _Optional[_Union[RecordingOperation, str]] = ..., recording_id: _Optional[bytes] = ..., durable_prefix: _Optional[int] = ..., detail: _Optional[str] = ...) -> None: ...

class CollectorControl(_message.Message):
    __slots__ = ("epoch_id", "ready", "flow", "durable")
    EPOCH_ID_FIELD_NUMBER: _ClassVar[int]
    READY_FIELD_NUMBER: _ClassVar[int]
    FLOW_FIELD_NUMBER: _ClassVar[int]
    DURABLE_FIELD_NUMBER: _ClassVar[int]
    epoch_id: int
    ready: StreamReady
    flow: StreamFlow
    durable: DurableControl
    def __init__(self, epoch_id: _Optional[int] = ..., ready: _Optional[_Union[StreamReady, _Mapping]] = ..., flow: _Optional[_Union[StreamFlow, _Mapping]] = ..., durable: _Optional[_Union[DurableControl, _Mapping]] = ...) -> None: ...
