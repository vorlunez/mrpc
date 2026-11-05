CXX ?= g++
AR ?= ar
CPPFLAGS += -Isrc/mlog/include
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra
CXXFLAGS += -pthread -MMD -MP
LDLIBS += -pthread

MLOG_SOURCES := $(wildcard src/mlog/*.cc)
MLOG_OBJECTS := $(patsubst src/mlog/%.cc,build/mlog/%.o,$(MLOG_SOURCES))
TEST_OBJECT := build/tests/mlog_test.o
LIBRARY := build/libmlog.a
TEST := build/mlog_test

.PHONY: all test clean
all: $(LIBRARY) $(TEST)

$(LIBRARY): $(MLOG_OBJECTS)
	$(AR) rcs $@ $^

$(TEST): $(TEST_OBJECT) $(LIBRARY)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/mlog/%.o: src/mlog/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TEST_OBJECT): tests/mlog_test.cc
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: $(TEST)
	./$(TEST)

clean:
	rm -rf build
	rm -f tests/mlog_test.log

-include $(MLOG_OBJECTS:.o=.d) $(TEST_OBJECT:.o=.d)
