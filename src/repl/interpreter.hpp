#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "api.hpp"

namespace clangtracer::v1 {
class ListModulesResponse;
class PreparedInjector;
}

namespace tracer_repl {
struct CppCompletions {
  std::string prefix;
  std::vector<std::string> items;
};

class CppEngine {
  using PreparedModules = std::function<clangtracer::v1::ListModulesResponse()>;
  struct MappingHeader {
    std::string path;
    std::string request_key;
    std::string stored_key;
    bool reused;
  };
  struct Impl;
  std::unique_ptr<Impl> impl_;
  Facade &facade_;
  std::vector<std::string> arguments_;
  void initialize();
  MappingHeader mapping_header(const std::vector<std::string> &headers,
                             const std::vector<std::string> &types,
                             const std::string &output, bool standalone,
                             const PreparedModules &modules = {});

public:
  CppEngine(Facade &facade, std::vector<std::string> arguments);
  ~CppEngine();
  bool evaluate(const std::string &source);
  CppCompletions complete(const std::string &source);
  std::string describe_api(const std::string &topic = "api");
  std::string describe_symbol(const std::string &symbol);
  const std::vector<std::string> &compiler_options() const {
    return arguments_;
  }
  std::vector<std::string> effective_compiler_options() const;
  void require_compiler_options_mutable() const;
  void set_compiler_options(std::vector<std::string> arguments);
  std::string import_header(const std::string &header,
                            const std::vector<std::string> &types = {},
                            const std::string &output = {});
  std::string import_headers(const std::vector<std::string> &headers,
                             const std::vector<std::string> &types = {},
                             const std::string &output = {},
                             const PreparedModules &modules = {});
  std::string import_prepared_adapter(const std::string &source,
                                      const PreparedModules &modules = {});
  std::string import_recorded_types(const clangtracer::v1::PreparedInjector &types);
  std::string generate_header(const std::string &header,
                              const std::vector<std::string> &types = {},
                              const std::string &output = {});
  void reset();
};
} // namespace tracer_repl
