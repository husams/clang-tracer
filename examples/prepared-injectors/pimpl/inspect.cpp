struct LocalFields {
  int version;
  int private_value;
};
namespace tracer_repl {
template <> struct EventCodec<::LocalFields> {
  static ::LocalFields decode(const EventValue &value) {
    const auto &implementation = value.field("_imp_snapshot");
    const auto &type = implementation.cpp_type();
    if (type == "HFW::ItemV1")
      return {implementation.field("version").as<int>(),
              implementation.field("private_code").as<int>()};
    if (type == "HFW::ItemV2")
      return {static_cast<int>(implementation.field("generation").as<unsigned>()),
              static_cast<int>(implementation.field("private_tag").as<long>())};
    if (type == "HFW::ItemV3")
      return {implementation.field("schema").as<int>(),
              implementation.field("private_trusted").as<bool>() ? 1 : 0};
    throw std::runtime_error("unexpected implementation type");
  }
};
}
bool check_implementation(const tracer_repl::EventValue &implementation,
                          const std::string &type) {
  if (implementation.cpp_type() != type)
    return false;
  if (type == "HFW::ItemV1")
    return implementation.field("version").as<int>() == 1 &&
        implementation.field("label").as<std::string>().find("ItemV1") !=
            std::string::npos &&
        implementation.field("details").field("build").as<int>() == 27 &&
        implementation.field("details").field("stable").as<bool>() &&
        implementation.field("private_code").as<int>() == 808;
  if (type == "HFW::ItemV2")
    return implementation.field("generation").as<unsigned>() == 2 &&
        implementation.field("owner").as<std::string>().find("ItemV2") !=
            std::string::npos &&
        implementation.field("counters").field("attempts").as<int>() == 12 &&
        implementation.field("counters").field("migrated").as<bool>() &&
        implementation.field("private_tag").as<long>() == 909;
  if (type == "HFW::ItemV3")
    return implementation.field("schema").as<int>() == 3 &&
        implementation.field("description").as<std::string>().find("ItemV3") !=
            std::string::npos &&
        implementation.field("metadata").field("region").as<short>() == 7 &&
        implementation.field("metadata").field("checksum").as<int>() == 313 &&
        implementation.field("private_trusted").as<bool>();
  return false;
}

const char *expected_member_type(int id, bool secondary) {
  if (secondary) {
    if (id == 101 || id == 505) return "HFW::ItemV2";
    if (id == 202) return "HFW::ItemV3";
    if (id == 303 || id == 404) return "HFW::ItemV1";
    return nullptr;
  }
  if (id == 101) return "HFW::ItemV1";
  if (id == 202) return "HFW::ItemV2";
  if (id == 303 || id == 606) return "HFW::ItemV3";
  return nullptr;
}

bool check_item(const tracer_repl::EventValue &root) {
  const int id = root.field("id").as<int>();
  if (id < 101 || id > 606)
    return false;
  if (root.field("_stamp").as<std::uint64_t>() == 0 ||
      root.field("_stamp_snapshot").field("serial").as<int>() != id + 700)
    return false;
  for (const auto &member : {std::string("_imp"), std::string("_other")}) {
    const bool secondary = member == "_other";
    const char *type = expected_member_type(id, secondary);
    const auto address = root.field(member).as<std::uint64_t>();
    const auto snapshot_name = member + "_snapshot";
    const bool present = root.fields().count(snapshot_name) != 0;
    if (id == 505 && !secondary) {
      if (address != 0 || present) return false;
    } else if (id == 606 && secondary) {
      if (address != 0 || present) return false;
    } else if (id == 404 && !secondary) {
      if (address == 0 || present) return false;
    } else {
      if (!type || address == 0 || !present ||
          !check_implementation(root.field(snapshot_name), type))
        return false;
    }
  }
  return true;
}

bool check_pimpl(const tracer_repl::Collection &captured, bool pointer_mode) {
  unsigned versions[3] = {}, unknown = 0, null_imp = 0, null_outer = 0;
  // The target calls referenceItem twice per iteration to populate the role demo.
  bool correct = captured.size() == (pointer_mode ? 7 : 12);
  for (const auto &event : captured) {
    if (pointer_mode && event.result() == "0x0") {
      ++null_outer;
      continue;
    }
    auto root = event.result_snapshot();
    const auto address = root.field("_imp").as<std::uint64_t>();
    if (address == 0) {
      ++null_imp;
      correct = correct && root.fields().count("_imp_snapshot") == 0;
      correct = correct && check_item(root);
      continue;
    }
    if (root.fields().count("_imp_snapshot") == 0) {
      ++unknown;
      correct = correct && check_item(root);
      continue;
    }
    const auto &implementation = root.field("_imp_snapshot");
    correct = correct && check_item(root);
    auto local = event.result_as<LocalFields>();
    const auto &type = implementation.cpp_type();
    if (type == "HFW::ItemV1") {
      ++versions[0];
      correct = correct && local.version == 1 && local.private_value == 808 &&
          check_implementation(implementation, "HFW::ItemV1");
    } else if (type == "HFW::ItemV2") {
      ++versions[1];
      correct = correct && local.version == 2 && local.private_value == 909 &&
          check_implementation(implementation, "HFW::ItemV2");
    } else if (type == "HFW::ItemV3") {
      ++versions[2];
      correct = correct && local.version == 3 && local.private_value == 1 &&
          check_implementation(implementation, "HFW::ItemV3");
    } else {
      correct = false;
    }
    std::cout << type << " version=" << local.version
              << " private=" << local.private_value << '\n';
  }
  return correct && versions[0] >= 1 && versions[1] >= 1 && versions[2] >= 1 &&
      unknown >= 1 && null_imp >= 1 &&
      (!pointer_mode || null_outer >= 1);
}

bool check_item_roles(const tracer_repl::Collection &captured) {
  bool correct = captured.size() == 6;
  unsigned receiver_null_imp = 0, argument_null_imp = 0;
  unsigned receiver_null_other = 0, argument_null_other = 0;
  for (const auto &event : captured) {
    const auto receiver = event.receiver_snapshot();
    const auto argument = event.argument_snapshot(0);
    correct = correct && receiver.cpp_type() == "HFW::Item" &&
        argument.cpp_type() == "HFW::Item" && check_item(receiver) &&
        check_item(argument) &&
        event.result_as<int>() == receiver.field("id").as<int>() -
                                      argument.field("id").as<int>();
    receiver_null_imp += receiver.field("_imp").as<std::uint64_t>() == 0;
    argument_null_imp += argument.field("_imp").as<std::uint64_t>() == 0;
    receiver_null_other += receiver.field("_other").as<std::uint64_t>() == 0;
    argument_null_other += argument.field("_other").as<std::uint64_t>() == 0;
  }
  return correct && receiver_null_imp >= 1 && argument_null_imp >= 1 &&
      receiver_null_other >= 1 && argument_null_other >= 1;
}
