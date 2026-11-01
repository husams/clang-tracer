#include <exception>
#include <iostream>

void verify_context_delivery();

int main() {
  try {
    verify_context_delivery();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
