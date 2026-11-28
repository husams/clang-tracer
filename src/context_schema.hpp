#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "tracer/compat.hpp"

namespace clang {
class ASTContext;
class CXXRecordDecl;
class FieldDecl;
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
  unordered_map,
  // Raw bytes: a union's object representation, a long double's value bytes.
  bytes
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
  // An unsigned enumerator above INT64_MAX keeps its bit pattern; see enum_unsigned.
  std::vector<std::pair<std::string, std::int64_t>> enum_values;
  std::uint32_t number = 0;
  SchemaKind kind = SchemaKind::message;
  unsigned bit_width = 0;
  bool repeated = false, optional = false;
  std::shared_ptr<const struct SchemaMessage> message;
  // Set instead of `message` where a record contains itself, directly or
  // through other records: the record is still being described higher up and
  // already owns this field, so the field refers back to it without owning it.
  std::weak_ptr<const struct SchemaMessage> recursive;
  std::shared_ptr<const SchemaField> key, value;
  std::size_t fixed_count = 0;
  // The record this field holds, whichever way it is referenced.
  std::shared_ptr<const struct SchemaMessage> record() const {
    return message ? message : recursive.lock();
  }
  // A wide enumeration does not fit a protobuf enum, whose numbers are 32-bit: its
  // underlying type is wider than 32 bits or an enumerator is outside the int32 range.
  // It is stored as an integer of the underlying width and signedness.
  bool enum_wide = false, enum_unsigned = false;
};

// A member that capture leaves out of a record. It keeps its field number.
struct SchemaUnavailable {
  // `name` is unique among the fields and the members left out of one message. It is
  // the member's own name unless that is shared with another member of the message
  // (then `Owner::name`), or an unnamed member's description, which carries its field
  // number when several share it: "(anonymous union #3)". `owner` is the record that
  // declares the member, which can be a base of the message's record.
  std::string name, cpp_type, reason, owner;
  std::uint32_t number = 0;
};

struct SchemaMessage {
  std::string cpp_type, full_name;
  std::vector<SchemaField> fields;
  std::vector<SchemaUnavailable> unavailable;
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
// Whether a record may contain itself (a tree or list node). The schema of such
// a record is a finite graph: the inner occurrence refers back to the outer one
// (SchemaField::recursive) and names the same descriptor message.
enum class SchemaRecursion { reject, allow };

// The members one generated capture unit leaves out, with the reason. The schema
// builder fills it; the codec generator and the type check read it, so the
// descriptor and the codec cannot disagree about a field or its number.
struct CapturePlan {
  std::map<const clang::FieldDecl *, std::string> omitted; // member -> reason
  std::set<const clang::FieldDecl *> described;            // members some schema kept
  bool conflict = false; // A member was omitted after a schema had kept it.
  // Extra per-member admission check: a reason, or empty to admit the member.
  std::function<std::string(clang::QualType)> reject;
  const std::string *reason(const clang::FieldDecl &field) const;
  void omit(const clang::FieldDecl &field, std::string reason);
};

// Generic scoped payloads need no state/shared_state marker. The root
// descriptor is a wrapper whose field 1 is named `value`.
struct SnapshotFieldSelection {
  const clang::CXXRecordDecl *record = nullptr;
  std::vector<std::string> names;
  // When set, a member that cannot be captured is left out instead of failing
  // the whole value. Null behaves exactly as before.
  CapturePlan *plan = nullptr;
  bool includes(const clang::CXXRecordDecl *root, const clang::FieldDecl &field) const;
};

SchemaResult build_payload_schema(clang::ASTContext &, clang::QualType payload,
                                  StateFamilyKey, SchemaLimits = {},
                                  CodecAccess = CodecAccess::public_members,
                                  const SnapshotFieldSelection & = {},
                                  SchemaRecursion = SchemaRecursion::reject);
// Emits owner-side Codec<T> specializations from declarations. The caller
// appends this source to the checked owner compilation unit; no user mappings.
CodecSourceResult render_state_codec_source(const clang::CXXRecordDecl &);
CodecSourceResult
render_state_codec_sources(::tracer::compat::span<const clang::CXXRecordDecl* const> roots,
                           CodecAccess = CodecAccess::public_members,
                           const SnapshotFieldSelection & = {},
                           SchemaRecursion = SchemaRecursion::reject);
// Combines marked State<T> record roots and generic payload QualTypes into one
// deduplicated recursive codec emission pass.
CodecSourceResult
render_context_codec_sources(clang::ASTContext&,
                             ::tracer::compat::span<const clang::CXXRecordDecl* const> state_roots,
                             ::tracer::compat::span<const clang::QualType> payload_roots,
                             CodecAccess = CodecAccess::public_members,
                             const SnapshotFieldSelection & = {},
                             SchemaRecursion = SchemaRecursion::reject);
struct SnapshotMemberAccessor {
  const clang::CXXRecordDecl *root_type = nullptr;
  std::string member_name;
  bool erased_any = false;
};
CodecSourceResult render_snapshot_member_accessors(
    clang::ASTContext &,
    ::tracer::compat::span<const clang::QualType> payload_roots,
    ::tracer::compat::span<const SnapshotMemberAccessor> members,
    CodecAccess = CodecAccess::snapshot_members,
    const SnapshotFieldSelection & = {},
    SchemaRecursion = SchemaRecursion::reject);
// Emits any required user-record codecs plus a root call that encodes payload
// as field 1; builtin/string/container roots use encode_value directly.
CodecSourceResult render_payload_codec_source(clang::ASTContext &,
                                              clang::QualType payload,
                                              CodecAccess = CodecAccess::public_members,
                                              const SnapshotFieldSelection & = {},
                                              SchemaRecursion = SchemaRecursion::reject);
// Resolves a dotted logical field path independently against each immutable
// revision. Missing paths remain explicit; renamed fields are never inferred.
std::vector<RevisionField>
query_field_path(::tracer::compat::span<const std::shared_ptr<const SchemaRecord>> revisions,
                 ::tracer::compat::span<const std::string_view> path);

} // namespace tracer::context_engine
