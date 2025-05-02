CXX := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Werror -Woverloaded-virtual -Wunused -Wcast-align -Wconversion -Wmisleading-indentation -Wdouble-promotion -Wimplicit-fallthrough -Wduplicated-branches -Wduplicated-cond -Wlogical-op -Wuseless-cast -fconcepts-diagnostics-depth=3 -MMD -MP

alL: build/server/server build/client/client

build/server/%.o : src/server/%.cpp | build/server 
	$(CXX) -c $(CXXFLAGS) -Isrc/common -Isrc/server $< -o $@

build/client/%.o : src/client/%.cpp | build/client 
	$(CXX) -c $(CXXFLAGS) -Isrc/common -Isrc/client $< -o $@

build/common/%.o : src/common/%.cpp | build/common 
	$(CXX) -c $(CXXFLAGS) -Isrc/common $< -o $@

build/common/handler/%.o : src/common/handler/%.cpp | build/common/handler
	$(CXX) -c $(CXXFLAGS) -Isrc/common $< -o $@

build/server/server: build/common/libcommon.a build/server/main.o
	g++ build/server/main.o build/common/libcommon.a -luring -o build/server/server

build/client/client: build/common/libcommon.a build/client/main.o
	g++ build/client/main.o build/common/libcommon.a -o build/client/client

build/common/libcommon.a: build/common/common.o build/common/handler/HeaderReader.o
	ar rcs build/common/libcommon.a build/common/common.o build/common/handler/HeaderReader.o

build/server: | build
	mkdir build/server

build/client: | build
	mkdir build/client

build/common: | build
	mkdir build/common

build/common/handler: | build/common
	mkdir build/common/handler

build:
	mkdir build

clean:
	rm -rf build

-include build/server/*.d
-include build/client/*.d
-include build/common/*.d
-include build/common/handler/*.d
