#pragma once

#include <array>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace value_examples {

struct Address {
  std::string city;
  int postal_code = 0;
};

// Public fields are captured; methods can be called on the reconstructed copy.
class Account {
public:
  int id = 7;
  std::string owner = "Ada";
  Address address{"London", 100};

  std::string describe() const {
    return owner + " #" + std::to_string(id) + " in " + address.city;
  }
};

using Coordinates = std::array<int, 3>;
using Addresses = std::vector<Address>;
using Numbers = std::list<int>;
using Messages = std::deque<std::string>;
using Matrix = std::vector<std::vector<int>>;
using Counts = std::map<std::string, int>;
using Groups = std::unordered_map<int, std::vector<int>>;
using SortedIds = std::set<int>;
using Labels = std::unordered_set<std::string>;

Account user_types(Address address, Account account);
std::string simple_types(bool enabled, int count, double ratio,
                         const std::string &text,
                         const std::optional<int> &serial);
bool sequence_types(const Coordinates &coordinates, const Addresses &addresses,
                    const Numbers &numbers, const Messages &messages,
                    const Matrix &matrix);
bool map_types(const Counts &counts);
bool unordered_map_types(const Groups &groups);
bool set_types(const SortedIds &ids);
bool unordered_set_types(const Labels &labels);

} // namespace value_examples
