#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "native_entry.hpp"
#include "object_map_range.hpp"
#include "object_pointee_range.hpp"
#include "tracer.pb.h"

namespace tracer::object_cache { struct RuntimeModule; }

namespace tracer::object_watch {
enum class Boundary { construct, destroy, add, get, remove, snapshot, writer_scope, collection_mutation };
struct Site {
  std::string bytes;
  std::optional<Boundary> boundary;
  std::optional<native_entry::ScalarStore> store;
  bool uncertain_complete = false;
  std::uint64_t inline_mutation = 0;
};
struct InlineMutation {
  using Range = std::pair<std::uint64_t, std::uint64_t>;
  std::uint64_t begin, entry_end;
  std::string receiver_register;
  std::int64_t receiver_offset = 0;
  std::vector<std::uint64_t> continuations;
  std::vector<Range> ranges;
};
struct Source {
  std::string function, file;
  unsigned line = 0, column = 0;
  std::vector<clangtracer::v1::ObjectObservation::InlineLocation> inline_chain;
};
struct UnionCase {
  enum class Type { signed_integer, unsigned_integer, real };
  std::string member;
  Type type;
  bool operator==(const UnionCase &) const = default;
};
struct Layout {
  std::uint64_t scalar, string_pointer, string_length;
  std::uint64_t vector_begin, vector_end, vector_capacity, stride, pointee;
  bool atomic = false;
  bool opaque_union = false;
  std::optional<std::uint64_t> union_tag_offset;
  std::map<std::uint32_t, UnionCase> union_cases;
  std::uint64_t vector_object = 0, key_offset = 0;
  bool logical_vector = false;
  bool logical_map = false;
  std::vector<std::uint64_t> pointer_offsets;
  MapLayout map;
  bool logical_collection() const { return logical_vector || logical_map || !pointer_offsets.empty(); }
  bool primary_rtti = false;
  std::uint64_t base_offset = 0;
};

class Plan {
  struct Image;
  std::unique_ptr<Image> image_;
  std::unique_ptr<Image> main_;
  std::map<std::uint32_t, std::unique_ptr<Image>> runtime_images_;
  const Image *image_for(std::uint64_t pc) const;

public:
  struct MethodBoundary {
    clangtracer::v1::ObjectWatchPlan::Binding binding;
    clangtracer::v1::ObjectObservation origin;
    std::string file_sha256;
    std::string body;
  };
  static MethodBoundary method_boundary(const std::string &module,
      const clangtracer::v1::PreparedInjector &prepared,
      const clangtracer::v1::DebugInfo *live = nullptr, bool allow_rebind = false,
      std::uint32_t pid = 0);

  Plan(std::uint32_t pid, std::string executable,
       const clangtracer::v1::ObjectWatchRequest &request);
  ~Plan();

  const std::string module;
  std::uint64_t bias = 0, typeinfo = 0;
  std::uint64_t writer_start = 0;
  std::uint64_t caller_start = 0;
  std::string_view caller_bytes;
  std::string_view writer_bytes;
  std::vector<std::pair<std::uint64_t, std::string_view>> writer_ranges;
  std::set<std::uint64_t> static_call_scopes;
  Layout layout{};
  std::map<std::uint64_t, Site> sites;
  std::map<std::uint64_t, std::size_t> boundaries;
  std::set<std::pair<std::uint64_t, std::uint64_t>> complete_constructor_returns;
  std::map<std::uint64_t, InlineMutation> inline_mutations;
  unsigned stores = 0, unsupported = 0;
  google::protobuf::RepeatedPtrField<clangtracer::v1::ObjectWatchRequest::SourceWriter> source_writers;

  std::string_view code(std::uint64_t pc) const;
  Source source(std::uint64_t pc) const;
  void describe_origin(std::uint64_t pc, const Source &source,
                       clangtracer::v1::ObjectObservation &event) const;
  void describe_runtime_origin(std::uint64_t pc, std::uint32_t generation,
      const object_cache::RuntimeModule &module, clangtracer::v1::ObjectObservation &event);
  bool describe_runtime_frame(std::uint64_t pc, std::uint32_t generation,
                              clangtracer::v1::StackFrame &frame) const;
  const std::string &module_at(std::uint64_t pc) const;
  std::uint64_t elf_pc(std::uint64_t pc) const;
  bool shared_writer() const { return bool(main_); }
  clangtracer::v1::ObjectWatchPlan describe(bool live) const;
  void verify_live(std::uint32_t pid) const;
  std::pair<std::uint64_t, std::size_t> application_range(
      std::uint32_t pid, const std::string &selector) const;
  std::pair<std::uint64_t, std::size_t> containing_function(std::uint64_t pc) const;
  std::map<std::uint64_t, std::vector<std::uint64_t>>
  direct_redirects(const std::vector<std::uint64_t> &entries, bool calls_only = false) const;
  std::string runtime_scope_code(const std::string &path,
      std::uint64_t bias, std::uint64_t start, const std::array<std::uint8_t, 32> &sha256,
      std::size_t width,
      const std::function<std::vector<std::uint64_t>(std::string_view)> &inspect) const;
};
std::string compare_prepared(const clangtracer::v1::ObjectWatchPlan &prepared,
                             const clangtracer::v1::ObjectWatchPlan &live, bool no_values);
std::string field_path(const clangtracer::v1::ObjectWatchRequest &request, const Layout &layout);
struct ScalarSample {
  std::optional<std::int32_t> value;
  std::optional<std::uint32_t> tag;
};
void set_scalar_values(clangtracer::v1::ObjectObservation &event, const Layout &layout,
                       ScalarSample old, ScalarSample next);
} // namespace tracer::object_watch
