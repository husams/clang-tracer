struct CustomFields {
  int summary;
};

namespace tracer_repl {
template <> struct EventCodec<::CustomFields> {
  static ::CustomFields decode(const EventValue &value) {
    return {value.field("_imp_snapshot").field("summary").as<int>()};
  }
};
}

const char *custom_expected_type(int id, bool secondary) {
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

bool check_custom_implementation(const tracer_repl::EventValue &implementation,
                                 const char *type) {
  if (implementation.cpp_type() != type)
    return false;
  if (std::string(type) == "HFW::ItemV2")
    return implementation.fields().size() == 1 &&
        implementation.field("summary").as<int>() == 1002;
  if (std::string(type) == "HFW::ItemV1")
    return implementation.field("label").as<std::string>().find("ItemV1") !=
            std::string::npos &&
        implementation.field("details").field("build").as<int>() == 27 &&
        implementation.field("private_code").as<int>() == 808;
  if (std::string(type) == "HFW::ItemV3")
    return implementation.field("description").as<std::string>().find("ItemV3") !=
            std::string::npos &&
        implementation.field("metadata").field("checksum").as<int>() == 313 &&
        implementation.field("private_trusted").as<bool>();
  return false;
}

bool check_custom_item(const tracer_repl::EventValue &root) {
  const int id = root.field("id").as<int>();
  if (root.field("_stamp").as<std::uint64_t>() == 0 ||
      root.field("_stamp_snapshot").field("serial").as<int>() != id + 700)
    return false;
  for (const auto &member : {std::string("_imp"), std::string("_other")}) {
    const bool secondary = member == "_other";
    const auto address = root.field(member).as<std::uint64_t>();
    const char *type = custom_expected_type(id, secondary);
    const auto snapshot = member + "_snapshot";
    const bool present = root.fields().count(snapshot) != 0;
    if ((id == 505 && !secondary) || (id == 606 && secondary)) {
      if (address != 0 || present) return false;
    } else if (id == 404 && !secondary) {
      if (address == 0 || present) return false;
    } else if (!type || address == 0 || !present ||
               !check_custom_implementation(root.field(snapshot), type)) {
      return false;
    }
  }
  return true;
}

bool check_custom_pimpl(const tracer_repl::Collection &captured) {
  unsigned versions[3] = {}, unknown = 0, null_members = 0, null_outer = 0;
  bool correct = captured.size() == 7;
  for (const auto &event : captured) {
    if (event.result() == "0x0") {
      ++null_outer;
      continue;
    }
    auto root = event.result_snapshot();
    correct = correct && check_custom_item(root);
    const auto address = root.field("_imp").as<std::uint64_t>();
    if (address == 0) {
      ++null_members;
      continue;
    }
    if (root.fields().count("_imp_snapshot") == 0) {
      ++unknown;
      continue;
    }
    const auto &implementation = root.field("_imp_snapshot");
    const auto &type = implementation.cpp_type();
    if (type == "HFW::ItemV1") {
      ++versions[0];
    } else if (type == "HFW::ItemV2") {
      ++versions[1];
      auto projected = event.result_as<CustomFields>();
      correct = correct && projected.summary == 1002;
      std::cout << type << " summary=" << projected.summary << '\n';
    } else if (type == "HFW::ItemV3") {
      ++versions[2];
    } else {
      correct = false;
    }
  }
  return correct && versions[0] >= 1 && versions[1] >= 1 && versions[2] >= 1 &&
      unknown >= 1 && null_members >= 1 && null_outer >= 1;
}
