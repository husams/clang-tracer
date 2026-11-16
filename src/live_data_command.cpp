#include <charconv>
#include <stdexcept>
#include <string_view>

#include "live_data.hpp"

namespace tracer::live {
namespace {
const std::string& value(const std::vector<std::string>& args, std::size_t& index) {
  const auto& option = args[index++];
  if (index == args.size() || args[index].empty() || args[index].starts_with("--")) {
    throw std::invalid_argument(option + " requires a value");
  }
  return args[index++];
}

void assign(std::string& destination, const std::vector<std::string>& args, std::size_t& index) {
  if (!destination.empty()) {
    throw std::invalid_argument("duplicate " + args[index]);
  }
  destination = value(args, index);
}

void validate_discovery(const std::string& file, const std::string& function) {
  if (file.empty() != function.empty()) {
    throw std::invalid_argument("--discover-file and --discover-function are required together");
  }
}
} // namespace

LiveDataUpdateCommand parse_live_data_update(const std::vector<std::string>& args) {
  if (args.empty() || args.front().empty() || args.front().starts_with("--")) {
    throw std::invalid_argument("update-data requires an update source file");
  }
  LiveDataUpdateCommand command;
  command.file = args.front();
  for (std::size_t index = 1; index < args.size();) {
    const auto& option = args[index];
    if (option == "--discover-file") {
      assign(command.discover_file, args, index);
    } else if (option == "--discover-function") {
      assign(command.discover_function, args, index);
    } else if (option == "--module") {
      assign(command.selection.module, args, index);
    } else if (option == "--address") {
      if (command.selection.address) {
        throw std::invalid_argument("duplicate --address");
      }
      std::string_view spelling = value(args, index);
      const bool hex = spelling.starts_with("0x") || spelling.starts_with("0X");
      if (hex) {
        spelling.remove_prefix(2);
      }
      std::uintptr_t address = 0;
      const auto end = spelling.data() + spelling.size();
      const auto parsed = std::from_chars(spelling.data(), end, address, hex ? 16 : 10);
      if (parsed.ec != std::errc{} || parsed.ptr != end || !address) {
        throw std::invalid_argument("invalid live object address");
      }
      command.selection.address = address;
    } else if (option == "--from-scan") {
      command.selection.scan_ids.push_back(value(args, index));
      while (index < args.size() && !args[index].starts_with("--")) {
        command.selection.scan_ids.push_back(args[index++]);
      }
    } else {
      throw std::invalid_argument("unknown update-data option " + option);
    }
  }
  validate_discovery(command.discover_file, command.discover_function);
  return command;
}

LiveHeapScanCommand parse_live_heap_scan(const std::vector<std::string>& args) {
  LiveHeapScanCommand command;
  for (std::size_t index = 0; index < args.size();) {
    const auto& option = args[index];
    if (option == "--type") {
      command.types.push_back(value(args, index));
    } else if (option == "--module") {
      assign(command.module, args, index);
    } else if (option == "--discover-file") {
      assign(command.discover_file, args, index);
    } else if (option == "--discover-function") {
      assign(command.discover_function, args, index);
    } else {
      throw std::invalid_argument("unknown scan-heap option " + option);
    }
  }
  if (command.types.empty()) {
    throw std::invalid_argument("scan-heap requires at least one --type");
  }
  validate_discovery(command.discover_file, command.discover_function);
  return command;
}
} // namespace tracer::live
