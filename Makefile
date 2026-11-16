# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res/Symbolic.svg
DISTRIBUTABLES += $(wildcard LICENSE*)

ifneq ($(MAKECMDGOALS),test)
include $(RACK_DIR)/plugin.mk
endif

# Engine checks, no Rack SDK needed (also writes test/out/*.wav)
test:
	$(CXX) -O3 -funsafe-math-optimizations -Wall -Wextra -std=c++17 -Isrc test/engine_test.cpp -o test/engine_test && test/engine_test
.PHONY: test
