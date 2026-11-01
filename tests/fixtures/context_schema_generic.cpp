#include <string>
#include <vector>

namespace generic_payload {
struct Scope {};
struct Record {
  int id = 0;
  std::string label;
};
using Text = std::string;
using Values = std::vector<int>;
using Object = Record;
} // namespace generic_payload
