# Build a fixture and record the same compiler invocation for source preparation.
OBJECTS := $(SOURCES:%.cpp=build/%.o)
COMPILE_DB := build/compile_commands.json

.PHONY: all clean FORCE
.DELETE_ON_ERROR:

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: %.cpp $(HEADERS) $(COMPILE_DB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Recheck compiler options each run. The writer preserves the timestamp when
# the compilation database is identical, so unchanged objects stay up to date.
$(COMPILE_DB): FORCE | build
	@python3 ../write-compile-commands.py $@ $(SOURCES) -- $(CXX) $(CPPFLAGS) $(CXXFLAGS)

build:
	mkdir -p $@

clean:
	rm -f $(TARGET) $(OBJECTS) $(COMPILE_DB)
