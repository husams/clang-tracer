#include "bounded_proto.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace ce = tracer::context_engine;
namespace {
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void wire_values() {
  std::array<std::byte, 256> storage{};
  ce::BoundedProtoWriter out(storage);
  require(out.u64(1, 150) && out.s64(2, -1) && out.boolean(3, true) &&
              out.string(4, "abc"),
          "scalar write failed");
  require(out.begin_message(5) && out.u64(1, 7) && out.end_message(),
          "message write failed");
  const std::array<unsigned char, 16> expected{
      0x08, 0x96, 0x01, 0x10, 0x01, 0x18, 0x01, 0x22,
      0x03, 'a',  'b',  'c',  0x2a, 0x02, 0x08, 0x07};
  const auto bytes = out.finish();
  require(bytes.size() == expected.size(), "wire size differs");
  for (std::size_t i = 0; i < bytes.size(); ++i)
    require(std::to_integer<unsigned char>(bytes[i]) == expected[i],
            "noncanonical wire bytes");
}
void sticky_limits() {
  std::array<std::byte, 8> storage{};
  ce::BoundedProtoWriter out(storage);
  require(out.u64(1, 1) && !out.string(2, "too-long"),
          "byte limit not enforced");
  require(!out.u64(3, 0) && out.finish().empty() &&
              out.status() == ce::CaptureStatus::bytes_exceeded,
          "incomplete event remained visible after failure");
  ce::CaptureLimits limits;
  limits.string_bytes = 1;
  ce::BoundedProtoWriter strings(storage, limits);
  require(!strings.string(1, "xx") &&
              strings.status() == ce::CaptureStatus::string_exceeded,
          "string limit not enforced");
  ce::BoundedProtoWriter fields(storage);
  require(!fields.u64(19000, 1) &&
              fields.status() == ce::CaptureStatus::invalid_field,
          "reserved field admitted");
}
void container_budgets() {
  std::array<std::byte, 8> storage{};
  ce::BoundedProtoWriter out(storage);
  std::size_t selected = 99;
  require(out.container_items(std::numeric_limits<std::uint64_t>::max(), 0,
                              selected) &&
              selected == 0,
          "zero cap visits omitted elements");
  require(out.container_items(5000, 4096, selected) && selected == 4096,
          "item cap not applied");
  require(!out.container_items(1, 100, selected) && selected == 0 &&
              out.status() == ce::CaptureStatus::items_exceeded,
          "aggregate item budget reset");
  ce::BoundedProtoWriter invalid(storage);
  require(!invalid.container_items(1, 4097, selected) &&
              invalid.status() == ce::CaptureStatus::invalid_cap,
          "invalid cap admitted");
}
void nesting_and_work() {
  std::array<std::byte, 1024> storage{};
  ce::CaptureLimits limits;
  limits.depth = 2;
  ce::BoundedProtoWriter out(storage, limits);
  require(out.begin_message(1) && out.begin_message(1) &&
              !out.begin_message(1) &&
              out.status() == ce::CaptureStatus::depth_exceeded,
          "nesting limit not enforced");
  ce::BoundedProtoWriter incomplete(storage);
  require(incomplete.begin_message(1) && incomplete.finish().empty() &&
              incomplete.status() == ce::CaptureStatus::incomplete_message,
          "open message published");
  limits.visits = 1;
  ce::BoundedProtoWriter visits(storage, limits);
  require(visits.u64(1, 1) && !visits.u64(2, 2) &&
              visits.status() == ce::CaptureStatus::visits_exceeded,
          "visit budget not enforced");
  limits.visits = 100;
  limits.byte_work = 1;
  ce::BoundedProtoWriter work(storage, limits);
  require(!work.u64(1, 1) && work.status() == ce::CaptureStatus::work_exceeded,
          "work budget not enforced");
}
void numeric_edges() {
  std::array<std::byte, 64> storage{};
  ce::BoundedProtoWriter out(storage);
  require(out.s64(1, std::numeric_limits<std::int64_t>::min()) &&
              out.u64(2, std::numeric_limits<std::uint64_t>::max()) &&
              out.f32(3, 1.0f) && out.f64(4, 1.0),
          "numeric edge write failed");
  const auto bytes = out.finish();
  require(bytes.size() == 36 && bytes[1] == std::byte{255} &&
              bytes[10] == std::byte{1} && bytes[25] == std::byte{128} &&
              bytes[26] == std::byte{63} && bytes[35] == std::byte{63},
          "numeric edge bytes differ");
}

void exact_fit_and_multibyte_lengths() {
  std::array<std::byte, 2> exact{};
  ce::BoundedProtoWriter empty(exact);
  require(empty.begin_message(1) && empty.end_message() &&
              empty.finish().size() == 2,
          "empty message needs unaccounted scratch bytes");
  std::array<std::byte, 256> storage{};
  std::array<char, 128> value{};
  ce::BoundedProtoWriter nested(storage);
  require(nested.begin_message(1) &&
              nested.string(1, std::string_view(value.data(), value.size())) &&
              nested.end_message(),
          "multibyte length relocation failed");
  const auto bytes = nested.finish();
  require(bytes.size() == 134 && bytes[0] == std::byte{10} &&
              bytes[1] == std::byte{131} && bytes[2] == std::byte{1} &&
              bytes[3] == std::byte{10} && bytes[4] == std::byte{128} &&
              bytes[5] == std::byte{1},
          "nested length prefix bytes differ");
}
} // namespace
int main() {
  try {
    wire_values();
    sticky_limits();
    container_budgets();
    nesting_and_work();
    numeric_edges();
    exact_fit_and_multibyte_lengths();
    std::cout << "6 bounded Protobuf writer checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
