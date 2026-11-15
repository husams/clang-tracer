// Run with `%cpp load report.cpp` after `%collect ... --as requests`.
// Prints one line per request with every call that request made.
for (const auto& event : tracer.batch("requests")) {
  auto state = event.state_snapshot();
  std::cout << "request " << state.field("request").as<int>() << ':';
  for (const auto& step : state.field("steps").elements())
    std::cout << ' ' << step.field("function").as<std::string>() << '('
              << step.field("argument").as<int>() << ")="
              << step.field("result").as<int>();
  std::cout << '\n';
}
