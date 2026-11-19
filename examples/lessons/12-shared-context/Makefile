# Builds the shared-context target without a CMake build directory.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fcf-protection=none -pthread

all: shared-context-target

shared-context-target: target.cpp
	$(CXX) $(CXXFLAGS) target.cpp -o shared-context-target

clean:
	rm -f shared-context-target server.log target.log

.PHONY: all clean
