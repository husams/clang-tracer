# Builds the lesson target as an optimized binary with debug information.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target

target: target.cpp cart.cpp cart.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp cart.cpp -o target

clean:
	rm -rf target line-watch *.db *.db-*

.PHONY: all clean
