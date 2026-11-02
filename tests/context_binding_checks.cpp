#include "context_binding.hpp"
#include "context_stdlib.hpp"
#include "context_test_config.hpp"
#include "engine_storage.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>

namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

template <class T> T take(llvm::Expected<T> result) {
  if (!result)
    throw std::runtime_error(llvm::toString(result.takeError()));
  return std::move(*result);
}

template <class T> void rejects(llvm::Expected<T> result, const char *reason) {
  require(!result, "unsupported owner/support path was admitted");
  const auto error = llvm::toString(result.takeError());
  if (error.find(reason) == std::string::npos)
    throw std::runtime_error("unexpected owner rejection: " + error +
                             "; expected " + reason);
}

std::vector<std::string> arguments() {
  std::vector<std::string> result{"-std=c++20", "-fPIC", "-O0",
                                  "-fno-stack-protector",
                                  "-resource-dir=" TRACER_TEST_RESOURCE_DIR};
  result.insert(result.end(), std::begin(kContextSystemArguments),
                std::end(kContextSystemArguments));
  return result;
}

auto compile(const std::string &source, std::vector<std::string> types = {}) {
  return take(tracer::compile_context_module(source, arguments(), types));
}

auto module(const std::string &source,
            const tracer::ContextCompilationContract &contract) {
  auto context = std::make_unique<llvm::LLVMContext>();
  llvm::SMDiagnostic diagnostic;
  auto ir = llvm::parseAssemblyString(source, diagnostic, *context);
  require(static_cast<bool>(ir), "binding IR fixture is invalid");
  ir->setTargetTriple(llvm::Triple(contract.target));
  ir->setDataLayout(contract.data_layout);
  return llvm::orc::ThreadSafeModule(std::move(ir), std::move(context));
}

const char *owner_ir = R"(
@counter = internal global i64 0
define i64 @owner_read() { %value = load i64, ptr @counter
  ret i64 %value }
define void @owner_add(i64 %amount) { %value = load i64, ptr @counter
  %next = add i64 %value, %amount
  store i64 %next, ptr @counter
  ret void }
)";
const char *callback_ir = R"(
declare i64 @owner_read()
declare void @owner_add(i64)
define i64 @callback() { call void @owner_add(i64 7)
  %value = call i64 @owner_read()
  ret i64 %value }
)";

void binding_and_retirement(
    const tracer::ContextCompilationContract &contract) {
  const auto dead_trap = R"(
    declare void @llvm.trap()
    define i64 @entry() {
      ret i64 42
    disconnected:
      call void @llvm.trap()
      unreachable
    }
  )";
  auto cleaned = take(tracer::PrivateContextCode::load(
      module(dead_trap, contract), contract, {"entry"}));
  auto cleaned_entry = take(cleaned->symbol("entry"));
  require(reinterpret_cast<std::uint64_t (*)()>(cleaned_entry.address())() ==
              42,
          "disconnected compiler trap blocked executable support admission");
  rejects(
      tracer::PrivateContextCode::load(
          module("declare void @llvm.trap()\n"
                 "define i64 @entry() { call void @llvm.trap()\n unreachable }",
                 contract),
          contract, {"entry"}),
      "unsealed intrinsic");
  auto owner = take(tracer::PrivateContextCode::load(
      module(owner_ir, contract), contract, {"owner_read", "owner_add"}));
  auto other = take(tracer::PrivateContextCode::load(
      module(owner_ir, contract), contract, {"owner_read", "owner_add"}));
  require(owner->generation() != other->generation(),
          "owner generation identity was reused");
  auto read = take(owner->symbol("owner_read"));
  auto add = take(owner->symbol("owner_add"));
  auto callback = take(tracer::PrivateContextCode::load(
      module(callback_ir, contract), contract, {"callback"}, {read, add},
      tracer::PrivateContextCode::Role::callback));
  auto callback_symbol = take(callback->symbol("callback"));
  auto call = reinterpret_cast<std::uint64_t (*)()>(callback_symbol.address());
  require(call() == 7 && call() == 14,
          "private callback did not borrow one exact owner's object");
  auto other_read = take(other->symbol("owner_read"));
  require(reinterpret_cast<std::uint64_t (*)()>(other_read.address())() == 0,
          "same-named separate owner symbols were interposed");
  rejects(tracer::PrivateContextCode::load(
              module(callback_ir, contract), contract, {"callback"},
              {read, take(other->symbol("owner_add"))},
              tracer::PrivateContextCode::Role::callback),
          "mixed owner generations");
  auto changed = contract;
  changed.language += "different signedness";
  rejects(tracer::PrivateContextCode::load(
              module(callback_ir, changed), changed, {"callback"}, {read, add},
              tracer::PrivateContextCode::Role::callback),
          "compilation contracts differ");
  rejects(tracer::PrivateContextCode::load(
              module(R"(
    declare i32 @owner_read()
    define i32 @callback() { %value = call i32 @owner_read()
      ret i32 %value })",
                     contract),
              contract, {"callback"}, {read},
              tracer::PrivateContextCode::Role::callback),
          "ABI mismatch");
  rejects(tracer::PrivateContextCode::load(
              module(owner_ir, contract), contract, {"owner_read"}, {read},
              tracer::PrivateContextCode::Role::callback),
          "shadows owner bridge");
  rejects(owner->symbol("counter"), "not a generated private export");
  std::weak_ptr<const tracer::PrivateContextCode> lifetime = owner;
  owner.reset();
  read = {};
  add = {};
  callback.reset();
  require(!lifetime.expired() && call() == 21,
          "owner code/unwind lease was released while callback code could "
          "enter it");
  callback_symbol = {};
  require(lifetime.expired(),
          "private code generations leaked after their last native lease");
}

void closure_rejections(const tracer::ContextCompilationContract &contract) {
  for (
      const auto &[ir, reason] :
      std::vector<std::pair<std::string, const char *>>{
          {"declare ptr @malloc(i64)\ndefine ptr @entry() { %p = call ptr "
           "@malloc(i64 4)\nret ptr %p }",
           "unsealed external helper"},
          {"declare void @_ZdlPv(ptr)\ndefine void @entry(ptr %p) { call void "
           "@_ZdlPv(ptr %p)\nret void }",
           "unsealed external helper"},
          {"define void @entry(ptr %f) { call void %f()\nret void }",
           "indirect call"},
          {"define ptr @entry() { ret ptr inttoptr (i64 4096 to ptr) }",
           "native address constant"},
          {"@f = internal global ptr inttoptr (i64 4096 to ptr)\ndefine ptr "
           "@entry() { %p = load ptr, ptr @f\nret ptr %p }",
           "native address constant"},
          {"@p = external global i64\ndefine i64 @entry() { %p = load i64, ptr "
           "@p\nret i64 %p }",
           "global/lifecycle"},
          {"@p = thread_local global i64 0\ndefine i64 @entry() { %p = load "
           "i64, ptr @p\nret i64 %p }",
           "global/lifecycle"},
          {"module asm \"nop\"\ndefine void @entry() { ret void }", "assembly"},
          {"declare void @llvm.memcpy.p1.p0.i64(ptr addrspace(1), ptr, i64, "
           "i1)\ndefine "
           "void @entry(ptr addrspace(1) %a, ptr %b) { call void "
           "@llvm.memcpy.p1.p0.i64(ptr addrspace(1) "
           "%a, ptr %b, i64 4, i1 false)\nret void }",
           "intrinsic address space"}})
    rejects(tracer::PrivateContextCode::load(module(ir, contract), contract,
                                             {"entry"}),
            reason);
  auto wrong = contract;
  wrong.data_layout += "-p9:64:64";
  rejects(tracer::PrivateContextCode::load(module(owner_ir, contract), wrong,
                                           {"owner_read"}),
          "IR target/layout");
  // Both supported 64-bit backends lower this operation to a compiler runtime
  // helper. IR closure alone cannot grant admission, even when the process has
  // that helper: eager private materialization must fail without its provider.
  auto backend = tracer::PrivateContextCode::load(
      module("define i128 @entry(i128 %a, i128 %b) { %q = udiv i128 %a, "
             "%b\nret i128 %q }",
             contract),
      contract, {"entry"});
  require(!backend,
          "backend-introduced helper escaped to the process symbol table");
  llvm::consumeError(backend.takeError());
}

void finalizer_return_pins_generation() {
  auto source =
      compile("extern \"C\" void finalizer(unsigned *flags) { "
              "__atomic_store_n(flags, 1u, __ATOMIC_RELEASE); "
              "while (!__atomic_load_n(flags + 1, __ATOMIC_ACQUIRE)) {} }");
  auto code = take(tracer::PrivateContextCode::load(
      std::move(source.module), source.contract, {"finalizer"}));
  auto symbol = take(code->symbol("finalizer"));
  std::weak_ptr<const tracer::PrivateContextCode> lifetime = code;
  alignas(std::atomic<unsigned>) unsigned flags[2] = {};
  std::thread native([lease = symbol, &flags] {
    reinterpret_cast<void (*)(unsigned *)>(lease.address())(flags);
    // The lease destructor is native code after the JIT finalizer has returned.
  });
  while (!__atomic_load_n(&flags[0], __ATOMIC_ACQUIRE)) {
    std::this_thread::yield();
  }
  code.reset();
  symbol = {};
  const bool pinned = !lifetime.expired();
  __atomic_store_n(&flags[1], 1u, __ATOMIC_RELEASE);
  native.join();
  require(pinned, "generation unloaded inside its running finalizer");
  require(lifetime.expired(),
          "generation survived finalizer return and final native release");
}

void retained_support_generation() {
  using Role = tracer::PrivateContextCode::Role;
  auto support_source = compile(
      "extern \"C\" long support_add(long a, long b) { return a + b; }");
  const auto contract = support_source.contract;
  auto support = take(tracer::PrivateContextCode::load(
      std::move(support_source.module), contract, {"support_add"}, {},
      Role::support));
  auto unrelated_source = compile(
      "extern \"C\" long support_add(long a, long b) { return a - b; }");
  auto unrelated = take(tracer::PrivateContextCode::load(
      std::move(unrelated_source.module), contract, {"support_add"}, {},
      Role::support));
  auto owner_source = compile(
      "extern \"C\" long support_add(long, long); "
      "extern \"C\" long owner_value() { return support_add(10, 20); }");
  require(owner_source.contract == contract,
          "same-profile support compilation contract differs");
  auto owner = take(tracer::PrivateContextCode::load(
      std::move(owner_source.module), contract, {"owner_value"},
      {take(support->symbol("support_add"))}, Role::owner));
  const auto callback_source =
      "extern \"C\" long support_add(long, long); "
      "extern \"C\" long owner_value(); "
      "extern \"C\" long callback() { return support_add(owner_value(), 12); }";
  auto good = compile(callback_source);
  auto callback = take(tracer::PrivateContextCode::load(
      std::move(good.module), contract, {"callback"},
      {take(owner->symbol("owner_value")),
       take(support->symbol("support_add"))},
      Role::callback));
  auto bad = compile(callback_source);
  rejects(tracer::PrivateContextCode::load(
              std::move(bad.module), contract, {"callback"},
              {take(owner->symbol("owner_value")),
               take(unrelated->symbol("support_add"))},
              Role::callback),
          "not from its owner's sealed generation");
  std::weak_ptr<const tracer::PrivateContextCode> retained = support;
  support.reset();
  owner.reset();
  auto symbol = take(callback->symbol("callback"));
  require(!retained.expired() &&
              reinterpret_cast<long (*)()>(symbol.address())() == 42,
          "retained out-of-line support did not survive owner retirement");
  symbol = {};
  callback.reset();
  require(retained.expired(),
          "transitive support generations leaked after final return");
}

void bounded_native_support() {
  using Role = tracer::PrivateContextCode::Role;
  const std::string source =
      "extern \"C\" void *__tracer_storage_allocate_v1(unsigned long long, "
      "unsigned long long); "
      "extern \"C\" unsigned __tracer_storage_release_v1(void *, unsigned long "
      "long, unsigned long long); "
      "extern \"C\" void *buffer(unsigned long long n) { return "
      "__tracer_storage_allocate_v1(n, 64); } "
      "extern \"C\" unsigned free_buffer(void *p) { return "
      "__tracer_storage_release_v1(p, 0, 0); }";
  auto first = compile(source);
  auto second = compile(source);
  auto one = take(tracer::PrivateContextCode::load(
      std::move(first.module), first.contract, {"buffer", "free_buffer"}, {},
      Role::support));
  auto two = take(tracer::PrivateContextCode::load(
      std::move(second.module), second.contract, {"buffer", "free_buffer"}, {},
      Role::support));
  const auto allocation = take(one->symbol("buffer"));
  const auto deallocation = take(one->symbol("free_buffer"));
  const auto other_allocation = take(two->symbol("buffer"));
  const auto other_deallocation = take(two->symbol("free_buffer"));
  auto allocate =
      reinterpret_cast<void *(*)(unsigned long long)>(allocation.address());
  auto release = reinterpret_cast<unsigned (*)(void *)>(deallocation.address());
  auto other_allocate = reinterpret_cast<void *(*)(unsigned long long)>(
      other_allocation.address());
  auto other_release =
      reinterpret_cast<unsigned (*)(void *)>(other_deallocation.address());
  void *pointer = allocate(300);
  require(pointer && reinterpret_cast<std::uintptr_t>(pointer) % 64 == 0,
          "private support could not allocate aligned engine storage");
  require(other_release(pointer) ==
              static_cast<unsigned>(
                  tracer::context_engine::StorageStatus::wrong_domain),
          "private support generation freed a foreign engine allocation");
  int application = 0;
  require(release(&application) ==
              static_cast<unsigned>(
                  tracer::context_engine::StorageStatus::foreign_pointer),
          "private support attempted application allocation release");
  require(release(pointer) == 0 && release(pointer) != 0 &&
              release(nullptr) == 0,
          "private support release ownership was not exactly once");
  pointer = allocate(40 * 1024 * 1024);
  require(
      pointer && !other_allocate(40 * 1024 * 1024),
      "private support generations exceeded the global 64 MiB storage ceiling");
  require(release(pointer) == 0, "private support allocation was leaked");
  auto untrusted_role = compile(source);
  rejects(tracer::PrivateContextCode::load(std::move(untrusted_role.module),
                                           untrusted_role.contract,
                                           {"buffer", "free_buffer"}),
          "reserved private support symbol");
  auto wrong_abi =
      compile("extern \"C\" unsigned __tracer_storage_allocate_v1(unsigned); "
              "extern \"C\" unsigned entry() { return "
              "__tracer_storage_allocate_v1(3); }");
  rejects(tracer::PrivateContextCode::load(std::move(wrong_abi.module),
                                           wrong_abi.contract, {"entry"}, {},
                                           Role::support),
          "reserved private support symbol or ABI");
  auto shadow = compile(
      "extern \"C\" int __tracer_native_storage_allocate_v1() { return 0; }");
  rejects(tracer::PrivateContextCode::load(
              std::move(shadow.module), shadow.contract,
              {"__tracer_native_storage_allocate_v1"}, {}, Role::support),
          "reserved private support symbol");
}

void cpp_bridge_pointees() {
  using Role = tracer::PrivateContextCode::Role;
  const auto exported_names = [](const auto &contract,
                                 std::vector<std::string> names) {
    return take(tracer::context_bridge_symbols(contract, names));
  };
  auto source =
      compile("struct Payload { long value; }; "
              "long view(const Payload& p) noexcept { return p.value; }");
  auto owner = take(tracer::PrivateContextCode::load(
      std::move(source.module), source.contract,
      exported_names(source.contract, {"view"})));
  const auto view_name = exported_names(source.contract, {"view"})[0];
  const auto view = take(owner->symbol(view_name));
  auto good = compile(
      "struct Payload { long value; }; long view(const Payload&) noexcept; "
      "extern \"C\" long callback() { Payload p{42}; return view(p); }");
  auto callback = take(
      tracer::PrivateContextCode::load(std::move(good.module), good.contract,
                                       {"callback"}, {view}, Role::callback));
  const auto entry = take(callback->symbol("callback"));
  require(reinterpret_cast<long (*)()>(entry.address())() == 42,
          "C++ private bridge failed to borrow a matching record");
  // Same symbol, same opaque-pointer LLVM ABI, same record size; the C++
  // field's signedness still differs and must fail before any bridge call.
  auto bad = compile(
      "struct Payload { unsigned long value; }; long view(const Payload&) "
      "noexcept; "
      "extern \"C\" long callback() { Payload p{42}; return view(p); }");
  rejects(tracer::PrivateContextCode::load(std::move(bad.module), bad.contract,
                                           {"callback"}, {view},
                                           Role::callback),
          "C++ pointee/definition mismatch");
  auto scalar =
      compile("extern \"C\" long scalar(long value) { return value; }");
  auto scalar_owner = take(tracer::PrivateContextCode::load(
      std::move(scalar.module), scalar.contract, {"scalar"}));
  auto changed =
      compile("extern \"C\" unsigned long scalar(unsigned long); "
              "extern \"C\" unsigned long callback() { return scalar(42); }");
  rejects(tracer::PrivateContextCode::load(
              std::move(changed.module), changed.contract, {"callback"},
              {take(scalar_owner->symbol("scalar"))}, Role::callback),
          "C++ pointee/definition mismatch");
}

void private_memory_support() {
  auto source = compile("extern \"C\" void bytes(unsigned char* to, const "
                        "unsigned char* from, unsigned long long n) { "
                        "if (n < 2) return; __builtin_memcpy(to, from, n); "
                        "__builtin_memmove(to + 2, to, n - 2); "
                        "__builtin_memset(to, 0x7b, 2); } "
                        "extern \"C\" void left(unsigned char* p, unsigned "
                        "long long n) { __builtin_memmove(p, p + 1, n); }");
  auto code = take(tracer::PrivateContextCode::load(
      std::move(source.module), source.contract, {"bytes", "left"}));
  auto bytes = take(code->symbol("bytes"));
  auto left = take(code->symbol("left"));
  const auto domain = tracer::context_engine::StorageDomain::create();
  auto allocation = domain->allocate(64, 8);
  require(allocation.pointer,
          "memory support fixture could not reserve private storage");
  auto *buffer = static_cast<unsigned char *>(allocation.pointer);
  for (unsigned i = 0; i < 32; ++i) {
    buffer[i] = 0;
    buffer[32 + i] = static_cast<unsigned char>(i + 1);
  }
  reinterpret_cast<void (*)(unsigned char *, const unsigned char *,
                            unsigned long long)>(bytes.address())(
      buffer, buffer + 32, 16);
  require(buffer[0] == 0x7b && buffer[1] == 0x7b,
          "private memset primitive changed byte semantics");
  for (unsigned i = 2; i < 16; ++i)
    require(buffer[i] == i - 1,
            "private copy/backward-overlap move corrupted bytes");
  reinterpret_cast<void (*)(unsigned char *, unsigned long long)>(
      left.address())(buffer, 15);
  require(buffer[0] == 0x7b && buffer[1] == 1 && buffer[14] == 14,
          "private forward-overlap move corrupted bytes");
  reinterpret_cast<void (*)(unsigned char *, const unsigned char *,
                            unsigned long long)>(bytes.address())(nullptr,
                                                                  nullptr, 0);
  require(domain->release(buffer) == tracer::context_engine::StorageStatus::ok,
          "private memory support leaked storage ownership");
}

void compiler_contracts() {
  const std::string source = "struct Nested { unsigned value; }; struct State "
                             "{ Nested data; long count; }; "
                             "extern \"C\" long entry() { return 42; }";
  auto first = compile(source, {"State"});
  auto same = compile(source, {"State"});
  require(first.contract == same.contract && first.contract.types.size() == 2,
          "independent Clang definitions did not establish an identical "
          "recursive contract");
  auto changed = compile("struct Nested { int value; }; struct State { Nested "
                         "data; long count; };",
                         {"State"});
  require(first.contract != changed.contract,
          "same-width signedness/definition change was ignored");
  auto packed = compile(
      "#pragma pack(push, 1)\n" + source + "\n#pragma pack(pop)\n", {"State"});
  require(first.contract != packed.contract,
          "record packing change was ignored");
  auto library =
      compile("#define _GLIBCXX_USE_CXX11_ABI 0\n" + source, {"State"});
  require(first.contract != library.contract,
          "standard library ABI profile change was ignored");
  auto code = take(tracer::PrivateContextCode::load(std::move(first.module),
                                                    first.contract, {"entry"}));
  const auto entry = take(code->symbol("entry"));
  require(reinterpret_cast<long (*)()>(entry.address())() == 42,
          "real Clang owner code did not execute in its private linker");
  // Real standard container code must not silently borrow application new,
  // delete, tree/throw/string helpers while its sealed provider is unavailable.
  auto container =
      compile("#include <string>\n#include <vector>\n#include <map>\n"
              "extern \"C\" long entry() { std::string s(256, 'x'); "
              "std::vector<long> v(20, 2); "
              "std::map<int, long> m; m[1] = v[0]; return s.size() + m[1]; }");
  rejects(tracer::PrivateContextCode::load(std::move(container.module),
                                           container.contract, {"entry"}),
          "unsupported_state_support_profile");
  auto initialized = compile(
      "struct X { X(); }; X value; extern \"C\" int entry() { return 1; }");
  rejects(tracer::PrivateContextCode::load(std::move(initialized.module),
                                           initialized.contract, {"entry"}),
          "global/lifecycle");
}
} // namespace

void verify_context_binding() {
#if defined(__linux__) && defined(__x86_64__)
  {
    auto library = take(tracer::ContextStdlibImage::acquire());
    auto compiled = compile(R"(
#include <any>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
extern "C" int sealed_owner_test() noexcept {
  try {
    std::any erased = std::make_shared<std::string>(1024, 'z');
    auto &text = *std::any_cast<std::shared_ptr<std::string>&>(erased);
    std::vector<std::string> values(3, text);
    try { values.at(10); } catch (const std::out_of_range&) { return values[1].size(); }
  } catch (...) { return -1; }
  return -2;
}
)");
    auto code = take(tracer::PrivateContextCode::load(
        std::move(compiled.module), std::move(compiled.contract),
        {"sealed_owner_test"}, {}, tracer::PrivateContextCode::Role::owner,
        library));
    auto entry = take(code->symbol("sealed_owner_test"));
    tracer::ContextStdlibImage::Frame frame(
        library, code.get(), &tracer::PrivateContextCode::unwind_lookup);
    require(frame.entered(), "sealed owner did not reserve exception capacity");
    struct Args {
      std::uintptr_t entry;
      int result = 0;
    } args{entry.address()};
    require(library->invoke(
                [](void *opaque) {
                  auto &args = *static_cast<Args *>(opaque);
                  args.result = reinterpret_cast<int (*)()>(args.entry)();
                },
                &args) &&
                args.result == 1024,
            "real private owner allocation, erased managers or JIT exception "
            "unwind failed");
  }
#endif
  compiler_contracts();
  retained_support_generation();
  bounded_native_support();
  cpp_bridge_pointees();
  private_memory_support();
  finalizer_return_pins_generation();
  const auto contract =
      compile("extern \"C\" int entry() { return 0; }").contract;
  binding_and_retirement(contract);
  closure_rejections(contract);
  std::cout << "private owner generation, ABI/layout, support closure and "
               "retirement checks passed\n";
}
