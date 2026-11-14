#pragma once

namespace tracer_snapshot {
template <class Record> struct FollowPointers;
template <> struct FollowPointers<snapshot_test::State> {
  static constexpr const char *fields[] = {"child_", "alias_"};
};
template <class Record> struct AnyPayloads;
template <> struct AnyPayloads<snapshot_test::State> {
  using extension_ = std::tuple<snapshot_test::Registered>;
};
}
