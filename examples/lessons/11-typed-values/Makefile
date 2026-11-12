# Builds the lesson target and build/compile_commands.json.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id

all: target build/compile_commands.json

target: src/target.cpp model.hpp
	$(CXX) $(CXXFLAGS) -I. $(LDFLAGS) src/target.cpp -o target

# The compilation database records this directory, so it is generated here.
# It leaves out -fno-pie: the REPL interpreter reuses these flags for its own JIT.
build/compile_commands.json: Makefile
	mkdir -p build
	printf '[\n  {\n    "directory": "%s",\n    "file": "src/target.cpp",\n    "arguments": ["%s", "-g", "-O0", "-I.", "-c", "src/target.cpp"]\n  }\n]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target build mappings.hpp

.PHONY: all clean
