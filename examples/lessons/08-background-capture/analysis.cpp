// Run with `%cpp load analysis.cpp` once `range` exists, then trigger two calls.
// Iteration waits for each event until the range finishes.
auto sums = range
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); });
for (auto value : sums) std::cout << value << '\n';
range.stop();
