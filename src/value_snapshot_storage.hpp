#pragma once
#include <cstddef>

extern "C" std::size_t tracer_snapshot_mark() noexcept;
extern "C" void tracer_snapshot_reset(std::size_t) noexcept;
// Reserves the returned extent before an encoder may write into it.
extern "C" std::byte *tracer_snapshot_storage(std::size_t *) noexcept;
extern "C" void tracer_snapshot_commit(std::size_t mark,
                                       std::size_t bytes) noexcept;

// Read the Itanium RTTI header of an automatically followed member without
// dereferencing an unverified pointer in the target process.
extern "C" bool tracer_snapshot_read_rtti(const void *base,
                                          const void **complete,
                                          const char **type_name) noexcept;
extern "C" bool tracer_snapshot_rtti_matches(
    const char *type_name, const char *expected, std::size_t expected_len,
    const void *complete, const void *base, std::size_t object_size,
    std::size_t object_alignment) noexcept;
