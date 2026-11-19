#pragma once

namespace tracer_snapshot {

template <class Record> struct FollowPointers;

template <> struct FollowPointers<HFW::HfwItem> {
  static constexpr const char *fields[] = {"_impl"};
};

} // namespace tracer_snapshot
