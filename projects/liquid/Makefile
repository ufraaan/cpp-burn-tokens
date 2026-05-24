CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -march=native -I/opt/homebrew/include
LDFLAGS  := -L/opt/homebrew/lib -lSDL2 -pthread
SRC      := src/main.cpp
OBJ      := $(SRC:.cpp=.o)
TARGET   := liquid

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
