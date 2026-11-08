#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include <unistd.h>

#include "fixtures/signature_return_types.hpp"
#include "fixtures/signature_fixture_api.hpp"

namespace signature_fixture {
struct Record {
  int value = 0;

  __attribute__((visibility("default"), noinline)) int score() const;
  __attribute__((visibility("default"), noinline)) int score();
  __attribute__((visibility("default"), noinline)) int nonconst_only();
  int unresolved_score() const;
};

int Record::score() const { return value * 3 + 1; }
int Record::score() { return value * 7 + 2; }
int Record::nonconst_only() { return value + 5; }

__attribute__((visibility("default"), noinline)) int
const_record_value_target(const Record record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return record.value + stack_padding_a + stack_padding_b;
}

__attribute__((visibility("default"), noinline)) Record &
record_reference_return(Record &record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  (void)stack_padding_a;
  (void)stack_padding_b;
  return record;
}

struct AbiRecord {
  __attribute__((ms_abi, visibility("default"), noinline)) int score() const;
  int value = 23;
};

__attribute__((ms_abi, visibility("default"), noinline)) int
AbiRecord::score() const {
  return value + 1;
}

namespace {
int nontrivial_copies = 0;
int nontrivial_moves = 0;
int nontrivial_destructions = 0;
} // namespace

NontrivialReturn::NontrivialReturn(int input) : value(input) {}
NontrivialReturn::NontrivialReturn(const NontrivialReturn &other)
    : value(other.value) {
  ++nontrivial_copies;
}
NontrivialReturn::NontrivialReturn(NontrivialReturn &&other) noexcept
    : value(other.value) {
  ++nontrivial_moves;
}
NontrivialReturn::~NontrivialReturn() { ++nontrivial_destructions; }
int NontrivialReturn::copies() noexcept { return nontrivial_copies; }
int NontrivialReturn::moves() noexcept { return nontrivial_moves; }
int NontrivialReturn::destructions() noexcept {
  return nontrivial_destructions;
}

SmallReturn small_return(std::int32_t input) { return {input, input + 1}; }
MixedReturn mixed_return(std::int32_t input) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return {static_cast<double>(input) + stack_padding_a + stack_padding_b + 0.5,
          input * 2};
}
LargeReturn large_return(std::uint64_t input) {
  return {input, input + 1, input + 2};
}
NontrivialReturn nontrivial_return(int input) { return NontrivialReturn(input); }
__attribute__((visibility("default"), noinline)) int
nontrivial_copy_count() {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return NontrivialReturn::copies() + stack_padding_a + stack_padding_b;
}
__attribute__((visibility("default"), noinline)) int
nontrivial_move_count() {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return NontrivialReturn::moves() + stack_padding_a + stack_padding_b;
}
__attribute__((visibility("default"), noinline)) int
nontrivial_destruction_count() {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return NontrivialReturn::destructions() + stack_padding_a + stack_padding_b;
}

__attribute__((visibility("default"), noinline)) bool boolean_target(bool value) {
  return !value;
}

__attribute__((visibility("default"), noinline)) bool
string_target(const std::string &value) {
  return value.empty();
}

__attribute__((visibility("default"), noinline)) int
signature_target(Record &record, int increment) {
  record.value += increment;
  return record.score() + increment;
}

__attribute__((visibility("default"), noinline)) int
record_pointer_target(Record **record) {
  return (*record)->value;
}

__attribute__((visibility("default"), noinline)) int
record_pointer_single_target(Record *record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return record->value + stack_padding_a + stack_padding_b;
}

__attribute__((visibility("default"), noinline)) int
record_pointer_reference_target(Record *&record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  return record->value + stack_padding_a + stack_padding_b;
}

__attribute__((visibility("default"), noinline)) Record *
record_pointer_return_target(Record &record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  (void)stack_padding_a;
  (void)stack_padding_b;
  return &record;
}

__attribute__((visibility("default"), noinline)) Record *&
record_pointer_reference_return_target(Record *&record) {
  volatile int stack_padding_a = 0;
  volatile int stack_padding_b = 0;
  (void)stack_padding_a;
  (void)stack_padding_b;
  return record;
}
} // namespace signature_fixture

namespace {
volatile std::sig_atomic_t keep_running = 1;
void stop_target(int) { keep_running = 0; }
} // namespace

int main() {
  std::signal(SIGINT, stop_target);
  std::signal(SIGTERM, stop_target);
  signature_fixture::Record record;
  signature_fixture::Record *record_pointer = &record;
  signature_fixture::AbiRecord abi_record;
  const std::string text = "signature fixture";
  std::printf("SIGNATURE_FIXTURE pid=%ld\n", static_cast<long>(::getpid()));
  std::fflush(stdout);
  while (keep_running != 0) {
    volatile int result = signature_fixture::signature_target(record, 1);
    volatile int const_record_value_result =
        signature_fixture::const_record_value_target(record);
    volatile int pointer_result =
        signature_fixture::record_pointer_target(&record_pointer);
    volatile int single_pointer_result =
        signature_fixture::record_pointer_single_target(&record);
    volatile int pointer_reference_result =
        signature_fixture::record_pointer_reference_target(record_pointer);
    volatile auto returned_record_pointer =
        signature_fixture::record_pointer_return_target(record);
    volatile auto &returned_record_pointer_reference =
        signature_fixture::record_pointer_reference_return_target(
            record_pointer);
    volatile int reference_result =
        signature_fixture::record_reference_return(record).value;
    volatile int abi_result = abi_record.score();
    volatile auto small = signature_fixture::small_return(11);
    volatile auto mixed = signature_fixture::mixed_return(13);
    volatile auto large = signature_fixture::large_return(17);
    volatile int nontrivial =
        signature_fixture::nontrivial_return(19).value;
    volatile int copies = signature_fixture::nontrivial_copy_count();
    volatile int moves = signature_fixture::nontrivial_move_count();
    volatile int destructions =
        signature_fixture::nontrivial_destruction_count();
    volatile bool boolean = signature_fixture::boolean_target(true);
    volatile bool string_boolean = signature_fixture::string_target(text);
    (void)result;
    (void)const_record_value_result;
    (void)pointer_result;
    (void)single_pointer_result;
    (void)pointer_reference_result;
    (void)returned_record_pointer;
    (void)returned_record_pointer_reference;
    (void)reference_result;
    (void)abi_result;
    (void)small;
    (void)mixed;
    (void)large;
    (void)nontrivial;
    (void)copies;
    (void)moves;
    (void)destructions;
    (void)boolean;
    (void)string_boolean;
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
}
