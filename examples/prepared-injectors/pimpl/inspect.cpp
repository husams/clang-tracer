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
bool check_pimpl(const tracer_repl::Collection &captured, bool pointer_mode) {
  unsigned versions[3] = {}, other = 0, null_members = 0, null_outer = 0;
  bool correct = captured.size() == 6;
  for (const auto &event : captured) {
    if (pointer_mode && event.result() == "0x0") {
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
      ++other;
      continue;
    }
    const auto &implementation = root.field("_imp_snapshot");
    auto local = event.result_as<LocalFields>();
    const auto &type = implementation.cpp_type();
    if (type == "HFW::ItemV1") {
      ++versions[0];
      correct = correct && local.version == 1 && local.private_value == 808 &&
          implementation.field("label").as<std::string>() == "live ItemV1" &&
          implementation.field("details").field("build").as<int>() == 27 &&
          implementation.field("details").field("stable").as<bool>() &&
          implementation.field("private_code").as<int>() == 808;
    } else if (type == "HFW::ItemV2") {
      ++versions[1];
      correct = correct && local.version == 2 && local.private_value == 909 &&
          implementation.field("owner").as<std::string>() == "live ItemV2" &&
          implementation.field("counters").field("attempts").as<int>() == 12 &&
          implementation.field("counters").field("migrated").as<bool>() &&
          implementation.field("private_tag").as<long>() == 909;
    } else if (type == "HFW::ItemV3") {
      ++versions[2];
      correct = correct && local.version == 3 && local.private_value == 1 &&
          implementation.field("description").as<std::string>() == "live ItemV3" &&
          implementation.field("metadata").field("region").as<short>() == 7 &&
          implementation.field("metadata").field("checksum").as<int>() == 313 &&
          implementation.field("private_trusted").as<bool>();
    } else {
      correct = false;
    }
    std::cout << type << " version=" << local.version
              << " private=" << local.private_value << '\n';
  }
  return correct && versions[0] >= 1 && versions[1] >= 1 && versions[2] >= 1 &&
      other >= 1 && null_members >= 1 &&
      (!pointer_mode || null_outer >= 1);
}
