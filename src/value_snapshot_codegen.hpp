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
  std::string arguments;
  std::string receiver;
  std::string result;
  std::string error;
  bool has_custom_encoder = false;
  bool has_followed_members = false;
};
// With follow_members false no pointer or reference member is followed: the
// fallback when code that follows them does not compile.
ValueSnapshotCode make_value_snapshot_code(clang::ASTContext &,
                                           const clangtracer::v1::ProbeSpec &,
                                           bool follow_members = true);
} // namespace tracer
