#include <fstream>

#include "callback_bridge.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    return 1;
  }
  std::ofstream output(argv[1]);
  output << tracer::make_class_callback_bridge(
      {"Interceptor", true, false, true, false});
  return output ? 0 : 1;
}
