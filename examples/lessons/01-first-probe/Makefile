# Builds the lesson target the way the user guide does.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id

all: target

target: target.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

clean:
	rm -rf target 

.PHONY: all clean
