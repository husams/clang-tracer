# Builds the lesson target as an optimized binary with debug information.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target

target: target.cpp label.cpp label.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp label.cpp -o target

clean:
	rm -rf target text-watch code-watch *.db *.db-* *.json

.PHONY: all clean
