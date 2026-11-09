#include <iostream>
#include <utility>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>

namespace {

struct Plugin {
  llvm::orc::JITDylib &scope;
  unsigned *counter;
  unsigned (*tick)();
};

Plugin load_plugin(llvm::orc::LLJIT &jit, const char *scope_name,
                   const char *object_path, const llvm::ExitOnError &check) {
  // A bare scope has no platform setup or shared symbol search order.
  auto &scope = jit.getExecutionSession().createBareJITDylib(scope_name);
  auto object = check(llvm::errorOrToExpected(
      llvm::MemoryBuffer::getFile(object_path, /*IsText=*/false)));
  check(jit.addObjectFile(scope, std::move(object)));
  auto counter = check(jit.lookup(scope, "plugin_counter")).toPtr<unsigned *>();
  auto tick = check(jit.lookup(scope, "tick")).toPtr<unsigned (*)()>();
  return {scope, counter, tick};
}

void show(const char *generation, const Plugin &plugin) {
  std::cout << generation
            << "  address=" << static_cast<const void *>(plugin.counter)
            << "  value=" << *plugin.counter << '\n';
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " plugin_a.o plugin_b.o\n";
    return 1;
  }
  if (llvm::InitializeNativeTarget() ||
      llvm::InitializeNativeTargetAsmPrinter()) {
    std::cerr << "Could not initialize LLVM for the native target\n";
    return 1;
  }

  llvm::ExitOnError check("JIT plugin demo: ");
  auto jit = check(llvm::orc::LLJITBuilder().create());
  auto a = load_plugin(*jit, "A.1", argv[1], check);
  auto b = load_plugin(*jit, "B.1", argv[2], check);
  show("A generation 1", a);
  show("B generation 1", b);
  if (a.counter == b.counter || *a.counter != 0 || *b.counter != 0) {
    std::cerr << "Expected two separate, zero-initialized globals\n";
    return 1;
  }

  if (a.tick() != 1 || b.tick() != 10 || a.tick() != 2 || b.tick() != 20) {
    std::cerr << "A and B did not advance independently\n";
    return 1;
  }
  show("A before reload", a);
  show("B before reload", b);

  // Stage A's replacement while both original scopes are still alive.
  auto new_a = load_plugin(*jit, "A.2", argv[1], check);
  if (new_a.counter == a.counter || new_a.counter == b.counter ||
      *new_a.counter != 0 || *a.counter != 2) {
    std::cerr << "Expected fresh storage for the replacement generation\n";
    return 1;
  }
  check(jit->getExecutionSession().removeJITDylib(a.scope));

  show("A generation 2", new_a);
  show("B after reload ", b);
  const auto b_after =
      check(jit->lookup(b.scope, "plugin_counter")).toPtr<unsigned *>();
  if (b_after != b.counter || *b.counter != 20 || new_a.tick() != 1 ||
      b.tick() != 30) {
    std::cerr << "Reload should reset A and preserve B's address and value\n";
    return 1;
  }
  show("A after tick   ", new_a);
  show("B after tick   ", b);
  std::cout << "PASS\n";
}
