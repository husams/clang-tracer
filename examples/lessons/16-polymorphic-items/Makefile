# Builds the lesson target and its compilation database.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id
HEADERS = item.hpp item_interface.hpp item_v1.hpp item_v2.hpp item_v3.hpp item_factory.hpp

all: target compile_commands.json

target: target.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

# The compilation database records this directory, so it is generated here.
compile_commands.json: Makefile
	printf '[\n  {\n    "directory": "%s",\n    "file": "target.cpp",\n    "arguments": ["%s", "-std=c++17", "-g", "-O0", "-fno-pie", "-c", "target.cpp"]\n  }\n]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target compile_commands.json prepared-create prepared-encode

.PHONY: all clean
