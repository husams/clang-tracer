// Run with `%cpp load encoded.cpp` after `%collect ... --as encoded`.
// Item::encode is the same function for every item. The receiver snapshot
// tells which implementation each call was dispatched to.
for (const auto& event : tracer.batch("encoded")) {
  auto item = event.receiver_snapshot();
  std::cout << "item " << item.field("id").as<int>() << " ("
            << item.field("impl_snapshot").cpp_type() << ") encode("
            << event.argument_as<std::string>(0) << ") = "
            << event.result_as<std::string>() << '\n';
}
