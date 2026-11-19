CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -pthread

ifeq ($(OS),Windows_NT)
    LIBS = -lws2_32 -lwsock32
endif

OBJS = build/Utils.o build/UserProfile.o build/Form.o build/Rule.o build/Benefit.o \
       build/Scheme.o build/Eligibility.o build/Combination.o build/Loader.o \
       build/Repository.o build/UsageLog.o build/Api.o

all: scheme_finder

scheme_finder: build/main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

test_engine: tests/test_engine.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $^ $(LIBS)

build/%.o: src/%.cpp $(wildcard src/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: test_engine
	./test_engine

run: scheme_finder
	./scheme_finder

clean:
	rm -rf build scheme_finder test_engine

.PHONY: all test run clean links

links:
	./check_links.sh
