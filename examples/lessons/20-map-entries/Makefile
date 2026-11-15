# Builds the lesson target as an optimized binary with debug information.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target

target: target.cpp stock.cpp stock.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp stock.cpp -o target

clean:
	rm -rf target price-watch *.db *.db-* *.json

.PHONY: all clean
