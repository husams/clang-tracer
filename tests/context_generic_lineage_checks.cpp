#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "context_activation.hpp"
#include "context_test_config.hpp"

namespace {
[[maybe_unused]] void require(bool value, const std::string &message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

class TemporaryDirectory final {
public:
  TemporaryDirectory() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("clang-tracer-generic-lineage-" + std::to_string(nonce));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  const std::filesystem::path &path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

[[maybe_unused]] tracer::ContextActivationRequest
request_for(const std::filesystem::path &include_directory,
            std::string file_name, std::string source,
            std::uint8_t lineage_byte, bool system_include = false) {
  tracer::ContextActivationRequest request;
  request.source = std::move(source);
  request.source_lineage_id[0] = lineage_byte;
  request.file_name = std::move(file_name);
  request.compiler_arguments = {"-std=c++20", "-fPIC", "-O0",
                                "-fno-stack-protector",
                                "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  request.compiler_arguments.emplace_back(system_include ? "-isystem" : "-I");
  request.compiler_arguments.push_back(include_directory.string());
  request.compiler_arguments.insert(request.compiler_arguments.end(),
                                    std::begin(kContextSystemArguments),
                                    std::end(kContextSystemArguments));
  return request;
}
} // namespace

void verify_context_generic_lineage() {
#if defined(__linux__) && defined(__x86_64__)
  TemporaryDirectory temporary;
  {
    std::ofstream scope_header(temporary.path() / "common_scope.hpp");
    scope_header << "namespace lineage_fixture { struct Scope {}; "
                    "struct UserRecord { int value; }; }\n";
    require(static_cast<bool>(scope_header),
            "could not write shared Scope fixture");
  }

  const auto init_request = request_for(
      temporary.path(), "generic-init-callback.cc",
      "#include \"common_scope.hpp\"\n"
      "#include <string>\n#include <vector>\n"
      "namespace callback_one { struct Initializer { void pre(const Call& "
      "call) "
      "{ call.context().init_scope<lineage_fixture::Scope>(42); "
      "call.context().init_scope<lineage_fixture::Scope>(std::string{}); "
      "call.context().init_scope<lineage_fixture::Scope>("
      "std::vector<lineage_fixture::UserRecord>{}); } }; }\n",
      0xa1, true);
  const auto get_request = request_for(
      temporary.path(), "generic-get-callback.cc",
      "#include \"common_scope.hpp\"\n"
      "#include <string>\n#include <vector>\n"
      "namespace callback_two { struct Reader { void pre(const Call& call) "
      "{ (void)call.context().get<lineage_fixture::Scope, int>(); "
      "(void)call.context().get<lineage_fixture::Scope, std::string>(); "
      "(void)call.context().get<lineage_fixture::Scope, "
      "std::vector<lineage_fixture::UserRecord>>(); } }; }\n",
      0xb2, true);

  tracer::ContextRegistry registry;
  const auto initialized =
      tracer::activate_context_source(init_request, registry);
  require(static_cast<bool>(initialized),
          "generic initializer activation failed: " + initialized.error);
  const auto read = tracer::activate_context_source(get_request, registry);
  require(static_cast<bool>(read),
          "generic reader activation failed: " + read.error);
  const bool reused =
      initialized.generation->scopes().size() == 1 &&
      read.generation->scopes().size() == 1 &&
      initialized.generation->scopes()[0]->states.size() == 3 &&
      initialized.generation->scopes()[0] == read.generation->scopes()[0] &&
      initialized.generation->scopes()[0]->registration ==
          read.generation->scopes()[0]->registration &&
      initialized.generation->scopes()[0]->states[0].identity ==
          read.generation->scopes()[0]->states[0].identity;
  if (!reused) {
    std::ostringstream details;
    details << " (initializer scopes="
            << initialized.generation->scopes().size()
            << ", reader scopes=" << read.generation->scopes().size();
    if (!initialized.generation->scopes().empty() &&
        !read.generation->scopes().empty()) {
      const auto &left = *initialized.generation->scopes()[0];
      const auto &right = *read.generation->scopes()[0];
      details << ", same_scope=" << (&left == &right) << ", same_registration="
              << (left.registration == right.registration)
              << ", state_counts=" << left.states.size() << '/'
              << right.states.size() << ", type_contracts_equal="
              << (left.type_contract.types == right.type_contract.types);
      const auto count = std::min(left.states.size(), right.states.size());
      for (std::size_t i = 0; i < count; ++i) {
        const auto &a = left.states[i];
        const auto &b = right.states[i];
        details << "; state[" << i << "]=" << a.dependency.payload_type
                << " identity=" << (a.identity == b.identity) << " schema="
                << (a.schema && b.schema &&
                    a.schema->revision_id == b.schema->revision_id &&
                    a.schema->descriptor_set == b.schema->descriptor_set)
                << " provenance="
                << (a.payload_provenance == b.payload_provenance)
                << " lifecycle="
                << (a.lifecycle_fingerprint == b.lifecycle_fingerprint);
      }
    }
    details << ')';
    throw std::runtime_error(
        "generic int state did not reuse the shared ScopeTag owner across "
        "callbacks" +
        details.str());
  }
  const auto &schema = *initialized.generation->scopes()[0]->states[0].schema;
  require(
      schema.family.source_lineage_id ==
          initialized.generation->scopes()[0]->source_lineage_id,
      "generic builtin payload family did not inherit its ScopeTag lineage");
  for (const auto &state : initialized.generation->scopes()[0]->states) {
    require(
        state.schema->family.source_lineage_id ==
            initialized.generation->scopes()[0]->source_lineage_id,
        "generic payload family did not retain its shared ScopeTag lineage");
  }
#endif
}
