// Deliberately incompatible definitions in the application's global scope.
// Loading/using the agent must never call these copies. No dependency headers
// or libraries are needed: the asm labels spell the pinned protobuf ABI.
#include <cstdio>
#include <cstdlib>

namespace {
[[noreturn]] void collision(const char *symbol) {
  std::fprintf(stderr, "agent bound to application's %s\n", symbol);
  std::abort();
}
} // namespace

extern "C" void foreign_protobuf_descriptors(const void *)
    asm("_ZN6google8protobuf8internal20AddDescriptorsRunnerC1EPKNS1_15DescriptorTableE");
extern "C" void foreign_protobuf_descriptors(const void *) {
  collision("protobuf AddDescriptorsRunner");
}
extern "C" void foreign_protobuf_defaults()
    asm("_ZN6google8protobuf8internal24InitProtobufDefaultsSlowEv");
extern "C" void foreign_protobuf_defaults() {
  collision("protobuf InitProtobufDefaultsSlow");
}
extern "C" void grpc_init() { collision("grpc_init"); }
extern "C" void grpc_shutdown() { collision("grpc_shutdown"); }
