# Builds libdemo.so with debug info, and a target that links to it.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie -pthread

all: target

libdemo.so: demo.cpp demo.hpp
	$(CXX) $(CXXFLAGS) -fPIC -shared demo.cpp -o $@

target: target.cpp demo.hpp libdemo.so
	$(CXX) $(CXXFLAGS) -no-pie -Wl,--build-id target.cpp -L. -ldemo -Wl,-rpath,'$$ORIGIN' -o $@

clean:
	rm -rf target libdemo.so *.tracer.sqlite recovered prepared-score

.PHONY: all clean
