#pragma once
#include <cstddef>

extern "C" std::size_t tracer_snapshot_mark() noexcept;
extern "C" void tracer_snapshot_reset(std::size_t) noexcept;
// Reserves the returned extent before an encoder may write into it.
extern "C" std::byte *tracer_snapshot_storage(std::size_t *) noexcept;
extern "C" void tracer_snapshot_commit(std::size_t mark,
                                       std::size_t bytes) noexcept;
