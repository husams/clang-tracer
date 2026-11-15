# Builds the lesson target and its compilation database.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O2 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target compile_commands.json

target: target.cpp cart.cpp cart.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp cart.cpp -o target

# The compilation database records this directory, so it is generated here.
compile_commands.json: Makefile
	printf '[\n  {\n    "directory": "%s",\n    "file": "cart.cpp",\n    "arguments": ["%s", "-std=c++17", "-g", "-O2", "-fPIE", "-c", "cart.cpp"]\n  }\n]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target compile_commands.json cart-watch generated *.db *.db-*

.PHONY: all clean
