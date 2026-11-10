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

bool check_custom_pimpl(const tracer_repl::Collection &captured) {
  unsigned versions[3] = {}, unknown = 0, null_members = 0, null_outer = 0;
  bool correct = captured.size() == 6;
  for (const auto &event : captured) {
    if (event.result() == "0x0") {
      ++null_outer;
      continue;
    }
    auto root = event.result_snapshot();
    const auto address = root.field("_imp").as<std::uint64_t>();
    if (address == 0) {
      ++null_members;
      correct = correct && root.fields().count("_imp_snapshot") == 0;
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
      correct = correct &&
          implementation.field("label").as<std::string>() == "live ItemV1" &&
          implementation.field("details").field("build").as<int>() == 27 &&
          implementation.field("private_code").as<int>() == 808;
    } else if (type == "HFW::ItemV2") {
      ++versions[1];
      const auto &fields = implementation.fields();
      auto projected = event.result_as<CustomFields>();
      correct = correct && fields.size() == 1 &&
          implementation.field("summary").as<int>() == 1002 &&
          projected.summary == 1002;
      std::cout << type << " summary=" << projected.summary << '\n';
    } else if (type == "HFW::ItemV3") {
      ++versions[2];
      correct = correct &&
          implementation.field("description").as<std::string>() == "live ItemV3" &&
          implementation.field("metadata").field("checksum").as<int>() == 313 &&
          implementation.field("private_trusted").as<bool>();
    } else {
      correct = false;
    }
  }
  return correct && versions[0] >= 1 && versions[1] >= 1 && versions[2] >= 1 &&
      unknown >= 1 && null_members >= 1 && null_outer >= 1;
}
