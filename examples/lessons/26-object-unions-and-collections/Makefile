CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

.PHONY: all clean
all: target

target: target.cpp cart.cpp cart.hpp discount.cpp discount.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp cart.cpp discount.cpp -o $@

clean:
	rm -f target target.sha256
