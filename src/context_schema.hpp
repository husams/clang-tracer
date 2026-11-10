#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "tracer/compat.hpp"

namespace clang {
class ASTContext;
class CXXRecordDecl;
class QualType;
} // namespace clang

namespace tracer::context_engine {

// Stable grouping identity; revision identity additionally includes the root,
// exact descriptor bytes, and canonical container-role table.
struct StateFamilyKey {
  std::array<std::uint8_t, 16> source_lineage_id{};
  std::string cpp_state_type, cpp_scope_type;
  friend bool operator==(const StateFamilyKey &left,
                         const StateFamilyKey &right) noexcept {
    return left.source_lineage_id == right.source_lineage_id &&
           left.cpp_state_type == right.cpp_state_type &&
           left.cpp_scope_type == right.cpp_scope_type;
  }
  friend bool operator!=(const StateFamilyKey &left,
                         const StateFamilyKey &right) noexcept {
    return !(left == right);
  }
};

enum class SchemaKind : std::uint8_t {
  boolean,
  signed_integer,
  unsigned_integer,
  float32,
  float64,
  enumeration,
  string,
  message,
  optional,
  array,
  vector,
  map,
  deque,
  list,
  set,
  unordered_set,
  unordered_map
};
enum class CapturePathSelector : std::uint8_t {
  field_number,
  list_index,
  string_key,
  signed_key,
  unsigned_key,
  bool_key
};

struct CapturePathStep {
  CapturePathSelector selector = CapturePathSelector::field_number;
  std::uint64_t number = 0;
  std::int64_t signed_value = 0;
  bool bool_value = false;
  std::string_view string_value{};
};

struct CaptureLimitView {
  ::tracer::compat::span<const CapturePathStep> path;
  std::uint64_t total_items = 0, captured_items = 0;
};

struct SchemaField {
  std::string name, cpp_type, enum_name;
  std::vector<std::pair<std::string, std::int32_t>> enum_values;
  std::uint32_t number = 0;
  SchemaKind kind = SchemaKind::message;
  unsigned bit_width = 0;
  bool repeated = false, optional = false;
  std::shared_ptr<const struct SchemaMessage> message;
  std::shared_ptr<const SchemaField> key, value;
  std::size_t fixed_count = 0;
};

struct SchemaMessage {
  std::string cpp_type, full_name;
  std::vector<SchemaField> fields;
  // Optional runtime display type when a callback encodes a source record as
  // a DTO with a distinct C++ layout.
  std::string snapshot_cpp_type;
};

struct SchemaRecord final {
  const StateFamilyKey family;
  const std::string root_type;
  const std::string revision_id;
  const std::string descriptor_set;
  const std::vector<std::string> container_roles;
  const std::shared_ptr<const SchemaMessage> root;
  const std::size_t accounted_bytes;
  SchemaRecord(StateFamilyKey, std::string, std::string, std::string,
               std::shared_ptr<const SchemaMessage>,
               std::vector<std::string> = {});
};

struct SchemaLimits {
  std::size_t descriptor_bytes = 1024 * 1024;
  std::size_t files = 64, messages = 1024, fields = 8192, depth = 32;
  std::size_t name_bytes = 4096;
};

struct SchemaResult {
  std::shared_ptr<const SchemaRecord> record;
  std::string error;

  explicit operator bool() const noexcept { return bool(record); }
};

// Canonical, unambiguous identity for one immutable schema revision. The
// descriptor bytes, family components, root and every ordered role are each
// length-prefixed before hashing; caller-provided role order is significant.
std::string compute_schema_revision_id(std::string_view descriptor_bytes,
                                       const StateFamilyKey& family, std::string_view root_type,
                                       ::tracer::compat::span<const std::string> container_roles);

struct CodecSourceResult {
  std::string source, error;

  explicit operator bool() const noexcept {
    return error.empty() && !source.empty();
  }
};

struct RevisionField {
  std::string revision_id;
  bool family_matches = true, present = false;
  SchemaKind kind = SchemaKind::message;
  std::string cpp_type;
  bool optional = false, repeated = false;
};

// Builds descriptors directly from the checked AST. The returned record owns
// only copied metadata and is safe to retain after the originating JIT retires.
SchemaResult build_state_schema(clang::ASTContext &,
                                const clang::CXXRecordDecl &, StateFamilyKey,
                                SchemaLimits = {});
enum class CodecAccess { public_members, snapshot_members };

// Generic scoped payloads need no state/shared_state marker. The root
// descriptor is a wrapper whose field 1 is named `value`.
SchemaResult build_payload_schema(clang::ASTContext &, clang::QualType payload,
                                  StateFamilyKey, SchemaLimits = {},
                                  CodecAccess = CodecAccess::public_members);
// Like build_payload_schema, with an additional synthetic `_imp_snapshot`
// field after the root's declared fields. The raw pointer member itself stays
// in the payload as an address, while the appended message describes its
// explicitly prepared implementation type.
SchemaResult build_pimpl_payload_schema(clang::ASTContext &,
                                        clang::QualType payload,
                                        StateFamilyKey,
                                        std::string_view member_name,
                                        clang::QualType implementation_type,
                                        SchemaLimits = {},
                                        CodecAccess = CodecAccess::public_members);
SchemaResult build_pimpl_payload_schema(clang::ASTContext &,
                                        clang::QualType payload,
                                        StateFamilyKey,
                                        std::string_view member_name,
                                        clang::QualType implementation_type,
                                        clang::QualType projection_type,
                                        SchemaLimits,
                                        CodecAccess);
// Emits owner-side Codec<T> specializations from declarations. The caller
// appends this source to the checked owner compilation unit; no user mappings.
CodecSourceResult render_state_codec_source(const clang::CXXRecordDecl &);
CodecSourceResult
render_state_codec_sources(::tracer::compat::span<const clang::CXXRecordDecl* const> roots,
                           CodecAccess = CodecAccess::public_members);
// Combines marked State<T> record roots and generic payload QualTypes into one
// deduplicated recursive codec emission pass.
CodecSourceResult
render_context_codec_sources(clang::ASTContext&,
                             ::tracer::compat::span<const clang::CXXRecordDecl* const> state_roots,
                             ::tracer::compat::span<const clang::QualType> payload_roots,
                             CodecAccess = CodecAccess::public_members);
CodecSourceResult render_pimpl_context_codec_sources(
    clang::ASTContext &,
    ::tracer::compat::span<const clang::QualType> payload_roots,
    clang::QualType root_type, std::string_view member_name,
    CodecAccess = CodecAccess::snapshot_members);
// Emits any required user-record codecs plus a root call that encodes payload
// as field 1; builtin/string/container roots use encode_value directly.
CodecSourceResult render_payload_codec_source(clang::ASTContext &,
                                              clang::QualType payload,
                                              CodecAccess = CodecAccess::public_members);
// Resolves a dotted logical field path independently against each immutable
// revision. Missing paths remain explicit; renamed fields are never inferred.
std::vector<RevisionField>
query_field_path(::tracer::compat::span<const std::shared_ptr<const SchemaRecord>> revisions,
                 ::tracer::compat::span<const std::string_view> path);

} // namespace tracer::context_engine
