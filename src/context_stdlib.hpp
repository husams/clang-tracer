#pragma once

#include "context_stdlib_abi.hpp"

#include <memory>
#include <string>
#include <string_view>

#include <llvm/Support/Error.h>

namespace tracer {
class ContextStdlibImage final
    : public std::enable_shared_from_this<ContextStdlibImage> {
public:
  using UnwindLookup = const void *(*)(const void *, std::uintptr_t,
                                       context_support::UnwindBases &) noexcept;
  static llvm::Expected<std::shared_ptr<const ContextStdlibImage>> acquire();
  ~ContextStdlibImage();
  ContextStdlibImage(const ContextStdlibImage &) = delete;
  ContextStdlibImage &operator=(const ContextStdlibImage &) = delete;
  llvm::Expected<std::uintptr_t> symbol(const std::string &) const;
  bool contains(std::string_view) const noexcept;
  const context_support::Profile &profile() const noexcept;
  bool warm_unwind(std::uintptr_t) const noexcept;
  bool validate_runtime() const noexcept;

  // Native admission before entering a hook/constructor: reserves a private
  // exception lane. Nested support calls reuse it; no support payload allocator
  // contention or exhaustion can consume the lane's exception reserve.
  // Cleanup admits only validated nonallocating, nonthrowing finalizers and
  // needs no exception lane, so pending tasks can always release their state.
  class Frame final {
  public:
    enum class Purpose { invocation, cleanup };
    explicit Frame(std::shared_ptr<const ContextStdlibImage>,
                   const void *guest = nullptr, UnwindLookup = nullptr,
                   Purpose = Purpose::invocation) noexcept;
    ~Frame() noexcept;
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Frame(Frame &&) = delete;
    Frame &operator=(Frame &&) = delete;
    bool entered() const noexcept { return entered_; }

  private:
    std::shared_ptr<const ContextStdlibImage> image_;
    const void *previous_guest_ = nullptr;
    UnwindLookup previous_lookup_ = nullptr;
    unsigned lane_ = 0;
    bool entered_ = false, nested_ = false;
  };

  // Last-resort controlled unwind for a violated private-support contract.
  // Native callers retain the image through this call and its final return.
  bool invoke(void (*function)(void *), void *) const noexcept;
  std::size_t outstanding_allocations() const noexcept;

private:
  struct Impl;
  explicit ContextStdlibImage(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};
} // namespace tracer
