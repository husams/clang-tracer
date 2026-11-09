#include "module_index_internal.hpp"
#include "module_index_format.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/Program.h>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace tracer::module_index {
namespace {
namespace j = llvm::json;

std::string text(const j::Object &object, llvm::StringRef key) {
  return object.getString(key).value_or("").str();
}

std::optional<std::uint64_t> number(const j::Object &object, llvm::StringRef key) {
  if (const auto *value = object.get(key)) return value->getAsUINT64();
  return {};
}

j::Value optional(std::optional<std::uint64_t> value) {
  return value ? j::Value(*value) : j::Value(nullptr);
}

template <class F> auto decode(std::string_view data, F parse) {
  auto value = j::parse(llvm::StringRef(data));
  if (!value) throw std::runtime_error("invalid module index: " + llvm::toString(value.takeError()));
  const auto *object = value->getAsObject();
  if (!object) throw std::runtime_error("invalid module index record");
  return parse(*object);
}

ModuleIdentity identity(const j::Object &o) {
  ModuleIdentity result{text(o, "image"), text(o, "sha256"), text(o, "build_id"),
                        text(o, "debug_file"), text(o, "debug_sha256"), {}};
  if (const auto *diagnostics = o.getArray("diagnostics"))
    for (const auto &value : *diagnostics)
      if (auto message = value.getAsString()) result.diagnostics.push_back(message->str());
  return result;
}

ModuleType type(const j::Object &o) {
  ModuleType result;
  result.id = text(o, "id");
  result.name = text(o, "name");
  result.kind = text(o, "kind");
  result.target_id = text(o, "target_id");
  result.file = text(o, "file");
  result.line = number(o, "line").value_or(0);
  result.size = number(o, "size");
  result.alignment = number(o, "alignment");
  result.count = number(o, "count");
  result.encoding = number(o, "encoding");
  result.declaration = o.getBoolean("declaration").value_or(false);
  result.namespace_scope = o.getBoolean("namespace_scope").value_or(false);
  if (const auto *fields = o.getArray("fields"))
    for (const auto &value : *fields)
      if (const auto *f = value.getAsObject())
        result.fields.push_back({text(*f, "name"), text(*f, "type_id"),
            number(*f, "byte_offset"), number(*f, "bit_offset"),
            number(*f, "bit_size"), f->getBoolean("base").value_or(false)});
  return result;
}

ModuleSymbol symbol(const j::Object &o) {
  ModuleSymbol result;
  result.id = text(o, "id");
  result.name = text(o, "name");
  result.selector = text(o, "selector");
  result.linkage = text(o, "linkage");
  result.kind = text(o, "kind");
  result.version = text(o, "version");
  result.type_id = text(o, "type_id");
  result.file = text(o, "file");
  result.diagnostic = text(o, "diagnostic");
  result.line = number(o, "line").value_or(0);
  result.address = number(o, "address").value_or(0);
  result.size = number(o, "size").value_or(0);
  result.exported = o.getBoolean("exported").value_or(false);
  result.default_version = o.getBoolean("default_version").value_or(false);
  result.has_debug = o.getBoolean("has_debug").value_or(false);
  result.member = o.getBoolean("member").value_or(false);
  if (const auto *parameters = o.getArray("parameters"))
    for (const auto &value : *parameters)
      if (const auto *p = value.getAsObject())
        result.parameters.push_back({text(*p, "name"), text(*p, "type_id")});
  return result;
}

struct Close {
  void operator()(sqlite3 *db) const { sqlite3_close(db); }
  void operator()(sqlite3_stmt *stmt) const { sqlite3_finalize(stmt); }
};
using Database = std::unique_ptr<sqlite3, Close>;
using Statement = std::unique_ptr<sqlite3_stmt, Close>;

void check(int status, sqlite3 *db) {
  if (status != SQLITE_OK) throw std::runtime_error("module index: " + std::string(sqlite3_errmsg(db)));
}

Database connect(const fs::path &path, int flags = SQLITE_OPEN_READONLY) {
  sqlite3 *db = nullptr;
  const auto status = sqlite3_open_v2(path.string().c_str(), &db, flags, nullptr);
  Database result(db);
  if (!db) throw std::runtime_error("cannot allocate module index database");
  check(status, db);
  check(sqlite3_exec(db, "PRAGMA trusted_schema=OFF", nullptr, nullptr, nullptr), db);
  return result;
}

Statement statement(sqlite3 *db, const char *sql) {
  sqlite3_stmt *stmt = nullptr;
  check(sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr), db);
  return Statement(stmt);
}

void sql_bind(sqlite3_stmt *stmt, int index, std::string_view value) {
  check(sqlite3_bind_text64(stmt, index, value.empty() ? "" : value.data(), value.size(),
                            SQLITE_TRANSIENT, SQLITE_UTF8), sqlite3_db_handle(stmt));
}

void sql_bind(sqlite3_stmt *stmt, int index, std::int64_t value) {
  check(sqlite3_bind_int64(stmt, index, value), sqlite3_db_handle(stmt));
}

bool step(sqlite3_stmt *stmt) {
  const auto status = sqlite3_step(stmt);
  if (status == SQLITE_ROW) return true;
  if (status != SQLITE_DONE) check(status, sqlite3_db_handle(stmt));
  return false;
}

std::string column(sqlite3_stmt *stmt, int index) {
  const auto *data = sqlite3_column_text(stmt, index);
  return data ? std::string(reinterpret_cast<const char *>(data), sqlite3_column_bytes(stmt, index))
              : std::string{};
}

void insert(sqlite3_stmt *stmt) {
  step(stmt);
  check(sqlite3_reset(stmt), sqlite3_db_handle(stmt));
}

struct TemporaryFile {
  fs::path path;
  ~TemporaryFile() {
    std::error_code error;
    fs::remove(path, error);
  }
};

std::uint64_t parse_page(const std::string &page, std::string_view prefix) {
  if (page.empty()) return 0;
  if (page.compare(0, prefix.size(), prefix) != 0)
    throw std::invalid_argument("module index page token does not match this query");
  std::uint64_t row = 0;
  const auto end = page.data() + page.size();
  const auto parsed = std::from_chars(page.data() + prefix.size(), end, row);
  if (parsed.ec != std::errc() || parsed.ptr != end || row > INT64_MAX)
    throw std::invalid_argument("invalid module index page token");
  return row;
}

template <class Entry, class Parser>
ModulePage<Entry> page(const ModuleIndex &index, const ModuleQuery &query, const char *sql,
                       const char *kind, bool exports, Parser parse) {
  const auto prefix = digest(index.identity().sha256 + index.identity().debug_sha256 +
      kind + (exports ? "1" : "0") + query.filter) + ":";
  const auto row = parse_page(query.page, prefix);
  auto db = connect(index.path());
  auto stmt = statement(db.get(), sql);
  sql_bind(stmt.get(), 1, static_cast<std::int64_t>(row));
  sql_bind(stmt.get(), 2, query.filter);
  sql_bind(stmt.get(), 3, static_cast<std::int64_t>(exports));
  ModulePage<Entry> result;
  std::int64_t last = 0;
  while (step(stmt.get())) {
    if (query.limit && result.entries.size() == query.limit) {
      result.next_page = prefix + std::to_string(last);
      break;
    }
    last = sqlite3_column_int64(stmt.get(), 0);
    result.entries.push_back(decode(column(stmt.get(), 1), parse));
  }
  return result;
}

template <class Entry, class Parser>
Entry exact(const fs::path &path, const char *sql, const std::string &key, Parser parse,
            const char *kind, const std::string &normalized = {}) {
  auto db = connect(path);
  auto stmt = statement(db.get(), sql);
  sql_bind(stmt.get(), 1, key);
  if (sqlite3_bind_parameter_count(stmt.get()) == 2) sql_bind(stmt.get(), 2, normalized);
  if (!step(stmt.get())) throw std::invalid_argument(std::string("unknown ") + kind + ": " + key);
  auto result = decode(column(stmt.get(), 0), parse);
  if (step(stmt.get())) {
    std::string ids = result.id;
    do { ids += ", " + decode(column(stmt.get(), 0), parse).id; } while (step(stmt.get()));
    throw std::invalid_argument(std::string("ambiguous_") + kind + ": " + key + " (" + ids + ")");
  }
  return result;
}
} // namespace

llvm::json::Object json(const ModuleIdentity &x) {
  j::Array diagnostics;
  for (const auto &message : x.diagnostics) diagnostics.push_back(message);
  return j::Object{{"image", x.image}, {"sha256", x.sha256}, {"build_id", x.build_id},
          {"debug_file", x.debug_file}, {"debug_sha256", x.debug_sha256},
          {"diagnostics", std::move(diagnostics)}, {"abi", "linux-x86-64-sysv"}};
}

llvm::json::Object json(const ModuleType &x) {
  j::Array fields;
  for (const auto &f : x.fields)
    fields.emplace_back(j::Object{{"name", f.name}, {"type_id", f.type_id},
        {"byte_offset", optional(f.byte_offset)}, {"bit_offset", optional(f.bit_offset)},
        {"bit_size", optional(f.bit_size)}, {"base", f.base}});
  return j::Object{{"id", x.id}, {"name", x.name}, {"kind", x.kind}, {"target_id", x.target_id},
          {"file", x.file}, {"line", x.line}, {"size", optional(x.size)},
          {"alignment", optional(x.alignment)}, {"count", optional(x.count)},
          {"encoding", optional(x.encoding)}, {"declaration", x.declaration},
          {"namespace_scope", x.namespace_scope}, {"fields", std::move(fields)}};
}

llvm::json::Object json(const ModuleSymbol &x) {
  j::Array parameters;
  for (const auto &p : x.parameters)
    parameters.emplace_back(j::Object{{"name", p.name}, {"type_id", p.type_id}});
  return j::Object{{"id", x.id}, {"name", x.name}, {"selector", x.selector},
          {"linkage", x.linkage}, {"kind", x.kind}, {"version", x.version},
          {"type_id", x.type_id}, {"file", x.file}, {"line", x.line},
          {"diagnostic", x.diagnostic}, {"address", x.address}, {"size", x.size},
          {"exported", x.exported}, {"default_version", x.default_version},
          {"has_debug", x.has_debug}, {"member", x.member},
          {"parameters", std::move(parameters)}};
}

std::string dump(j::Value value) { return llvm::formatv("{0}", std::move(value)).str(); }

std::string format_json(const ModuleType &entry) { return dump(json(entry)); }
std::string format_json(const ModuleSymbol &entry) { return dump(json(entry)); }

template <class T> std::string format_page(const ModulePage<T> &page) {
  j::Array entries;
  for (const auto &entry : page.entries) entries.emplace_back(json(entry));
  return dump(j::Object{{"entries", std::move(entries)}, {"next_page", page.next_page}});
}

std::string format_json(const ModulePage<ModuleType> &page) { return format_page(page); }
std::string format_json(const ModulePage<ModuleSymbol> &page) { return format_page(page); }

std::string normalize_selector(std::string_view value) {
  std::string out;
  bool space = false;
  const auto identifier = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
  for (unsigned char c : value) {
    if (std::isspace(c)) { space = true; continue; }
    if (space && !out.empty() && identifier(out.back()) && identifier(c)) out += ' ';
    out += static_cast<char>(c);
    space = false;
  }
  if (out.compare(0, 2, "::") == 0) out.erase(0, 2);
  return out;
}

std::string read_file(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot read " + path.string());
  return {std::istreambuf_iterator<char>(input), {}};
}

void write_file(const fs::path &path, std::string_view value) {
  std::ofstream output(path, std::ios::binary);
  output.write(value.data(), value.size());
  output.close();
  if (!output) throw std::runtime_error("cannot write " + path.string());
}
} // namespace tracer::module_index

namespace tracer_module {
using namespace ::tracer::module_index;

ModuleSource ModuleSource::file(fs::path image, fs::path debug_file) {
  return {std::move(image), std::move(debug_file), {}};
}

ModuleIndex ModuleIndex::build(const ModuleSource &source, const fs::path &output,
                               const ModuleIndexOptions &options) {
  if (output.empty()) throw std::invalid_argument("module index output path is required");
  if (fs::exists(output) || fs::is_symlink(output))
    throw std::invalid_argument("module index output already exists");
  llvm::SmallString<128> temp_path;
  if (auto error = llvm::sys::fs::createUniqueFile(output.string() + ".%%%%%%", temp_path))
    throw std::system_error(error, "cannot create temporary module index");
  TemporaryFile temp{temp_path.str().str()};
  auto db = connect(temp.path, SQLITE_OPEN_READWRITE);
  check(sqlite3_exec(db.get(), R"sql(
    PRAGMA application_id=1129598281;
    PRAGMA user_version=1;
    BEGIN;
    CREATE TABLE metadata (data TEXT NOT NULL);
    CREATE TABLE types (id TEXT UNIQUE NOT NULL, name TEXT NOT NULL, definition INTEGER NOT NULL,
                        data TEXT NOT NULL);
    CREATE INDEX types_by_name ON types(name,definition);
    CREATE TABLE symbols (id TEXT UNIQUE NOT NULL, name TEXT NOT NULL, selector TEXT NOT NULL,
                          linkage TEXT NOT NULL, version TEXT NOT NULL, address INTEGER NOT NULL,
                          exported INTEGER NOT NULL, data TEXT NOT NULL);
    CREATE INDEX symbols_by_name ON symbols(name);
    CREATE INDEX symbols_by_selector ON symbols(selector);
    CREATE INDEX symbols_by_linkage ON symbols(linkage,version);
    CREATE INDEX symbols_by_address ON symbols(address);
  )sql", nullptr, nullptr, nullptr), db.get());
  {
    auto types = statement(db.get(), "INSERT INTO types VALUES (?1,?2,?3,?4)");
    auto symbols = statement(db.get(), "INSERT INTO symbols VALUES (?1,?2,?3,?4,?5,?6,?7,?8)");
    const auto put_type = [&](const ModuleType &entry) {
      sql_bind(types.get(), 1, entry.id);
      sql_bind(types.get(), 2, entry.name);
      sql_bind(types.get(), 3, static_cast<std::int64_t>(!entry.declaration));
      sql_bind(types.get(), 4, dump(json(entry)));
      insert(types.get());
    };
    ModuleType void_type;
    void_type.id = void_type.name = void_type.kind = "void";
    put_type(void_type);
    const auto metadata = scan(source, options, put_type, [&](const ModuleSymbol &entry) {
      sql_bind(symbols.get(), 1, entry.id);
      sql_bind(symbols.get(), 2, entry.name);
      sql_bind(symbols.get(), 3, normalize_selector(entry.selector));
      sql_bind(symbols.get(), 4, entry.linkage);
      sql_bind(symbols.get(), 5, entry.version);
      sql_bind(symbols.get(), 6, static_cast<std::int64_t>(entry.address));
      sql_bind(symbols.get(), 7, static_cast<std::int64_t>(entry.exported));
      sql_bind(symbols.get(), 8, dump(json(entry)));
      insert(symbols.get());
    });
    auto info = statement(db.get(), "INSERT INTO metadata VALUES (?1)");
    sql_bind(info.get(), 1, dump(json(metadata)));
    insert(info.get());
  }
  check(sqlite3_exec(db.get(), "COMMIT", nullptr, nullptr, nullptr), db.get());
  db.reset();
  fs::create_hard_link(temp.path, output);
  return open(output);
}

ModuleIndex ModuleIndex::open(const fs::path &path) {
  ModuleIndex result;
  result.path_ = fs::canonical(path);
  auto db = connect(result.path_);
  auto version = statement(db.get(), "SELECT application_id,user_version FROM pragma_application_id,pragma_user_version");
  if (!step(version.get()) || sqlite3_column_int(version.get(), 0) != 1129598281 ||
      sqlite3_column_int(version.get(), 1) != 1)
    throw std::invalid_argument("unsupported module index format");
  auto stmt = statement(db.get(), "SELECT data FROM metadata");
  if (!step(stmt.get())) throw std::invalid_argument("module index has no metadata");
  result.identity_ = decode(column(stmt.get(), 0), ::tracer::module_index::identity);
  if (result.identity_.image.empty() || result.identity_.sha256.size() != 64 ||
      result.identity_.debug_sha256.size() != 64)
    throw std::invalid_argument("module index metadata has no valid image identity");
  return result;
}

ModulePage<ModuleType> ModuleIndex::types(const ModuleQuery &query) const {
  return page<ModuleType>(*this, query,
      "SELECT rowid,data FROM types WHERE rowid>?1 AND name<>'' AND (?2='' OR name GLOB ?2)"
      " AND ?3=0 ORDER BY rowid", "type", false, type);
}

ModulePage<ModuleSymbol> ModuleIndex::symbols(const SymbolQuery &query) const {
  return page<ModuleSymbol>(*this, query,
      "SELECT rowid,data FROM symbols WHERE rowid>?1 AND (?2='' OR name GLOB ?2 OR selector GLOB ?2 OR linkage GLOB ?2)"
      " AND (?3=0 OR exported=1) ORDER BY rowid", "symbol", query.exports, symbol);
}

ModuleType ModuleIndex::find_type(const std::string &name) const {
  return exact<ModuleType>(path_,
      "SELECT data FROM types WHERE name=?1 AND definition=(SELECT MAX(definition) FROM types WHERE name=?1)",
      name, type, "type");
}

ModuleSymbol ModuleIndex::find_symbol(const std::string &name) const {
  return exact<ModuleSymbol>(path_,
      "SELECT data FROM symbols WHERE name=?1 OR linkage=?1 OR selector=?2 OR "
      "(version<>'' AND linkage||'@'||version=?1)",
      name, symbol, "symbol", normalize_selector(name));
}

ModuleType ModuleIndex::get_type(const std::string &id) const {
  return exact<ModuleType>(path_, "SELECT data FROM types WHERE id=?1", id, type, "type");
}

ModuleSymbol ModuleIndex::get_symbol(const std::string &id) const {
  return exact<ModuleSymbol>(path_, "SELECT data FROM symbols WHERE id=?1", id, symbol, "symbol");
}

std::filesystem::path ModuleIndex::prepare_probe(const std::string &symbol_id,
    const std::filesystem::path &module, const std::filesystem::path &output,
    const std::filesystem::path &generated, bool metadata_only) const {
  const auto symbol = get_symbol(symbol_id);
  std::string executable = "clang-tracer-prepare-injectors-native";
  if (const char *configured = std::getenv("TRACER_PREPARE_EXECUTABLE"); configured && *configured) {
    executable = configured;
  } else {
    std::error_code error;
    const auto self = fs::read_symlink("/proc/self/exe", error);
    if (!error && fs::is_regular_file(self.parent_path() / executable, error))
      executable = (self.parent_path() / executable).string();
  }
  auto program = llvm::sys::findProgramByName(executable);
  if (!program) throw std::system_error(program.getError(), "cannot find native probe preparer");
  const auto directory = fs::absolute(output);
  std::vector<std::string> command{*program, "--selector", symbol.selector, "--id", symbol.id,
      "--module-index", path_.string(), "--output", directory.string()};
  if (!module.empty()) command.insert(command.end(), {"--module", module.string()});
  if (!generated.empty()) command.insert(command.end(), {"--generated", generated.string()});
  if (metadata_only) command.push_back("--metadata-only");
  std::vector<llvm::StringRef> argv(command.begin(), command.end());
  if (llvm::sys::ExecuteAndWait(*program, argv) != 0)
    throw std::runtime_error("indexed probe preparation failed");
  return directory;
}

} // namespace tracer_module
