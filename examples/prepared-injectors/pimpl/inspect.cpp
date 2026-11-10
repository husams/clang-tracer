struct LocalFields {
  int version;
  int private_code;
};
namespace tracer_repl {
template <> struct EventCodec<::LocalFields> {
  static ::LocalFields decode(const EventValue &value) {
    const auto &implementation = value.field("_imp_snapshot");
    if (implementation.cpp_type() != "HFW::ItemV1")
      throw std::runtime_error("unexpected implementation type");
    return {implementation.field("version").as<int>(),
            implementation.field("private_code").as<int>()};
  }
};
}
bool check_pimpl(const tracer_repl::Collection &captured, bool pointer_mode) {
  unsigned matching = 0, other = 0, null_members = 0, null_outer = 0;
  bool correct = captured.size() == 4;
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
    ++matching;
    const auto &implementation = root.field("_imp_snapshot");
    correct = correct && implementation.cpp_type() == "HFW::ItemV1" &&
        implementation.field("version").as<int>() == 1 &&
        implementation.field("label").as<std::string>() == "live ItemV1" &&
        implementation.field("details").field("build").as<int>() == 27 &&
        implementation.field("details").field("stable").as<bool>() &&
        implementation.field("private_code").as<int>() == 808;
    auto local = event.result_as<LocalFields>();
    correct = correct && local.version == 1 && local.private_code == 808;
    std::cout << implementation.cpp_type() << " version=" << local.version
              << " private_code=" << local.private_code << '\n';
  }
  return correct && matching >= 1 && other >= 1 && null_members >= 1 &&
      (!pointer_mode || null_outer >= 1);
}
