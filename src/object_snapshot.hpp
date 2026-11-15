#pragma once

#include "repl/event_value.hpp"
#include "context_transport.pb.h"
#include "tracer.pb.h"

namespace tracer_repl {
using Snapshot = clangtracer::context::v1::ValueSnapshot;
std::string context_descriptor_type(std::string_view cpp_type);

EventValue decode_owned_value(
    const std::string &cpp_type, const std::string &root_type,
    const std::string &descriptors, const std::string &payload,
    const google::protobuf::RepeatedPtrField<std::string> &roles,
    const google::protobuf::RepeatedPtrField<std::string> &types,
    bool context_state, bool wrapped);
EventValue decode_snapshot(const Snapshot &snapshot, bool include_members = true);

struct SnapshotChange {
  std::string path;
  std::string detail;
  bool gap;
  std::optional<EventValue> before = {};
  std::optional<EventValue> after = {};
};

void encode_observation_value(const EventValue &value,
    clangtracer::v1::ObjectObservation::SnapshotValue &output);

void validate_snapshot_keys(const std::vector<std::string> &keys);
std::vector<SnapshotChange> compare_snapshots(const Snapshot &before, const Snapshot &after,
                                             const std::vector<std::string> &keys,
                                             const std::vector<std::string> &fields = {});
} // namespace tracer_repl
