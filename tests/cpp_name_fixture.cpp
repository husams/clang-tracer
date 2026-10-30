struct UnsupportedType {};

class UnsupportedFixture {
public:
  UnsupportedType unsupported(UnsupportedType value);
};

class X {
public:
  virtual int f(int value);
  int f(long value);
};

class VolatileOnly {
public:
  int v(int value) volatile;
};

class UnsignedOnly {
public:
  int u(unsigned int value);
};
