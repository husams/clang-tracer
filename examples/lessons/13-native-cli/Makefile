# Builds the lesson target the way the user guide does.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie
# -rdynamic exports add so "--function _Z3addii" can find it by linkage name.
LDFLAGS ?= -no-pie -rdynamic -Wl,--build-id

all: target

target: target.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

clean:
	rm -rf target agent.log

.PHONY: all clean
