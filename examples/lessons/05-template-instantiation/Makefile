# Builds the lesson target and its compilation database.
CXX ?= c++
CXXFLAGS ?= -g -O0 -fno-omit-frame-pointer -fno-pie
LDFLAGS ?= -no-pie -Wl,--build-id

all: target compile_commands.json

target: target.cpp echo.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp -o target

# The compilation database records this directory, so it is generated here.
compile_commands.json: Makefile
	printf '[\n  {\n    "directory": "%s",\n    "file": "target.cpp",\n    "arguments": ["%s", "-g", "-O0", "-fno-pie", "-c", "target.cpp"]\n  }\n]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target compile_commands.json prepared-echo

.PHONY: all clean
