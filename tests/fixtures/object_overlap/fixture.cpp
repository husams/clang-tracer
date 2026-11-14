#include <atomic>
#include <cstdio>
#include <thread>

#include <unistd.h>

#if defined(TRACE_TEST_SVE_STATE) || defined(TRACE_TEST_SME_STATE)
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <linux/prctl.h>
#include <sys/prctl.h>
#if defined(TRACE_TEST_SME_STATE)
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif
#endif

#if defined(__aarch64__) && !defined(TRACE_TEST_IDLE_ATTACH)
#include "../object_aarch64/rendezvous.hpp"
#endif

namespace overlap_test {
struct Record {
  int value;
  Record();
  ~Record();
};

__attribute__((noinline, noclone)) Record::Record() : value(17) {
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}
__attribute__((noinline, noclone)) Record::~Record() {
  std::puts("RETIRED");
  asm volatile("nop\n nop\n nop\n nop\n nop" ::: "memory");
}

#if defined(TRACE_TEST_SME_STATE)
extern "C" __attribute__((noinline, noclone)) void overlap_write(
    Record *record, int value, const std::atomic<unsigned> *gate = nullptr) {
  std::size_t svl = 0;
  asm volatile("rdsvl %0, #1" : "=r"(svl));
  const auto hwcap2 = ::getauxval(AT_HWCAP2);
  const auto features = unsigned(bool(hwcap2 & HWCAP2_SME2)) |
                        (unsigned(bool(hwcap2 & HWCAP2_SME_FA64)) << 1);
  std::array<unsigned char, 512> rows{}, saved_rows{}, vectors{}, saved_vectors{};
  std::array<unsigned char, 96> predicates{}, saved_predicates{};
  std::array<unsigned char, 64> zt{}, saved_zt{};
  std::array<std::uint64_t, 2> lazy_save{};
  rows.fill(0x31);
  std::fill(rows.begin() + 256, rows.end(), 0x7f);
  zt.fill(0x44);
  std::uint64_t svcr, tpidr2;
  asm volatile(
      "smstart\n"
      "msr tpidr2_el0, %[lazy]\n"
      "mov w12, #0\n"
      "ldr za[w12, #0], [%[rows]]\n"
      "rdsvl x12, #1\n"
      "sub x12, x12, #1\n"
      "add x10, %[rows], #256\n"
      "ldr za[w12, #0], [x10]\n"
      "tbz %w[features], #0, 3f\n"
      "ldr zt0, [%[zt]]\n"
      "3: ptrue p0.b\n"
      "ptrue p15.b, vl4\n"
      "tbz %w[features], #1, 4f\n"
      "wrffr p0.b\n"
      "rdffr p14.b\n"
      "str p14, [%[predicates], #2, mul vl]\n"
      "4: mov z0.b, #0x19\n"
      "mov z31.b, #0x65\n"
      "str z0, [%[vectors]]\n"
      "str z31, [%[vectors], #1, mul vl]\n"
      "str p0, [%[predicates]]\n"
      "str p15, [%[predicates], #1, mul vl]\n"
      "cbz %[gate], 1f\n"
      "0: ldar w9, [%[gate]]\n"
      "cbz w9, 0b\n"
      "b 2f\n"
      "1: str %w[value], [%[record]]\n"
      "2: mrs %[svcr], svcr\n"
      "mrs %[tpidr2], tpidr2_el0\n"
      "str z0, [%[saved_vectors]]\n"
      "str z31, [%[saved_vectors], #1, mul vl]\n"
      "str p0, [%[saved_predicates]]\n"
      "str p15, [%[saved_predicates], #1, mul vl]\n"
      "tbz %w[features], #1, 5f\n"
      "rdffr p14.b\n"
      "str p14, [%[saved_predicates], #2, mul vl]\n"
      "5: mov w12, #0\n"
      "str za[w12, #0], [%[saved_rows]]\n"
      "rdsvl x12, #1\n"
      "sub x12, x12, #1\n"
      "add x10, %[saved_rows], #256\n"
      "str za[w12, #0], [x10]\n"
      "tbz %w[features], #0, 6f\n"
      "str zt0, [%[saved_zt]]\n"
      "6: msr tpidr2_el0, xzr\n"
      "smstop\n"
      : [svcr] "=&r"(svcr), [tpidr2] "=&r"(tpidr2)
      : [record] "r"(record), [value] "r"(value), [gate] "r"(gate), [features] "r"(features),
        [rows] "r"(rows.data()), [saved_rows] "r"(saved_rows.data()), [zt] "r"(zt.data()),
        [saved_zt] "r"(saved_zt.data()), [vectors] "r"(vectors.data()),
        [saved_vectors] "r"(saved_vectors.data()), [predicates] "r"(predicates.data()),
        [saved_predicates] "r"(saved_predicates.data()), [lazy] "r"(lazy_save.data())
      : "memory", "cc", "x9", "x10", "x12", "z0", "z31", "p0", "p14", "p15");
  if (svcr != 3 || tpidr2 != reinterpret_cast<std::uintptr_t>(lazy_save.data()) ||
      std::memcmp(vectors.data(), saved_vectors.data(), 2 * svl) ||
      std::memcmp(predicates.data(), saved_predicates.data(), 3 * svl / 8) ||
      std::memcmp(rows.data(), saved_rows.data(), svl) ||
      std::memcmp(rows.data() + 256, saved_rows.data() + 256, svl) ||
      ((features & 1) && zt != saved_zt))
    std::abort();
}
#elif defined(TRACE_TEST_SVE_STATE)
extern "C" __attribute__((noinline, noclone)) void overlap_write(
    Record *record, int value, const std::atomic<unsigned> *gate = nullptr) {
  std::size_t vector_length = 0;
  asm volatile("rdvl %0, #1" : "=r"(vector_length));
  std::array<unsigned char, 512> vectors_before{}, vectors_after{};
  std::array<unsigned char, 96> predicates_before{}, predicates_after{};
  asm volatile(
      "ptrue p0.b\n"
      "ptrue p15.b, vl4\n"
      "wrffr p0.b\n"
      "mov z0.b, #0x31\n"
      "mov z31.b, #0x7f\n"
      "str z0, [%[before]]\n"
      "str z31, [%[before], #1, mul vl]\n"
      "str p0, [%[pred_before]]\n"
      "str p15, [%[pred_before], #1, mul vl]\n"
      "rdffr p14.b\n"
      "str p14, [%[pred_before], #2, mul vl]\n"
      "cbz %[gate], 1f\n"
      "0: ldar w9, [%[gate]]\n"
      "cbz w9, 0b\n"
      "b 2f\n"
      "1: str %w[value], [%[record]]\n"
      "2: str z0, [%[after]]\n"
      "str z31, [%[after], #1, mul vl]\n"
      "str p0, [%[pred_after]]\n"
      "str p15, [%[pred_after], #1, mul vl]\n"
      "rdffr p14.b\n"
      "str p14, [%[pred_after], #2, mul vl]\n"
      : : [record] "r"(record), [value] "r"(value), [gate] "r"(gate),
          [before] "r"(vectors_before.data()), [after] "r"(vectors_after.data()),
          [pred_before] "r"(predicates_before.data()), [pred_after] "r"(predicates_after.data())
      : "memory", "x9", "z0", "z31", "p0", "p14", "p15");
  if (std::memcmp(vectors_before.data(), vectors_after.data(), 2 * vector_length) ||
      std::memcmp(predicates_before.data(), predicates_after.data(), 3 * vector_length / 8))
    std::abort();
}
#else
extern "C" __attribute__((noinline, noclone)) void overlap_write(Record &record, int value) {
#if defined(__aarch64__)
  asm volatile("str %w1, [%0]" :: "r"(&record), "r"(value) : "memory");
#else
  asm volatile("movl %1, (%0)" :: "r"(&record), "r"(value) : "memory");
#endif
}
#endif
}

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(TRACE_TEST_SVE_STATE) || defined(TRACE_TEST_SME_STATE)
  const auto *requested = std::getenv("TRACE_TEST_SVE_VL");
#if defined(TRACE_TEST_SME_STATE)
  const auto vector_length = ::prctl(PR_SME_SET_VL, requested ? std::atoi(requested) : 64);
  if (vector_length < 0)
    return 5;
  std::printf("SME vector length=%d\n", vector_length & PR_SME_VL_LEN_MASK);
#else
  const auto vector_length = ::prctl(PR_SVE_SET_VL, requested ? std::atoi(requested) : 64);
  if (vector_length < 0)
    return 5;
  std::printf("SVE vector length=%d\n", vector_length & PR_SVE_VL_LEN_MASK);
#endif
  std::atomic<unsigned> gate{0};
  std::thread release_gate([&] {
    while (::access("write.now", F_OK))
      ::usleep(1000);
    gate.store(1, std::memory_order_release);
  });
#endif
#if defined(TRACE_TEST_WORKER_ATTACH)
  std::atomic<bool> release_idle{false};
  std::thread idle_worker([&] {
    while (!release_idle.load(std::memory_order_acquire))
      ::usleep(2000);
  });
#endif
  std::puts("READY");
#if defined(TRACE_TEST_SVE_STATE) || defined(TRACE_TEST_SME_STATE)
  overlap_test::overlap_write(nullptr, 0, &gate);
  release_gate.join();
#elif defined(__aarch64__) && !defined(TRACE_TEST_IDLE_ATTACH)
  arm_rendezvous();
#else
  while (::access("write.now", F_OK))
#if defined(TRACE_TEST_WORKER_ATTACH)
    std::this_thread::yield();
  release_idle.store(true, std::memory_order_release);
  idle_worker.join();
#else
    ::usleep(1000);
#endif
#endif
  overlap_test::Record record;
  std::atomic<bool> go{false};
  const auto run = [&](int base) {
    while (!go.load(std::memory_order_acquire)) {}
    for (int index = 0; index != 512; ++index) {
#if defined(TRACE_TEST_SVE_STATE) || defined(TRACE_TEST_SME_STATE)
      overlap_test::overlap_write(&record, base + index);
#else
      overlap_test::overlap_write(record, base + index);
#endif
    }
  };
  std::thread first(run, 1000), second(run, 2000);
  go.store(true, std::memory_order_release);
  first.join();
  second.join();
  std::printf("DONE value=%d\n", record.value);
  while (::access("stop.now", F_OK))
    ::usleep(1000);
  return record.value != 1511 && record.value != 2511;
}
