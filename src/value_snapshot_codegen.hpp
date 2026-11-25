#pragma once
#include <string>
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
  // At least one value is captured by generated code, not just reported.
  bool has_automatic_capture = false;
};
// With follow_members false no pointer or reference member is followed: the
// fallback when code that follows them does not compile.  A non-empty
// capture_unavailable reports every value that would be captured as
// unsupported with that detail and generates no codec for it: the fallback
// when the capture code does not compile.
ValueSnapshotCode make_value_snapshot_code(clang::ASTContext &,
                                           const clangtracer::v1::ProbeSpec &,
                                           bool follow_members = true,
                                           const std::string &capture_unavailable = {});
} // namespace tracer
