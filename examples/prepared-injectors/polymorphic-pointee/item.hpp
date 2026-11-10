#pragma once

namespace HFW {
struct Prefix {
  virtual ~Prefix();
  int prefix = 12;
};

struct Base {
  virtual ~Base();
  int id = 11;
};

struct Child final : Prefix, Base {
  int x = 42;
};

struct HfwManager {
  unsigned next = 0;
  Base *createItem();
};
} // namespace HFW
