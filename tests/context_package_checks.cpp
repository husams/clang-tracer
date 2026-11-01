#include "context_package.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void write_source(const std::filesystem::path &path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << "struct IdenticalStateName { int value; };\n";
  require(static_cast<bool>(output), "could not create package fixture");
}
} // namespace

void verify_context_packages() {
  namespace fs = std::filesystem;
  using tracer::context_package::PackageCatalog;
  const auto root = fs::temp_directory_path() /
                    ("clang-tracer-package-" + std::to_string(::getpid()));
  std::error_code ignored;
  fs::remove_all(root, ignored);
  const auto first_package = root / "first-package";
  const auto second_package = root / "second-package";
  const auto nested_package = first_package / "nested-package";
  fs::create_directories(first_package / ".git");
  fs::create_directories(second_package / ".git");
  fs::create_directories(nested_package / ".git");
  const auto callback_a = first_package / "callbacks" / "one.cpp";
  const auto callback_b = first_package / "callbacks" / "two.cpp";
  const auto imported_state = first_package / "include" / "state.hpp";
  const auto unrelated_same_name = second_package / "state.hpp";
  const auto nested_source = nested_package / "state.hpp";
  write_source(callback_a);
  write_source(callback_b);
  write_source(imported_state);
  write_source(unrelated_same_name);
  write_source(nested_source);

  const auto first = PackageCatalog::admit_source(callback_a);
  const auto sibling = PackageCatalog::admit_source(callback_b);
  const auto owner = PackageCatalog::admit_source(imported_state);
  const auto independent = PackageCatalog::admit_source(unrelated_same_name);
  const auto nested = PackageCatalog::admit_source(nested_source);
  require(first.retained && sibling.retained && owner.retained &&
              independent.retained && nested.retained,
          "package admission did not persist engine metadata");
  require(first.id == sibling.id && sibling.id == owner.id,
          "callbacks and imported definitions in one owner package diverged");
  require(first.id != independent.id && first.id != nested.id,
          "identical C++ names in unrelated packages were merged");

  const auto common_git = root / "worktree-common" / ".git";
  const auto worktree_a = root / "worktree-a";
  const auto worktree_b = root / "worktree-b";
  for (const auto *name : {"a", "b"}) {
    const auto gitdir = common_git / "worktrees" / name;
    fs::create_directories(gitdir);
    std::ofstream(gitdir / "commondir") << "../..\n";
  }
  for (const auto &worktree : {worktree_a, worktree_b}) {
    fs::create_directories(worktree);
    const auto name = worktree == worktree_a ? "a" : "b";
    std::ofstream(worktree / ".git")
        << "gitdir: ../worktree-common/.git/worktrees/" << name << '\n';
    write_source(worktree / "src" / "callback.cpp");
  }
  const auto from_worktree_a =
      PackageCatalog::admit_source(worktree_a / "src" / "callback.cpp");
  const auto from_worktree_b =
      PackageCatalog::admit_source(worktree_b / "src" / "callback.cpp");
  require(from_worktree_a.retained && from_worktree_b.retained &&
              from_worktree_a.id == from_worktree_b.id,
          "Git worktrees did not reuse their engine-owned package lineage");

  const auto renamed = first_package / "callbacks" / "renamed.cpp";
  fs::rename(callback_a, renamed);
  const auto after_rename = PackageCatalog::admit_source(renamed);
  const auto after_reload = PackageCatalog::resolve_source(renamed);
  require(after_rename.retained && after_reload.retained &&
              after_rename.id == first.id && after_reload.id == first.id,
          "file rename or process-style reload changed retained lineage");

  const auto standalone = root / "unverifiable.cpp";
  const auto missing = PackageCatalog::resolve_source(standalone);
  const auto another_missing = PackageCatalog::resolve_source(
      root / "another-unverifiable.cpp");
  require(!missing.retained && !missing.diagnostic.empty() &&
              missing.id != another_missing.id,
          "missing lineage metadata was silently treated as retained");
  fs::remove_all(root, ignored);
}
