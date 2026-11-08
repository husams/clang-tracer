#pragma once

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Tooling/Inclusions/StandardLibrary.h>

#include "prepared_header_search.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <system_error>

namespace tracer::prepared {

// Resolves declaration dependencies against a HeaderSearch built from the
// compiler's trusted toolchain-only invocation. It deliberately does not use
// SourceManager's system-header bit as evidence: application paths may be
// marked system by user-supplied -isystem options in the original command.
class HeaderDiscovery {
public:
  HeaderDiscovery(const clang::ASTContext &ast, const clang::HeaderSearch &toolchain_search)
      : source_manager_(ast.getSourceManager()), toolchain_search_(toolchain_search) {}

  std::optional<std::string> headerFor(const clang::NamedDecl &decl,
                                       const std::set<std::string> &excluded_headers) const {
    if (!toolchain_file(decl.getLocation()) ||
        is_excluded(decl.getLocation(), excluded_headers))
      return std::nullopt;

    clang::tooling::stdlib::Recognizer recognize;
    if (const auto symbol = recognize(&decl)) {
      for (const auto &candidate : symbol->headers()) {
        auto spelling = angle_spelling(candidate.name());
        if (spelling && toolchain_header(toolchain_search_, *spelling))
          return spelling;
      }
    }

    // For C library and vendor types, prefer the outermost already-included
    // toolchain header that encloses this declaration. This walks actual
    // SourceManager inclusion provenance and then checks the resulting
    // spelling against the clean toolchain search roots.
    auto location = source_manager_.getSpellingLoc(decl.getLocation());
    auto file_id = source_manager_.getFileID(location);
    std::optional<std::string> selected;
    while (file_id.isValid()) {
      const auto entry = source_manager_.getFileEntryRefForID(file_id);
      if (entry) {
        const auto path = canonical(entry->getName().str());
        if (path && !is_excluded(*path, excluded_headers) && toolchain_file(*path)) {
          bool angled = true;
          auto spelling = toolchain_search_.suggestPathToFileForDiagnostics(
              *path, {}, source_manager_.getFilename(source_manager_.getLocForStartOfFile(
                           source_manager_.getMainFileID())), &angled);
          if (resolves_to(spelling, *path))
            selected = std::move(spelling);
        }
      }
      const auto include_location = source_manager_.getIncludeLoc(file_id);
      if (include_location.isInvalid())
        break;
      file_id = source_manager_.getFileID(include_location);
    }
    return selected;
  }

private:
  static std::optional<std::string> angle_spelling(llvm::StringRef name) {
    if (name.size() < 3 || name.front() != '<' || name.back() != '>')
      return std::nullopt;
    std::string spelling = name.drop_front().drop_back().str();
    return valid_include_name(spelling) ? std::optional<std::string>(std::move(spelling))
                                        : std::nullopt;
  }

  static std::optional<std::string> canonical(std::string path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);
    if (error)
      return std::nullopt;
    return result.string();
  }

  std::optional<std::string> canonical_input_path(std::string path) const {
    std::filesystem::path input(path);
    if (input.is_relative()) {
      const auto main_file = source_manager_.getFilename(source_manager_.getLocForStartOfFile(
          source_manager_.getMainFileID()));
      input = std::filesystem::path(main_file.str()).parent_path() / input;
    }
    return canonical(input.string());
  }

  bool toolchain_file(clang::SourceLocation location) const {
    location = source_manager_.getSpellingLoc(location);
    if (location.isInvalid())
      return false;
    const auto entry = source_manager_.getFileEntryRefForID(source_manager_.getFileID(location));
    if (!entry)
      return false;
    const auto path = canonical(entry->getName().str());
    return path && toolchain_file(*path);
  }

  bool toolchain_file(const std::string &path) const {
    bool angled = true;
    const auto spelling = toolchain_search_.suggestPathToFileForDiagnostics(
        path, {}, source_manager_.getFilename(source_manager_.getLocForStartOfFile(
                     source_manager_.getMainFileID())), &angled);
    return resolves_to(spelling, path);
  }

  bool resolves_to(const std::string &spelling, const std::string &expected_path) const {
    const auto entry = toolchain_header(toolchain_search_, spelling);
    if (!entry)
      return false;
    const auto resolved = canonical(entry->getName().str());
    return resolved && *resolved == expected_path;
  }

  bool is_excluded(clang::SourceLocation location,
                   const std::set<std::string> &excluded_headers) const {
    location = source_manager_.getSpellingLoc(location);
    if (location.isInvalid())
      return true;
    const auto entry = source_manager_.getFileEntryRefForID(source_manager_.getFileID(location));
    if (!entry)
      return true;
    const auto path = canonical(entry->getName().str());
    return !path || is_excluded(*path, excluded_headers);
  }

  bool is_excluded(const std::string &path,
                   const std::set<std::string> &excluded_headers) const {
    for (const auto &header : excluded_headers) {
      const auto normalized = canonical_input_path(header);
      if (normalized && *normalized == path)
        return true;
    }
    return false;
  }

  const clang::SourceManager &source_manager_;
  const clang::HeaderSearch &toolchain_search_;
};

} // namespace tracer::prepared
