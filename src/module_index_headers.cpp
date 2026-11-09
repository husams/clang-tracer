#include "module_index_internal.hpp"

#include <llvm/BinaryFormat/Dwarf.h>

#include <map>
#include <set>
#include <stdexcept>

namespace tracer::module_index {
namespace {
bool identifier(std::string_view text) {
  constexpr std::string_view initial = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
  return !text.empty() && initial.find(text.front()) != text.npos &&
      text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") == text.npos;
}

std::pair<std::string, std::string> scope(const std::string &name) {
  std::size_t start = 0, split = name.find("::");
  while (split != name.npos) {
    if (!identifier(std::string_view(name).substr(start, split - start)))
      throw std::invalid_argument("unsupported C++ scope: " + name);
    start = split + 2;
    split = name.find("::", start);
  }
  if (!identifier(std::string_view(name).substr(start)))
    throw std::invalid_argument("unsupported C++ name: " + name);
  return {start ? name.substr(0, start - 2) : "", name.substr(start)};
}

std::string in_namespace(const std::string &space, const std::string &declaration) {
  return space.empty() ? declaration + "\n"
      : "namespace " + space + " { " + declaration + " }\n";
}

bool record(const ModuleType &type) {
  return type.kind == "struct" || type.kind == "class" || type.kind == "union";
}

bool indirect(const ModuleType &type) {
  return type.kind == "pointer" || type.kind == "reference" || type.kind == "rvalue_reference";
}

// Verify both the spelling and the DWARF encoding/width against Linux LP64.
std::string scalar(const ModuleType &type) {
  namespace dw = llvm::dwarf;
  using Fact = std::pair<std::uint64_t, std::uint64_t>;
  static const std::map<std::string, Fact> facts = {
      {"bool", {1, dw::DW_ATE_boolean}}, {"char", {1, dw::DW_ATE_signed_char}},
      {"signed char", {1, dw::DW_ATE_signed_char}}, {"unsigned char", {1, dw::DW_ATE_unsigned_char}},
      {"short", {2, dw::DW_ATE_signed}}, {"short int", {2, dw::DW_ATE_signed}},
      {"unsigned short", {2, dw::DW_ATE_unsigned}}, {"short unsigned int", {2, dw::DW_ATE_unsigned}},
      {"unsigned short int", {2, dw::DW_ATE_unsigned}}, {"int", {4, dw::DW_ATE_signed}},
      {"unsigned int", {4, dw::DW_ATE_unsigned}}, {"long", {8, dw::DW_ATE_signed}},
      {"long int", {8, dw::DW_ATE_signed}}, {"unsigned long", {8, dw::DW_ATE_unsigned}},
      {"long unsigned int", {8, dw::DW_ATE_unsigned}}, {"unsigned long int", {8, dw::DW_ATE_unsigned}},
      {"long long", {8, dw::DW_ATE_signed}}, {"long long int", {8, dw::DW_ATE_signed}},
      {"unsigned long long", {8, dw::DW_ATE_unsigned}},
      {"long long unsigned int", {8, dw::DW_ATE_unsigned}},
      {"unsigned long long int", {8, dw::DW_ATE_unsigned}},
      {"float", {4, dw::DW_ATE_float}}, {"double", {8, dw::DW_ATE_float}},
  };
  const auto found = facts.find(type.name);
  if (found == facts.end() || type.size != found->second.first ||
      type.encoding != found->second.second)
    throw std::invalid_argument("unsupported scalar representation: " + type.name);
  return type.name;
}

class Generator {
  const ModuleIndex &index_;
  std::map<std::string, ModuleType> types_;
  std::map<std::string, std::string> declarations_;
  std::map<std::string, std::string> viewed_;
  std::set<std::string> active_;
  llvm::json::Array omitted_;
  std::string views_;

  const ModuleType &get(const std::string &id) {
    if (const auto found = types_.find(id); found != types_.end()) return found->second;
    return types_.emplace(id, index_.get_type(id)).first->second;
  }

  void forward(const ModuleType &type) {
    if (!type.namespace_scope || type.name.compare(0, 5, "std::") == 0)
      throw std::invalid_argument("unsupported record identity: " + type.name);
    auto [space, name] = scope(type.name);
    const auto tag = type.kind == "union" ? "union" : "struct";
    const auto text = in_namespace(space, std::string(tag) + " " + name + ";");
    const auto [found, inserted] = declarations_.emplace(type.name, text);
    if (!inserted && found->second != text)
      throw std::invalid_argument("conflicting declarations for " + type.name);
  }

  // Reject cycles in typedef/qualifier chains, including malformed saved data.
  struct Visit {
    std::set<std::string> &active;
    const std::string &id;
    ~Visit() { active.erase(id); }
  };

  std::string spelling(const std::string &id, bool behind_pointer = false) {
    if (!active_.insert(id).second) throw std::invalid_argument("cyclic C++ type dependency");
    const Visit visit{active_, id};
    const auto &type = get(id);
    if (type.kind == "void") return "void";
    if (type.kind == "scalar") return scalar(type);
    if (type.kind == "alias") return spelling(type.target_id, behind_pointer);
    if (type.kind == "const" || type.kind == "volatile")
      return spelling(type.target_id, behind_pointer) + " " + type.kind;
    if (indirect(type)) {
      if (type.size && type.size != 8)
        throw std::invalid_argument("unsupported pointer or reference width");
      const auto suffix = type.kind == "pointer" ? "*" : type.kind == "reference" ? "&" : "&&";
      return spelling(type.target_id, true) + suffix;
    }
    if (record(type) && behind_pointer) {
      forward(type);
      return "::" + type.name;
    }
    throw std::invalid_argument("unsupported_call_abi: " + type.kind + " " + type.name);
  }

  const ModuleType &unqualified(std::string id, bool &is_volatile, bool follow_indirect = false) {
    std::set<std::string> seen;
    for (;;) {
      if (!seen.insert(id).second) throw std::invalid_argument("cyclic C++ type dependency");
      const auto &type = get(id);
      is_volatile |= type.kind == "volatile";
      if (type.kind != "const" && type.kind != "volatile" && type.kind != "alias" &&
          !(follow_indirect && indirect(type)))
        return type;
      id = type.target_id;
    }
  }

  void view(const std::string &id, bool explicit_type) {
    bool is_volatile = false;
    const auto &type = unqualified(id, is_volatile, true);
    if (type.kind == "scalar" || type.kind == "void") return;
    if (!record(type) || type.kind == "union" || type.declaration || !type.size || is_volatile) {
      if (explicit_type && record(type)) forward(type);
      omitted_.push_back(type.id + ": no supported record view");
      return;
    }
    forward(type);
    const auto [found, inserted] = viewed_.emplace(type.name, type.id);
    if (!inserted) {
      if (found->second != type.id)
        throw std::invalid_argument("conflicting record views for " + type.name);
      return;
    }
    auto [space, name] = scope(type.name);
    const auto view_space = space.empty() ? "tracer_recovered" : "tracer_recovered::" + space;
    views_ += "namespace " + view_space + " {\nstruct " + name + "View {\n";
    std::set<std::string> members;
    for (const auto &field : type.fields) {
      try {
        if (field.base || field.bit_offset || field.bit_size || !field.byte_offset ||
            !identifier(field.name) || field.name == name + "View" ||
            !members.insert(field.name).second)
          throw std::invalid_argument("member has no supported constant byte offset");
        bool field_volatile = false;
        const auto &value = unqualified(field.type_id, field_volatile);
        const bool array = value.kind == "array";
        const auto &element = array ? unqualified(value.target_id, field_volatile) : value;
        const auto &storage = element.kind == "enum"
            ? unqualified(element.target_id, field_volatile) : element;
        if (field_volatile || storage.kind != "scalar" || !storage.size ||
            (element.kind == "enum" && element.size != storage.size) ||
            (array && (!value.count || !*value.count)) || *field.byte_offset > *type.size)
          throw std::invalid_argument("member type is unsupported");
        const auto count = array ? *value.count : 1;
        if ((*type.size - *field.byte_offset) / *storage.size < count)
          throw std::invalid_argument("member falls outside the record");
        const auto cpp = scalar(storage);
        if (array) views_ += "  template <unsigned long Index>\n";
        views_ += "  static " + cpp + " " + field.name + "(const ::" + type.name +
            "& object) noexcept {\n";
        if (array)
          views_ += "    static_assert(Index < " + std::to_string(count) + "UL);\n";
        views_ += "    " + cpp + " value;\n    __builtin_memcpy(&value, "
            "reinterpret_cast<const unsigned char*>(__builtin_addressof(object)) + " +
            std::to_string(*field.byte_offset) + "UL" + (array ? " + Index * sizeof(value)" : "") +
            ", sizeof(value));\n    return value;\n  }\n";
      } catch (const std::invalid_argument &error) {
        omitted_.push_back(type.id + "." + field.name + ": " + error.what());
      }
    }
    views_ += "};\n}\n";
  }

public:
  explicit Generator(const ModuleIndex &index) : index_(index) {}

  std::string signature(const ModuleSymbol &symbol) {
    if (symbol.kind != "function" || !symbol.has_debug || !symbol.diagnostic.empty())
      throw std::invalid_argument("unsupported_call_abi: " + symbol.selector + " " + symbol.diagnostic);
    auto result = spelling(symbol.type_id) + "(";
    for (std::size_t i = 0; i < symbol.parameters.size(); ++i) {
      if (i) result += ", ";
      const auto argument = spelling(symbol.parameters[i].type_id);
      if (argument == "void")
        throw std::invalid_argument("missing debug parameter type: " + symbol.parameters[i].name);
      result += argument;
    }
    return result + ")";
  }

  HeaderBundle generate(const ModuleSelection &selection, bool for_preparation) {
    if (selection.symbols.empty() && selection.types.empty())
      throw std::invalid_argument("select at least one type or symbol");
    llvm::json::Array types, symbols;
    std::string functions;
    for (const auto &id : std::set<std::string>(selection.types.begin(), selection.types.end())) {
      const auto type = index_.get_type(id);
      types.push_back(id);
      view(id, true);
    }
    for (const auto &id : std::set<std::string>(selection.symbols.begin(), selection.symbols.end())) {
      const auto symbol = index_.get_symbol(id);
      symbols.push_back(id);
      try {
        const auto prototype = signature(symbol);
        for (const auto &parameter : symbol.parameters) view(parameter.type_id, false);
        view(symbol.type_id, false);
        if (symbol.member) {
          omitted_.push_back(id + ": member declaration requires original class definition");
          continue;
        }
        if (!for_preparation) {
          auto [space, name] = scope(symbol.name);
          const auto paren = prototype.find('(');
          const std::string prefix = symbol.linkage == symbol.name ? "extern \"C\" " : "";
          functions += in_namespace(space, prefix + prototype.substr(0, paren) + " " + name +
                                    prototype.substr(paren) + ";");
        }
      } catch (const std::invalid_argument &error) {
        if (for_preparation) throw;
        omitted_.push_back(id + ": " + error.what());
      }
    }
    std::string forward_declarations;
    for (const auto &[name, text] : declarations_) forward_declarations += text;
    const auto declarations = "#pragma once\n" + forward_declarations + functions;
    const auto views = "#pragma once\n#include \"declarations.hpp\"\n" + views_;
    auto manifest = dump(llvm::json::Object{{"format", 1},
        {"module", json(index_.identity())}, {"types", std::move(types)},
        {"symbols", std::move(symbols)}, {"omitted", std::move(omitted_)},
        {"declarations_sha256", digest(declarations)}, {"views_sha256", digest(views)}});
    return {declarations, views, manifest + "\n", forward_declarations + views_};
  }
};
} // namespace

HeaderBundle headers(const ModuleIndex &index, const ModuleSelection &selection) {
  return Generator(index).generate(selection, false);
}

IndexedCpp indexed_cpp(const ModuleIndex &index, const ModuleSymbol &symbol) {
  Generator generator(index);
  auto signature = generator.signature(symbol);
  auto bundle = generator.generate({{symbol.id}, {}}, true);
  return {std::move(signature), std::move(bundle.preamble)};
}

} // namespace tracer::module_index

namespace tracer_module {

void ModuleIndex::emit_headers(const ModuleSelection &selection,
                               const std::filesystem::path &output) const {
  namespace fs = std::filesystem;
  const auto bundle = ::tracer::module_index::headers(*this, selection);
  if (!fs::create_directory(output))
    throw std::runtime_error("generated header output must be a new directory");
  try {
    ::tracer::module_index::write_file(output / "declarations.hpp", bundle.declarations);
    ::tracer::module_index::write_file(output / "views.hpp", bundle.views);
    ::tracer::module_index::write_file(output / "manifest.json", bundle.manifest);
  } catch (...) {
    fs::remove_all(output);
    throw;
  }
}
} // namespace tracer_module
