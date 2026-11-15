#include "live_plan.hpp"

#include "debug_resolver.hpp"
#include "live_compile.hpp"
#include "native_entry.hpp"
#include "tracer/protocol.hpp"

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <google/protobuf/text_format.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Object/BuildID.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>

namespace tracer::live {
namespace {
namespace api = clangtracer::v1;
namespace fs = std::filesystem;
using Elf = llvm::object::ELF64LEObjectFile;

std::string read_file(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("cannot read build or source input: " + path.string());
  return {std::istreambuf_iterator<char>(input), {}};
}

std::string digest(llvm::StringRef bytes) {
  return llvm::toHex(llvm::SHA256::hash(llvm::arrayRefFromStringRef(bytes)), true);
}

std::string site(const Definition &definition) {
  return definition.path + ':' + std::to_string(definition.line);
}

std::string signature(const std::string &symbol, const Definition &definition) {
  return definition.type.empty() ? llvm::demangle(symbol) : definition.type;
}

struct Symbol {
  std::string name;
  std::uint64_t address;
  std::uint64_t size;
  unsigned type;
  bool local;
  std::string file;
  auto operator<=>(const Symbol &) const = default;
};

struct Image {
  std::string path;
  llvm::object::OwningBinary<llvm::object::Binary> binary;
  const Elf &elf;
  std::vector<Symbol> symbols;
  std::map<std::string, unsigned> file_markers;

  static llvm::object::OwningBinary<llvm::object::Binary> open(const std::string &path) {
    auto image = llvm::object::createBinary(path);
    if (!image)
      throw std::runtime_error(llvm::toString(image.takeError()));
    return std::move(*image);
  }

  Image(const std::string &module, const std::string &sha)
      : path(module), binary(open(module)), elf(as_elf(*binary.getBinary())) {
    if ((!sha.empty() && digest(elf.getMemoryBufferRef().getBuffer()) != sha) ||
        llvm::object::getBuildID(&elf).empty())
      throw std::runtime_error("build manifest does not identify the ELF build");
    const auto append = [&](const auto &symbol, const std::string &file) {
      const llvm::object::ELFSymbolRef native(symbol);
      const auto type = native.getELFType();
      if (type != llvm::ELF::STT_FUNC && type != llvm::ELF::STT_OBJECT &&
          type != llvm::ELF::STT_TLS && type != llvm::ELF::STT_GNU_IFUNC)
        return;
      auto flags = symbol.getFlags();
      auto address = symbol.getAddress();
      auto name = symbol.getName();
      if (!flags || !address || !name) {
        if (!flags) llvm::consumeError(flags.takeError());
        if (!address) llvm::consumeError(address.takeError());
        if (!name) llvm::consumeError(name.takeError());
        throw std::runtime_error("cannot read complete ELF symbol inventory");
      }
      const auto local = native.getBinding() == llvm::ELF::STB_LOCAL;
      if (*address && !name->empty() && !(*flags & llvm::object::SymbolRef::SF_Undefined))
        symbols.push_back({name->str(), *address, native.getSize(), type, local,
                           local ? file : std::string{}});
    };
    std::string file;
    for (const auto &item : elf.symbols()) {
      if (llvm::object::ELFSymbolRef(item).getELFType() == llvm::ELF::STT_FILE) {
        auto name = item.getName();
        if (!name)
          throw std::runtime_error(llvm::toString(name.takeError()));
        file = fs::path(name->str()).filename().string();
        ++file_markers[file];
      } else {
        append(item, file);
      }
    }
    for (const auto &item : elf.getDynamicSymbolIterators())
      append(item, {});
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
  }

  static const Elf &as_elf(const llvm::object::Binary &binary) {
    const auto *object = llvm::dyn_cast<Elf>(&binary);
    if (!object || object->getArch() != llvm::Triple::x86_64)
      throw std::runtime_error("build manifest requires a Linux x86-64 ELF");
    return *object;
  }

  const Symbol *find(const std::string &name, std::optional<bool> local = std::nullopt) const {
    const auto item = std::lower_bound(symbols.begin(), symbols.end(), name,
        [](const Symbol &symbol, const std::string &value) { return symbol.name < value; });
    const Symbol *match = nullptr;
    for (auto it = item; it != symbols.end() && it->name == name; ++it) {
      if (local && it->local != *local)
        continue;
      if (match)
        throw std::runtime_error("ambiguous duplicate ELF symbol: " + name);
      match = &*it;
    }
    return match;
  }

  const Symbol *find_local(const std::string &name, const std::string &unit) const {
    const auto file = fs::path(unit).filename().string();
    const auto marker = file_markers.find(file);
    if (marker == file_markers.end() || marker->second != 1)
      return nullptr;
    const auto first = std::lower_bound(symbols.begin(), symbols.end(), name,
        [](const Symbol &symbol, const std::string &value) { return symbol.name < value; });
    const Symbol *match = nullptr;
    for (auto it = first; it != symbols.end() && it->name == name; ++it) {
      if (!it->local || it->file != file)
        continue;
      if (match)
        return nullptr;
      match = &*it;
    }
    return match;
  }

  std::string_view code(const Symbol &symbol) const {
    auto headers = elf.getELFFile().program_headers();
    if (!headers)
      throw std::runtime_error("cannot read ELF program headers: " + llvm::toString(headers.takeError()));
    for (const auto &segment : *headers) {
      if (segment.p_type != llvm::ELF::PT_LOAD || !(segment.p_flags & llvm::ELF::PF_X) ||
          symbol.address < segment.p_vaddr || symbol.address - segment.p_vaddr >= segment.p_filesz)
        continue;
      const auto offset = symbol.address - segment.p_vaddr;
      if (symbol.size <= segment.p_filesz - offset)
        return elf.getMemoryBufferRef().getBuffer().substr(segment.p_offset + offset, symbol.size);
    }
    throw std::runtime_error("symbol is outside verified executable file bytes: " + symbol.name);
  }
};

bool within(const fs::path &root, const fs::path &path) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && *relative.begin() != "..";
}

std::string resolve_path(const std::string &directory, const std::string &file) {
  if (!fs::path(directory).is_absolute())
    throw std::runtime_error("relative compilation directory: " + directory);
  return (fs::path(directory) / file).lexically_normal().string();
}

TranslationUnit profile(const clang::tooling::CompileCommand &command, const std::string &path) {
  const auto adjust = clang::tooling::combineAdjusters(
      clang::tooling::getClangStripOutputAdjuster(),
      clang::tooling::getClangStripDependencyFileAdjuster());
  auto arguments = adjust(command.CommandLine, command.Filename);
  if (arguments.empty())
    throw std::runtime_error("empty compile profile: " + path);
  const auto driver = fs::path(arguments.front()).filename().string();
  if (!driver.starts_with("clang++") && driver != "g++" && !driver.starts_with("g++-") &&
      !driver.ends_with("-linux-gnu-g++") && driver != "c++")
    throw std::runtime_error("unverified C++ compiler profile: " + driver);
  arguments.erase(arguments.begin());
  std::string optimization = "-O0";
  std::erase_if(arguments, [&](const std::string &argument) {
    if (argument.starts_with("-O"))
      optimization = argument;
    if (argument.starts_with("@") || argument.starts_with("-flto") ||
        argument.starts_with("-fplugin") || argument.starts_with("-fsanitize") ||
        argument.starts_with("-mabi") || argument.starts_with("-fabi-version") ||
        argument == "-Xclang" || argument == "-include-pch" || argument == "-ftime-trace" ||
        argument.starts_with("-fprofile"))
      throw std::runtime_error("unverified compile flag: " + argument);
    return argument == "-c" || argument == command.Filename ||
           (!argument.empty() && argument.front() != '-' &&
            resolve_path(command.Directory, argument) == path);
  });
  if (optimization != "-O0")
    throw std::runtime_error("optimized or folded callers require verified coverage and a rebuild");
  arguments.push_back("-fno-inline");
  return {path, command.Directory, std::move(arguments)};
}

struct Sources {
  fs::path root;
  std::vector<SourceFile> baseline;
  std::vector<SourceFile> current;
  std::map<std::string, std::string> saved;
  std::set<std::string> submitted;
};

Sources snapshot_inputs(const api::LiveBuildManifest &manifest,
                        const api::LiveUpdateRequest &request, const Revision &active) {
  Sources result;
  result.root = fs::canonical(manifest.source_root());
  const auto snapshot = fs::canonical(manifest.snapshot_root());
  auto add = [&](const std::string &path, std::string baseline) {
    if (result.saved.count(path))
      throw std::runtime_error("duplicate source snapshot input: " + path);
    const auto existing = active.sources.find(path);
    auto current = existing == active.sources.end() ? baseline : existing->second.contents;
    result.baseline.push_back({path, std::move(current)});
    result.saved.emplace(path, std::move(baseline));
  };
  for (const auto &input : manifest.inputs()) {
    const auto relative = fs::path(input.path()).lexically_normal();
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
      throw std::runtime_error("invalid path in build input inventory: " + input.path());
    auto original = read_file(snapshot / relative);
    if (digest(original) != input.sha256())
      throw std::runtime_error("build input checksum differs: " + input.path());
    add((result.root / relative).string(), std::move(original));
  }
  if (result.baseline.empty())
    throw std::runtime_error("build manifest has no original source inputs");
  std::map<std::string, std::string> overrides;
  for (const auto &source : request.sources()) {
    const auto path = fs::canonical(source.path()).string();
    if (!within(result.root, path) || !result.submitted.insert(path).second)
      throw std::runtime_error("invalid or duplicate submitted source: " + path);
    if (read_file(path) != source.contents())
      throw Stale("saved source changed after submission: " + path);
    overrides.emplace(path, source.contents());
  }
  for (const auto &file : result.baseline) {
    auto current = overrides.find(file.path);
    result.current.push_back(current == overrides.end() ? file
                             : SourceFile{file.path, current->second});
    if (current != overrides.end())
      overrides.erase(current);
  }
  for (const auto &[path, source] : active.sources) {
    if (result.saved.count(path))
      continue;
    result.baseline.push_back(source);
    auto current = overrides.find(path);
    result.current.push_back(current == overrides.end() ? source
                             : SourceFile{path, current->second});
    if (current != overrides.end())
      overrides.erase(current);
  }
  for (auto &[path, contents] : overrides)
    result.current.push_back({path, std::move(contents)});
  return result;
}

struct Body {
  std::string name;
  std::string unit;
  Definition definition;
  bool inconsistent = false;
};

void collect(std::map<std::string, Body> &all, const std::string &path,
             std::map<std::string, Definition> definitions, std::set<std::string> *affected = nullptr) {
  for (auto &[name, definition] : definitions) {
    const auto key = definition.local ? local_import_name(path, name) : name;
    if (affected)
      affected->insert(key);
    const auto item = all.find(key);
    if (item == all.end())
      all.emplace(key, Body{name, path, std::move(definition)});
    else if (item->second.definition.implementation != definition.implementation ||
             item->second.definition.abi != definition.abi)
      item->second.inconsistent = true;
  }
}

Entry bind(std::uint32_t pid, const Image &image, const Symbol &symbol,
           const Definition &definition, const Revision &active) {
  if (symbol.type != llvm::ELF::STT_FUNC || !symbol.size)
    throw std::runtime_error("requires an exact callable ELF symbol with a defined extent");
  const auto bias = client::bind_live_executable(pid, image.path, image.elf,
                                                 symbol.address, symbol.size);
  const auto address = bias + symbol.address;
  const auto prior = active.entries.find(address);
  if (prior != active.entries.end())
    return prior->second.binding;
  const auto code = image.code(symbol);
  const auto prefix = native_entry::nop_prefix(address, code.substr(0, kMaxPatchBytes), 14);
  if (!prefix || prefix >= code.size())
    throw std::runtime_error("rebuild required: entry needs a reserved 14-byte NOP overwrite window");
  return {symbol.name, signature(symbol.name, definition), site(definition), image.path,
          symbol.address, address, symbol.size, std::string(code.substr(0, prefix)),
          llvm::toHex(llvm::object::getBuildID(&image.elf), true),
          native_entry::may_return_at_end(address, code)};
}

bool thunk_for(const std::string &symbol, const std::string &demangled) {
  return symbol.starts_with("_ZT") && llvm::demangle(symbol).ends_with(" thunk to " + demangled);
}

bool forwards_to_body(const Image &image, const Symbol &thunk, const Symbol &body,
                      const std::string &callable, std::set<std::uint64_t> &chain) {
  using native_entry::Flow;
  if (!chain.insert(thunk.address).second)
    return false;
  const auto decoded = native_entry::scan(thunk.address, image.code(thunk));
  if (decoded.decode_failed || decoded.instructions.empty())
    return false;
  const auto forwards = [&](std::uint64_t address) {
    if (address == body.address)
      return true;
    const auto next = std::ranges::find_if(image.symbols, [&](const Symbol &symbol) {
      return symbol.address == address && symbol.size && symbol.type == llvm::ELF::STT_FUNC &&
             thunk_for(symbol.name, callable);
    });
    if (next == image.symbols.end())
      return false;
    auto path = chain;
    return forwards_to_body(image, *next, body, callable, path);
  };

  // Every returning path must call the body or a verified forwarding
  // thunk. An external unconditional jump may also forward the call.
  std::set<std::pair<std::uint64_t, bool>> pending{{thunk.address, false}};
  bool reached_body = false;
  while (!pending.empty()) {
    auto [pc, forwarded] = *pending.begin();
    pending.erase(pending.begin());
    const auto current = std::ranges::lower_bound(decoded.instructions, pc, {},
                                                   &native_entry::Instruction::address);
    if (current == decoded.instructions.end() || current->address != pc)
      return false;
    if (current->flow == Flow::returns) {
      if (!forwarded)
        return false;
      continue;
    }
    if (current->flow == Flow::call) {
      if (!current->target || !forwards(*current->target))
        return false;
      reached_body = forwarded = true;
    } else if (current->flow == Flow::branch) {
      if (!current->target)
        return false;
      const auto destination = *current->target;
      if (destination >= thunk.address && destination - thunk.address < thunk.size) {
        if (destination <= pc)
          return false;
        pending.emplace(destination, forwarded);
      } else {
        if (current->falls_through || !forwards(destination))
          return false;
        reached_body = true;
        continue;
      }
    }
    if (current->falls_through)
      pending.emplace(pc + current->size, forwarded);
    else if (current->flow != Flow::branch)
      return false;
  }
  return reached_body;
}
} // namespace

Entry bind_native_entry(std::uint32_t pid, const std::string &module,
                        const std::string &linkage, const Revision &active, bool overwrite) {
  const Image image(module, {});
  const auto *symbol = image.find(linkage);
  if (!symbol)
    throw std::runtime_error("missing exact native entry: " + linkage);
  if (!overwrite) {
    if (symbol->type != llvm::ELF::STT_FUNC || !symbol->size)
      throw std::runtime_error("helper does not have an exact native code range: " + linkage);
    const auto bias = client::bind_live_executable(pid, module, image.elf,
                                                   symbol->address, symbol->size);
    return {linkage, llvm::demangle(linkage), {}, module, symbol->address,
            bias + symbol->address, symbol->size, {},
            llvm::toHex(llvm::object::getBuildID(&image.elf), true)};
  }
  for (const auto &alias : image.symbols) {
    if (alias.name != linkage && alias.type == llvm::ELF::STT_FUNC &&
        (alias.address == symbol->address || thunk_for(alias.name, llvm::demangle(linkage))))
      throw std::runtime_error("rebuild required: native alias or thunk needs coverage proof: " + alias.name);
  }
  return bind(pid, image, *symbol, {}, active);
}

std::string live_build_module(const std::string &manifest_path) {
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(read_file(manifest_path), &manifest) ||
      manifest.module().empty())
    throw std::runtime_error("live build manifest does not identify a module");
  return fs::canonical(manifest.module()).string();
}

BuildInputs open_native_build(const std::string &executable,
                             const api::LiveUpdateRequest &request) {
  const auto manifest_path = request.build_manifest();
  const auto database_path = request.compilation_database();
  const auto manifest_contents = read_file(manifest_path);
  const auto database_contents = read_file(database_path);
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(manifest_contents, &manifest) ||
      manifest.module_sha256().size() != 64 ||
      digest(database_contents) != manifest.compilation_database_sha256())
    throw std::runtime_error("invalid or stale live build manifest or compile database");
  const auto module = fs::canonical(manifest.module()).string();
  if (module != executable)
    throw std::runtime_error("build manifest does not identify the selected executable");
  const Image image(module, manifest.module_sha256());
  std::string error;
  const auto database = clang::tooling::JSONCompilationDatabase::loadFromBuffer(
      database_contents, error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!database)
    throw std::runtime_error("cannot read compilation database: " + error);

  const auto sources = snapshot_inputs(manifest, {}, {});
  BuildInputs result;
  result.module = module;
  result.source_root = sources.root.string();
  result.baseline.files = sources.baseline;
  std::set<std::string> seen;
  for (const auto &command : database->getAllCompileCommands()) {
    const auto path = resolve_path(command.Directory, command.Filename);
    if (!sources.saved.count(path))
      continue;
    if (!seen.insert(path).second)
      throw std::runtime_error("ambiguous compilation profiles: " + path);
    result.baseline.translation_units.push_back(profile(command, path));
  }
  if (result.baseline.translation_units.empty())
    throw std::runtime_error("no build-bound compilation profiles for the source inventory");
  result.revalidate = [manifest_path, manifest_contents, database_path, database_contents,
                       module, sha = manifest.module_sha256()] {
    if (read_file(manifest_path) != manifest_contents ||
        read_file(database_path) != database_contents || digest(read_file(module)) != sha)
      throw Stale("ELF, compile profile or build manifest changed after staging");
  };
  return result;
}

ExtensionSources extension_sources(std::uint32_t pid, const std::string &module,
    const api::LiveUpdateRequest &request, const std::vector<std::string> &paths,
    const std::string &registry) {
  ExtensionSources result;
  const auto build = paths.empty() ? BuildInputs{} : open_native_build(module, request);
  // bind_live_executable checks the actual loaded build ID, not just the
  // pathname. For source execution the selected executable provides main.
  const Image image(module, {});
  const Symbol *anchor = image.find("main");
  if (!anchor) {
    // Advisory scans also accept shared libraries without main.
    const auto found = std::find_if(image.symbols.begin(), image.symbols.end(),
        [](const Symbol &symbol) { return symbol.type == llvm::ELF::STT_FUNC && symbol.size; });
    if (found != image.symbols.end())
      anchor = &*found;
  }
  if (!anchor)
    throw std::runtime_error("no exact ELF code range for live object identity");
  const auto bias = client::bind_live_executable(pid, module, image.elf, anchor->address, anchor->size);
  result.build_id = llvm::toHex(llvm::object::getBuildID(&image.elf), true);
  if (!registry.empty()) {
    try {
      result.entry = bind_native_entry(pid, module, registry, {}, false).address;
    } catch (const std::exception &error) {
      throw std::runtime_error("rebuild required: managed registry entry could not be bound: " +
                               std::string(error.what()));
    }
  }
  if (paths.empty())
    return result;
  result.jit = build.baseline;
  result.jit.translation_units.clear();

  auto arguments = build.baseline.translation_units.front().compiler_arguments;
  const auto directory = build.baseline.translation_units.front().directory;
  // A callback outside the compilation database may use the target's
  // common profile. Reject conflicting ABI options instead of guessing.
  for (const auto &unit : build.baseline.translation_units)
    if (unit.directory != directory || unit.compiler_arguments != arguments)
      throw std::runtime_error("live callbacks require one unambiguous recorded target compile profile");
  const auto sdk = fs::path(TRACER_LIVE_SDK_DIR);
  const auto sdk_dir = fs::exists(sdk / "clang_tracer/live.hpp")
      ? sdk.string() : TRACER_LIVE_BUILD_SDK_DIR;
  arguments.insert(arguments.end(), {"-I", sdk_dir});
  result.jit.compiler_arguments = arguments;

  std::map<std::string, std::string> submitted;
  for (const auto &path : paths) {
    auto content = read_file(path);
    const auto original = std::find_if(request.sources().begin(), request.sources().end(),
        [&](const auto &item) { return item.path() == path; });
    if (original != request.sources().end() && original->contents() != content)
      throw Stale("saved source changed after submission: " + path);
    if (!submitted.emplace(path, content).second)
      throw std::runtime_error("duplicate live source path: " + path);
    auto snapshot = std::find_if(result.jit.files.begin(), result.jit.files.end(),
        [&](const SourceFile &file) { return file.path == path; });
    if (snapshot == result.jit.files.end())
      result.jit.files.push_back({path, std::move(content)});
    else
      snapshot->contents = std::move(content);
    const auto extension = fs::path(path).extension();
    if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".C")
      result.jit.translation_units.push_back({path, directory, arguments});
  }

  // Admit only requested references into the exact loaded image.
  // Link-time relocations stay target-native, including external data.
  for (const auto &unit : result.jit.translation_units) {
    const auto compiled = compile_unit(result.jit, unit);
    const auto add_import = [&](const auto &value, unsigned type) {
      if (!value.isDeclaration() || value.getName().empty())
        return;
      const auto name = value.getName().str();
      const auto *symbol = image.find(name, false);
      if (symbol && symbol->type == type && symbol->address <= UINTPTR_MAX - bias)
        result.jit.imports.emplace(name, bias + symbol->address);
    };
    for (const auto &function : *compiled.module)
      add_import(function, llvm::ELF::STT_FUNC);
    for (const auto &global : compiled.module->globals())
      if (!global.isThreadLocal())
        add_import(global, llvm::ELF::STT_OBJECT);
    for (const auto &dependency : compiled.inputs) {
      if (within(build.source_root, dependency) &&
          std::none_of(result.jit.files.begin(), result.jit.files.end(), [&](const auto &file) {
            return file.path == dependency;
          }))
        throw std::runtime_error("unrecorded project dependency: " + dependency);
    }
  }
  result.validate = [submitted = std::move(submitted), validate_build = build.revalidate] {
    validate_build();
    for (const auto &[path, contents] : submitted)
      if (read_file(path) != contents)
        throw Stale("newer source save invalidated the staged snapshot: " + path);
  };
  return result;
}

void validate_entry(std::uint32_t pid, const Entry &entry) {
  auto binary = Image::open(entry.module);
  const auto &elf = Image::as_elf(*binary.getBinary());
  if (llvm::toHex(llvm::object::getBuildID(&elf), true) != entry.build_id)
    throw Stale("native ELF build ID changed: " + entry.symbol);
  const auto bias = client::bind_live_executable(pid, entry.module, elf,
                                                  entry.elf_address, entry.function_size);
  if (bias + entry.elf_address != entry.address)
    throw Stale("native image address changed: " + entry.symbol);
}

Plan plan_sources(std::uint32_t pid, const std::string &executable,
                  const api::LiveUpdateRequest &request, const Revision &active,
                  const std::function<bool()> &cancelled) {
  if (request.sources().empty())
    throw std::runtime_error("submit at least one source or header");
  const auto manifest_path = request.build_manifest();
  const auto manifest_contents = read_file(manifest_path);
  api::LiveBuildManifest manifest;
  if (!google::protobuf::TextFormat::ParseFromString(manifest_contents, &manifest))
    throw std::runtime_error("invalid build input manifest: " + manifest_path);
  if (manifest.source_root().empty() || manifest.snapshot_root().empty() ||
      manifest.module().empty() || manifest.module_sha256().size() != 64)
    throw std::runtime_error("build manifest requires source root, snapshot root and module");
  const auto module = fs::canonical(manifest.module()).string();
  if (module != executable)
    throw std::runtime_error("initial live profile requires the executable's exact ELF build");
  Image image(module, manifest.module_sha256());
  const auto database_path = request.compilation_database();
  const auto database_contents = read_file(database_path);
  if (digest(database_contents) != manifest.compilation_database_sha256())
    throw std::runtime_error("compile database differs from the build manifest");
  std::string error;
  auto database = clang::tooling::JSONCompilationDatabase::loadFromBuffer(
      database_contents, error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!database)
    throw std::runtime_error("cannot read compilation database: " + error);

  auto sources = snapshot_inputs(manifest, request, active);
  const auto commands = database->getAllCompileCommands();
  std::map<std::string, TranslationUnit> profiles;
  for (const auto &command : commands) {
    const auto path = resolve_path(command.Directory, command.Filename);
    if (!sources.saved.count(path) && !sources.submitted.count(path) &&
        !active.sources.count(path))
      continue;
    if (!profiles.emplace(path, profile(command, path)).second)
      throw std::runtime_error("ambiguous compilation profiles: " + path);
  }
  if (profiles.empty())
    throw std::runtime_error("no build-bound compilation profiles for the source inventory");

  JitRequest current;
  current.files = sources.current;
  JitRequest baseline;
  baseline.files = sources.baseline;
  std::map<std::string, Body> old_bodies, new_bodies;
  std::set<std::string> covered, affected;
  std::map<std::string, std::set<std::string>> native_locals;
  struct Reference {
    std::string name;
    std::string unit;
    bool local;
    bool has_storage_definition;
    std::optional<std::uint64_t> size;
  };
  std::multimap<std::string, Reference> referenced;
  Plan result;
  const auto block = [&](const std::string &path, const std::string &name, std::string diagnostic) {
    auto &candidate = result.candidates.emplace_back();
    candidate.set_state(api::LiveUpdateStatus::BLOCKED);
    candidate.set_symbol(name);
    candidate.set_source(path);
    candidate.set_module(module);
    candidate.set_diagnostic(std::move(diagnostic));
  };
  for (const auto &[path, unit] : profiles) {
    if (cancelled && cancelled())
      throw Stale("live source compilation cancelled");
    const auto compile_snapshot = [&](const JitRequest &snapshot) {
      return compile_unit(snapshot, unit, nullptr, {}, CompileInspection::native_metadata);
    };
    std::optional<CompiledUnit> before;
    if (sources.saved.count(path) || active.sources.count(path))
      before.emplace(compile_snapshot(baseline));
    // Only submitted inputs differ between the snapshots. Reuse the first
    // compilation when neither the TU nor any of its includes was submitted.
    const bool changed = !before || sources.submitted.count(path) ||
        std::ranges::any_of(before->inputs,
            [&](const auto &input) { return sources.submitted.count(input); });
    std::optional<CompiledUnit> updated;
    if (changed)
      updated.emplace(compile_snapshot(current));
    const auto &after = updated ? *updated : *before;
    covered.insert(path);
    for (const auto &dependency : after.inputs) {
      if (within(sources.root, dependency) &&
          std::none_of(sources.current.begin(), sources.current.end(), [&](const auto &file) {
            return dependency == file.path;
          }))
        throw std::runtime_error("unrecorded project dependency: " + dependency);
      covered.insert(dependency);
    }
    auto after_definitions = definitions(after);
    if (before) {
      if (sources.saved.count(path))
        for (const auto &value : before->module->global_values())
          if (value.hasName() && value.hasLocalLinkage())
            native_locals[value.getName().str()].insert(path);
      if (changed) {
        for (const auto &[name, layout] : before->record_layouts) {
          const auto updated = after.record_layouts.find(name);
          if (updated != after.record_layouts.end() && updated->second != layout)
            block(path, name, "rebuild required: existing record layout changed");
        }
        const auto old_data = data_definitions(*before);
        const auto new_data = data_definitions(after);
        for (const auto &[name, storage] : old_data) {
          const auto updated = new_data.find(name);
          if (updated == new_data.end() || updated->second != storage)
            block(path, name, "rebuild required: existing data definition changed");
        }
        for (const auto &[name, storage] : new_data) {
          (void)storage;
          if (!old_data.contains(name))
            block(path, name, "rebuild required: new persistent data definition");
        }
      }
      collect(old_bodies, path, changed ? definitions(*before) : after_definitions,
              changed ? &affected : nullptr);
      covered.insert(before->inputs.begin(), before->inputs.end());
    }
    collect(new_bodies, path, std::move(after_definitions));
    // A previous revision may have supplied a TU with no ELF counterpart.
    // Relink its accepted snapshot so new callers can resolve those helpers.
    if (changed || !sources.saved.count(path)) {
      current.translation_units.push_back(unit);
      current.translation_units.back().reuse_stream_initializer =
          sources.saved.count(path) && before && has_standard_stream_initializer(*before->module);
      for (const auto &value : after.module->global_values()) {
        if (!value.hasName())
          continue;
        const auto *global = llvm::dyn_cast<llvm::GlobalVariable>(&value);
        if (global && mergeable_literal(*global))
          continue;
        const auto name = value.getName().str();
        const auto import = value.hasLocalLinkage() ? local_import_name(path, name) : name;
        std::optional<std::uint64_t> size;
        if (global && global->getValueType()->isSized())
          size = after.module->getDataLayout().getTypeAllocSize(global->getValueType()).getFixedValue();
        referenced.emplace(import, Reference{name, path, value.hasLocalLinkage(),
                                             global && global->hasInitializer(), size});
      }
    }
  }
  for (const auto &path : sources.submitted)
    if (!covered.count(path))
      throw std::runtime_error("no verified translation unit profile or dependency uses " + path);

  result.jit = std::move(current);
  for (const auto &file : result.jit.files)
    if (sources.submitted.count(file.path))
      result.accepted_sources.push_back(file);
  std::set<std::string> replaced;
  std::map<std::uintptr_t, std::string> entries;
  const auto physical_body = [](const std::string &name, const auto &definitions) {
    const auto item = definitions.find(name);
    if (item == definitions.end())
      return std::string{};
    return item->second.definition.alias_target.empty()
        ? name : item->second.definition.alias_target;
  };
  for (const auto &[key, body] : old_bodies) {
    if (!affected.contains(key))
      continue;
    const auto &name = body.name;
    const auto next = new_bodies.find(key);
    const auto &old = body.definition;
    if (next != new_bodies.end() && old.implementation == next->second.definition.implementation &&
        old.abi == next->second.definition.abi && !next->second.inconsistent && !body.inconsistent)
      continue;
    const auto *symbol = old.local ? image.find_local(name, body.unit) : image.find(name, false);
    if (!symbol) {
      if (!within(sources.root, old.path))
        continue;
      auto &candidate = result.candidates.emplace_back();
      candidate.set_symbol(name);
      candidate.set_signature(signature(name, old));
      candidate.set_source(site(old));
      candidate.set_module(module);
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic(old.local
          ? "rebuild required: local definition has no exact ELF translation-unit entry"
          : "rebuild required: changed definition has no standalone native entry");
      continue;
    }
    auto &candidate = result.candidates.emplace_back();
    candidate.set_symbol(name);
    candidate.set_signature(signature(name, old));
    candidate.set_source(site(next == new_bodies.end() ? old : next->second.definition));
    candidate.set_module(module);
    try {
      if (!old.has_source_abi || !old.supported_abi || next == new_bodies.end() || body.inconsistent || next->second.inconsistent ||
          old.abi != next->second.definition.abi || !next->second.definition.supported_abi)
        throw std::runtime_error("rebuild required: emitted implementations or callable ABI differ");
      const auto callable = llvm::demangle(name);
      for (const auto &alias : image.symbols) {
        if (&alias == symbol || alias.type != llvm::ELF::STT_FUNC)
          continue;
        if (alias.address == symbol->address) {
          const auto prior = physical_body(name, old_bodies);
          const auto updated = physical_body(name, new_bodies);
          if (alias.size != symbol->size || prior.empty() || updated.empty() ||
              physical_body(alias.name, old_bodies) != prior ||
              physical_body(alias.name, new_bodies) != updated)
            throw std::runtime_error("rebuild required: unverified folded alias " + alias.name);
          continue;
        }
        if (alias.address > symbol->address && alias.address - symbol->address < symbol->size)
          throw std::runtime_error("rebuild required: overlapping callable entry " + alias.name);
        if (thunk_for(alias.name, callable)) {
          std::set<std::uint64_t> chain;
          if (forwards_to_body(image, alias, *symbol, callable, chain))
            continue;
          const auto previous_thunk = old_bodies.find(alias.name);
          const auto updated_thunk = new_bodies.find(alias.name);
          if (previous_thunk == old_bodies.end() || updated_thunk == new_bodies.end() ||
              !previous_thunk->second.definition.has_source_abi ||
              previous_thunk->second.definition.abi != updated_thunk->second.definition.abi ||
              previous_thunk->second.definition.implementation ==
                  updated_thunk->second.definition.implementation)
            throw std::runtime_error("rebuild required: standalone thunk needs an emitted replacement " +
                                     alias.name);
        }
      }
      if (old.local && (!old.alias_target.empty() || !next->second.definition.alias_target.empty()))
        throw std::runtime_error("rebuild required: local lifecycle alias needs a verified native entry");
      const auto canonical = physical_body(key, new_bodies);
      auto binding = bind(pid, image, *symbol, old, active);
      candidate.set_address(binding.address);
      candidate.set_state(api::LiveUpdateStatus::PLANNED);
      if (entries.emplace(binding.address, canonical).second) {
        binding.symbol = old.local ? name : canonical;
        result.entries.push_back(std::move(binding));
      } else if (entries.at(binding.address) != canonical) {
        throw std::runtime_error("rebuild required: multiple definitions name one native entry");
      }
      replaced.insert(key);
      replaced.insert(canonical);
    } catch (const std::exception &failure) {
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic(failure.what());
    }
  }

  const auto *main = image.find("main");
  if (!main)
    throw std::runtime_error("cannot verify the executable load bias without main");
  const auto bias = client::bind_live_executable(pid, module, image.elf, main->address, main->size);
  for (const auto &[import, ref] : referenced) {
    const auto &name = ref.name;
    const bool local = ref.local;
    if (replaced.count(import))
      continue;
    if (local) {
      const auto prior = native_locals.find(name);
      if (prior == native_locals.end() || !prior->second.contains(ref.unit))
        continue;
    }
    const auto *symbol = local ? image.find_local(name, ref.unit) : image.find(name, false);
    if (local && !symbol && ref.size) {
      block(ref.unit, name, "rebuild required: no exact ELF translation-unit storage");
      continue;
    }
    if (!symbol || !symbol->size ||
        (symbol->type != llvm::ELF::STT_FUNC && symbol->type != llvm::ELF::STT_OBJECT))
      continue;
    if (symbol->type == llvm::ELF::STT_FUNC) {
      const auto mapped_bias = client::bind_live_executable(pid, module, image.elf,
                                                            symbol->address, symbol->size);
      if (mapped_bias != bias)
        throw Stale("import moved across ELF instances: " + name);
    }
    // Some external IR declarations, including RTTI vtables, describe a
    // prefix or placeholder for larger native storage.
    if (symbol->type == llvm::ELF::STT_OBJECT && ref.size &&
        (*ref.size > symbol->size || (ref.has_storage_definition && *ref.size != symbol->size))) {
      block(ref.unit, name, "rebuild required: imported storage size differs from the ELF");
      continue;
    }
    result.jit.imports.emplace(import, bias + symbol->address);
  }
  for (const auto &entry : result.entries)
    result.jit.entry_symbols.push_back(entries.at(entry.address));
  if (result.entries.size() > 1)
    result.safe_point = SafePoint::drained_stack;

  result.validate_sources = [files = std::move(sources.submitted),
      snapshots = result.jit.files, module, sha = manifest.module_sha256(),
      database_path, database_contents, manifest_path, manifest_contents] {
    if (read_file(database_path) != database_contents || read_file(manifest_path) != manifest_contents ||
        digest(read_file(module)) != sha)
      throw Stale("ELF, compile profile or build manifest changed after staging");
    for (const auto &file : snapshots)
      if (files.count(file.path) && read_file(file.path) != file.contents)
        throw Stale("newer source save invalidated the staged snapshot: " + file.path);
  };
  return result;
}
} // namespace tracer::live
