#pragma once

namespace snapshot_unwind {
class State {
  int value_ = 7;

public:
  void fail();
  int value() const { return value_; }
};
}
