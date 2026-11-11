#pragma once
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace clang {
class CompilerInstance;
}

namespace tracer_repl {
struct GeneratedRecordMappings {
  std::string source;
  std::vector<std::string> types;
};

// Called only in the disposable, syntax-only checker after parsing the header.
// A non-null allowlist enables reporting of unbound external destructor dependencies.
// With destructors_final, a destructor missing from the allowlist is treated as
// permanently unavailable instead of as a dependency still to be resolved.
GeneratedRecordMappings
generate_record_mappings(clang::CompilerInstance& compiler, const std::vector<std::string>& types,
                         const std::function<bool(const std::string&)>& viable,
                         const std::set<std::string>* external_destructors,
                         bool destructors_final = false);
} // namespace tracer_repl
