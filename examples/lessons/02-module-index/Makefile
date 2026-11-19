# Builds the lesson target the way the user guide does.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id

all: target

target: target.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

# Separate-debug copy used by the "--debug-file" step.
split: target
	cp target target-split
	objcopy --only-keep-debug target-split target-split.debug
	objcopy --strip-debug --add-gnu-debuglink=target-split.debug target-split

clean:
	rm -rf target target-split target-split.debug *.sqlite recovered-add prepared-add

.PHONY: all split clean
