#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

#include "live_mock.hpp"
#include "tracer.pb.h"

namespace tracer::live {
namespace {
namespace fs = std::filesystem;
constexpr std::string_view prefix = "clangtracer_live_mock_";

bool identifier(std::string_view value) {
  if (value.empty() ||
      value.find_first_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") !=
          std::string_view::npos) {
    return false;
  }
  return value.front() < '0' || value.front() > '9';
}

bool qualified_identifier(std::string_view value) {
  if (value.starts_with("::")) {
    value.remove_prefix(2);
  }
  while (true) {
    const auto split = value.find("::");
    if (!identifier(value.substr(0, split))) {
      return false;
    }
    if (split == std::string_view::npos) {
      return true;
    }
    value.remove_prefix(split + 2);
  }
}

SourceFile snapshot(std::string_view path, std::string_view kind) {
  const auto name = fs::absolute(path).lexically_normal();
  if (!fs::is_regular_file(name)) {
    throw std::invalid_argument("cannot read mock " + std::string(kind) + ": " + name.string());
  }
  std::ifstream file(name, std::ios::binary);
  if (!file) {
    throw std::invalid_argument("cannot read mock " + std::string(kind) + ": " + name.string());
  }
  std::string contents;
  char block[8192];
  while (file.read(block, sizeof block) || file.gcount()) {
    contents.append(block, static_cast<std::size_t>(file.gcount()));
  }
  if (file.bad() || !file.eof()) {
    throw std::invalid_argument("cannot read mock " + std::string(kind) + ": " + name.string());
  }
  return {name.string(), std::move(contents)};
}

std::vector<std::string> tokens(const clangtracer::v1::LiveUpdateRequest& request) {
  if (request.arguments_size() != 1) {
    throw std::invalid_argument("%live mock accepts exactly one target signature");
  }
  std::vector<std::string> result{request.arguments(0)};
  for (const auto& option : request.options()) {
    result.push_back(option.name());
    result.push_back(option.value());
  }
  return result;
}
} // namespace

MockCommand parse_live_mock(std::span<const std::string> arguments) {
  if (arguments.empty() || arguments.front().find('(') == std::string::npos ||
      arguments.front().find(')') == std::string::npos) {
    throw std::invalid_argument("%live mock requires an exact callable signature");
  }
  MockCommand result;
  result.selector = arguments.front();
  std::string source_path;
  std::string fixture_path;
  std::set<std::string> aliases{"original", "fixture_text"};
  for (std::size_t i = 1; i < arguments.size();) {
    const auto& option = arguments[i++];
    const auto next = [&]() -> const std::string& {
      if (i == arguments.size() || arguments[i].empty() || arguments[i].starts_with("--")) {
        throw std::invalid_argument("missing value for mock option " + option);
      }
      return arguments[i++];
    };
    if (option == "--bind") {
      const auto& binding = next();
      const auto separator = binding.find('=');
      const auto alias = binding.substr(0, separator);
      if (separator == std::string::npos || separator + 1 == binding.size() || !identifier(alias) ||
          alias.starts_with(prefix) || !aliases.insert(alias).second) {
        throw std::invalid_argument("invalid or duplicate mock binding: " + binding);
      }
      result.bindings.push_back({alias, binding.substr(separator + 1)});
      continue;
    }
    if (option == "--recursion") {
      if (result.recursive_original || next() != "original") {
        throw std::invalid_argument("expected one --recursion original option");
      }
      result.recursive_original = true;
      continue;
    }
    std::string* destination = nullptr;
    if (option == "--file") {
      destination = &source_path;
    } else if (option == "--fixture") {
      destination = &fixture_path;
    } else if (option == "--function") {
      destination = &result.callback;
    }
    if (!destination || !destination->empty()) {
      throw std::invalid_argument("unknown or duplicate mock option: " + option);
    }
    *destination = next();
  }
  if (source_path.empty() || !qualified_identifier(result.callback)) {
    throw std::invalid_argument("%live mock requires --file FILE and --function FUNCTION");
  }
  result.source.path = fs::absolute(source_path).lexically_normal().string();
  if (!fixture_path.empty()) {
    result.fixture = SourceFile{fs::absolute(fixture_path).lexically_normal().string(), {}};
  }
  return result;
}

MockCommand snapshot_live_mock(std::span<const std::string> arguments) {
  auto result = parse_live_mock(arguments);
  result.source = snapshot(result.source.path, "source");
  if (result.fixture) {
    result.fixture = snapshot(result.fixture->path, "fixture");
  }
  return result;
}

void snapshot_live_mock(clangtracer::v1::LiveUpdateRequest& request) {
  if (!request.sources().empty()) {
    throw std::invalid_argument("mock source snapshots already supplied");
  }
  const auto command = snapshot_live_mock(tokens(request));
  const auto append = [&](const SourceFile& snapshot) {
    auto* source = request.add_sources();
    source->set_path(snapshot.path);
    source->set_contents(snapshot.contents);
  };
  append(command.source);
  if (command.fixture) {
    append(*command.fixture);
  }
  for (auto& option : *request.mutable_options()) {
    if (option.name() == "--file") {
      option.set_value(command.source.path);
    }
    if (option.name() == "--fixture") {
      option.set_value(command.fixture->path);
    }
  }
}

MockCommand read_live_mock(const clangtracer::v1::LiveUpdateRequest& request) {
  auto command = parse_live_mock(tokens(request));
  if (request.sources_size() != (command.fixture ? 2 : 1) ||
      request.sources(0).path() != command.source.path ||
      (command.fixture && request.sources(1).path() != command.fixture->path)) {
    throw std::invalid_argument(
        "mock source or fixture snapshot is missing or does not match its option");
  }
  command.source.contents = request.sources(0).contents();
  if (command.fixture) {
    command.fixture->contents = request.sources(1).contents();
  }
  return command;
}
} // namespace tracer::live
