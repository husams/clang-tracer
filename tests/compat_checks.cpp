#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "tracer/compat.hpp"

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

using tracer::compat::span;

static_assert(std::is_same<tracer::compat::remove_cvref_t<const int&>, int>::value,
              "remove_cvref_t removes references and cv qualifiers");
static_assert(std::is_constructible<span<int>, int*, std::size_t>::value,
              "span can be constructed from pointer and count");
static_assert(std::is_constructible<span<const int>, span<int>>::value,
              "mutable span converts to a const-element span");
static_assert(!std::is_constructible<span<int>, span<const int>>::value,
              "const elements cannot convert to mutable span");
static_assert(std::is_constructible<span<const int>, const int*, std::size_t>::value,
              "const span accepts const data");
static_assert(!std::is_constructible<span<int>, const int*, std::size_t>::value,
              "mutable span rejects const data");
static_assert(!std::is_constructible<span<int>, const std::vector<int>&>::value,
              "mutable span rejects a const vector");
static_assert(!std::is_constructible<span<int>, const std::array<int, 3>&>::value,
              "mutable span rejects a const std::array");

struct alignas(32) AlignedValue {
  explicit AlignedValue(int number)
      : value(number) {}

  int value;
};

struct StringRefLike {
  const char* data() const { return text; }

  std::size_t size() const { return length; }

  const char* text;
  std::size_t length;
};
} // namespace

int main() {
  int values[] = {3, 5, 8, 13};
  span borrowed(values);
  require(borrowed.size() == 4 && borrowed.size_bytes() == sizeof(values),
          "array deduction did not produce the expected span");
  borrowed[1] = 21;
  require(values[1] == 21, "span mutation did not reach the borrowed array");

  std::array<int, 3> array{{2, 4, 6}};
  span from_array(array);
  std::vector<int> vector{7, 9, 11};
  span from_vector(vector);
  require(from_array.size() == 3 && from_vector.size() == 3 && from_vector[2] == 11,
          "CTAD from std::array or std::vector failed");

  const span<const int> immutable(values);
  require(immutable[0] == 3 && immutable.size() == 4,
          "const-element span did not preserve the source values");
  const auto empty = borrowed.subspan(borrowed.size());
  const auto middle = borrowed.subspan(1, 2);
  require(empty.empty() && empty.data() == values + 4 && middle.size() == 2 && middle[0] == 21 &&
              middle[1] == 8,
          "empty or bounded subspan has incorrect view semantics");
  const auto first = borrowed.first(2);
  require(first.size() == 2 && first[1] == 21, "first() did not return the requested prefix");

  auto bytes = tracer::compat::as_bytes(borrowed);
  require(bytes.size() == sizeof(values), "as_bytes returned the wrong size");
  auto writable = tracer::compat::as_writable_bytes(borrowed);
  const int replacement = 34;
  std::memcpy(writable.data(), &replacement, sizeof(replacement));
  require(values[0] == replacement, "as_writable_bytes did not refer to the original storage");

  const float negative_zero = -0.0f;
  const std::uint32_t negative_zero_bits = tracer::compat::bit_cast<std::uint32_t>(negative_zero);
  require((negative_zero_bits & 0x80000000u) != 0, "bit_cast lost the sign bit of negative zero");
  const float restored = tracer::compat::bit_cast<float>(negative_zero_bits);
  require(restored == 0.0f && std::signbit(restored), "bit_cast did not round-trip negative zero");
  const std::uint32_t pattern = 0x3f800000u;
  require(tracer::compat::bit_cast<float>(pattern) == 1.0f,
          "bit_cast did not preserve the requested float representation");

  require(tracer::compat::countr_zero(0u) == 32,
          "countr_zero(0) must return the unsigned type's bit width");
  require(tracer::compat::countr_zero(0x40u) == 6 && tracer::compat::countr_zero(0x80000000u) == 31,
          "countr_zero returned an incorrect count");

  typename std::aligned_storage<sizeof(AlignedValue), alignof(AlignedValue)>::type storage;
  auto* aligned = reinterpret_cast<AlignedValue*>(&storage);
  AlignedValue* constructed = tracer::compat::construct_at(aligned, 42);
  require(constructed == aligned && constructed->value == 42 &&
              reinterpret_cast<std::uintptr_t>(constructed) % alignof(AlignedValue) == 0,
          "construct_at did not initialize correctly aligned storage");
  constructed->~AlignedValue();

  const std::map<std::string, int> lookup{{"alpha", 1}, {"beta", 2}};
  require(tracer::compat::contains(lookup, std::string("alpha")) &&
              tracer::compat::contains(lookup, "beta") &&
              !tracer::compat::contains(lookup, "gamma"),
          "contains did not handle associative containers and literals");
  const std::string text = "compatibility";
  const std::string_view view(text);
  const StringRefLike ref{text.data(), text.size()};
  require(tracer::compat::contains(text, "pat") &&
              tracer::compat::contains(view, std::string("bility")) &&
              tracer::compat::contains(ref, "tibility") &&
              !tracer::compat::contains(text, "missing"),
          "contains did not handle string and string_view values");
  require(tracer::compat::starts_with(text, "comp") &&
              tracer::compat::starts_with(view, std::string("compat")) &&
              tracer::compat::starts_with(ref, "compat") && tracer::compat::starts_with(text, "") &&
              tracer::compat::ends_with(text, "ility") &&
              tracer::compat::ends_with(view, std::string("ibility")) &&
              tracer::compat::ends_with(ref, "ibility") && tracer::compat::ends_with(text, ""),
          "starts_with or ends_with failed for views, strings, or empty patterns");
  require(!tracer::compat::starts_with(text, "pat") && !tracer::compat::ends_with(view, "compat"),
          "starts_with or ends_with accepted a nonmatching pattern");
  return 0;
}
