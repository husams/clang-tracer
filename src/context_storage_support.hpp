#pragma once

#include <cstdint>

namespace tracer::context_engine {
// Native loader catalog only. These entry points do not throw or call user
// code. The first ABI argument is a retained engine-issued StorageDomain*.
std::uintptr_t storage_allocate_entrypoint() noexcept;
std::uintptr_t storage_release_entrypoint() noexcept;
std::uintptr_t memory_copy_entrypoint() noexcept;
std::uintptr_t memory_move_entrypoint() noexcept;
std::uintptr_t memory_set_entrypoint() noexcept;
} // namespace tracer::context_engine
