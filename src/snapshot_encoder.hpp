#pragma once

#include <clang/AST/Type.h>
#include <string>
#include <vector>

namespace clang {
class ASTContext;
}

namespace tracer {
struct SnapshotEncoder {
  clang::QualType input;
  clang::QualType output;
};

struct SnapshotEncoders {
  std::vector<SnapshotEncoder> encoders;
  std::string error;
  bool requested = false;
};

// Collect user-authored tracer_snapshot::encode overloads from the callback AST.
SnapshotEncoders find_snapshot_encoders(clang::ASTContext &);
} // namespace tracer
