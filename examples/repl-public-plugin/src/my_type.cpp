#include <my_api/my_type.hpp>

namespace my_api {

MyType my_instance{};

void MyType::add(unsigned amount) noexcept {
  count += amount;
}

} // namespace my_api
