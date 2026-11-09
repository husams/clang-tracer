#include "module_index_internal.hpp"

#include <charconv>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace tracer_module;
using namespace tracer::module_index;

class Options {
  std::map<std::string, std::vector<std::string>> values_;
public:
  Options(int argc, char **argv) {
    for (int i = 2; i < argc; ++i) {
      const std::string name = argv[i];
      if (name == "--exported" || name == "--metadata-only") {
        values_[name].push_back("1");
      } else {
        if (name.rfind("--", 0) != 0 || i + 1 >= argc)
          throw std::invalid_argument("invalid option: " + name);
        values_[name].push_back(argv[++i]);
      }
    }
  }

  void allow(std::initializer_list<const char *> names) const {
    for (const auto &[name, values] : values_) {
      if (std::find(names.begin(), names.end(), name) == names.end())
        throw std::invalid_argument("unexpected option: " + name);
      if (values.size() > 1 && name != "--type" && name != "--symbol")
        throw std::invalid_argument("duplicate option: " + name);
    }
  }
  std::vector<std::string> all(const std::string &name) const {
    const auto found = values_.find(name);
    return found == values_.end() ? std::vector<std::string>{} : found->second;
  }
  std::string get(const std::string &name) const {
    const auto found = values_.find(name);
    return found == values_.end() ? "" : found->second.front();
  }
  std::string require(const std::string &name) const {
    auto value = get(name);
    if (value.empty()) throw std::invalid_argument("missing " + name);
    return value;
  }
  template <class Number> Number number(const std::string &name) const {
    const auto text = get(name);
    if (text.empty()) return 0;
    Number number = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc() || parsed.ptr != text.data() + text.size())
      throw std::invalid_argument("invalid number for " + name);
    return number;
  }
};

template <class T> llvm::json::Object page_json(const ModulePage<T> &page) {
  llvm::json::Array entries;
  for (const auto &entry : page.entries) entries.emplace_back(json(entry));
  return llvm::json::Object{{"entries", std::move(entries)}, {"next_page", page.next_page}};
}

void print(llvm::json::Object object) {
  std::cout << dump(std::move(object)) << '\n';
}

} // namespace

int main(int argc, char **argv) {
  try {
    const std::string command = argc > 1 ? argv[1] : "--help";
    if (command == "--help") {
      std::cout << "clang-tracer-module index --file ELF --output INDEX [--debug-file FILE]\n"
                   "clang-tracer-module info|types|symbols --index INDEX\n"
                   "clang-tracer-module show --index INDEX (--type NAME | --symbol SELECTOR)\n"
                   "clang-tracer-module emit --index INDEX [--type ID] [--symbol ID] --output DIR\n";
      return 0;
    }
    const Options options(argc, argv);
    if (command == "index") {
      options.allow({"--file", "--output", "--debug-file", "--max-entries", "--max-depth"});
      const auto image = options.require("--file");
      const auto output = options.get("--output");
      const auto index = ModuleIndex::build(
          ModuleSource::file(image, options.get("--debug-file")),
          output.empty() ? image + ".tracer.sqlite" : output,
          {options.number<std::uint64_t>("--max-entries"),
                                       options.number<std::uint32_t>("--max-depth"), {}});
      print(json(index.identity()));
      return 0;
    }
    const auto index = ModuleIndex::open(options.require("--index"));
    if (command == "info") {
      options.allow({"--index"});
      print(json(index.identity()));
    } else if (command == "types" || command == "symbols") {
      if (command == "types") options.allow({"--index", "--filter", "--page", "--limit"});
      else options.allow({"--index", "--filter", "--page", "--limit", "--exported"});
      const ModuleQuery query{options.get("--filter"), options.get("--page"),
                              options.number<std::size_t>("--limit")};
      if (command == "types") print(page_json(index.types(query)));
      else print(page_json(index.symbols({query, !options.get("--exported").empty()})));
    } else if (command == "show") {
      options.allow({"--index", "--type", "--symbol", "--id"});
      if (options.all("--type").size() + options.all("--symbol").size() +
              options.all("--id").size() != 1)
        throw std::invalid_argument("select exactly one --type, --symbol, or --id");
      const auto type = options.get("--type"), symbol = options.get("--symbol"), id = options.get("--id");
      if (!type.empty()) print(json(index.find_type(type)));
      else if (!symbol.empty()) print(json(index.find_symbol(symbol)));
      else if (!id.empty() && id[0] == 's') print(json(index.get_symbol(id)));
      else if (!id.empty()) print(json(index.get_type(id)));
      else throw std::invalid_argument("select --type, --symbol, or --id");
    } else if (command == "emit") {
      options.allow({"--index", "--type", "--symbol", "--output"});
      index.emit_headers({options.all("--symbol"), options.all("--type")}, options.require("--output"));
    } else {
      throw std::invalid_argument("unknown module command: " + command);
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "module: " << error.what() << '\n';
    return 1;
  }
}
