from . import context_transport_pb2 as _context_transport_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ValueType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    VALUE_TYPE_UNSPECIFIED: _ClassVar[ValueType]
    VOID: _ClassVar[ValueType]
    INT32: _ClassVar[ValueType]
    INT64: _ClassVar[ValueType]
    UINT64: _ClassVar[ValueType]
    FLOAT32: _ClassVar[ValueType]
    FLOAT64: _ClassVar[ValueType]
    POINTER: _ClassVar[ValueType]
    OBJECT: _ClassVar[ValueType]

class CallableKind(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    CALLABLE_KIND_UNSPECIFIED: _ClassVar[CallableKind]
    FUNCTION: _ClassVar[CallableKind]
    METHOD: _ClassVar[CallableKind]

class TracePhase(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TRACE_PHASE_UNSPECIFIED: _ClassVar[TracePhase]
    PRE: _ClassVar[TracePhase]
    POST: _ClassVar[TracePhase]
VALUE_TYPE_UNSPECIFIED: ValueType
VOID: ValueType
INT32: ValueType
INT64: ValueType
UINT64: ValueType
FLOAT32: ValueType
FLOAT64: ValueType
POINTER: ValueType
OBJECT: ValueType
CALLABLE_KIND_UNSPECIFIED: CallableKind
FUNCTION: CallableKind
METHOD: CallableKind
TRACE_PHASE_UNSPECIFIED: TracePhase
PRE: TracePhase
POST: TracePhase

class Signature(_message.Message):
    __slots__ = ("kind", "result", "arguments")
    KIND_FIELD_NUMBER: _ClassVar[int]
    RESULT_FIELD_NUMBER: _ClassVar[int]
    ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    kind: CallableKind
    result: ValueType
    arguments: _containers.RepeatedScalarFieldContainer[ValueType]
    def __init__(self, kind: _Optional[_Union[CallableKind, str]] = ..., result: _Optional[_Union[ValueType, str]] = ..., arguments: _Optional[_Iterable[_Union[ValueType, str]]] = ...) -> None: ...

class ReceiverField(_message.Message):
    __slots__ = ("name", "canonical_type", "bit_offset", "bit_width", "bit_size", "bit_alignment", "is_mutable", "access", "abi_identity")
    NAME_FIELD_NUMBER: _ClassVar[int]
    CANONICAL_TYPE_FIELD_NUMBER: _ClassVar[int]
    BIT_OFFSET_FIELD_NUMBER: _ClassVar[int]
    BIT_WIDTH_FIELD_NUMBER: _ClassVar[int]
    BIT_SIZE_FIELD_NUMBER: _ClassVar[int]
    BIT_ALIGNMENT_FIELD_NUMBER: _ClassVar[int]
    IS_MUTABLE_FIELD_NUMBER: _ClassVar[int]
    ACCESS_FIELD_NUMBER: _ClassVar[int]
    ABI_IDENTITY_FIELD_NUMBER: _ClassVar[int]
    name: str
    canonical_type: str
    bit_offset: int
    bit_width: int
    bit_size: int
    bit_alignment: int
    is_mutable: bool
    access: str
    abi_identity: str
    def __init__(self, name: _Optional[str] = ..., canonical_type: _Optional[str] = ..., bit_offset: _Optional[int] = ..., bit_width: _Optional[int] = ..., bit_size: _Optional[int] = ..., bit_alignment: _Optional[int] = ..., is_mutable: bool = ..., access: _Optional[str] = ..., abi_identity: _Optional[str] = ...) -> None: ...

class ReceiverContext(_message.Message):
    __slots__ = ("translation_unit", "working_directory", "compiler_arguments", "record_name", "clang_version", "target_triple", "size", "alignment", "fields", "is_volatile")
    TRANSLATION_UNIT_FIELD_NUMBER: _ClassVar[int]
    WORKING_DIRECTORY_FIELD_NUMBER: _ClassVar[int]
    COMPILER_ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    RECORD_NAME_FIELD_NUMBER: _ClassVar[int]
    CLANG_VERSION_FIELD_NUMBER: _ClassVar[int]
    TARGET_TRIPLE_FIELD_NUMBER: _ClassVar[int]
    SIZE_FIELD_NUMBER: _ClassVar[int]
    ALIGNMENT_FIELD_NUMBER: _ClassVar[int]
    FIELDS_FIELD_NUMBER: _ClassVar[int]
    IS_VOLATILE_FIELD_NUMBER: _ClassVar[int]
    translation_unit: str
    working_directory: str
    compiler_arguments: _containers.RepeatedScalarFieldContainer[str]
    record_name: str
    clang_version: str
    target_triple: str
    size: int
    alignment: int
    fields: _containers.RepeatedCompositeFieldContainer[ReceiverField]
    is_volatile: bool
    def __init__(self, translation_unit: _Optional[str] = ..., working_directory: _Optional[str] = ..., compiler_arguments: _Optional[_Iterable[str]] = ..., record_name: _Optional[str] = ..., clang_version: _Optional[str] = ..., target_triple: _Optional[str] = ..., size: _Optional[int] = ..., alignment: _Optional[int] = ..., fields: _Optional[_Iterable[_Union[ReceiverField, _Mapping]]] = ..., is_volatile: bool = ...) -> None: ...

class DebugField(_message.Message):
    __slots__ = ("name", "builtin_type", "byte_offset", "byte_size", "access", "type_chain")
    NAME_FIELD_NUMBER: _ClassVar[int]
    BUILTIN_TYPE_FIELD_NUMBER: _ClassVar[int]
    BYTE_OFFSET_FIELD_NUMBER: _ClassVar[int]
    BYTE_SIZE_FIELD_NUMBER: _ClassVar[int]
    ACCESS_FIELD_NUMBER: _ClassVar[int]
    TYPE_CHAIN_FIELD_NUMBER: _ClassVar[int]
    name: str
    builtin_type: str
    byte_offset: int
    byte_size: int
    access: str
    type_chain: _containers.RepeatedCompositeFieldContainer[DebugTypeLevel]
    def __init__(self, name: _Optional[str] = ..., builtin_type: _Optional[str] = ..., byte_offset: _Optional[int] = ..., byte_size: _Optional[int] = ..., access: _Optional[str] = ..., type_chain: _Optional[_Iterable[_Union[DebugTypeLevel, _Mapping]]] = ...) -> None: ...

class DebugTypeLevel(_message.Message):
    __slots__ = ("kind", "byte_size", "dwarf_encoding", "is_const", "is_volatile")
    class Kind(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNSPECIFIED: _ClassVar[DebugTypeLevel.Kind]
        POINTER: _ClassVar[DebugTypeLevel.Kind]
        BUILTIN: _ClassVar[DebugTypeLevel.Kind]
        VOID: _ClassVar[DebugTypeLevel.Kind]
    UNSPECIFIED: DebugTypeLevel.Kind
    POINTER: DebugTypeLevel.Kind
    BUILTIN: DebugTypeLevel.Kind
    VOID: DebugTypeLevel.Kind
    KIND_FIELD_NUMBER: _ClassVar[int]
    BYTE_SIZE_FIELD_NUMBER: _ClassVar[int]
    DWARF_ENCODING_FIELD_NUMBER: _ClassVar[int]
    IS_CONST_FIELD_NUMBER: _ClassVar[int]
    IS_VOLATILE_FIELD_NUMBER: _ClassVar[int]
    kind: DebugTypeLevel.Kind
    byte_size: int
    dwarf_encoding: int
    is_const: bool
    is_volatile: bool
    def __init__(self, kind: _Optional[_Union[DebugTypeLevel.Kind, str]] = ..., byte_size: _Optional[int] = ..., dwarf_encoding: _Optional[int] = ..., is_const: bool = ..., is_volatile: bool = ...) -> None: ...

class DebugInfo(_message.Message):
    __slots__ = ("module", "device", "inode", "file_size", "modification_ns", "change_ns", "selector", "record_name", "record_size", "fields", "receiver_volatile", "arguments", "entry_virtual_address", "entry_size", "runtime_entry")
    MODULE_FIELD_NUMBER: _ClassVar[int]
    DEVICE_FIELD_NUMBER: _ClassVar[int]
    INODE_FIELD_NUMBER: _ClassVar[int]
    FILE_SIZE_FIELD_NUMBER: _ClassVar[int]
    MODIFICATION_NS_FIELD_NUMBER: _ClassVar[int]
    CHANGE_NS_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    RECORD_NAME_FIELD_NUMBER: _ClassVar[int]
    RECORD_SIZE_FIELD_NUMBER: _ClassVar[int]
    FIELDS_FIELD_NUMBER: _ClassVar[int]
    RECEIVER_VOLATILE_FIELD_NUMBER: _ClassVar[int]
    ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    ENTRY_VIRTUAL_ADDRESS_FIELD_NUMBER: _ClassVar[int]
    ENTRY_SIZE_FIELD_NUMBER: _ClassVar[int]
    RUNTIME_ENTRY_FIELD_NUMBER: _ClassVar[int]
    module: str
    device: int
    inode: int
    file_size: int
    modification_ns: int
    change_ns: int
    selector: str
    record_name: str
    record_size: int
    fields: _containers.RepeatedCompositeFieldContainer[DebugField]
    receiver_volatile: bool
    arguments: _containers.RepeatedCompositeFieldContainer[DebugArgument]
    entry_virtual_address: int
    entry_size: int
    runtime_entry: int
    def __init__(self, module: _Optional[str] = ..., device: _Optional[int] = ..., inode: _Optional[int] = ..., file_size: _Optional[int] = ..., modification_ns: _Optional[int] = ..., change_ns: _Optional[int] = ..., selector: _Optional[str] = ..., record_name: _Optional[str] = ..., record_size: _Optional[int] = ..., fields: _Optional[_Iterable[_Union[DebugField, _Mapping]]] = ..., receiver_volatile: bool = ..., arguments: _Optional[_Iterable[_Union[DebugArgument, _Mapping]]] = ..., entry_virtual_address: _Optional[int] = ..., entry_size: _Optional[int] = ..., runtime_entry: _Optional[int] = ...) -> None: ...

class DebugArgument(_message.Message):
    __slots__ = ("index", "cpp_type", "is_reference", "is_object_value", "object_type")
    INDEX_FIELD_NUMBER: _ClassVar[int]
    CPP_TYPE_FIELD_NUMBER: _ClassVar[int]
    IS_REFERENCE_FIELD_NUMBER: _ClassVar[int]
    IS_OBJECT_VALUE_FIELD_NUMBER: _ClassVar[int]
    OBJECT_TYPE_FIELD_NUMBER: _ClassVar[int]
    index: int
    cpp_type: str
    is_reference: bool
    is_object_value: bool
    object_type: DebugInfo
    def __init__(self, index: _Optional[int] = ..., cpp_type: _Optional[str] = ..., is_reference: bool = ..., is_object_value: bool = ..., object_type: _Optional[_Union[DebugInfo, _Mapping]] = ...) -> None: ...

class HeaderContext(_message.Message):
    __slots__ = ("working_directory", "compiler_arguments", "inference")
    WORKING_DIRECTORY_FIELD_NUMBER: _ClassVar[int]
    COMPILER_ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    INFERENCE_FIELD_NUMBER: _ClassVar[int]
    working_directory: str
    compiler_arguments: _containers.RepeatedScalarFieldContainer[str]
    inference: str
    def __init__(self, working_directory: _Optional[str] = ..., compiler_arguments: _Optional[_Iterable[str]] = ..., inference: _Optional[str] = ...) -> None: ...

class ProbeSpec(_message.Message):
    __slots__ = ("symbol", "signature", "callback_source", "receiver", "receiver_unavailable_reason", "debug_info", "argument_names", "include_headers", "header_context", "stack_depth", "include_internal_stack_frames", "print_arguments", "print_result", "callback_package_path")
    SYMBOL_FIELD_NUMBER: _ClassVar[int]
    SIGNATURE_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_SOURCE_FIELD_NUMBER: _ClassVar[int]
    RECEIVER_FIELD_NUMBER: _ClassVar[int]
    RECEIVER_UNAVAILABLE_REASON_FIELD_NUMBER: _ClassVar[int]
    DEBUG_INFO_FIELD_NUMBER: _ClassVar[int]
    ARGUMENT_NAMES_FIELD_NUMBER: _ClassVar[int]
    INCLUDE_HEADERS_FIELD_NUMBER: _ClassVar[int]
    HEADER_CONTEXT_FIELD_NUMBER: _ClassVar[int]
    STACK_DEPTH_FIELD_NUMBER: _ClassVar[int]
    INCLUDE_INTERNAL_STACK_FRAMES_FIELD_NUMBER: _ClassVar[int]
    PRINT_ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    PRINT_RESULT_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_PACKAGE_PATH_FIELD_NUMBER: _ClassVar[int]
    symbol: str
    signature: Signature
    callback_source: str
    receiver: ReceiverContext
    receiver_unavailable_reason: str
    debug_info: DebugInfo
    argument_names: _containers.RepeatedScalarFieldContainer[str]
    include_headers: _containers.RepeatedScalarFieldContainer[str]
    header_context: HeaderContext
    stack_depth: int
    include_internal_stack_frames: bool
    print_arguments: bool
    print_result: bool
    callback_package_path: str
    def __init__(self, symbol: _Optional[str] = ..., signature: _Optional[_Union[Signature, _Mapping]] = ..., callback_source: _Optional[str] = ..., receiver: _Optional[_Union[ReceiverContext, _Mapping]] = ..., receiver_unavailable_reason: _Optional[str] = ..., debug_info: _Optional[_Union[DebugInfo, _Mapping]] = ..., argument_names: _Optional[_Iterable[str]] = ..., include_headers: _Optional[_Iterable[str]] = ..., header_context: _Optional[_Union[HeaderContext, _Mapping]] = ..., stack_depth: _Optional[int] = ..., include_internal_stack_frames: bool = ..., print_arguments: bool = ..., print_result: bool = ..., callback_package_path: _Optional[str] = ...) -> None: ...

class AttachRequest(_message.Message):
    __slots__ = ("pid",)
    PID_FIELD_NUMBER: _ClassVar[int]
    pid: int
    def __init__(self, pid: _Optional[int] = ...) -> None: ...

class Session(_message.Message):
    __slots__ = ("id", "pid", "state", "probes", "error", "application_modules")
    class State(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        STATE_UNSPECIFIED: _ClassVar[Session.State]
        ATTACHED: _ClassVar[Session.State]
        DETACHED: _ClassVar[Session.State]
        EXITED: _ClassVar[Session.State]
        FAILED: _ClassVar[Session.State]
    STATE_UNSPECIFIED: Session.State
    ATTACHED: Session.State
    DETACHED: Session.State
    EXITED: Session.State
    FAILED: Session.State
    ID_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    STATE_FIELD_NUMBER: _ClassVar[int]
    PROBES_FIELD_NUMBER: _ClassVar[int]
    ERROR_FIELD_NUMBER: _ClassVar[int]
    APPLICATION_MODULES_FIELD_NUMBER: _ClassVar[int]
    id: str
    pid: int
    state: Session.State
    probes: _containers.RepeatedCompositeFieldContainer[Probe]
    error: str
    application_modules: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, id: _Optional[str] = ..., pid: _Optional[int] = ..., state: _Optional[_Union[Session.State, str]] = ..., probes: _Optional[_Iterable[_Union[Probe, _Mapping]]] = ..., error: _Optional[str] = ..., application_modules: _Optional[_Iterable[str]] = ...) -> None: ...

class AddProbeRequest(_message.Message):
    __slots__ = ("session_id", "spec")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    SPEC_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    spec: ProbeSpec
    def __init__(self, session_id: _Optional[str] = ..., spec: _Optional[_Union[ProbeSpec, _Mapping]] = ...) -> None: ...

class ResolveProbeRequest(_message.Message):
    __slots__ = ("session_id", "selector", "include_headers", "compilation_database")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    INCLUDE_HEADERS_FIELD_NUMBER: _ClassVar[int]
    COMPILATION_DATABASE_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    selector: str
    include_headers: _containers.RepeatedScalarFieldContainer[str]
    compilation_database: str
    def __init__(self, session_id: _Optional[str] = ..., selector: _Optional[str] = ..., include_headers: _Optional[_Iterable[str]] = ..., compilation_database: _Optional[str] = ...) -> None: ...

class Probe(_message.Message):
    __slots__ = ("id", "session_id", "symbol", "signature", "active", "callback_address", "entry_address", "original_bytes_restored", "agent_dropped_events", "callback_errors")
    ID_FIELD_NUMBER: _ClassVar[int]
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    SYMBOL_FIELD_NUMBER: _ClassVar[int]
    SIGNATURE_FIELD_NUMBER: _ClassVar[int]
    ACTIVE_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_ADDRESS_FIELD_NUMBER: _ClassVar[int]
    ENTRY_ADDRESS_FIELD_NUMBER: _ClassVar[int]
    ORIGINAL_BYTES_RESTORED_FIELD_NUMBER: _ClassVar[int]
    AGENT_DROPPED_EVENTS_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_ERRORS_FIELD_NUMBER: _ClassVar[int]
    id: int
    session_id: str
    symbol: str
    signature: Signature
    active: bool
    callback_address: int
    entry_address: int
    original_bytes_restored: bool
    agent_dropped_events: int
    callback_errors: int
    def __init__(self, id: _Optional[int] = ..., session_id: _Optional[str] = ..., symbol: _Optional[str] = ..., signature: _Optional[_Union[Signature, _Mapping]] = ..., active: bool = ..., callback_address: _Optional[int] = ..., entry_address: _Optional[int] = ..., original_bytes_restored: bool = ..., agent_dropped_events: _Optional[int] = ..., callback_errors: _Optional[int] = ...) -> None: ...

class RemoveProbeRequest(_message.Message):
    __slots__ = ("session_id", "probe_id")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    PROBE_ID_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    probe_id: int
    def __init__(self, session_id: _Optional[str] = ..., probe_id: _Optional[int] = ...) -> None: ...

class DetachRequest(_message.Message):
    __slots__ = ("session_id",)
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    def __init__(self, session_id: _Optional[str] = ...) -> None: ...

class ListSessionsRequest(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...

class ListSessionsResponse(_message.Message):
    __slots__ = ("sessions",)
    SESSIONS_FIELD_NUMBER: _ClassVar[int]
    sessions: _containers.RepeatedCompositeFieldContainer[Session]
    def __init__(self, sessions: _Optional[_Iterable[_Union[Session, _Mapping]]] = ...) -> None: ...

class WatchEventsRequest(_message.Message):
    __slots__ = ("session_ids", "after_sequence", "pid")
    SESSION_IDS_FIELD_NUMBER: _ClassVar[int]
    AFTER_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    session_ids: _containers.RepeatedScalarFieldContainer[str]
    after_sequence: int
    pid: int
    def __init__(self, session_ids: _Optional[_Iterable[str]] = ..., after_sequence: _Optional[int] = ..., pid: _Optional[int] = ...) -> None: ...

class Value(_message.Message):
    __slots__ = ("type", "signed_value", "unsigned_value", "real_value")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    SIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    UNSIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    REAL_VALUE_FIELD_NUMBER: _ClassVar[int]
    type: ValueType
    signed_value: int
    unsigned_value: int
    real_value: float
    def __init__(self, type: _Optional[_Union[ValueType, str]] = ..., signed_value: _Optional[int] = ..., unsigned_value: _Optional[int] = ..., real_value: _Optional[float] = ...) -> None: ...

class NamedValue(_message.Message):
    __slots__ = ("name", "value", "argument_index")
    NAME_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    ARGUMENT_INDEX_FIELD_NUMBER: _ClassVar[int]
    name: str
    value: Value
    argument_index: int
    def __init__(self, name: _Optional[str] = ..., value: _Optional[_Union[Value, _Mapping]] = ..., argument_index: _Optional[int] = ...) -> None: ...

class StackFrame(_message.Message):
    __slots__ = ("pc", "stack_pointer", "kind", "module", "module_offset", "function", "file", "line", "application_frame")
    class Kind(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        NATIVE: _ClassVar[StackFrame.Kind]
        TARGET_ENTRY: _ClassVar[StackFrame.Kind]
        JIT_BRIDGE: _ClassVar[StackFrame.Kind]
    NATIVE: StackFrame.Kind
    TARGET_ENTRY: StackFrame.Kind
    JIT_BRIDGE: StackFrame.Kind
    PC_FIELD_NUMBER: _ClassVar[int]
    STACK_POINTER_FIELD_NUMBER: _ClassVar[int]
    KIND_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    MODULE_OFFSET_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_FIELD_NUMBER: _ClassVar[int]
    FILE_FIELD_NUMBER: _ClassVar[int]
    LINE_FIELD_NUMBER: _ClassVar[int]
    APPLICATION_FRAME_FIELD_NUMBER: _ClassVar[int]
    pc: int
    stack_pointer: int
    kind: StackFrame.Kind
    module: str
    module_offset: int
    function: str
    file: str
    line: int
    application_frame: bool
    def __init__(self, pc: _Optional[int] = ..., stack_pointer: _Optional[int] = ..., kind: _Optional[_Union[StackFrame.Kind, str]] = ..., module: _Optional[str] = ..., module_offset: _Optional[int] = ..., function: _Optional[str] = ..., file: _Optional[str] = ..., line: _Optional[int] = ..., application_frame: bool = ...) -> None: ...

class StackTrace(_message.Message):
    __slots__ = ("status", "requested_depth", "frames", "detail", "application_only", "hidden_frames", "captured_depth")
    class Status(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        DISABLED: _ClassVar[StackTrace.Status]
        COMPLETE: _ClassVar[StackTrace.Status]
        TRUNCATED: _ClassVar[StackTrace.Status]
        PARTIAL: _ClassVar[StackTrace.Status]
    DISABLED: StackTrace.Status
    COMPLETE: StackTrace.Status
    TRUNCATED: StackTrace.Status
    PARTIAL: StackTrace.Status
    STATUS_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_DEPTH_FIELD_NUMBER: _ClassVar[int]
    FRAMES_FIELD_NUMBER: _ClassVar[int]
    DETAIL_FIELD_NUMBER: _ClassVar[int]
    APPLICATION_ONLY_FIELD_NUMBER: _ClassVar[int]
    HIDDEN_FRAMES_FIELD_NUMBER: _ClassVar[int]
    CAPTURED_DEPTH_FIELD_NUMBER: _ClassVar[int]
    status: StackTrace.Status
    requested_depth: int
    frames: _containers.RepeatedCompositeFieldContainer[StackFrame]
    detail: str
    application_only: bool
    hidden_frames: int
    captured_depth: int
    def __init__(self, status: _Optional[_Union[StackTrace.Status, str]] = ..., requested_depth: _Optional[int] = ..., frames: _Optional[_Iterable[_Union[StackFrame, _Mapping]]] = ..., detail: _Optional[str] = ..., application_only: bool = ..., hidden_frames: _Optional[int] = ..., captured_depth: _Optional[int] = ...) -> None: ...

class TraceEvent(_message.Message):
    __slots__ = ("protocol_version", "session_id", "probe_id", "pid", "tid", "sequence", "probe_sequence", "monotonic_time_ns", "object_address", "arguments", "callback_value", "agent_dropped_events", "server_dropped_events", "phase", "call_id", "result", "named_arguments", "object_fields", "stack", "context")
    PROTOCOL_VERSION_FIELD_NUMBER: _ClassVar[int]
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    PROBE_ID_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    TID_FIELD_NUMBER: _ClassVar[int]
    SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    PROBE_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    MONOTONIC_TIME_NS_FIELD_NUMBER: _ClassVar[int]
    OBJECT_ADDRESS_FIELD_NUMBER: _ClassVar[int]
    ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_VALUE_FIELD_NUMBER: _ClassVar[int]
    AGENT_DROPPED_EVENTS_FIELD_NUMBER: _ClassVar[int]
    SERVER_DROPPED_EVENTS_FIELD_NUMBER: _ClassVar[int]
    PHASE_FIELD_NUMBER: _ClassVar[int]
    CALL_ID_FIELD_NUMBER: _ClassVar[int]
    RESULT_FIELD_NUMBER: _ClassVar[int]
    NAMED_ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    OBJECT_FIELDS_FIELD_NUMBER: _ClassVar[int]
    STACK_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    protocol_version: int
    session_id: str
    probe_id: int
    pid: int
    tid: int
    sequence: int
    probe_sequence: int
    monotonic_time_ns: int
    object_address: int
    arguments: _containers.RepeatedCompositeFieldContainer[Value]
    callback_value: int
    agent_dropped_events: int
    server_dropped_events: int
    phase: TracePhase
    call_id: int
    result: Value
    named_arguments: _containers.RepeatedCompositeFieldContainer[NamedValue]
    object_fields: _containers.RepeatedCompositeFieldContainer[NamedValue]
    stack: StackTrace
    context: _context_transport_pb2.ContextEventEnvelope
    def __init__(self, protocol_version: _Optional[int] = ..., session_id: _Optional[str] = ..., probe_id: _Optional[int] = ..., pid: _Optional[int] = ..., tid: _Optional[int] = ..., sequence: _Optional[int] = ..., probe_sequence: _Optional[int] = ..., monotonic_time_ns: _Optional[int] = ..., object_address: _Optional[int] = ..., arguments: _Optional[_Iterable[_Union[Value, _Mapping]]] = ..., callback_value: _Optional[int] = ..., agent_dropped_events: _Optional[int] = ..., server_dropped_events: _Optional[int] = ..., phase: _Optional[_Union[TracePhase, str]] = ..., call_id: _Optional[int] = ..., result: _Optional[_Union[Value, _Mapping]] = ..., named_arguments: _Optional[_Iterable[_Union[NamedValue, _Mapping]]] = ..., object_fields: _Optional[_Iterable[_Union[NamedValue, _Mapping]]] = ..., stack: _Optional[_Union[StackTrace, _Mapping]] = ..., context: _Optional[_Union[_context_transport_pb2.ContextEventEnvelope, _Mapping]] = ...) -> None: ...
