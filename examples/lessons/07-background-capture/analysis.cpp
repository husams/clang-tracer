// Run with %cpp load analysis.cpp after `range` has captured two POST events.
using namespace tracer_repl;

auto snapshot = range.snapshot();
auto sums = snapshot
    | filter([](const Event& event) { return event.phase() == "POST"; })
    | transform([](const Event& event) { return event.result_as<int>(); })
    | to_vector();
for (auto value : sums) std::cout << value << '\n';
range.stop();
