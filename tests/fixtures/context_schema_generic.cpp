#include <cstddef>
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

// Values the wire format has no scalar for (issue #181).
union Number {
  int i;
  float f;
};
enum class Big : unsigned long long { large = 1ull << 40, top = ~0ull };
enum class Low : long long { low = -9223372036854775807LL - 1 };
struct Key {
  int a = 1;
  int b = 2;
  virtual ~Key() = default;
  int twice(int n) { return 2 * n; }
  virtual int scaled(int n) { return 3 * n; }
};
struct Scalars {
  Number number{5};
  union {
    int i = 6;
    float f;
  };
  long double extended = 1.0L + 0x1p-60L;
  __int128 wide = -((static_cast<__int128>(1) << 100) + 5);
  unsigned __int128 uwide = ~static_cast<unsigned __int128>(0) - 1;
  Big big = Big::top;
  Low low = Low::low;
  int Key::*data = &Key::b;
  int Key::*no_data = nullptr;
  int (Key::*function)(int) = &Key::twice;
  int (Key::*virtual_function)(int) = &Key::scaled;
  std::nullptr_t null = nullptr;
  volatile int counter = 7;
  std::vector<Number> numbers{Number{8}};
};
using Abi = Scalars;
} // namespace generic_payload
