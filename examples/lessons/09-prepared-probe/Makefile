# Builds the target and writes the compile_commands.json that %probe prepare reads.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie -pthread
LDFLAGS ?= -no-pie -Wl,--build-id

all: target compile_commands.json

target: target.cpp add.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

compile_commands.json: Makefile
	printf '[{"directory":"%s","file":"target.cpp","arguments":["%s","-g","-O0","-fno-pie","-c","target.cpp"]}]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target compile_commands.json prepared

.PHONY: all clean
