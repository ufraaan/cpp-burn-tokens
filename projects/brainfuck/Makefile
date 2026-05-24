CXX      := clang++
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra
SRCS     := src/main.cpp src/brainfuck.cpp
OBJS     := $(SRCS:.cpp=.o)
TARGET   := brainfuck

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
