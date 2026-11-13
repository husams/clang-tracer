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
};
ValueSnapshotCode make_value_snapshot_code(clang::ASTContext &,
                                           const clangtracer::v1::ProbeSpec &);
} // namespace tracer
