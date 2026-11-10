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

bool check_auto_implementation(const tracer_repl::EventValue &implementation,
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

const char *auto_expected_type(int id, bool secondary) {
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

bool check_auto_item(const tracer_repl::EventValue &root) {
  const int id = root.field("id").as<int>();
  if (id < 101 || id > 606 ||
      root.field("_stamp").as<std::uint64_t>() == 0 ||
      root.fields().count("_stamp_snapshot") != 0)
    return false;
  if (root.field("_unreadable").as<std::uint64_t>() == 0 ||
      root.fields().count("_unreadable_snapshot") != 0)
    return false;

  for (const auto &member : {std::string("_imp"), std::string("_other")}) {
    const bool secondary = member == "_other";
    const auto address = root.field(member).as<std::uint64_t>();
    const char *type = auto_expected_type(id, secondary);
    const auto snapshot_name = member + "_snapshot";
    const bool present = root.fields().count(snapshot_name) != 0;
    if ((id == 505 && !secondary) || (id == 606 && secondary)) {
      if (address != 0 || present) return false;
    } else if (id == 404 && !secondary) {
      // This implementation is local to item.cpp and absent from prepared headers.
      if (address == 0 || present) return false;
    } else if (!type || address == 0 || !present ||
               !check_auto_implementation(root.field(snapshot_name), type)) {
      return false;
    }
  }
  return true;
}

bool check_auto_pimpl(const tracer_repl::Collection &captured,
                      bool pointer_mode) {
  unsigned versions[3] = {}, unmapped = 0, null_member = 0, null_outer = 0;
  bool correct = captured.size() == (pointer_mode ? 7 : 12);
  for (const auto &event : captured) {
    if (pointer_mode && event.result() == "0x0") {
      ++null_outer;
      continue;
    }

    const auto root = event.result_snapshot();
    correct = correct && check_auto_item(root);
    const auto address = root.field("_imp").as<std::uint64_t>();
    if (address == 0) {
      ++null_member;
      continue;
    }
    if (root.fields().count("_imp_snapshot") == 0) {
      ++unmapped;
      continue;
    }

    const auto &implementation = root.field("_imp_snapshot");
    auto local = event.result_as<LocalFields>();
    const auto &type = implementation.cpp_type();
    if (type == "HFW::ItemV1") {
      ++versions[0];
      correct = correct && local.version == 1 && local.private_value == 808;
    } else if (type == "HFW::ItemV2") {
      ++versions[1];
      correct = correct && local.version == 2 && local.private_value == 909;
    } else if (type == "HFW::ItemV3") {
      ++versions[2];
      correct = correct && local.version == 3 && local.private_value == 1;
    } else {
      correct = false;
    }
  }
  return correct && versions[0] >= 1 && versions[1] >= 1 && versions[2] >= 1 &&
      unmapped >= 1 && null_member >= 1 &&
      (!pointer_mode || null_outer >= 1);
}
