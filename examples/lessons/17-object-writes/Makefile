# Builds the lesson target as an optimized binary with debug information.
# The object watch needs the constructor, destructor and writer as real
# functions, so they live in their own source file.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target

target: target.cpp account.cpp account.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp account.cpp -o target

clean:
	rm -rf target account-watch *.db *.db-*

.PHONY: all clean
