// Standard-library records as callable results, arguments and receivers, for the automatic
// resolution of their real headers. Everything here must build with GCC 8.5 in C++17.
#pragma once
#include <array>
#include <functional>
#include <map>
#include <random>
#include <string>
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

}  // namespace stdh
