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
    BOOL: _ClassVar[ValueType]

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

class RecordCallingConvention(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    RECORD_CALLING_CONVENTION_UNSPECIFIED: _ClassVar[RecordCallingConvention]
    RECORD_PASS_BY_VALUE: _ClassVar[RecordCallingConvention]
    RECORD_PASS_BY_REFERENCE: _ClassVar[RecordCallingConvention]
VALUE_TYPE_UNSPECIFIED: ValueType
VOID: ValueType
INT32: ValueType
INT64: ValueType
UINT64: ValueType
FLOAT32: ValueType
FLOAT64: ValueType
POINTER: ValueType
OBJECT: ValueType
BOOL: ValueType
CALLABLE_KIND_UNSPECIFIED: CallableKind
FUNCTION: CallableKind
METHOD: CallableKind
TRACE_PHASE_UNSPECIFIED: TracePhase
PRE: TracePhase
POST: TracePhase
RECORD_CALLING_CONVENTION_UNSPECIFIED: RecordCallingConvention
RECORD_PASS_BY_VALUE: RecordCallingConvention
RECORD_PASS_BY_REFERENCE: RecordCallingConvention

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
    __slots__ = ("module", "device", "inode", "file_size", "modification_ns", "change_ns", "selector", "record_name", "record_size", "fields", "receiver_volatile", "arguments", "entry_virtual_address", "entry_size", "runtime_entry", "result_object_type", "result_cpp_type", "result_is_object_value", "result_is_reference", "result_pass_by_reference", "methods", "record_pass_by_reference", "record_is_opaque", "record_alignment", "record_calling_convention")
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
    RESULT_OBJECT_TYPE_FIELD_NUMBER: _ClassVar[int]
    RESULT_CPP_TYPE_FIELD_NUMBER: _ClassVar[int]
    RESULT_IS_OBJECT_VALUE_FIELD_NUMBER: _ClassVar[int]
    RESULT_IS_REFERENCE_FIELD_NUMBER: _ClassVar[int]
    RESULT_PASS_BY_REFERENCE_FIELD_NUMBER: _ClassVar[int]
    METHODS_FIELD_NUMBER: _ClassVar[int]
    RECORD_PASS_BY_REFERENCE_FIELD_NUMBER: _ClassVar[int]
    RECORD_IS_OPAQUE_FIELD_NUMBER: _ClassVar[int]
    RECORD_ALIGNMENT_FIELD_NUMBER: _ClassVar[int]
    RECORD_CALLING_CONVENTION_FIELD_NUMBER: _ClassVar[int]
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
    result_object_type: DebugInfo
    result_cpp_type: str
    result_is_object_value: bool
    result_is_reference: bool
    result_pass_by_reference: bool
    methods: _containers.RepeatedCompositeFieldContainer[DebugMethod]
    record_pass_by_reference: bool
    record_is_opaque: bool
    record_alignment: int
    record_calling_convention: RecordCallingConvention
    def __init__(self, module: _Optional[str] = ..., device: _Optional[int] = ..., inode: _Optional[int] = ..., file_size: _Optional[int] = ..., modification_ns: _Optional[int] = ..., change_ns: _Optional[int] = ..., selector: _Optional[str] = ..., record_name: _Optional[str] = ..., record_size: _Optional[int] = ..., fields: _Optional[_Iterable[_Union[DebugField, _Mapping]]] = ..., receiver_volatile: bool = ..., arguments: _Optional[_Iterable[_Union[DebugArgument, _Mapping]]] = ..., entry_virtual_address: _Optional[int] = ..., entry_size: _Optional[int] = ..., runtime_entry: _Optional[int] = ..., result_object_type: _Optional[_Union[DebugInfo, _Mapping]] = ..., result_cpp_type: _Optional[str] = ..., result_is_object_value: bool = ..., result_is_reference: bool = ..., result_pass_by_reference: bool = ..., methods: _Optional[_Iterable[_Union[DebugMethod, _Mapping]]] = ..., record_pass_by_reference: bool = ..., record_is_opaque: bool = ..., record_alignment: _Optional[int] = ..., record_calling_convention: _Optional[_Union[RecordCallingConvention, str]] = ...) -> None: ...

class DebugMethod(_message.Message):
    __slots__ = ("name", "linkage_symbol", "cpp_result_type", "cpp_parameter_types", "is_const", "is_volatile", "is_static", "is_virtual", "runtime_entry", "calling_convention", "ref_qualifier", "is_variadic")
    NAME_FIELD_NUMBER: _ClassVar[int]
    LINKAGE_SYMBOL_FIELD_NUMBER: _ClassVar[int]
    CPP_RESULT_TYPE_FIELD_NUMBER: _ClassVar[int]
    CPP_PARAMETER_TYPES_FIELD_NUMBER: _ClassVar[int]
    IS_CONST_FIELD_NUMBER: _ClassVar[int]
    IS_VOLATILE_FIELD_NUMBER: _ClassVar[int]
    IS_STATIC_FIELD_NUMBER: _ClassVar[int]
    IS_VIRTUAL_FIELD_NUMBER: _ClassVar[int]
    RUNTIME_ENTRY_FIELD_NUMBER: _ClassVar[int]
    CALLING_CONVENTION_FIELD_NUMBER: _ClassVar[int]
    REF_QUALIFIER_FIELD_NUMBER: _ClassVar[int]
    IS_VARIADIC_FIELD_NUMBER: _ClassVar[int]
    name: str
    linkage_symbol: str
    cpp_result_type: str
    cpp_parameter_types: _containers.RepeatedScalarFieldContainer[str]
    is_const: bool
    is_volatile: bool
    is_static: bool
    is_virtual: bool
    runtime_entry: int
    calling_convention: int
    ref_qualifier: str
    is_variadic: bool
    def __init__(self, name: _Optional[str] = ..., linkage_symbol: _Optional[str] = ..., cpp_result_type: _Optional[str] = ..., cpp_parameter_types: _Optional[_Iterable[str]] = ..., is_const: bool = ..., is_volatile: bool = ..., is_static: bool = ..., is_virtual: bool = ..., runtime_entry: _Optional[int] = ..., calling_convention: _Optional[int] = ..., ref_qualifier: _Optional[str] = ..., is_variadic: bool = ...) -> None: ...

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

class PreparedInjector(_message.Message):
    __slots__ = ("format_version", "module_sha256", "module_name", "selector", "symbol", "signature", "argument_names", "target_triple")
    FORMAT_VERSION_FIELD_NUMBER: _ClassVar[int]
    MODULE_SHA256_FIELD_NUMBER: _ClassVar[int]
    MODULE_NAME_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    SYMBOL_FIELD_NUMBER: _ClassVar[int]
    SIGNATURE_FIELD_NUMBER: _ClassVar[int]
    ARGUMENT_NAMES_FIELD_NUMBER: _ClassVar[int]
    TARGET_TRIPLE_FIELD_NUMBER: _ClassVar[int]
    format_version: int
    module_sha256: bytes
    module_name: str
    selector: str
    symbol: str
    signature: Signature
    argument_names: _containers.RepeatedScalarFieldContainer[str]
    target_triple: str
    def __init__(self, format_version: _Optional[int] = ..., module_sha256: _Optional[bytes] = ..., module_name: _Optional[str] = ..., selector: _Optional[str] = ..., symbol: _Optional[str] = ..., signature: _Optional[_Union[Signature, _Mapping]] = ..., argument_names: _Optional[_Iterable[str]] = ..., target_triple: _Optional[str] = ...) -> None: ...

class ProbeSpec(_message.Message):
    __slots__ = ("symbol", "signature", "callback_source", "receiver", "receiver_unavailable_reason", "debug_info", "argument_names", "include_headers", "header_context", "stack_depth", "include_internal_stack_frames", "print_arguments", "print_result", "callback_package_path", "compile_options", "prepared")
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
    COMPILE_OPTIONS_FIELD_NUMBER: _ClassVar[int]
    PREPARED_FIELD_NUMBER: _ClassVar[int]
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
    compile_options: _containers.RepeatedScalarFieldContainer[str]
    prepared: PreparedInjector
    def __init__(self, symbol: _Optional[str] = ..., signature: _Optional[_Union[Signature, _Mapping]] = ..., callback_source: _Optional[str] = ..., receiver: _Optional[_Union[ReceiverContext, _Mapping]] = ..., receiver_unavailable_reason: _Optional[str] = ..., debug_info: _Optional[_Union[DebugInfo, _Mapping]] = ..., argument_names: _Optional[_Iterable[str]] = ..., include_headers: _Optional[_Iterable[str]] = ..., header_context: _Optional[_Union[HeaderContext, _Mapping]] = ..., stack_depth: _Optional[int] = ..., include_internal_stack_frames: bool = ..., print_arguments: bool = ..., print_result: bool = ..., callback_package_path: _Optional[str] = ..., compile_options: _Optional[_Iterable[str]] = ..., prepared: _Optional[_Union[PreparedInjector, _Mapping]] = ...) -> None: ...

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
    __slots__ = ("session_id", "selector", "include_headers", "compilation_database", "module", "debug_max_entries", "debug_max_depth", "compile_options", "prepared")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    INCLUDE_HEADERS_FIELD_NUMBER: _ClassVar[int]
    COMPILATION_DATABASE_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    DEBUG_MAX_ENTRIES_FIELD_NUMBER: _ClassVar[int]
    DEBUG_MAX_DEPTH_FIELD_NUMBER: _ClassVar[int]
    COMPILE_OPTIONS_FIELD_NUMBER: _ClassVar[int]
    PREPARED_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    selector: str
    include_headers: _containers.RepeatedScalarFieldContainer[str]
    compilation_database: str
    module: str
    debug_max_entries: int
    debug_max_depth: int
    compile_options: _containers.RepeatedScalarFieldContainer[str]
    prepared: PreparedInjector
    def __init__(self, session_id: _Optional[str] = ..., selector: _Optional[str] = ..., include_headers: _Optional[_Iterable[str]] = ..., compilation_database: _Optional[str] = ..., module: _Optional[str] = ..., debug_max_entries: _Optional[int] = ..., debug_max_depth: _Optional[int] = ..., compile_options: _Optional[_Iterable[str]] = ..., prepared: _Optional[_Union[PreparedInjector, _Mapping]] = ...) -> None: ...

class ValidateProbeRequest(_message.Message):
    __slots__ = ("session_id", "selector", "callback_source", "include_headers", "compilation_database", "compile_options", "level", "module", "debug_max_entries", "debug_max_depth", "prepared")
    class Level(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        COMPILE: _ClassVar[ValidateProbeRequest.Level]
        SYNTAX: _ClassVar[ValidateProbeRequest.Level]
    COMPILE: ValidateProbeRequest.Level
    SYNTAX: ValidateProbeRequest.Level
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_SOURCE_FIELD_NUMBER: _ClassVar[int]
    INCLUDE_HEADERS_FIELD_NUMBER: _ClassVar[int]
    COMPILATION_DATABASE_FIELD_NUMBER: _ClassVar[int]
    COMPILE_OPTIONS_FIELD_NUMBER: _ClassVar[int]
    LEVEL_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    DEBUG_MAX_ENTRIES_FIELD_NUMBER: _ClassVar[int]
    DEBUG_MAX_DEPTH_FIELD_NUMBER: _ClassVar[int]
    PREPARED_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    selector: str
    callback_source: str
    include_headers: _containers.RepeatedScalarFieldContainer[str]
    compilation_database: str
    compile_options: _containers.RepeatedScalarFieldContainer[str]
    level: ValidateProbeRequest.Level
    module: str
    debug_max_entries: int
    debug_max_depth: int
    prepared: PreparedInjector
    def __init__(self, session_id: _Optional[str] = ..., selector: _Optional[str] = ..., callback_source: _Optional[str] = ..., include_headers: _Optional[_Iterable[str]] = ..., compilation_database: _Optional[str] = ..., compile_options: _Optional[_Iterable[str]] = ..., level: _Optional[_Union[ValidateProbeRequest.Level, str]] = ..., module: _Optional[str] = ..., debug_max_entries: _Optional[int] = ..., debug_max_depth: _Optional[int] = ..., prepared: _Optional[_Union[PreparedInjector, _Mapping]] = ...) -> None: ...

class ValidationDiagnostic(_message.Message):
    __slots__ = ("stage", "severity", "file", "line", "column", "message")
    STAGE_FIELD_NUMBER: _ClassVar[int]
    SEVERITY_FIELD_NUMBER: _ClassVar[int]
    FILE_FIELD_NUMBER: _ClassVar[int]
    LINE_FIELD_NUMBER: _ClassVar[int]
    COLUMN_FIELD_NUMBER: _ClassVar[int]
    MESSAGE_FIELD_NUMBER: _ClassVar[int]
    stage: str
    severity: str
    file: str
    line: int
    column: int
    message: str
    def __init__(self, stage: _Optional[str] = ..., severity: _Optional[str] = ..., file: _Optional[str] = ..., line: _Optional[int] = ..., column: _Optional[int] = ..., message: _Optional[str] = ...) -> None: ...

class ValidationReport(_message.Message):
    __slots__ = ("success", "level", "effective_options", "diagnostics", "prepared_used")
    SUCCESS_FIELD_NUMBER: _ClassVar[int]
    LEVEL_FIELD_NUMBER: _ClassVar[int]
    EFFECTIVE_OPTIONS_FIELD_NUMBER: _ClassVar[int]
    DIAGNOSTICS_FIELD_NUMBER: _ClassVar[int]
    PREPARED_USED_FIELD_NUMBER: _ClassVar[int]
    success: bool
    level: ValidateProbeRequest.Level
    effective_options: _containers.RepeatedScalarFieldContainer[str]
    diagnostics: _containers.RepeatedCompositeFieldContainer[ValidationDiagnostic]
    prepared_used: bool
    def __init__(self, success: bool = ..., level: _Optional[_Union[ValidateProbeRequest.Level, str]] = ..., effective_options: _Optional[_Iterable[str]] = ..., diagnostics: _Optional[_Iterable[_Union[ValidationDiagnostic, _Mapping]]] = ..., prepared_used: bool = ...) -> None: ...

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

class ListModulesRequest(_message.Message):
    __slots__ = ("session_id",)
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    def __init__(self, session_id: _Optional[str] = ...) -> None: ...

class ListModulesResponse(_message.Message):
    __slots__ = ("modules",)
    MODULES_FIELD_NUMBER: _ClassVar[int]
    modules: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, modules: _Optional[_Iterable[str]] = ...) -> None: ...

class ListSymbolsRequest(_message.Message):
    __slots__ = ("session_id", "query", "module", "page_size", "page_token")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    QUERY_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    PAGE_SIZE_FIELD_NUMBER: _ClassVar[int]
    PAGE_TOKEN_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    query: str
    module: str
    page_size: int
    page_token: str
    def __init__(self, session_id: _Optional[str] = ..., query: _Optional[str] = ..., module: _Optional[str] = ..., page_size: _Optional[int] = ..., page_token: _Optional[str] = ...) -> None: ...

class FunctionSymbol(_message.Message):
    __slots__ = ("selector", "linkage_symbol", "module_path")
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    LINKAGE_SYMBOL_FIELD_NUMBER: _ClassVar[int]
    MODULE_PATH_FIELD_NUMBER: _ClassVar[int]
    selector: str
    linkage_symbol: str
    module_path: str
    def __init__(self, selector: _Optional[str] = ..., linkage_symbol: _Optional[str] = ..., module_path: _Optional[str] = ...) -> None: ...

class ListSymbolsResponse(_message.Message):
    __slots__ = ("symbols", "next_page_token")
    SYMBOLS_FIELD_NUMBER: _ClassVar[int]
    NEXT_PAGE_TOKEN_FIELD_NUMBER: _ClassVar[int]
    symbols: _containers.RepeatedCompositeFieldContainer[FunctionSymbol]
    next_page_token: str
    def __init__(self, symbols: _Optional[_Iterable[_Union[FunctionSymbol, _Mapping]]] = ..., next_page_token: _Optional[str] = ...) -> None: ...

class ListSymbolsPageToken(_message.Message):
    __slots__ = ("session_id", "pid", "process_start_time", "query", "module", "page_size", "mapping_fingerprint", "last_selector", "last_linkage_symbol", "last_module_path")
    SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    PROCESS_START_TIME_FIELD_NUMBER: _ClassVar[int]
    QUERY_FIELD_NUMBER: _ClassVar[int]
    MODULE_FIELD_NUMBER: _ClassVar[int]
    PAGE_SIZE_FIELD_NUMBER: _ClassVar[int]
    MAPPING_FINGERPRINT_FIELD_NUMBER: _ClassVar[int]
    LAST_SELECTOR_FIELD_NUMBER: _ClassVar[int]
    LAST_LINKAGE_SYMBOL_FIELD_NUMBER: _ClassVar[int]
    LAST_MODULE_PATH_FIELD_NUMBER: _ClassVar[int]
    session_id: str
    pid: int
    process_start_time: int
    query: str
    module: str
    page_size: int
    mapping_fingerprint: str
    last_selector: str
    last_linkage_symbol: str
    last_module_path: str
    def __init__(self, session_id: _Optional[str] = ..., pid: _Optional[int] = ..., process_start_time: _Optional[int] = ..., query: _Optional[str] = ..., module: _Optional[str] = ..., page_size: _Optional[int] = ..., mapping_fingerprint: _Optional[str] = ..., last_selector: _Optional[str] = ..., last_linkage_symbol: _Optional[str] = ..., last_module_path: _Optional[str] = ...) -> None: ...

class WatchEventsRequest(_message.Message):
    __slots__ = ("session_ids", "after_sequence", "pid", "names")
    SESSION_IDS_FIELD_NUMBER: _ClassVar[int]
    AFTER_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    NAMES_FIELD_NUMBER: _ClassVar[int]
    session_ids: _containers.RepeatedScalarFieldContainer[str]
    after_sequence: int
    pid: int
    names: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, session_ids: _Optional[_Iterable[str]] = ..., after_sequence: _Optional[int] = ..., pid: _Optional[int] = ..., names: _Optional[_Iterable[str]] = ...) -> None: ...

class ReadEventsRequest(_message.Message):
    __slots__ = ("filter", "limit", "through_sequence", "server_instance_id")
    FILTER_FIELD_NUMBER: _ClassVar[int]
    LIMIT_FIELD_NUMBER: _ClassVar[int]
    THROUGH_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    SERVER_INSTANCE_ID_FIELD_NUMBER: _ClassVar[int]
    filter: WatchEventsRequest
    limit: int
    through_sequence: int
    server_instance_id: str
    def __init__(self, filter: _Optional[_Union[WatchEventsRequest, _Mapping]] = ..., limit: _Optional[int] = ..., through_sequence: _Optional[int] = ..., server_instance_id: _Optional[str] = ...) -> None: ...

class ReadEventsResponse(_message.Message):
    __slots__ = ("server_instance_id", "through_sequence", "events", "server_dropped_events")
    SERVER_INSTANCE_ID_FIELD_NUMBER: _ClassVar[int]
    THROUGH_SEQUENCE_FIELD_NUMBER: _ClassVar[int]
    EVENTS_FIELD_NUMBER: _ClassVar[int]
    SERVER_DROPPED_EVENTS_FIELD_NUMBER: _ClassVar[int]
    server_instance_id: str
    through_sequence: int
    events: _containers.RepeatedCompositeFieldContainer[TraceEvent]
    server_dropped_events: int
    def __init__(self, server_instance_id: _Optional[str] = ..., through_sequence: _Optional[int] = ..., events: _Optional[_Iterable[_Union[TraceEvent, _Mapping]]] = ..., server_dropped_events: _Optional[int] = ...) -> None: ...

class AcknowledgeEventsRequest(_message.Message):
    __slots__ = ("server_instance_id", "sequences")
    SERVER_INSTANCE_ID_FIELD_NUMBER: _ClassVar[int]
    SEQUENCES_FIELD_NUMBER: _ClassVar[int]
    server_instance_id: str
    sequences: _containers.RepeatedScalarFieldContainer[int]
    def __init__(self, server_instance_id: _Optional[str] = ..., sequences: _Optional[_Iterable[int]] = ...) -> None: ...

class AcknowledgeEventsResponse(_message.Message):
    __slots__ = ("removed",)
    REMOVED_FIELD_NUMBER: _ClassVar[int]
    removed: int
    def __init__(self, removed: _Optional[int] = ...) -> None: ...

class Value(_message.Message):
    __slots__ = ("type", "signed_value", "unsigned_value", "real_value", "snapshot")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    SIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    UNSIGNED_VALUE_FIELD_NUMBER: _ClassVar[int]
    REAL_VALUE_FIELD_NUMBER: _ClassVar[int]
    SNAPSHOT_FIELD_NUMBER: _ClassVar[int]
    type: ValueType
    signed_value: int
    unsigned_value: int
    real_value: float
    snapshot: _context_transport_pb2.ValueSnapshot
    def __init__(self, type: _Optional[_Union[ValueType, str]] = ..., signed_value: _Optional[int] = ..., unsigned_value: _Optional[int] = ..., real_value: _Optional[float] = ..., snapshot: _Optional[_Union[_context_transport_pb2.ValueSnapshot, _Mapping]] = ...) -> None: ...

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
    __slots__ = ("protocol_version", "session_id", "probe_id", "pid", "tid", "sequence", "probe_sequence", "monotonic_time_ns", "object_address", "arguments", "callback_value", "agent_dropped_events", "server_dropped_events", "phase", "call_id", "result", "named_arguments", "object_fields", "stack", "context", "function_name", "server_instance_id")
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
    FUNCTION_NAME_FIELD_NUMBER: _ClassVar[int]
    SERVER_INSTANCE_ID_FIELD_NUMBER: _ClassVar[int]
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
    function_name: str
    server_instance_id: str
    def __init__(self, protocol_version: _Optional[int] = ..., session_id: _Optional[str] = ..., probe_id: _Optional[int] = ..., pid: _Optional[int] = ..., tid: _Optional[int] = ..., sequence: _Optional[int] = ..., probe_sequence: _Optional[int] = ..., monotonic_time_ns: _Optional[int] = ..., object_address: _Optional[int] = ..., arguments: _Optional[_Iterable[_Union[Value, _Mapping]]] = ..., callback_value: _Optional[int] = ..., agent_dropped_events: _Optional[int] = ..., server_dropped_events: _Optional[int] = ..., phase: _Optional[_Union[TracePhase, str]] = ..., call_id: _Optional[int] = ..., result: _Optional[_Union[Value, _Mapping]] = ..., named_arguments: _Optional[_Iterable[_Union[NamedValue, _Mapping]]] = ..., object_fields: _Optional[_Iterable[_Union[NamedValue, _Mapping]]] = ..., stack: _Optional[_Union[StackTrace, _Mapping]] = ..., context: _Optional[_Union[_context_transport_pb2.ContextEventEnvelope, _Mapping]] = ..., function_name: _Optional[str] = ..., server_instance_id: _Optional[str] = ...) -> None: ...
