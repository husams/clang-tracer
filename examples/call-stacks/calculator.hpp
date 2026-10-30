#pragma once

class StackCalculator {
public:
  int bias = 7;
  int calls = 0;
  int multiply(int value, int factor);
};

int stack_add(int left, int right);
int library_dispatch(StackCalculator &calculator, int value, int factor);
