#pragma once

#include <any>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace snapshot_test {
struct Primary {
  virtual ~Primary();
  int primary = 2;
};
struct Secondary {
  virtual ~Secondary();
  int secondary = 3;
};
struct Known final : Primary, Secondary {
  int visible = 11;
};
struct MoveBomb {
  int payload = 42;
  MoveBomb() = default;
  explicit MoveBomb(int error);
  MoveBomb(const MoveBomb &) = delete;
  MoveBomb &operator=(const MoveBomb &) = delete;
  MoveBomb(MoveBomb &&);
  MoveBomb &operator=(MoveBomb &&);
};
struct Registered {
  int value;
};
class State {
  std::vector<std::unique_ptr<int>> items_;
  std::vector<int> large_ = std::vector<int>(4300, 21);
  std::vector<std::vector<int>> nested_{{3, 4}, {7, 8}};
  std::map<int, int> map_{{7, 10}, {9, 20}};
  std::unordered_map<int, int> hash_{{1, 10}, {2, 20}, {3, 30}};
  std::variant<int, int, MoveBomb> note_{std::in_place_index<0>, 5};
  std::any extension_{Registered{12}};
  Secondary *child_;
  Secondary *alias_;

public:
  explicit State(Secondary &child);
  void rehash_only();
  void update_registered();
  void mutate();
  void make_valueless();
  void replace_child();
};
} // namespace snapshot_test
