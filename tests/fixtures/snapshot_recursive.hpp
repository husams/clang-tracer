#pragma once
// Record types that contain themselves, for the automatic capture tests.
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace recursive_fixture {

// By value, through a container.
struct TreeNode {
  int v = 0;
  std::string name;
  std::vector<TreeNode> kids;
};

// Through an owning pointer.
struct ListNode {
  int v = 0;
  std::unique_ptr<ListNode> next;
};

// Through shared pointers: cycles and objects reached twice are possible.
struct SharedNode {
  int v = 0;
  std::shared_ptr<SharedNode> other;
  std::shared_ptr<SharedNode> next;
  std::string text;
};

// Two records that contain each other.
struct Folder;
struct Entry {
  std::string name;
  std::unique_ptr<Folder> folder;
};
struct Folder {
  int id = 0;
  std::vector<Entry> entries;
  std::map<std::string, Folder> named;
};

// Private members and a base that holds the derived record.
class Hidden;
class HiddenBase {
  std::vector<Hidden> kids_;

public:
  std::vector<Hidden> &kids() { return kids_; }
};
class Hidden : public HiddenBase {
  int v_ = 0;

public:
  explicit Hidden(int v = 0) : v_(v) {}
  int v() const { return v_; }
};

// Through containers of containers.
struct Grid {
  int id = 0;
  std::vector<std::vector<Grid>> cells;
  std::map<int, std::vector<Grid>> index;
};

// A record that does not contain itself but holds records that do.
struct Holder {
  std::shared_ptr<SharedNode> left;
  std::shared_ptr<SharedNode> right;
  std::unique_ptr<Folder> folder;
  int tail = 0;
};

} // namespace recursive_fixture
