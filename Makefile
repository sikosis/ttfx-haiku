CXX ?= c++
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc

TARGET := ttfx
SOURCES := src/main.cpp src/terminal.cpp src/effects.cpp src/registry.cpp src/rng.cpp
BUILD_TAG := $(shell uname -s)-$(shell uname -m)
BUILD_DIR := build/$(BUILD_TAG)
PLATFORM_TARGET := $(BUILD_DIR)/$(TARGET)
TERMINAL_TEST := $(BUILD_DIR)/terminal-session-test
OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean test install

all: $(PLATFORM_TARGET)
	cp $(PLATFORM_TARGET) $(TARGET)

$(PLATFORM_TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDENCIES)

$(TERMINAL_TEST): tests/terminal_session.cpp src/terminal.cpp src/terminal.h
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/terminal_session.cpp src/terminal.cpp -o $@

test: all $(TERMINAL_TEST)
	./tests/smoke.sh ./$(TARGET)
	./tests/terminal-session.sh ./$(TERMINAL_TEST)

install: all
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/ttfx"

clean:
	rm -rf build $(TARGET)

PREFIX ?= /boot/system/non-packaged
