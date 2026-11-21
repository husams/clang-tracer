#pragma once
// Virtual methods in every shape a probe meets:
//  - Shape: a pure virtual method (no body anywhere), a virtual method with a body, and a
//    non-virtual control; Square overrides both virtuals, Plain only the pure one;
//  - Mixed: a second polymorphic base, so the override reached through Tagged is called through
//    an adjusting thunk;
//  - Counter: a non-polymorphic control for object probes.
// Constructors, destructors and methods are defined out of line in repl_virtual.cpp.
namespace virt {
class Shape {
public:
  Shape();
  virtual ~Shape();
  virtual int sides() const = 0;
  virtual int area() const;
  int plain() const;
};
class Square : public Shape {
public:
  explicit Square(int side);
  ~Square() override;
  int sides() const override;
  int area() const override;

private:
  int side_;
};
class Plain : public Shape {
public:
  Plain();
  ~Plain() override;
  int sides() const override;
};
class Tagged {
public:
  Tagged();
  virtual ~Tagged();
  virtual int tag() const;
};
class Mixed : public Shape, public Tagged {
public:
  Mixed();
  ~Mixed() override;
  int sides() const override;
  int tag() const override;
};
class Counter {
public:
  Counter();
  int add(int amount);

private:
  int value_ = 0;
};
// One pass: every call below happens exactly once, through a base reference where one exists.
int run_once(int pass);
} // namespace virt
