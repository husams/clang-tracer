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
  auto operator<=>(const Symbol &) const = default;
};

struct Image {
  std::string path;
  llvm::object::OwningBinary<llvm::object::Binary> binary;
  const Elf &elf;
  std::vector<Symbol> symbols;

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
    const auto append = [&](const auto &symbol) {
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
      if (*address && !name->empty() && !(*flags & llvm::object::SymbolRef::SF_Undefined))
        symbols.push_back({name->str(), *address, native.getSize(), type});
    };
    for (const auto &item : elf.symbols())
      append(item);
    for (const auto &item : elf.getDynamicSymbolIterators())
      append(item);
    std::sort(symbols.begin(), symbols.end());
    symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
  }

  static const Elf &as_elf(const llvm::object::Binary &binary) {
    const auto *object = llvm::dyn_cast<Elf>(&binary);
    if (!object || object->getArch() != llvm::Triple::x86_64)
      throw std::runtime_error("build manifest requires a Linux x86-64 ELF");
    return *object;
  }

  const Symbol *find(const std::string &name) const {
    const auto item = std::lower_bound(symbols.begin(), symbols.end(), name,
        [](const Symbol &symbol, const std::string &value) { return symbol.name < value; });
    if (item == symbols.end() || item->name != name)
      return nullptr;
    if (std::next(item) != symbols.end() && std::next(item)->name == name)
      throw std::runtime_error("ambiguous duplicate ELF symbol: " + name);
    return &*item;
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
  Definition definition;
  bool inconsistent = false;
};

void collect(std::map<std::string, Body> &all, const CompiledUnit &unit) {
  for (auto &[name, definition] : definitions(unit)) {
    const auto item = all.find(name);
    if (item == all.end())
      all.emplace(name, Body{std::move(definition)});
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
          llvm::toHex(llvm::object::getBuildID(&image.elf), true)};
}

bool thunk_for(const std::string &symbol, const std::string &demangled) {
  return symbol.starts_with("_ZT") && llvm::demangle(symbol).ends_with(" thunk to " + demangled);
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
  if (!registry.empty())
    result.entry = bind_native_entry(pid, module, registry, {}, false).address;
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
    if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".C") {
      result.jit.translation_units.push_back({path, directory, arguments});
    }
  }

  // Admit only requested references into the exact loaded image.
  // Link-time relocations stay target-native, including external data.
  for (const auto &unit : result.jit.translation_units) {
    const auto compiled = compile_unit(result.jit, unit);
    const auto add_import = [&](const auto &value, unsigned type) {
      if (!value.isDeclaration() || value.getName().empty())
        return;
      const auto name = value.getName().str();
      const auto *symbol = image.find(name);
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
  std::set<std::string> covered, referenced;
  for (const auto &[path, unit] : profiles) {
    if (cancelled && cancelled())
      throw Stale("live source compilation cancelled");
    auto after = compile_unit(current, unit);
    covered.insert(path);
    for (const auto &dependency : after.inputs) {
      if (within(sources.root, dependency) &&
          std::none_of(sources.current.begin(), sources.current.end(), [&](const auto &file) {
            return dependency == file.path;
          }))
        throw std::runtime_error("unrecorded project dependency: " + dependency);
      covered.insert(dependency);
    }
    const bool existing = sources.saved.count(path) || active.sources.count(path);
    if (existing) {
      auto before = compile_unit(baseline, unit);
      collect(old_bodies, before);
      covered.insert(before.inputs.begin(), before.inputs.end());
    }
    collect(new_bodies, after);
    // Link only affected units. All other references must bind to the exact
    // existing ELF symbol or the allowed target runtime.
    const bool changed = sources.submitted.count(path) ||
        std::any_of(after.inputs.begin(), after.inputs.end(),
                     [&](const auto &input) { return sources.submitted.count(input); });
    if (changed) {
      current.translation_units.push_back(unit);
      for (const auto &function : *after.module)
        if (function.hasName())
          referenced.insert(function.getName().str());
      for (const auto &global : after.module->globals())
        if (global.hasName())
          referenced.insert(global.getName().str());
      for (const auto &alias : after.module->aliases())
        referenced.insert(alias.getName().str());
    }
  }
  for (const auto &path : sources.submitted)
    if (!covered.count(path))
      throw std::runtime_error("no verified translation unit profile or dependency uses " + path);

  Plan result;
  result.jit = std::move(current);
  for (const auto &file : result.jit.files)
    if (sources.submitted.count(file.path))
      result.accepted_sources.push_back(file);
  std::set<std::string> replaced;
  std::set<std::uintptr_t> entries;
  for (const auto &[name, body] : old_bodies) {
    const auto next = new_bodies.find(name);
    const auto &old = body.definition;
    if (next != new_bodies.end() && old.implementation == next->second.definition.implementation &&
        old.abi == next->second.definition.abi && !next->second.inconsistent && !body.inconsistent)
      continue;
    const auto *symbol = image.find(name);
    if (!symbol) {
      if (!within(sources.root, old.path))
        continue;
      auto &candidate = result.candidates.emplace_back();
      candidate.set_symbol(name);
      candidate.set_signature(signature(name, old));
      candidate.set_source(site(old));
      candidate.set_module(module);
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic("rebuild required: changed definition has no standalone native entry");
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
      for (const auto &alias : image.symbols) {
        if (alias.name == name || alias.type != llvm::ELF::STT_FUNC)
          continue;
        if (alias.address == symbol->address ||
            (alias.address > symbol->address && alias.address - symbol->address < symbol->size))
          throw std::runtime_error("rebuild required: overlapping callable entry or folded alias " + alias.name);
        if (thunk_for(alias.name, llvm::demangle(name))) {
          const auto thunk = native_entry::scan(alias.address, image.code(alias));
          if (thunk.decode_failed || std::none_of(thunk.instructions.begin(), thunk.instructions.end(),
                    [&](const auto &instruction) { return instruction.target == symbol->address; }))
            throw std::runtime_error("rebuild required: standalone or unresolved thunk " + alias.name);
        }
      }
      auto binding = bind(pid, image, *symbol, old, active);
      if (!entries.insert(binding.address).second)
        throw std::runtime_error("rebuild required: multiple definitions name one native entry");
      candidate.set_address(binding.address);
      candidate.set_state(api::LiveUpdateStatus::PLANNED);
      result.entries.push_back(std::move(binding));
      replaced.insert(name);
    } catch (const std::exception &failure) {
      candidate.set_state(api::LiveUpdateStatus::BLOCKED);
      candidate.set_diagnostic(failure.what());
    }
  }

  const auto *main = image.find("main");
  if (!main)
    throw std::runtime_error("cannot verify the executable load bias without main");
  const auto bias = client::bind_live_executable(pid, module, image.elf, main->address, main->size);
  for (const auto &name : referenced) {
    if (replaced.count(name))
      continue;
    const auto *symbol = image.find(name);
    if (!symbol || !symbol->size ||
        (symbol->type != llvm::ELF::STT_FUNC && symbol->type != llvm::ELF::STT_OBJECT))
      continue;
    if (symbol->type == llvm::ELF::STT_FUNC) {
      const auto mapped_bias = client::bind_live_executable(pid, module, image.elf,
                                                            symbol->address, symbol->size);
      if (mapped_bias != bias)
        throw Stale("import moved across ELF instances: " + name);
    }
    result.jit.imports.emplace(name, bias + symbol->address);
  }
  for (const auto &entry : result.entries)
    result.jit.entry_symbols.push_back(entry.symbol);
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
