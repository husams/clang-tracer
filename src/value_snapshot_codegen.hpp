#pragma once
#include <string>
#include <vector>
namespace clang {
class ASTContext;
}
namespace clangtracer {
namespace v1 {
class ProbeSpec;
}
} // namespace clangtracer
namespace tracer {
struct ValueSnapshotCode {
  std::string declarations;
  std::string scope;
  std::string argument_storage;
  std::string receiver_storage;
  std::string result_storage;
  // Grouped object-watch snapshots keep storage alive through both phases.
  std::string storage;
  std::string arguments;
  std::string receiver;
  std::string result;
  std::string error;
  bool has_custom_encoder = false;
  bool has_followed_members = false;
  // Some record is captured without a member it cannot encode (a partial snapshot).
  bool has_omitted_members = false;
  // One line per member left out of an argument, receiver or result, with the reason.
  std::vector<std::string> notes;
  // At least one value is captured by generated code, not just reported.
  bool has_automatic_capture = false;
};
// With extended false the code is the plain variant: no pointer or reference
// member is followed and a record that cannot be captured whole is not captured
// at all. It is the fallback when the extended code does not compile.
// A non-empty capture_unavailable reports every value that would be captured as
// unsupported with that detail and generates no codec for it: the fallback when
// the plain code does not compile either.
ValueSnapshotCode make_value_snapshot_code(clang::ASTContext &,
                                           const clangtracer::v1::ProbeSpec &,
                                           bool extended = true,
                                           const std::string &capture_unavailable = {});
} // namespace tracer
