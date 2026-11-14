// Run with `%cpp load report.cpp` after `%collect ... --as items`.
// Prints each created item: the handle, the implementation the factory
// chose, and the fields that implementation stores.
for (const auto& event : tracer.batch("items")) {
  auto item = event.result_snapshot();
  std::cout << "item " << item.field("id").as<int>() << ": ";
  if (item.fields().count("impl_snapshot") == 0) {
    std::cout << "no implementation captured\n";
    continue;
  }
  const auto& impl = item.field("impl_snapshot");
  const std::string type = impl.cpp_type();
  std::cout << type << '\n';

  if (type == "ItemV1" || type == "ItemV2")
    std::cout << "  label=" << impl.field("label").as<std::string>()
              << " revision=" << impl.field("revision").as<int>() << '\n';

  if (type == "ItemV2") {
    std::cout << "  tags:";
    for (const auto& tag : impl.field("tags").as<std::vector<std::string>>())
      std::cout << ' ' << tag;
    std::cout << "\n  attributes:";
    for (const auto& [name, value] :
         impl.field("attributes").as<std::map<std::string, int>>())
      std::cout << ' ' << name << '=' << value;
    std::cout << '\n';
  }

  if (type == "ItemV3") {
    // Decode the whole implementation into a local ItemV3, containers
    // included. It is rebuilt field by field, so reach it with `->`.
    auto v3 = impl.as<ItemV3>();
    std::cout << "  title=" << v3->title << '\n';
    for (const auto& part : v3->parts) {
      std::cout << "  part " << part.name << ' ' << part.size.width << 'x'
                << part.size.height << " batches:";
      for (int batch : part.batches) std::cout << ' ' << batch;
      std::cout << '\n';
    }
    for (const auto& [warehouse, counts] : v3->stock) {
      std::cout << "  stock " << warehouse << ':';
      for (int count : counts) std::cout << ' ' << count;
      std::cout << '\n';
    }
    for (const auto& change : v3->history) {
      std::cout << "  change:";
      for (const auto& word : change) std::cout << ' ' << word;
      std::cout << '\n';
    }
  }
}
