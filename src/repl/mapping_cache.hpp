#pragma once
#include <filesystem>
#include <string>

namespace tracer_repl {
std::string mapping_digest(const std::string &text);
std::string mapping_file_digest(const std::filesystem::path &path);
std::string mapping_search_root(const std::filesystem::path &path);
std::filesystem::path mapping_cache_directory();
bool valid_mapping_header(const std::filesystem::path &path,
                          const std::string &key);
void save_mapping_header(const std::filesystem::path &path,
                         const std::string &key, const std::string &body);
std::string mapping_header_summary(const std::filesystem::path &path);
} // namespace tracer_repl
