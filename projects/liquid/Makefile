CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -march=native -Xclang -fopenmp -I/opt/homebrew/include -I/opt/homebrew/Cellar/libomp/22.1.6/include
LDFLAGS  := -L/opt/homebrew/lib -L/opt/homebrew/Cellar/libomp/22.1.6/lib -lSDL2 -lSDL2_ttf -pthread -lomp
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
