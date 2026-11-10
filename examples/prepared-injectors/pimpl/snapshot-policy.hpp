#pragma once

namespace tracer_snapshot {

template <class Record> struct FollowPointers;

template <> struct FollowPointers<HFW::Item> {
  static constexpr const char *fields[] = {"_imp", "_other", "_stamp"};
};

} // namespace tracer_snapshot
