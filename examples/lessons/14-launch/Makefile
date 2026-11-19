# Builds a target with the launch checkpoint from <tracer/launch.hpp>.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id
# Headers of the installed Clang Tracer. Override when installed elsewhere.
TRACER_INCLUDE ?= /usr/local/opt/clang-tracer/current/include

all: target

target: target.cpp
	$(CXX) $(CXXFLAGS) -I$(TRACER_INCLUDE) $(LDFLAGS) target.cpp -o target

clean:
	rm -rf target

.PHONY: all clean
