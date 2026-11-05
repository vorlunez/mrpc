CXX ?= g++
AR ?= ar
CPPFLAGS += -Isrc/mlog/include
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
CXXFLAGS += -pthread -MMD -MP
LDLIBS += -pthread

MLOG_SOURCES := $(wildcard src/mlog/*.cc)
MLOG_OBJECTS := $(patsubst src/mlog/%.cc,build/mlog/%.o,$(MLOG_SOURCES))
TEST_NAMES := mlog_test event_base_test
TEST_OBJECTS := $(addprefix build/tests/,$(addsuffix .o,$(TEST_NAMES)))
LIBRARY := build/libmlog.a
TESTS := $(addprefix build/,$(TEST_NAMES))

.PHONY: all test clean
all: $(LIBRARY) $(TESTS)

$(LIBRARY): $(MLOG_OBJECTS)
	$(AR) rcs $@ $^

$(TESTS): build/%: build/tests/%.o $(LIBRARY)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/mlog/%.o: src/mlog/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/tests/%.o: tests/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: $(TESTS)
	./build/mlog_test
	./build/event_base_test

clean:
	rm -rf build
	rm -f tests/mlog_test.log
	rm -f tests/event_base_*.log

-include $(MLOG_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)
