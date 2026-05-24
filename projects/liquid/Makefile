CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -march=native -I/opt/homebrew/include -Isrc
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -pthread
SRC      := src/main.cpp src/sph.cpp src/renderer.cpp
OBJ      := $(SRC:.cpp=.o)
TARGET   := liquid

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp src/types.hpp src/sph.hpp src/renderer.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
