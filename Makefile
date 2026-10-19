CXX ?= c++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wno-misleading-indentation
all: build/ritual-codec
build/ritual-codec: src/main.cpp src/classic_engine.hpp src/audio_input.hpp src/rit_stream.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@
build/test_engine: tests/test_engine.cpp src/classic_engine.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) tests/test_engine.cpp -o $@
test: build/test_engine
	./build/test_engine
clean:
	rm -rf build
.PHONY: all test clean
