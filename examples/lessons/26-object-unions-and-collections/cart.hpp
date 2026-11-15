#pragma once

#include <vector>

namespace shop {
struct Line {
  unsigned id;
  int quantity;
};

using Lines = std::vector<Line>;

struct Cart {
  Lines lines;

  void rebuild();
};

// Each function changes the shape of the vector in one way.
void move_to_front(Lines &lines); // rotates the last line to the front
void drop_front(Lines &lines);    // erases the first line
void add_line(Lines &lines);      // appends a new line 12
} // namespace shop
