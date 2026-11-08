#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include "native_detour.hpp"

namespace nd = tracer::native_detour;

namespace {
void require(bool value, const std::string &message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

struct Mapping {
  void *address = MAP_FAILED;
  std::size_t size = 0;

  explicit Mapping(std::size_t bytes) : size(bytes) {
    address = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(address != MAP_FAILED, "test mmap failed");
  }

  ~Mapping() {
    if (address != MAP_FAILED) {
      ::munmap(address, size);
    }
  }

  Mapping(const Mapping &) = delete;
  Mapping &operator=(const Mapping &) = delete;
};

struct Trampoline {
  void *address = MAP_FAILED;
  std::size_t size = 0;

  ~Trampoline() {
    if (address != MAP_FAILED) {
      ::munmap(address, size);
    }
  }
};

std::int32_t read_disp(const std::uint8_t *bytes) {
  std::int32_t value;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

void put_disp(std::vector<std::uint8_t> &code, std::size_t field,
              std::size_t end, const std::uint8_t *entry, const void *target) {
  const auto delta = reinterpret_cast<std::intptr_t>(target) -
                     reinterpret_cast<std::intptr_t>(entry + end);
  require(delta >= INT32_MIN && delta <= INT32_MAX,
          "test displacement does not fit");
  const auto value = static_cast<std::int32_t>(delta);
  std::memcpy(code.data() + field, &value, sizeof(value));
}

void check_rx(void *address) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line)) {
    unsigned long long begin = 0, end = 0;
    char permissions[5]{};
    if (std::sscanf(line.c_str(), "%llx-%llx %4s", &begin, &end, permissions) ==
            3 &&
        reinterpret_cast<std::uintptr_t>(address) >= begin &&
        reinterpret_cast<std::uintptr_t>(address) < end) {
      require(permissions[0] == 'r' && permissions[1] == '-' &&
                  permissions[2] == 'x',
              "trampoline was not sealed RX");
      return;
    }
  }
  throw std::runtime_error("trampoline has no mapping");
}

void execute_cases(bool data_before) {
  const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  Mapping original(3 * page);
  auto *base = static_cast<std::uint8_t *>(original.address);
  auto *entry = base + page;
  auto *data = reinterpret_cast<int *>(base + (data_before ? 0 : 2 * page));
  int *second = data + 1;

  // All cases execute relocated instructions and then jump to the untouched
  // original continuation. Both signs of the original displacement are used.
  enum Kind { store, load, lea, immediate, imul, multiple, scalar_float };

  for (auto kind :
       {store, load, lea, immediate, imul, multiple, scalar_float}) {
    require(::mprotect(entry, page, PROT_READ | PROT_WRITE) == 0,
            "could not make fixture writable");
    std::vector<std::uint8_t> code;
    if (kind == store) {
      code = {0x55, 0x48, 0x89, 0xe5, 0x89, 0x7d, 0xfc, 0x8b, 0x45,
              0xfc, 0x89, 0x05, 0,    0,    0,    0,    0x5d, 0xc3};
      put_disp(code, 12, 16, entry, data);
    } else if (kind == immediate) {
      code = {0x55, 0x48, 0x89, 0xe5, 0xc7, 0x05, 0,    0,
              0,    0,    0x78, 0x56, 0x34, 0x12, 0x5d, 0xc3};
      put_disp(code, 6, 14, entry, data);
    } else if (kind == lea) {
      code = {0x48, 0x8d, 0x05, 0, 0, 0, 0};
      code.resize(14, 0x90);
      code.push_back(0xc3);
      put_disp(code, 3, 7, entry, data);
    } else if (kind == imul) {
      code = {0x6b, 0x05, 0, 0, 0, 0, 0x03};
      code.resize(14, 0x90);
      code.push_back(0xc3);
      put_disp(code, 2, 7, entry, data);
    } else if (kind == multiple) {
      code = {0x8b, 0x05, 0, 0, 0, 0, 0x89, 0x3d, 0, 0, 0, 0, 0x90, 0x90, 0xc3};
      put_disp(code, 2, 6, entry, data);
      put_disp(code, 8, 12, entry, second);
    } else if (kind == scalar_float) {
      code = {0xf3, 0x0f, 0x10, 0x05, 0, 0, 0, 0};
      code.resize(14, 0x90);
      code.push_back(0xc3);
      put_disp(code, 4, 8, entry, data);
    } else {
      code = {0x8b, 0x05, 0, 0, 0, 0};
      code.resize(14, 0x90);
      code.push_back(0xc3);
      put_disp(code, 2, 6, entry, data);
    }
    std::copy(code.begin(), code.end(), entry);
    require(::mprotect(entry, page, PROT_READ | PROT_EXEC) == 0,
            "could not seal fixture");
    nd::Patch patch;
    std::string error;
    require(nd::decode({entry, code.size()},
                       reinterpret_cast<std::uintptr_t>(entry), patch, error),
            "decode failed: " + error);
    require(patch.relocations.size() == (kind == multiple ? 2 : 1),
            "unexpected relocation count");
    Trampoline trampoline;
    require(nd::create_trampoline(patch, trampoline.address, trampoline.size,
                                  error),
            "trampoline failed: " + error);
    check_rx(trampoline.address);
    for (const auto &relocation : patch.relocations) {
      const auto next = reinterpret_cast<std::uintptr_t>(trampoline.address) +
                        relocation.instruction_end;
      const auto displacement =
          read_disp(static_cast<const std::uint8_t *>(trampoline.address) +
                    relocation.displacement_offset);
      require(next + displacement == relocation.target,
              "relocation changed the effective address");
    }
    *data = 19;
    *second = -1;
    if (kind == lea) {
      auto call = reinterpret_cast<void *(*)()>(trampoline.address);
      require(call() == data, "relocated LEA returned the wrong address");
    } else if (kind == store || kind == immediate) {
      auto call = reinterpret_cast<void (*)(int)>(trampoline.address);
      call(73);
      require(*data == (kind == store ? 73 : 0x12345678),
              "relocated store wrote the wrong location or value");
    } else if (kind == scalar_float) {
      const float value = 1.25F;
      std::memcpy(data, &value, sizeof(value));
      auto call = reinterpret_cast<float (*)()>(trampoline.address);
      require(call() == value, "relocated scalar SIMD load failed");
    } else {
      auto call = reinterpret_cast<int (*)(int)>(trampoline.address);
      require(call(73) == (kind == imul ? 57 : 19),
              "relocated load read the wrong value");
      if (kind == multiple) {
        require(*second == 73, "second relocation wrote the wrong location");
      }
    }
    require(std::equal(code.begin(), code.end(), entry),
            "trampoline preparation mutated the target");
    require(std::equal(patch.original.begin(),
                       patch.original.begin() + patch.size, entry),
            "original restoration bytes were modified");
  }
}

void stack_compare() {
  const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  Mapping original(page);
  // GCC 8.5 entry followed by a continuation that consumes the compare flags.
  const std::array<std::uint8_t, 25> code{
      0x55, 0x48, 0x89, 0xe5,        // push %rbp; mov %rsp,%rbp
      0x48, 0x89, 0x7d, 0xf8,        // mov %rdi,-8(%rbp)
      0x48, 0x89, 0x75, 0xf0,        // mov %rsi,-16(%rbp)
      0x48, 0x83, 0x7d, 0xf8, 0x00,  // cmpq $0,-8(%rbp)
      0x0f, 0x95, 0xc0,              // setne %al
      0x0f, 0xb6, 0xc0, 0x5d, 0xc3}; // movzbl %al,%eax; pop %rbp; ret
  std::memcpy(original.address, code.data(), code.size());
  require(::mprotect(original.address, page, PROT_READ | PROT_EXEC) == 0,
          "could not make compare fixture executable");
  nd::Patch patch;
  std::string error;
  require(nd::decode(code, reinterpret_cast<std::uintptr_t>(original.address),
                     patch, error) &&
              patch.size == 17 && patch.relocations.empty(),
          "GCC stack compare was not admitted as a copy-only instruction");
  Trampoline trampoline;
  require(nd::create_trampoline(patch, trampoline.address, trampoline.size, error),
          "could not create the stack compare trampoline");
  const auto call = reinterpret_cast<int (*)(const void *, const void *)>(trampoline.address);
  require(call(nullptr, nullptr) == 0 && call(&patch, nullptr) == 1,
          "copied stack compare did not preserve the condition flags");
}

void rejection_cases() {
  for (const auto &start : std::vector<std::vector<std::uint8_t>>{
           {0xe8, 0, 0, 0, 0},
           {0xeb, 0},
           {0xc3},
           {0x0f, 0xa2},
           {0xf3, 0x0f, 0x1e, 0xfa},
           {0x48, 0x83, 0x3d, 0, 0x10, 0, 0, 0}, // RIP-relative CMP64mi8
           {0x67, 0x8b, 0x05, 0, 0x10, 0, 0},
           {0x64, 0x8b, 0x05, 0, 0x10, 0, 0},
           {0x8b, 0x05, 0xfa, 0xff, 0xff, 0xff}}) {
    auto code = start;
    code.resize(32, 0x90);
    nd::Patch patch;
    patch.size = 99; // Decode failure must not publish a partial plan.
    std::string error;
    require(!nd::decode(code, 0x100000000, patch, error),
            "unsafe entry was accepted");
    require(patch.size == 99 && !error.empty(),
            "decode failure published partial state");
  }
  std::array<std::uint8_t, 14> truncated;
  truncated.fill(0x90);
  truncated.back() = 0x48;
  nd::Patch patch;
  std::string error;
  require(!nd::decode(truncated, 0x100000000, patch, error),
          "truncated final instruction was accepted");

  // Put the same incomplete instruction against an unreadable page.
  const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  Mapping guard(2 * page);
  auto *base = static_cast<std::uint8_t *>(guard.address);
  require(::mprotect(base + page, page, PROT_NONE) == 0,
          "could not protect guard page");
  std::copy(truncated.begin(), truncated.end(), base + page - truncated.size());
  require(!nd::decode(
              {base + page - truncated.size(), truncated.size()},
              reinterpret_cast<std::uintptr_t>(base + page - truncated.size()),
              patch, error),
          "guard-page truncation was accepted");
}

void short_entry_relay() {
  const auto page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
  Mapping original(page);
  auto *entry = static_cast<std::uint8_t *>(original.address);
  // GCC's unoptimized one-line getter ends in RET at byte 14.
  const std::array<std::uint8_t, 14> code{
      0x55, 0x48, 0x89, 0xe5, 0x48, 0x89, 0x7d, 0xf8,
      0x48, 0x8b, 0x45, 0xf8, 0x5d, 0xc3};
  std::copy(code.begin(), code.end(), entry);
  const auto address = reinterpret_cast<std::uintptr_t>(entry);
  nd::Patch patch;
  std::string error;
  require(!nd::decode(code, address, patch, error),
          "long jump admitted a return instruction");
  require(!nd::decode(std::span(code).first(4), address, patch, error,
                      nd::kRelativeJumpBytes),
          "short jump admitted a range shorter than the jump");
  require(nd::decode(code, address, patch, error, nd::kRelativeJumpBytes) &&
              patch.size == 8,
          "short jump failed to cover complete entry instructions");

  Trampoline trampoline;
  require(nd::create_trampoline(patch, trampoline.address, trampoline.size,
                                error),
          "short trampoline allocation failed: " + error);
  const auto replacement = +[](std::uint64_t value) { return value + 1; };
  require(nd::prepare_entry_jump(patch, trampoline.address, trampoline.size,
                                reinterpret_cast<const void *>(replacement),
                                {entry, patch.size}, error),
          "short entry relay failed: " + error);
  check_rx(trampoline.address);
  require(::mprotect(entry, page, PROT_READ | PROT_EXEC) == 0,
          "could not seal short-entry fixture");
  using Getter = std::uint64_t (*)(std::uint64_t);
  require(reinterpret_cast<Getter>(trampoline.address)(42) == 42,
          "short trampoline changed the original result");
  require(reinterpret_cast<Getter>(entry)(42) == 43,
          "short entry did not reach the replacement");
}

void displacement_limits() {
  std::vector<std::uint8_t> code{0x8b, 0x05, 0xfa, 0x0f, 0, 0};
  code.resize(14, 0x90);
  nd::Patch patch;
  std::string error;
  require(nd::decode(code, 0x100000000, patch, error),
          "boundary decode failed");
  const auto &r = patch.relocations.front();
  const auto center = r.target - r.instruction_end;
  const auto lower = center - INT32_MAX;
  const auto upper = center + std::uint64_t(INT32_MAX) + 1;
  for (auto destination : {lower, upper}) {
    std::array<std::uint8_t, 64> output{};
    require(nd::relocate(patch, destination, output, error),
            "signed disp32 boundary rejected: " + error);
    require(read_disp(output.data() + r.displacement_offset) ==
                (destination == lower ? INT32_MAX : INT32_MIN),
            "signed disp32 boundary encoded incorrectly");
  }
  for (auto destination :
       {lower - 1, upper + 1, std::numeric_limits<std::uintptr_t>::max()}) {
    std::array<std::uint8_t, 64> output;
    output.fill(0xcc);
    require(!nd::relocate(patch, destination, output, error),
            "out-of-range displacement was accepted");
    require(std::all_of(output.begin(), output.end(),
                        [](auto byte) { return byte == 0xcc; }),
            "failed relocation partially wrote output");
  }
  require(std::equal(code.begin(), code.end(), patch.original.begin()),
          "relocation mutated original bytes");

  // No single trampoline can reach these two references. Fail before mapping
  // or publishing anything; the caller's outputs remain untouched.
  patch.relocations = {{2, 6, 0x1000}, {8, 14, 0x1000000000}};
  void *address = MAP_FAILED;
  std::size_t size = 0;
  require(!nd::create_trampoline(patch, address, size, error),
          "impossible relocation constraints were accepted");
  require(address == MAP_FAILED && size == 0,
          "failed allocation published a trampoline");
}

void overlap_boundaries() {
  const std::uintptr_t entry = 0x100000000;
  for (const int delta : {-4, -3, 0, 13, 14}) {
    std::vector<std::uint8_t> code{0x8b, 0x05, 0, 0, 0, 0};
    code.resize(14, 0x90);
    const std::int32_t displacement = delta - 6;
    std::memcpy(code.data() + 2, &displacement, sizeof(displacement));
    nd::Patch patch;
    std::string error;
    const bool accepted = nd::decode(code, entry, patch, error);
    require(accepted == (delta == -4 || delta == 14),
            "four-byte access overlap boundary was classified incorrectly");
  }
  // LEA preserves the original address without reading overwritten bytes.
  std::vector<std::uint8_t> code{0x48, 0x8d, 0x05, 0xf9, 0xff, 0xff, 0xff};
  code.resize(14, 0x90);
  nd::Patch patch;
  std::string error;
  require(nd::decode(code, entry, patch, error) &&
              patch.relocations.front().target == entry,
          "address-only LEA into the patch window was rejected");
}
} // namespace

int main() {
  try {
    execute_cases(false);
    execute_cases(true);
    stack_compare();
    rejection_cases();
    short_entry_relay();
    displacement_limits();
    overlap_boundaries();
    std::cout << "native detour relocation checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
