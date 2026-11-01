#include <exception>
#include <iostream>

void verify_context_schema_roundtrip();

int main() {
  try {
    verify_context_schema_roundtrip();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
