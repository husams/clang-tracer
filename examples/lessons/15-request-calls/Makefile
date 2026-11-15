# Builds the checkout service the way the user guide builds its targets.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie -pthread
LDFLAGS ?= -no-pie -Wl,--build-id

all: target

target: target.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

clean:
	rm -rf target

.PHONY: all clean
