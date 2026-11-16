# Builds the lesson target and its compilation database. The target is built
# without optimisation or inlining and with patchable function entries, so
# every method has a clean entry to hook.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -g -O0 -fno-inline -fpatchable-function-entry=8 -fno-omit-frame-pointer -fPIE -fcf-protection=none
LDFLAGS ?= -pie -Wl,--build-id

all: target compile_commands.json

target: target.cpp account.cpp account.hpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) target.cpp account.cpp -o target

# The compilation database records this directory, so it is generated here.
compile_commands.json: Makefile
	printf '[\n  {\n    "directory": "%s",\n    "file": "account.cpp",\n    "arguments": ["%s", "-std=c++17", "-g", "-O0", "-fno-inline", "-fpatchable-function-entry=8", "-fPIE", "-c", "account.cpp"]\n  }\n]\n' \
	  "$(CURDIR)" "$$(command -v $(CXX))" > $@

clean:
	rm -rf target compile_commands.json account.object *.db *.db-*

.PHONY: all clean
