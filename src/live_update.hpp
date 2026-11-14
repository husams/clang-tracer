#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tracer::live {

struct SourceFile {
  std::string path;
  std::string contents;
};

struct TranslationUnit {
  std::string path;
  std::string directory;
  std::vector<std::string> compiler_arguments;
};

struct JitRequest {
  std::vector<SourceFile> files;
  std::vector<std::string> compiler_arguments;
  std::map<std::string, std::uintptr_t> imports;
  // An empty list treats every .cc/.cpp/.cxx input as a translation unit
  // with the shared arguments. Otherwise the list gives each unit its profile.
  std::vector<TranslationUnit> translation_units;
  std::vector<std::string> entry_symbols;
};

class JitImage {
public:
  virtual ~JitImage() = default;
  virtual std::uintptr_t lookup(std::string_view symbol) = 0;
};

std::shared_ptr<JitImage> compile_in_target(const JitRequest &request);

struct Entry {
  std::string symbol;
  std::string signature;
  std::string source;
  std::string module;
  std::uint64_t elf_address = 0;
  std::uintptr_t address = 0;
  std::size_t function_size = 0;
  std::string original_bytes;
  std::string build_id;
  bool return_at_end = true;
  // A verified reserved NOP window leaves a stable native bypass at this
  // address, including the original unwind metadata and argument registers.
  [[nodiscard]] std::uintptr_t original() const noexcept {
    return address + original_bytes.size();
  }
};

enum class SafePoint { independent_entry, drained_stack };

} // namespace tracer::live
