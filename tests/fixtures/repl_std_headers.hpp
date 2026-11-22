// Standard-library records as callable results, arguments and receivers, for the automatic
// resolution of their real headers. Everything here must build with GCC 8.5 in C++17.
#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stdh {

struct Person {
  int id;
  double score;
};

class Record {
public:
  Record();
  std::string label() const;
  int count() const;

private:
  long n_ = 7;
  std::string s_;
};

// A program-defined template whose arguments take every kind DWARF can describe: a type, an
// integral, a boolean, a character and an enumeration constant, a template and a parameter
// pack holding pointer, array and function types.
enum class Mode : unsigned char { Slow = 1, Fast = 7 };
template <class T>
struct Wrapper {
  T value;
};
template <class T, int N, bool B, char C, Mode M, template <class> class Tmpl, class... Rest>
struct Gadget {
  T first;
  Tmpl<T> second;
  int total = N;
};
using Probe = Gadget<const char *, -3, true, 'z', Mode::Fast, Wrapper, long, int[3], int(int),
                     double (*)(int)>;
int inspect(const Probe &gadget);
Probe make_gadget();

std::string make_label(int n);
std::vector<Person> make_people(int n);
int sum_people(const std::vector<Person> &people);
std::array<int, 4> make_array(int base);
std::map<int, int, std::greater<int>> make_ordered(int n);
std::vector<std::pair<int, std::string>> make_pairs(int n);
// A record no built-in header mapping covers, passed by reference.
double midpoint(const std::uniform_real_distribution<double> &range);

// Element types whose DWARF spelling differs from the compiler's (long int, long unsigned int).
std::vector<long> make_longs(int n);
std::vector<std::size_t> make_sizes(int n);
// The only standard type is std::string_view: no dual-ABI class is involved.
std::size_t view_size(std::string_view text);
// <chrono> also supplies the records its types name as arguments (std::ratio, the clock).
std::chrono::milliseconds make_delay(int n);
std::chrono::system_clock::time_point make_stamp(int n);
// A callable type whose DWARF spelling differs from the compiler's (long int).
long call_twice(const std::function<long(long)> &function, long value);
// Program enumerations as template arguments: scoped with a fixed type, and unscoped.
enum Level { Low, High = 300 };
std::vector<Mode> make_modes(int n);
std::optional<Level> make_level(int n);
// A program record with a std::string member, which DWARF recovery treats as opaque: a
// container that only points at it works, one that holds it by value needs its header.
struct Named {
  std::string name;
  int rank;
};
std::vector<Named> make_named(int n);
std::optional<Named> find_named(int rank);
// A constant of a signed type that needs more than seven bits, an array of unknown bound, a
// multi-dimensional array and a pointer to function as template arguments.
long ratio_den(const std::ratio<1, 200> &ratio);
int sum_array(const std::unique_ptr<int[]> &values, int count);
int grid_sum(const std::array<int[2][3], 1> &grid);
int apply_all(const std::map<int, int (*)(int)> &handlers, int value);

}  // namespace stdh
