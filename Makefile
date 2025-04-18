CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Werror -Woverloaded-virtual -Wunused -Wcast-align -Wconversion -Wmisleading-indentation -Wdouble-promotion -Wimplicit-fallthrough -Wduplicated-branches -Wduplicated-cond -Wlogical-op -Wuseless-cast -fconcepts-diagnostics-depth=3 -MMD -MP

alL: build/server/server build/client/client

build/server/%.o : src/server/%.cpp | build/server 
	$(CXX) -c $(CXXFLAGS) -Isrc/common -Isrc/server $< -o $@

build/client/%.o : src/client/%.cpp | build/client 
	$(CXX) -c $(CXXFLAGS) -Isrc/common -Isrc/client $< -o $@

build/common/%.o : src/common/%.cpp | build/common 
	$(CXX) -c $(CXXFLAGS) -Isrc/common $< -o $@

build/server/server: build/common/libcommon.a build/server/main.o
	g++ build/server/main.o build/common/libcommon.a -o build/server/server

build/client/client: build/common/libcommon.a build/client/main.o
	g++ build/client/main.o build/common/libcommon.a -o build/client/client

build/common/libcommon.a: build/common/common.o
	ar rcs build/common/libcommon.a build/common/common.o

build/server: | build
	mkdir build/server

build/client: | build
	mkdir build/client

build/common: | build
	mkdir build/common

build:
	mkdir build

clean:
	rm -rf build

-include build/server/main.d
-include build/client/main.d
-include build/common/common.d
