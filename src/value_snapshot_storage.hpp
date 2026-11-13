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
extern "C" bool tracer_snapshot_read_rtti_provider(const void *base, const void **complete,
    const char **type_name, const void **type_info, const void **vtable) noexcept;
extern "C" bool tracer_snapshot_readable(const void *address, std::size_t size,
                                           std::size_t alignment) noexcept;
extern "C" bool tracer_snapshot_rtti_matches(
    const char *type_name, const char *expected, std::size_t expected_len,
    const void *complete, const void *base, std::size_t object_size,
    std::size_t object_alignment) noexcept;

// True when an unverified pointer designates an aligned object whose pages
// are all readable. Used for pointees that have no RTTI to check.
extern "C" bool tracer_snapshot_readable(const void *object, std::size_t size,
                                         std::size_t alignment) noexcept;

// Raises the per-thread storage, the payload slice and the string scratch to what a
// probe asked for (--snapshot-arena, --snapshot-slice), and the members and levels of
// followed members an event may carry (--follow-members, --follow-depth plus the
// root); the largest request wins.
extern "C" void tracer_snapshot_configure(unsigned arena_bytes, unsigned slice, unsigned members,
                                          unsigned depth) noexcept;
// Copies up to the string scratch (--snapshot-slice) out of the target into
// thread-local scratch with a checked read. Null when any of it is unreadable.
extern "C" const char *tracer_snapshot_copy_bytes(const void *source,
                                                  std::size_t size) noexcept;
// True when the whole range lies in that scratch.
extern "C" bool tracer_snapshot_is_scratch(const void *address, std::size_t size) noexcept;
// Reads a NUL-terminated string of at most `capacity` (<= the scratch) bytes into the
// same scratch, page by page; the text is truncated at the bound.
extern "C" const char *tracer_snapshot_read_cstring(const char *text,
                                                    std::size_t capacity,
                                                    std::size_t *length) noexcept;
extern "C" std::byte *tracer_snapshot_storage_limited(std::size_t *capacity,
                                                      std::size_t reserve) noexcept;
